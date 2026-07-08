/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

// --ir integration: pick the functions the EIR pipeline can own, lower and
// verify them, and tag their AST nodes for the legacy pipeline to skip.
//
// candidates are top-level FunctionDeclarations.  a candidate's free names
// may be:
//   - true globals (console, Math, ...): lowered as get_global;
//   - named imports from non-native modules: lowered as module_slot_load
//     (or folded, when the export is a const literal);
//   - sibling candidates, in call position only: lowered as direct calls
//     into the EIR-emitted function (no closure dispatch).  viability is
//     a fixed point: a candidate depending on a fallen-back sibling falls
//     back too.
// anything else (module-level vars, namespace/default imports, siblings
// used as values, unsupported syntax) falls back per function via
// LowerNotSupported.
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

// names bound at module scope (anything that is NOT a real global)
function collectModuleScopeNames(toplevelBody) {
    let names = new Set();
    for (let stmt of toplevelBody) {
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

// local name -> { module, slot, constval? } for named imports from
// non-native modules
function collectImports(toplevelBody, module_infos) {
    let imports = new Map();
    if (!module_infos) return imports;
    for (let stmt of toplevelBody) {
        if (stmt.type !== b.ImportDeclaration) continue;
        if (!stmt.source_path) continue;
        let moduleString = stmt.source_path.value;
        if (moduleString[0] === "@") continue; // native modules resolve differently
        let module_info = module_infos.get(moduleString);
        if (!module_info || module_info.isNative()) continue;
        for (let spec of stmt.specifiers) {
            if (spec.type !== b.ImportSpecifier) continue; // default/namespace fall back
            if (!module_info.exports.has(spec.imported.name)) continue;
            let export_info = module_info.exports.get(spec.imported.name);
            let entry = {
                module: moduleString,
                slot: export_info.slot_num,
            };
            // const exports fold to their literal at compile time (matches
            // new-cc's constval propagation)
            if (export_info.constval && export_info.constval.type === b.Literal)
                entry.constval = export_info.constval;
            imports.set(spec.local.name, entry);
        }
    }
    return imports;
}

// module-scope names that are ever assigned at the top level; calls into
// those can't be made direct
function collectAssignedNames(toplevelBody) {
    let assigned = new Set();
    let walk = (n) => {
        if (!n || typeof n !== "object") return;
        if (Array.isArray(n)) {
            for (let el of n) walk(el);
            return;
        }
        // don't descend into functions: assignments there hit closures at
        // runtime, which sibling-call viability doesn't depend on... but a
        // nested assignment to a module fn name DOES invalidate direct
        // calls, so we conservatively descend everywhere.
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

// tree is the post-insert_toplevel_func AST (tree.body[0] is the toplevel
// function).  returns { lowered, fellback } counts.
export function collectEIRFunctions(tree, filename, module_infos) {
    let toplevel = tree.body[0];
    let body = toplevel.body.body;

    let moduleNames = collectModuleScopeNames(body);
    let imports = collectImports(body, module_infos);
    let assigned = collectAssignedNames(body);

    // phase 1: analyze every top-level function declaration
    let candidates = new Map(); // name -> { stmt, analysis, info, viable, reason }
    for (let stmt of body) {
        if (stmt.type !== b.FunctionDeclaration || !stmt.id) continue;
        let name = stmt.id.name;
        if (candidates.has(name)) {
            candidates.get(name).viable = false;
            candidates.get(name).reason = "redeclared at module scope";
            continue;
        }
        let entry = { stmt: stmt, viable: true, reason: null };
        candidates.set(name, entry);
        if (assigned.has(name)) {
            entry.viable = false;
            entry.reason = "reassigned at module scope";
            continue;
        }
        try {
            entry.analysis = new ScopeAnalysis();
            entry.info = entry.analysis.analyzeFunction(stmt, name);
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
                if (imports.has(name)) continue; // handled via module slots
                let sib = candidates.get(name);
                if (
                    sib &&
                    sib.viable &&
                    sib !== entry &&
                    !entry.analysis.globalValueNames.has(name)
                )
                    continue; // direct call to a viable sibling
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
        if (entry.viable) siblings.set(entry.stmt.id.name, entry.info.name);
    }
    let mod_ctx = { imports: imports, siblings: siblings };

    let fellback = 0;
    let succeeded = [];
    for (let entry of candidates.values()) {
        if (!entry.viable) {
            if (entry.reason) {
                debug.log(1, `EIR: ${filename}: '${entry.stmt.id.name}' falls back (${entry.reason})`);
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
            debug.log(1, `EIR: ${filename}: '${entry.stmt.id.name}' failed late (${e.message}); disabling EIR for this file`);
            return { lowered: 0, fellback: candidates.size };
        }
    }

    if (succeeded.length > 0) verifyModule(eir_module);

    // only now (everything lowered + verified) tag nodes and empty bodies
    for (let entry of succeeded) {
        entry.stmt.eir_module = eir_module;
        entry.stmt.eir_main = entry.info.name;
        entry.stmt.body = { type: b.BlockStatement, body: [], loc: entry.stmt.loc };
        debug.log(1, `EIR: ${filename}: '${entry.stmt.id.name}' lowered`);
    }

    return { lowered: succeeded.length, fellback: fellback };
}
