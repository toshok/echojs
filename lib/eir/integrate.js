/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

// --ir integration: pick the functions the EIR pipeline can own, lower and
// verify them, and tag their AST nodes for the legacy pipeline to skip.
//
// candidates are top-level function declarations (exported or not) and
// top-level single-declarator `var f = function () {}` initializers whose
// name is never reassigned.  a candidate's free names may be:
//   - true globals (console, Math, ...): lowered as get_global;
//   - named imports from non-native modules: lowered as module_slot_load
//     (or folded, when the export is a const literal);
//   - this module's own exported bindings: module_slot_load/store against
//     the "%self" module global (const-literal exports fold; exported
//     functions/classes read in value position load the slot, so closure
//     identity is preserved);
//   - non-exported module-level bindings with literal initializers that
//     are never reassigned: folded to the literal;
//   - sibling candidates in call position: lowered as direct calls into
//     the EIR-emitted function (no closure dispatch).  viability is a
//     fixed point: a candidate depending on a fallen-back sibling falls
//     back too.
//   - non-exported module-level vars promoted to hidden slots by
//     gather-imports: module_slot_load/store on "%self" (the legacy
//     pipeline routes its accesses through the same slots).
// anything else (non-exported function declarations used as values,
// unsupported syntax) falls back per function via LowerNotSupported.
//
// all of a file's candidates lower into ONE shared EIR module so direct
// calls resolve within it; tagged nodes keep their (emptied) body through
// the legacy passes, and the legacy visitFunction emits a forwarding
// thunk to the EIR-emitted function (see compiler.js).

import * as b from "../ast-builder";
import * as debug from "../debug";
import { ScopeAnalysis } from "./scopes";
import { lowerAnalyzedFunction } from "./lower";
import { LowerNotSupported, isLowerNotSupported } from "./errors";
import { Module } from "./ir";
import { verifyModule } from "./verifier";

function collectPatternNames(pat, out) {
    if (!pat) return;
    switch (pat.type) {
        case b.Identifier:
            out.add(pat.name);
            return;
        case b.ArrayPattern:
            for (let el of pat.elements) collectPatternNames(el, out);
            return;
        case b.ObjectPattern:
            for (let p of pat.properties) collectPatternNames(p.value, out);
            return;
        case b.SpreadElement:
            collectPatternNames(pat.argument, out);
            return;
        default:
            return;
    }
}

// module-scope statements may be wrapped in `export`
function unwrapExport(stmt) {
    if (stmt.type === b.ExportNamedDeclaration && stmt.declaration && !Array.isArray(stmt.declaration))
        return stmt.declaration;
    return stmt;
}

// names bound at module scope (anything that is NOT a real global)
function collectModuleScopeNames(toplevelBody) {
    let names = new Set();
    for (let wrapped of toplevelBody) {
        let stmt = unwrapExport(wrapped);
        switch (stmt.type) {
            case b.FunctionDeclaration:
            case b.ClassDeclaration:
                if (stmt.id) names.add(stmt.id.name);
                break;
            case b.VariableDeclaration:
                for (let d of stmt.declarations) collectPatternNames(d.id, names);
                break;
            case b.ImportDeclaration:
                for (let spec of stmt.specifiers) {
                    if (spec.local) names.add(spec.local.name);
                    else if (spec.id) names.add(spec.id.name);
                }
                break;
            default:
                break;
        }
    }
    return names;
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

// the candidate function node + its module-scope name, or null
function candidateOf(wrapped) {
    let stmt = unwrapExport(wrapped);
    if (stmt.type === b.FunctionDeclaration && stmt.id)
        return { name: stmt.id.name, fnNode: stmt };
    // var f = function () { ... };  or  var f = (x) => ...;  (single
    // declarator only)
    if (
        stmt.type === b.VariableDeclaration &&
        stmt.declarations.length === 1 &&
        stmt.declarations[0].id.type === b.Identifier &&
        stmt.declarations[0].init &&
        (stmt.declarations[0].init.type === b.FunctionExpression ||
            stmt.declarations[0].init.type === b.ArrowFunctionExpression)
    )
        return { name: stmt.declarations[0].id.name, fnNode: stmt.declarations[0].init };
    return null;
}

// toplevel-as-EIR (phase 3, --ir-toplevel): lower the WHOLE module —
// toplevel statements, import/export init, and every nested function —
// as one EIR unit.  module-scope bindings resolve through the same refs
// machinery candidates use (slots for exported/promoted names,
// const-literal folds); everything else is an ordinary toplevel local,
// captured into the toplevel's environment as needed.  all-or-nothing
// per module: any LowerNotSupported anywhere returns false and the
// caller falls back to per-candidate collection below.  the legacy
// pipeline keeps only the module-resolution scaffolding, which wraps the
// EIR toplevel (see emitEIRToplevel in compiler.js).
export function collectEIRToplevel(tree, filename, module_infos, this_module_info) {
    let toplevel = tree.body[0];
    let body = toplevel.body.body;

    let assigned = collectAssignedNames(body);
    let refs = collectModuleRefs(body, module_infos, this_module_info);
    addModuleConstLiterals(body, assigned, refs);

    let moduleSlotNames = new Set(refs.keys());

    try {
        let analysis = new ScopeAnalysis();
        let info = analysis.analyzeToplevel(toplevel, toplevel.id.name, moduleSlotNames);

        // no direct sibling calls in toplevel mode: a slot-backed module
        // function may capture the toplevel environment, which a caller's
        // envParam wouldn't carry.  calls go slot-load + invoke_closure.
        let mod_ctx = { refs: refs, siblings: new Map(), this_module_info: this_module_info };

        let eir_module = new Module(filename);
        lowerAnalyzedFunction(info, analysis, eir_module, mod_ctx);
        verifyModule(eir_module);

        toplevel.eir_module = eir_module;
        toplevel.eir_main = info.name;
        toplevel.body = { type: b.BlockStatement, body: [], loc: toplevel.loc };
        debug.log(1, `EIR: ${filename}: whole module lowered (toplevel-as-EIR)`);
        return true;
    } catch (e) {
        if (!isLowerNotSupported(e)) throw e;
        debug.log(1, `EIR: ${filename}: toplevel falls back (${e.message})`);
        return false;
    }
}

// tree is the post-insert_toplevel_func AST (tree.body[0] is the toplevel
// function).  returns { lowered, fellback } counts.
export function collectEIRFunctions(tree, filename, module_infos, this_module_info, exclude_fns) {
    let toplevel = tree.body[0];
    let body = toplevel.body.body;

    let moduleNames = collectModuleScopeNames(body);
    let assigned = collectAssignedNames(body);
    let refs = collectModuleRefs(body, module_infos, this_module_info);
    addModuleConstLiterals(body, assigned, refs);

    // phase 1: analyze every candidate
    let candidates = new Map(); // name -> { fnNode, analysis, info, viable, reason }
    for (let wrapped of body) {
        let cand = candidateOf(wrapped);
        if (!cand) continue;
        if (candidates.has(cand.name)) {
            candidates.get(cand.name).viable = false;
            candidates.get(cand.name).reason = "redeclared at module scope";
            continue;
        }
        let entry = { fnNode: cand.fnNode, name: cand.name, viable: true, reason: null };
        candidates.set(cand.name, entry);
        if (exclude_fns && exclude_fns.some((pat) => cand.name.indexOf(pat) !== -1)) {
            entry.viable = false;
            entry.reason = "excluded via --ir-exclude-fn";
            continue;
        }
        if (assigned.has(cand.name)) {
            entry.viable = false;
            entry.reason = "reassigned at module scope";
            continue;
        }
        try {
            entry.analysis = new ScopeAnalysis();
            entry.info = entry.analysis.analyzeFunction(cand.fnNode, cand.name);
        } catch (e) {
            if (!(isLowerNotSupported(e))) throw e;
            entry.viable = false;
            entry.reason = e.message;
        }
    }

    // phase 2: viability fixed point over free names
    let changed = true;
    while (changed) {
        changed = false;
        for (let entry of candidates.values()) {
            if (!entry.viable) continue;
            for (let name of entry.analysis.globalNames) {
                if (!moduleNames.has(name)) continue; // a real global

                // direct call to a viable sibling wins over any slot ref
                let sib = candidates.get(name);
                if (
                    sib &&
                    sib.viable &&
                    sib !== entry &&
                    !entry.analysis.globalValueNames.has(name)
                )
                    continue;

                let ref = refs.get(name);
                if (ref) {
                    if (entry.analysis.globalAssignedNames.has(name) && !ref.writable) {
                        entry.viable = false;
                        entry.reason = `assigns read-only module binding '${name}'`;
                        changed = true;
                        break;
                    }
                    continue; // resolved via module slot / constant fold
                }

                entry.viable = false;
                entry.reason = `references module binding '${name}'`;
                changed = true;
                break;
            }
        }
    }

    // phase 3: lower every viable candidate into one shared module
    let eir_module = new Module(filename);
    let siblings = new Map(); // local name -> eir function name
    for (let entry of candidates.values()) {
        if (entry.viable) siblings.set(entry.name, entry.info.name);
    }
    let mod_ctx = { refs: refs, siblings: siblings };

    let fellback = 0;
    let succeeded = [];
    for (let entry of candidates.values()) {
        if (!entry.viable) {
            if (entry.reason) {
                debug.log(1, `EIR: ${filename}: '${entry.name}' falls back (${entry.reason})`);
                fellback++;
            }
            continue;
        }
        try {
            lowerAnalyzedFunction(entry.info, entry.analysis, eir_module, mod_ctx);
            succeeded.push(entry);
        } catch (e) {
            if (!(isLowerNotSupported(e))) throw e;
            // lowering found something analysis didn't model.  siblings may
            // hold direct-call references into this function, so the whole
            // file's EIR set is abandoned (nothing has been tagged yet).
            debug.log(1, `EIR: ${filename}: '${entry.name}' failed late (${e.message}); disabling EIR for this file`);
            return { lowered: 0, fellback: candidates.size };
        }
    }

    if (succeeded.length > 0) verifyModule(eir_module);

    // only now (everything lowered + verified) tag nodes and empty bodies
    for (let entry of succeeded) {
        entry.fnNode.eir_module = eir_module;
        entry.fnNode.eir_main = entry.info.name;
        entry.fnNode.body = { type: b.BlockStatement, body: [], loc: entry.fnNode.loc };
        // expression-bodied arrows just got a block body; keep the legacy
        // DesugarArrowFunctions pass from wrapping it in a return
        entry.fnNode.expression = false;
        debug.log(1, `EIR: ${filename}: '${entry.name}' lowered`);
    }

    return { lowered: succeeded.length, fellback: fellback };
}
