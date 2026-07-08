/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

// --ir integration: pick the functions the EIR pipeline can own, lower and
// verify them, and tag their AST nodes for the legacy pipeline to skip.
//
// v1 candidate rules (each one is a fallback, not a failure):
//   - top-level FunctionDeclarations only;
//   - the whole subtree must lower (LowerNotSupported falls back);
//   - the subtree must be closed: no references to module-scope bindings
//     (imports, other top-level functions/vars) -- only true globals.
//     module-scope access needs env/module-slot interop with the legacy
//     pipeline, which comes later.
//
// Tagged nodes keep their (emptied) body through the legacy passes; the
// legacy visitFunction emits a forwarding thunk to the EIR-emitted
// function instead of a body (see compiler.js).

import * as b from "../ast-builder";
import * as debug from "../debug";
import { ScopeAnalysis } from "./scopes";
import { lowerAnalyzedFunction } from "./lower";
import { LowerNotSupported } from "./errors";
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

// tree is the post-insert_toplevel_func AST (tree.body[0] is the toplevel
// function).  returns { lowered, fellback } counts.
export function collectEIRFunctions(tree, filename) {
    let toplevel = tree.body[0];
    let moduleNames = collectModuleScopeNames(toplevel.body.body);

    let lowered = 0;
    let fellback = 0;

    for (let stmt of toplevel.body.body) {
        if (stmt.type !== b.FunctionDeclaration || !stmt.id) continue;

        try {
            let analysis = new ScopeAnalysis();
            let info = analysis.analyzeFunction(stmt, stmt.id.name);

            // closed-subtree check: every free name must be a real global
            let module_ref = null;
            for (let name of analysis.globalNames) {
                if (moduleNames.has(name)) {
                    module_ref = name;
                    break;
                }
            }
            if (module_ref !== null) {
                debug.log(
                    1,
                    `EIR: ${filename}: '${stmt.id.name}' falls back (references module binding '${module_ref}')`
                );
                fellback++;
                continue;
            }

            let eir_module = new Module(info.name);
            lowerAnalyzedFunction(info, analysis, eir_module);
            verifyModule(eir_module);

            stmt.eir_module = eir_module;
            stmt.eir_main = info.name;
            // the legacy pipeline still visits this node (and emits the
            // forwarding thunk); it doesn't need the body.
            stmt.body = { type: b.BlockStatement, body: [], loc: stmt.loc };
            lowered++;
            debug.log(1, `EIR: ${filename}: '${stmt.id.name}' lowered (${eir_module.functions.length} fns)`);
        } catch (e) {
            if (!(e instanceof LowerNotSupported)) throw e;
            debug.log(1, `EIR: ${filename}: '${stmt.id.name}' falls back (${e.message})`);
            fellback++;
        }
    }

    return { lowered: lowered, fellback: fellback };
}
