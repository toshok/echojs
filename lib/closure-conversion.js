/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

import { DesugarArguments } from "./passes/desugar-arguments";
import { DesugarImportExport } from "./passes/desugar-import-export";
import { DesugarClasses } from "./passes/desugar-classes";
import { DesugarDestructuring } from "./passes/desugar-destructuring";
import { DesugarUpdateAssignments } from "./passes/desugar-update-assignments";
import { DesugarTemplates } from "./passes/desugar-templates";
import { DesugarArrowFunctions } from "./passes/desugar-arrow-functions";
import { DesugarGeneratorFunctions } from "./passes/desugar-generator-functions";
import { DesugarDefaults } from "./passes/desugar-defaults";
import { DesugarRestParameters } from "./passes/desugar-rest-parameters";
import { DesugarForOf } from "./passes/desugar-for-of";
import { DesugarSpread } from "./passes/desugar-spread";
import { DesugarMetaProperties } from "./passes/desugar-metaproperties";
import { HoistFuncDecls } from "./passes/hoist-func-decls";
import { FuncDeclsToVars } from "./passes/func-decls-to-vars";
import { DesugarLetLoopVars } from "./passes/desugar-let-loopvars";
import { HoistVars } from "./passes/hoist-vars";
import { NameAnonymousFunctions } from "./passes/name-anonymous-functions";
import { NewClosureConvert } from "./passes/new-cc";
//import { IIFEIdioms }                  from './passes/iife-idioms';
import { LambdaLift } from "./passes/lambda-lift";

import * as escodegen from "../external-deps/escodegen/escodegen-es6";
import * as debug from "./debug";

// the HoistFuncDecls phase transforms the AST to give v8 semantics
// when faced with multiple function declarations within the same
// function scope.
//
const enable_hoist_func_decls_pass = true;

// pipeline-agnostic AST->AST rewrites that run BEFORE collectEIRFunctions
// (phase 2 of the legacy-removal plan): constructs EIR has no native
// lowering for arrive there as %-intrinsic calls, which lower through
// lib/eir/intrinsics.js.  the legacy pipeline consumes the same output
// (its own %-intrinsic handling predates EIR), so both pipelines see one
// AST.
//
// DesugarClasses, then DesugarDestructuring, then
// DesugarGeneratorFunctions, then DesugarSpread — the legacy relative
// order: super(...args) desugars into %constructSuper(ref, ...args)
// first, patterns unfold into member/iterator reads, generator methods
// desugar as plain function expressions, and the spread pass then
// rewrites what remains.  running these before DesugarImportExport means
// `export class Foo` reaches it as `export let Foo = <iife>(...)` — the
// same %moduleSetSlot store.
//
// only the FIRST destructuring run hoists; the second (below) cleans up
// the patterns DesugarForOf re-emits.  trailing ...rest params pass
// through it untouched (EIR is native; the legacy rest pass strips them
// later).
//
// DesugarSpread also stays in the main list below as a safety net for
// spreads synthesized by later passes (currently none).
const pre_eir_passes = [
    DesugarClasses,
    DesugarDestructuring,
    DesugarGeneratorFunctions,
    DesugarSpread,
    DesugarMetaProperties,
];

const passes = [
    DesugarImportExport,
    DesugarRestParameters,
    DesugarUpdateAssignments,
    DesugarTemplates,
    DesugarArrowFunctions,
    DesugarDefaults,
    DesugarForOf,
    // DesugarForOf re-emits the loop's binding pattern as a fresh let
    // declaration (`let [k,v] = %iter_next.value`), so destructuring has
    // to run again after it.  the first DesugarDestructuring pass still
    // has to run before DesugarDefaults, which assumes simple params.
    DesugarDestructuring,
    DesugarSpread,
    enable_hoist_func_decls_pass ? HoistFuncDecls : null,
    FuncDeclsToVars,
    DesugarLetLoopVars,
    HoistVars,
    NameAnonymousFunctions,
    DesugarArguments,
    NewClosureConvert,
    //IIFEIdioms,
    LambdaLift,
];

function runPasses(passList, tree, filename, modules, options) {
    passList.forEach((passType) => {
        if (!passType) return;
        try {
            debug.time(2, passType.name);
            let pass = new passType(options, filename, modules);
            tree = pass.visit(tree);
            debug.timeEnd(2, passType.name);
            if (options.debug_passes.has(passType.name)) {
                console.log(`after: ${passType.name}`);
                console.log(escodegen.generate(tree));
            }

            debug.log(2, `after: ${passType.name}`);
            debug.log(2, () => escodegen.generate(tree));
            debug.log(3, () => {
                if (typeof __ejs != "undefined")
                    __ejs.GC.dumpAllocationStats(`after ${passType.name}`);
            });
        } catch (e) {
            debug.log(2, `exception in pass ${passType.name}`);
            debug.log(2, e);
            throw e;
        }
    });

    return tree;
}

// runs in compile() before collectEIRFunctions, on both the --ir and
// legacy paths
export function preEIRConvert(tree, filename, modules, options) {
    return runPasses(pre_eir_passes, tree, filename, modules, options);
}

export function convert(tree, filename, modules, options) {
    debug.log("before:");
    debug.log(() => escodegen.generate(tree));

    return runPasses(passes, tree, filename, modules, options);
}
