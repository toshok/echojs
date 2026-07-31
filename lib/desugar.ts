/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

import { DesugarModernOps } from "./passes/desugar-modern-ops";
import { DesugarAsyncFunctions } from "./passes/desugar-async-functions";
import { DesugarClasses } from "./passes/desugar-classes";
import { DesugarDestructuring } from "./passes/desugar-destructuring";
import { DesugarGeneratorFunctions } from "./passes/desugar-generator-functions";
import { DesugarSpread } from "./passes/desugar-spread";
import { DesugarMetaProperties } from "./passes/desugar-metaproperties";
import { HoistFuncDecls } from "./passes/hoist-func-decls";
import { TransformPass } from "./node-visitor";

import * as escodegen from "../external-deps/escodegen/escodegen-es6";
import * as debug from "./debug";

import type { Program } from "./estree";
import type { CompilerOptions } from "./options";
import type { ModuleInfo } from "./module-info";

type PassConstructor = new (
    options: CompilerOptions,
    filename: string,
    modules: Map<string, ModuleInfo>
) => TransformPass;

// the AST->AST desugar passes that run before EIR collection: constructs
// EIR has no native lowering for arrive there as %-intrinsic calls, which
// lower through lib/eir/intrinsics.ts.
//
// DesugarModernOps and DesugarAsyncFunctions run before the
// ES6 tier; then DesugarClasses, DesugarDestructuring,
// DesugarGeneratorFunctions, DesugarSpread: super(...args) desugars into
// %constructSuper(ref, ...args) first, patterns unfold into
// member/iterator reads, generator methods (and the async desugar's
// synthesized generators) desugar as plain function expressions, and the
// spread pass then rewrites what remains.
//
// HoistFuncDecls hoists last: nothing after it (spread/meta emit no
// function declarations) re-creates block-level decls.  it gives v8
// semantics — block-level declarations hoist to function scope, and
// same-name redeclarations collapse to the last one; at the toplevel it
// also moves the closure slot stores to the top, where hoisting says
// they belong.
const pre_eir_passes: PassConstructor[] = [
    // DesugarModernOps first: optional chains / logical assignment desugar
    // to plain ES6 (arrow iifes) the later passes consume; `super` inside
    // the synthesized arrows is still rewritten by DesugarClasses below.
    // DesugarAsyncFunctions next: async methods become plain methods
    // before the class machinery, and its synthesized generators take the
    // normal generator/destructuring pipeline.
    DesugarModernOps,
    DesugarAsyncFunctions,
    DesugarClasses,
    DesugarDestructuring,
    DesugarGeneratorFunctions,
    DesugarSpread,
    DesugarMetaProperties,
    HoistFuncDecls,
];

// the self-hosted runtime exposes GC statistics through a global
declare const __ejs:
    | { GC: { dumpAllocationStats(tag: string): void } }
    | undefined;

function runPasses(
    passList: PassConstructor[],
    tree: Program,
    filename: string,
    modules: Map<string, ModuleInfo>,
    options: CompilerOptions
): Program {
    for (const passType of passList) {
        try {
            debug.time(2, passType.name);
            const pass = new passType(options, filename, modules);
            tree = pass.visit(tree) as Program;
            debug.timeEnd(2, passType.name);
            if (options.debug_passes.has(passType.name)) {
                console.log(`after: ${passType.name}`);
                console.log(escodegen.generate(tree));
            }

            debug.log(2, `after: ${passType.name}`);
            debug.log(2, () => escodegen.generate(tree));
            debug.log(3, () => {
                if (typeof __ejs != "undefined") __ejs.GC.dumpAllocationStats(`after ${passType.name}`);
                return "";
            });
        } catch (e) {
            debug.log(2, `exception in pass ${passType.name}`);
            debug.log(2, String(e));
            throw e;
        }
    }

    return tree;
}

// runs in compile() before collectEIRToplevel
export function preEIRConvert(
    tree: Program,
    filename: string,
    modules: Map<string, ModuleInfo>,
    options: CompilerOptions
): Program {
    return runPasses(pre_eir_passes, tree, filename, modules, options);
}
