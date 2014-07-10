/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */
//
//   This pass walks the tree and moves all function expressions to the toplevel.
//
//   at the point where this pass runs there are a couple of assumptions:
// 
//   1. there are no function declarations anywhere in the program.  They have all
//      been converted to 'var X = %makeClosure(%env_Y, function (%env) { ... })'
// 
//   2. There are no free variables in the function expressions.
//
module b from '../ast-builder';

import { FunctionDeclaration } from '../ast-builder';
import { intrinsic, genGlobalFunctionName, genAnonymousFunctionName } from '../echo-util';

import { createArgScratchArea_id } from '../common-ids';
import { TransformPass } from '../node-visitor';

export class LambdaLift extends TransformPass {
    constructor (options, filename) {
        super();
        this.filename = filename;
        this.functions = [];
    }

    visitProgram (n) {
        super();
        n.body = this.functions.concat(n.body);
        return n;
    }

    maybePrependScratchArea (n) {
        if (n.scratch_size > 0)
            n.body.body.unshift(b.expressionStatement(intrinsic(createArgScratchArea_id, [ b.literal(n.scratch_size) ])));
    }
    
    visitFunctionDeclaration (n) {
        n.body = this.visit(n.body);
        this.maybePrependScratchArea(n);
        return n;
    }

    visitFunctionExpression (n) {
        let global_name;
        if (n.displayName)
            global_name = genGlobalFunctionName(n.displayName, this.filename);
        else if (n.id && n.id.name)
            global_name = genGlobalFunctionName(n.id.name, this.filename);
        else
            global_name = genAnonymousFunctionName(this.filename);
        
        n.type = FunctionDeclaration;
        n.id = b.identifier(global_name);

        this.functions.push(n);

        n.body = this.visit(n.body);

        this.maybePrependScratchArea(n);

        n.params.unshift(b.identifier(`%env_${n.ejs_env.parent.id}`));
        
        return b.identifier(global_name);
    }
}
