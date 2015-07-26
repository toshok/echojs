/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

import { DesugarArguments }            from './passes/desugar-arguments';
import { DesugarImportExport }         from './passes/desugar-import-export';
import { DesugarClasses }              from './passes/desugar-classes';
import { DesugarDestructuring }        from './passes/desugar-destructuring';
import { DesugarUpdateAssignments }    from './passes/desugar-update-assignments';
import { DesugarTemplates }            from './passes/desugar-templates';
import { DesugarArrowFunctions }       from './passes/desugar-arrow-functions';
import { DesugarGeneratorFunctions }   from './passes/desugar-generator-functions';
import { DesugarDefaults }             from './passes/desugar-defaults';
import { DesugarRestParameters }       from './passes/desugar-rest-parameters';
import { DesugarForOf }                from './passes/desugar-for-of';
import { DesugarSpread }               from './passes/desugar-spread';
import { HoistFuncDecls }              from './passes/hoist-func-decls';
import { FuncDeclsToVars }             from './passes/func-decls-to-vars';
import { DesugarLetLoopVars }          from './passes/desugar-let-loopvars';
import { HoistVars }                   from './passes/hoist-vars';
import { NameAnonymousFunctions }      from './passes/name-anonymous-functions';
import { NewClosureConvert }           from './passes/new-cc';
import { IIFEIdioms }                  from './passes/iife-idioms';
import { LambdaLift }                  from './passes/lambda-lift';

import * as escodegen from '../escodegen/escodegen-es6';
import * as debug from './debug';

//{ CFA2 } = require 'jscfa2'

// switch this to true if you want to experiment with the new CFA2
// code.  definitely, definitely not ready for prime time.
const enable_cfa2 = false;

// the HoistFuncDecls phase transforms the AST to give v8 semantics
// when faced with multiple function declarations within the same
// function scope.
//
const enable_hoist_func_decls_pass = true;

const passes = [
    DesugarImportExport,
    DesugarClasses,
    DesugarRestParameters,
    DesugarDestructuring,
    DesugarUpdateAssignments,
    DesugarTemplates,
    DesugarGeneratorFunctions,
    DesugarArrowFunctions,
    DesugarDefaults,
    DesugarForOf,
    DesugarSpread,
    enable_hoist_func_decls_pass ? HoistFuncDecls : null,
    FuncDeclsToVars,
    DesugarLetLoopVars,
    HoistVars,
    NameAnonymousFunctions,
    DesugarArguments,
    // enable_cfa2 ? CFA2 : null,
    NewClosureConvert,
    //IIFEIdioms,
    LambdaLift
];

export function convert (tree, filename, modules, options) {
    debug.log("before:");
    debug.log( () => escodegen.generate(tree) );

    passes.forEach((passType) => {
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
            debug.log(3, () => { if (typeof __ejs != "undefined") __ejs.GC.dumpAllocationStats(`after ${passType.name}`); });
	    }
        catch (e) {
            debug.log(2, `exception in pass ${passType.name}`);
            debug.log(2, e);
            throw e;
	    }
    });

    return tree;
}
