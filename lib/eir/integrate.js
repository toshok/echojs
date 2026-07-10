/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
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
//     the "%self" module global (const-literal exports fold; exported
//     functions/classes read in value position load the slot, so closure
//     identity is preserved);
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
import { LowerNotSupported, isLowerNotSupported } from "./errors";
import { Module } from "./ir";
import { FunctionBuilder } from "./builder";
import { verifyModule } from "./verifier";
import { printModule } from "./printer";

// --dump-after eir: print the lowered (verified) EIR module
function dumpRequested(options) {
    return options && options.debug_passes && options.debug_passes.has("eir");
}

function dumpModule(filename, mode, eir_module) {
    console.log(`// EIR module for ${filename} (${mode})`);
    console.log(printModule(eir_module));
}

// only primitive literals fold; regex literals are objects and need
// runtime construction
function isFoldableLiteral(n) {
    return n && n.type === b.Literal && (n.value === null || typeof n.value !== "object");
}

// the module-slot reference map: local name -> { module, slot, constval?,
// writable }.  covers named imports and this module's own exported
// bindings.
function collectModuleRefs(toplevelBody, module_infos, this_module_info) {
    let refs = new Map();

    // imports.  native modules ("@llvm" etc) share the ModuleInfo slot
    // machinery with JS modules — named imports from either kind are slot
    // loads, exactly like the legacy %moduleGetSlot path.  namespace
    // imports bind the module object itself (module_get_exotic); member
    // accesses on it are ordinary property gets.
    if (module_infos) {
        for (let stmt of toplevelBody) {
            if (stmt.type !== b.ImportDeclaration) continue;
            if (!stmt.source_path) continue;
            let moduleString = stmt.source_path.value;
            let module_info = module_infos.get(moduleString);
            if (!module_info) continue;
            for (let spec of stmt.specifiers) {
                if (spec.type === b.ImportNamespaceSpecifier) {
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
                if (spec.type === b.ImportSpecifier) imported_name = spec.imported.name;
                else if (spec.type === b.ImportDefaultSpecifier) imported_name = "default";
                else continue;
                let export_info = module_info.exports.get(imported_name);
                if (!export_info || export_info.promoted) continue;
                let entry = {
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
            if (wrapped.type !== b.ExportNamedDeclaration) continue;
            let decl = wrapped.declaration;
            if (!decl || Array.isArray(decl)) continue;
            if (decl.type === b.VariableDeclaration) {
                let is_const = decl.kind === "const";
                for (let d of decl.declarations) {
                    if (d.id.type !== b.Identifier) continue;
                    if (!this_module_info.exports.has(d.id.name)) continue;
                    let export_info = this_module_info.exports.get(d.id.name);
                    let entry = {
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
                (decl.type === b.FunctionDeclaration || decl.type === b.ClassDeclaration) &&
                decl.id
            ) {
                // an exported function/class read in value position loads
                // the slot the legacy toplevel stored the (single) closure
                // in — identity-correct, unlike minting a new closure per
                // reference.  writes fall back (writable: false).
                if (!this_module_info.exports.has(decl.id.name)) continue;
                let export_info = this_module_info.exports.get(decl.id.name);
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
            if (stmt.type === b.VariableDeclaration) {
                for (let d of stmt.declarations) {
                    if (d.id.type !== b.Identifier) continue;
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
                (stmt.type === b.FunctionDeclaration || stmt.type === b.ClassDeclaration) &&
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
function addModuleConstLiterals(toplevelBody, assigned, refs) {
    for (let stmt of toplevelBody) {
        if (stmt.type !== b.VariableDeclaration) continue; // exported ones already in refs
        for (let d of stmt.declarations) {
            if (d.id.type !== b.Identifier) continue;
            if (!isFoldableLiteral(d.init)) continue;
            if (assigned.has(d.id.name)) continue;
            if (refs.has(d.id.name)) continue;
            refs.set(d.id.name, { module: null, slot: -1, constval: d.init, writable: false });
        }
    }
}

// module-scope names that are ever assigned at the top level; calls into
// those can't be made direct and their literals can't fold
function collectAssignedNames(toplevelBody) {
    let assigned = new Set();
    let walk = (n) => {
        if (!n || typeof n !== "object") return;
        if (Array.isArray(n)) {
            for (let el of n) walk(el);
            return;
        }
        // conservatively descend everywhere, including into nested
        // functions: a nested assignment to a module-scope name still
        // invalidates direct calls / const folding.
        if (n.type === b.AssignmentExpression && n.left && n.left.type === b.Identifier)
            assigned.add(n.left.name);
        if (n.type === b.UpdateExpression && n.argument && n.argument.type === b.Identifier)
            assigned.add(n.argument.name);
        for (let k of Object.keys(n)) {
            if (k === "loc") continue;
            walk(n[k]);
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
function uniqueFnName(eir_module, base) {
    let names = new Set(eir_module.functions.map((f) => f.name));
    let name = base;
    for (let i = 1; names.has(name); i++) name = `${base}$${i}`;
    return name;
}

function buildModuleAccessors(eir_module, this_module_info) {
    let accessors = [];
    this_module_info.exports.forEach((export_info, key) => {
        if (export_info.promoted) return; // hidden slot: no accessors

        let getter_name = uniqueFnName(eir_module, `get_export_${key}`);
        {
            let fb = new FunctionBuilder(getter_name, ["%env", "%this"]);
            let cv = export_info.constval;
            let v;
            if (cv && cv.type === b.Literal && cv.value === null) v = fb.constNull();
            else if (cv && cv.type === b.Literal && typeof cv.value === "number")
                v = fb.constNumber(cv.value);
            else if (cv && cv.type === b.Literal && typeof cv.value === "string")
                v = fb.constAtom(cv.value);
            else if (cv && cv.type === b.Literal && typeof cv.value === "boolean")
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
function normalizeDefaultExports(body) {
    for (let i = 0; i < body.length; i++) {
        let stmt = body[i];
        if (stmt.type !== b.ExportDefaultDeclaration) continue;
        let decl = stmt.declaration;
        if (!decl) continue;
        if (decl.type === b.FunctionDeclaration) {
            if (decl.id) {
                stmt.declaration = b.identifier(decl.id.name);
                body.splice(i, 0, decl);
                i++;
            } else {
                decl.type = b.FunctionExpression;
            }
        } else if (
            decl.type === b.VariableDeclaration &&
            decl.declarations.length === 1 &&
            decl.declarations[0].id.type === b.Identifier
        ) {
            // `export default class Foo {}` arrives here post-DesugarClasses
            // as `let Foo = <class expr>`
            stmt.declaration = b.identifier(decl.declarations[0].id.name);
            body.splice(i, 0, decl);
            i++;
        }
    }
}

export function collectEIRToplevel(tree, filename, module_infos, this_module_info, options) {
    let toplevel = tree.body[0];
    let body = toplevel.body.body;
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

        toplevel.eir_module = eir_module;
        toplevel.eir_main = info.name;
        toplevel.body = { type: b.BlockStatement, body: [], loc: toplevel.loc };
        debug.log(1, `EIR: ${filename}: whole module lowered (toplevel-as-EIR)`);
        if (dumpRequested(options)) dumpModule(filename, "toplevel-as-EIR", eir_module);
        return { eir_module: eir_module, accessors: accessors };
    } catch (e) {
        if (!isLowerNotSupported(e)) throw e;
        // there is no legacy pipeline to fall back to anymore: surface
        // the reason as a compile error at the call site
        return { error: e.message };
    }
}

