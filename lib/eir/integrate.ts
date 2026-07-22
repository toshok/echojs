/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// EIR integration: lower the whole module — toplevel statements,
// import/export init, every nested function, and the export accessor
// functions — as one EIR unit (see collectEIRToplevel).
//
// module-scope free names resolve through the refs map built here:
//   - true globals (console, Math, ...): lowered as get_global;
//   - named imports from non-native modules: lowered as module_slot_load
//     (or folded, when the export is a const literal);
//   - this module's own exported bindings: module_slot_load/store against
//     the "%self" module global;
//   - non-exported module-level bindings with literal initializers that
//     are never reassigned: folded to the literal;
//   - non-exported module-level vars promoted to hidden slots by
//     gather-imports: module_slot_load/store on "%self".
// anything else unsupported throws LowerNotSupported, which compile()
// reports as a compile error — there is no other pipeline.

import * as b from "../ast-builder";
import * as debug from "../debug";
import { ScopeAnalysis } from "./scopes";
import { lowerAnalyzedFunction } from "./lower";
import type { ModuleRef, ModCtx } from "./lower";
import { isLowerNotSupported } from "./errors";
import { Module } from "./ir";
import { FunctionBuilder } from "./builder";
import { verifyModule } from "./verifier";
import { injectLowTierProbes } from "./lowtier-probe";
import { optimizeModule } from "./optimize";
import { printModule } from "./printer";
import type * as e from "../estree";
import type { ModuleInfo } from "../module-info";
import type { CompilerOptions } from "../options";

// one export's accessor pair, by EIR function name (compiler.ts resolves
// them against the emitted module in emitModuleResolution)
export interface ModuleAccessor {
    key: string;
    getter: string;
    setter: string;
}

export type CollectResult =
    | { eir_module: Module; accessors: ModuleAccessor[]; error?: undefined }
    | { error: string; eir_module?: undefined; accessors?: undefined };

// --dump-after eir: print the lowered (verified) EIR module
function dumpRequested(options: CompilerOptions | undefined): boolean {
    return !!(options && options.debug_passes && options.debug_passes.has("eir"));
}

// --dump-after eir-opt: print the module again after optimization
function dumpOptRequested(options: CompilerOptions | undefined): boolean {
    return !!(options && options.debug_passes && options.debug_passes.has("eir-opt"));
}

function dumpModule(filename: string, mode: string, eir_module: Module): void {
    console.log(`// EIR module for ${filename} (${mode})`);
    console.log(printModule(eir_module));
}

// only primitive literals fold; regex literals are objects and need
// runtime construction
function isFoldableLiteral(n: e.Expression | null | undefined): n is e.Literal {
    return !!n && n.type === "Literal" && (n.value === null || typeof n.value !== "object");
}

// the module-slot reference map: local name -> { module, slot, constval?,
// writable }.  covers named imports and this module's own exported
// bindings.
function collectModuleRefs(
    toplevelBody: e.Statement[],
    module_infos: Map<string, ModuleInfo> | null,
    this_module_info: ModuleInfo | null
): Map<string, ModuleRef> {
    let refs = new Map();

    // imports.  native modules ("@llvm" etc) share the ModuleInfo slot
    // machinery with JS modules — named imports from either kind are slot
    // loads, exactly like the legacy %moduleGetSlot path.  namespace
    // imports bind the module object itself (module_get_exotic); member
    // accesses on it are ordinary property gets.
    if (module_infos) {
        for (let stmt of toplevelBody) {
            if (stmt.type !== "ImportDeclaration") continue;
            if (!stmt.source_path) continue;
            let moduleString = stmt.source_path.value;
            let module_info = module_infos.get(moduleString);
            if (!module_info) continue;
            for (let spec of stmt.specifiers) {
                if (spec.type === "ImportNamespaceSpecifier") {
                    // module_info rides along so lowering can resolve
                    // ns.member accesses to slot loads at compile time
                    // (mirroring new-cc's visitMemberExpression rewrite —
                    // JS module objects don't support runtime property
                    // lookup of their exports)
                    refs.set(spec.local.name, {
                        exotic: moduleString,
                        module_info: module_info,
                        writable: false,
                    });
                    continue;
                }
                // named/default imports resolve through slot loads, which
                // need the module's link-time global — natives don't have
                // one (their module object only exists at runtime)
                if (module_info.isNative()) continue;
                let imported_name;
                if (spec.type === "ImportSpecifier") imported_name = spec.imported.name;
                else if (spec.type === "ImportDefaultSpecifier") imported_name = "default";
                else continue;
                let export_info = module_info.exports.get(imported_name);
                if (!export_info || export_info.promoted) continue;
                const entry: import("./lower").SlotRef = {
                    module: moduleString,
                    slot: export_info.slot_num,
                    writable: false,
                };
                // const exports fold to their literal at compile time
                // (matches new-cc's constval propagation)
                if (isFoldableLiteral(export_info.constval))
                    entry.constval = export_info.constval;
                refs.set(spec.local.name, entry);
            }
        }
    }

    // this module's own exported let/var/const bindings, via the "%self"
    // module global.  only declaration-form exports resolve this way;
    // specifier-only exports (`export { X }`) alias another binding whose
    // own resolution stands.  exported names shadow same-named imports,
    // so these are set second.
    if (this_module_info) {
        for (let wrapped of toplevelBody) {
            if (wrapped.type !== "ExportNamedDeclaration") continue;
            let decl = wrapped.declaration;
            if (!decl || Array.isArray(decl)) continue;
            if (decl.type === "VariableDeclaration") {
                let is_const = decl.kind === "const";
                for (let d of decl.declarations) {
                    if (d.id.type !== "Identifier") continue;
                    if (!this_module_info.exports.has(d.id.name)) continue;
                    const export_info = this_module_info.exports.get(d.id.name)!;
                    const entry: import("./lower").SlotRef = {
                        module: "%self",
                        slot: export_info.slot_num,
                        writable: !is_const,
                    };
                    if (is_const && isFoldableLiteral(export_info.constval)) {
                        entry.constval = export_info.constval;
                        entry.writable = false;
                    }
                    refs.set(d.id.name, entry);
                }
            } else if (
                (decl.type === "FunctionDeclaration" || decl.type === "ClassDeclaration") &&
                decl.id
            ) {
                // an exported function/class read in value position loads
                // the slot the legacy toplevel stored the (single) closure
                // in — identity-correct, unlike minting a new closure per
                // reference.  writes fall back (writable: false).
                if (!this_module_info.exports.has(decl.id.name)) continue;
                const export_info = this_module_info.exports.get(decl.id.name)!;
                refs.set(decl.id.name, {
                    module: "%self",
                    slot: export_info.slot_num,
                    writable: false,
                });
            }
        }
    }

    // non-exported module-level vars promoted to hidden slots by
    // gather-imports: read/write through the "%self" module global, the
    // same storage the legacy pipeline uses after the DesugarImportExport
    // rewrite.  const-declared ones (non-literal initializers) are
    // read-only.
    if (this_module_info) {
        for (let stmt of toplevelBody) {
            if (stmt.type === "VariableDeclaration") {
                for (let d of stmt.declarations) {
                    if (d.id.type !== "Identifier") continue;
                    if (refs.has(d.id.name)) continue;
                    let export_info = this_module_info.exports.get(d.id.name);
                    if (!export_info || !export_info.promoted) continue;
                    refs.set(d.id.name, {
                        module: "%self",
                        slot: export_info.slot_num,
                        writable: stmt.kind !== "const",
                    });
                }
            } else if (
                (stmt.type === "FunctionDeclaration" || stmt.type === "ClassDeclaration") &&
                stmt.id
            ) {
                if (refs.has(stmt.id.name)) continue;
                let export_info = this_module_info.exports.get(stmt.id.name);
                if (!export_info || !export_info.promoted) continue;
                refs.set(stmt.id.name, {
                    module: "%self",
                    slot: export_info.slot_num,
                    writable: true,
                });
            }
        }
    }

    return refs;
}

// non-exported module-level bindings with literal initializers that are
// never reassigned: fold-only refs (no slot)
function addModuleConstLiterals(
    toplevelBody: e.Statement[],
    assigned: Set<string>,
    refs: Map<string, ModuleRef>
): void {
    for (let stmt of toplevelBody) {
        if (stmt.type !== "VariableDeclaration") continue; // exported ones already in refs
        for (let d of stmt.declarations) {
            if (d.id.type !== "Identifier") continue;
            if (!isFoldableLiteral(d.init)) continue;
            if (assigned.has(d.id.name)) continue;
            if (refs.has(d.id.name)) continue;
            refs.set(d.id.name, { module: null, slot: -1, constval: d.init, writable: false });
        }
    }
}

// module-scope names that are ever assigned at the top level; calls into
// those can't be made direct and their literals can't fold
function collectAssignedNames(toplevelBody: e.Statement[]): Set<string> {
    const assigned = new Set<string>();
    // reflective object-graph walk (the same legitimate-unknown seam as
    // gather-imports' var scanner)
    const walk = (n: unknown): void => {
        if (!n || typeof n !== "object") return;
        if (Array.isArray(n)) {
            for (const el of n) walk(el);
            return;
        }
        const node = n as e.Node;
        // conservatively descend everywhere, including into nested
        // functions: a nested assignment to a module-scope name still
        // invalidates direct calls / const folding.
        if (node.type === "AssignmentExpression" && node.left && node.left.type === "Identifier")
            assigned.add(node.left.name);
        if (node.type === "UpdateExpression" && node.argument && node.argument.type === "Identifier")
            assigned.add(node.argument.name);
        for (const k of Object.keys(node)) {
            if (k === "loc") continue;
            walk((node as unknown as Record<string, unknown>)[k]);
        }
    };
    walk(toplevelBody);
    return assigned;
}

// each non-promoted export gets a getter (and setter) function on the
// module object so importers resolve it lazily.  these used to be tiny
// AST FunctionExpressions compiled by the legacy visitor — the last
// thing it compiled; they're built directly as EIR now.  getters fold
// primitive const exports (matching the legacy getExportGetter);
// everything else loads the export's slot on "%self".
function uniqueFnName(eir_module: Module, base: string): string {
    let names = new Set(eir_module.functions.map((f) => f.name));
    let name = base;
    for (let i = 1; names.has(name); i++) name = `${base}$${i}`;
    return name;
}

function buildModuleAccessors(eir_module: Module, this_module_info: ModuleInfo): ModuleAccessor[] {
    const accessors: ModuleAccessor[] = [];
    this_module_info.exports.forEach((export_info, key) => {
        if (export_info.promoted) return; // hidden slot: no accessors

        let getter_name = uniqueFnName(eir_module, `get_export_${key}`);
        {
            let fb = new FunctionBuilder(getter_name, ["%env", "%this"]);
            let cv = export_info.constval;
            let v;
            if (cv && cv.type === "Literal" && cv.value === null) v = fb.constNull();
            else if (cv && cv.type === "Literal" && typeof cv.value === "number")
                v = fb.constNumber(cv.value);
            else if (cv && cv.type === "Literal" && typeof cv.value === "string")
                v = fb.constAtom(cv.value);
            else if (cv && cv.type === "Literal" && typeof cv.value === "boolean")
                v = fb.constBool(cv.value);
            else
                v = fb.emit("module_slot_load", [], {
                    module: "%self",
                    slot: export_info.slot_num,
                });
            fb.emit("return", [v], {});
            eir_module.addFunction(fb.fn);
        }

        let setter_name = uniqueFnName(eir_module, `set_export_${key}`);
        {
            let fb = new FunctionBuilder(setter_name, ["%env", "%this", "value"]);
            let v = fb.readVariable("value", fb.cur);
            fb.emit("module_slot_store", [v], {
                module: "%self",
                slot: export_info.slot_num,
            });
            fb.emit("return", [fb.constUndefined()], {});
            eir_module.addFunction(fb.fn);
        }

        accessors.push({ key: key, getter: getter_name, setter: setter_name });
    });
    return accessors;
}

// lower the WHOLE module — toplevel statements, import/export init, and
// every nested function — as one EIR unit.  module-scope bindings
// resolve through the refs machinery (slots for exported/promoted
// names, const-literal folds); everything else is an ordinary toplevel
// local, captured into the toplevel's environment as needed.  returns
// { eir_module, accessors } on success or { error } when something
// doesn't lower — which compile() turns into a compile error.  the
// emitted module is wrapped by compiler.js's module-resolution
// scaffolding (see emitEIRToplevel).
// `export default function f() {}` declares a module-scope binding AND
// stores the default-export slot.  normalize to the two statements that
// say exactly that; unnamed `export default function () {}` is just an
// expression-form default export.
function normalizeDefaultExports(body: e.Statement[]): void {
    for (let i = 0; i < body.length; i++) {
        const stmt = body[i]!;
        if (stmt.type !== "ExportDefaultDeclaration") continue;
        let decl = stmt.declaration;
        if (!decl) continue;
        if (decl.type === "FunctionDeclaration") {
            if (decl.id) {
                stmt.declaration = b.identifier(decl.id.name);
                body.splice(i, 0, decl);
                i++;
            } else {
                // an unnamed default function is just an expression-form
                // default export (in-place retype)
                (decl as { type: string }).type = "FunctionExpression";
            }
        } else if (
            decl.type === "VariableDeclaration" &&
            decl.declarations.length === 1 &&
            decl.declarations[0]!.id.type === "Identifier"
        ) {
            // `export default class Foo {}` arrives here post-DesugarClasses
            // as `let Foo = <class expr>`
            stmt.declaration = b.identifier((decl.declarations[0]!.id as e.Identifier).name);
            body.splice(i, 0, decl);
            i++;
        }
    }
}

export function collectEIRToplevel(
    tree: e.Program,
    filename: string,
    module_infos: Map<string, ModuleInfo> | null,
    this_module_info: ModuleInfo,
    options: CompilerOptions
): CollectResult {
    const toplevel = tree.body[0] as e.FunctionDeclaration;
    const body = toplevel.body.body;
    normalizeDefaultExports(body);

    let assigned = collectAssignedNames(body);
    let refs = collectModuleRefs(body, module_infos, this_module_info);
    addModuleConstLiterals(body, assigned, refs);

    let moduleSlotNames = new Set(refs.keys());

    try {
        let analysis = new ScopeAnalysis();
        let info = analysis.analyzeToplevel(toplevel, toplevel.id.name, moduleSlotNames);

        // module functions call each other through their slots
        // (slot-load + invoke_closure): a slot-backed function may capture
        // the toplevel environment, which a direct caller's envParam
        // wouldn't carry.  direct calls stay a devirtualization
        // opportunity for the optimizer, which can prove capture shapes.
        let mod_ctx = {
            refs: refs,
            this_module_info: this_module_info,
            module_infos: module_infos,
        };

        let eir_module = new Module(filename);
        lowerAnalyzedFunction(info, analysis, eir_module, mod_ctx);
        let accessors = buildModuleAccessors(eir_module, this_module_info);
        verifyModule(eir_module);
        // the as-lowered dump must precede optimization (which mutates
        // the module in place)
        if (dumpRequested(options)) dumpModule(filename, "toplevel-as-EIR", eir_module);

        // testing: EJS_EIR_LOWTIER=1 swaps the bodies of the lowtier_*
        // probe functions (test/eir-lowtier1.js) for hand-built low-tier
        // EIR, so the Phase 2 ops can be executed end to end before
        // lowering emits them (Phase 3).  Same mold as EJS_NO_EIR_OPT.
        if (process.env["EJS_EIR_LOWTIER"]) {
            const n = injectLowTierProbes(eir_module);
            if (n > 0) verifyModule(eir_module);
        }

        // debugging/measurement: EJS_NO_EIR_OPT=1 disables the EIR
        // optimizer without touching the LLVM pass pipeline (-O0 changes
        // both), mirroring the EJS_NO_PROMOTE bisect hook
        if (options.opt_level > 0 && !process.env["EJS_NO_EIR_OPT"]) {
            const stats = optimizeModule(eir_module);
            if (
                stats.allocs_sunk ||
                stats.reads_folded ||
                stats.calls_inlined ||
                stats.iters_folded ||
                stats.dead_removed
            )
                debug.log(
                    1,
                    `EIR-opt: ${filename}: ${stats.calls_inlined} call(s) inlined, ` +
                        `${stats.allocs_sunk} alloc(s) sunk, ${stats.reads_folded} read(s) folded, ` +
                        `${stats.iters_folded} iterator walk(s) folded, ` +
                        `${stats.dead_removed} dead inst(s) removed`
                );
            verifyModule(eir_module);
            if (dumpOptRequested(options)) dumpModule(filename, "optimized", eir_module);
        }

        toplevel.eir_module = eir_module;
        toplevel.eir_main = info.name;
        toplevel.body = { type: "BlockStatement", body: [], loc: toplevel.loc };
        debug.log(1, `EIR: ${filename}: whole module lowered (toplevel-as-EIR)`);
        return { eir_module: eir_module, accessors: accessors };
    } catch (e) {
        if (!isLowerNotSupported(e)) throw e;
        // there is no legacy pipeline to fall back to anymore: surface
        // the reason as a compile error at the call site
        return { error: e.message };
    }
}

