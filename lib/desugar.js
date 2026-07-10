/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

import { DesugarClasses } from "./passes/desugar-classes";
import { DesugarDestructuring } from "./passes/desugar-destructuring";
import { DesugarGeneratorFunctions } from "./passes/desugar-generator-functions";
import { DesugarSpread } from "./passes/desugar-spread";
import { DesugarMetaProperties } from "./passes/desugar-metaproperties";
import { HoistFuncDecls } from "./passes/hoist-func-decls";

import * as escodegen from "../external-deps/escodegen/escodegen-es6";
import * as debug from "./debug";

// the AST->AST desugar passes that run before EIR collection: constructs
// EIR has no native lowering for arrive there as %-intrinsic calls, which
// lower through lib/eir/intrinsics.js.
//
// DesugarClasses, then DesugarDestructuring, then
// DesugarGeneratorFunctions, then DesugarSpread: super(...args) desugars
// into %constructSuper(ref, ...args) first, patterns unfold into
// member/iterator reads, generator methods desugar as plain function
// expressions, and the spread pass then rewrites what remains.
//
// HoistFuncDecls hoists last: nothing after it (spread/meta emit no
// function declarations) re-creates block-level decls.  it gives v8
// semantics — block-level declarations hoist to function scope, and
// same-name redeclarations collapse to the last one; at the toplevel it
// also moves the closure slot stores to the top, where hoisting says
// they belong.
const pre_eir_passes = [
    DesugarClasses,
    DesugarDestructuring,
    DesugarGeneratorFunctions,
    DesugarSpread,
    DesugarMetaProperties,
    HoistFuncDecls,
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

// runs in compile() before collectEIRToplevel
export function preEIRConvert(tree, filename, modules, options) {
    return runPasses(pre_eir_passes, tree, filename, modules, options);
}
