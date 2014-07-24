/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */
//
// special pass to inline some common idioms dealing with IIFEs
// (immediately invoked function expressions).
//
//    (function(x1, x2, ...) { ... body ... }(y1, y2, ...);
//
// This is a common way to provide scoping in ES5 and earlier.  It is
// unnecessary with the addition of 'let' in ES6.  We assume that all
// bindings in 'body' have been replace by 'let' or 'const' (meaning
// that all hoisting has been done.)
//
// we translate this form into the following equivalent inlined form:
//
//    {
//        let x1 = y1;
//        let x2 = y2;
//
//        ...
// 
//        { ... body ... }
//    }
//
// we limit the inlining to those where count(y) <= count(x).
// otherwise we'd need to ensure that the evaluation of the extra y's
// takes place before the body is executed, even if they aren't used.
//
// Another form we can optimize is the following:
//
//    (function() { body }).call(this)
//
// this form can be inlined directly as:
//
//    { body }
// 

module escodegen from '../../escodegen/escodegen-es6';

module b from '../ast-builder';

import { ThisExpression, MemberExpression } from '../ast-builder';
import { Stack } from '../stack-es6';
import { startGenerator, is_intrinsic, intrinsic } from '../echo-util';

import { TransformPass } from '../node-visitor';

import { getLocal_id } from '../common-ids';

export class IIFEIdioms extends TransformPass {
    constructor (options) {
        super(options);
        this.function_stack = new Stack;
        this.iife_generator = startGenerator();
    }

    visitFunction (n) {
        this.function_stack.push(n);
        let rv = super(n);
        this.function_stack.pop();
        return rv;
    }

    maybeInlineIIFE (candidate, n) {
        let arity = candidate.arguments[0].arguments[1].params.length;
        let arg_count = candidate.arguments.length - 1; // %invokeClosure's first arg is the callee

        if (arg_count > arity) return n;

        // at this point we know we have an IIFE in an expression statement, ala:
        //
        // (function(x, ...) { ...body...})(y, ...);
        //
        // so just inline { ...body... } in place of the
        // expression statement, after doing some magic to fix
        // up argument bindings (done here) and return
        // statements in the body (done in LLVMIRVisitor).
        // 
        let iife_rv_id = b.identifier(`%iife_rv_${this.iife_generator()}`);

        let replacement = b.blockStatement();

        replacement.body.push(b.letDeclaration(iife_rv_id, b.undefinedLit()));

        for (let i = 0; i < arity; i ++) {
            replacement.body.push(b.letDeclaration(candidate.arguments[0].arguments[1].params[i], (i < arg_count) ? candidate.arguments[i+1] : b.undefinedLit()));
        }
        
        this.function_stack.top.scratch_size = Math.max(this.function_stack.top.scratch_size, candidate.arguments[0].arguments[1].scratch_size);

        let body = candidate.arguments[0].arguments[1].body;
        body.ejs_iife_rv = iife_rv_id;
        body.fromIIFE = true;

        replacement.body.push(body);

        if (is_intrinsic(n.expression, "%setSlot")) {
            n.expression.arguments[2] = intrinsic(getLocal_id, [iife_rv_id]);
            replacement.body.push(n);
        }
        else if (is_intrinsic(n.expression, "%setGlobal") || is_intrinsic(n.expression, "%setLocal")) {
            n.expression.arguments[1] = intrinsic(getLocal_id, [iife_rv_id]);
            replacement.body.push(n);
        }
        
        return replacement;
    }

    maybeInlineIIFECall (candidate, n) {
        if (candidate.arguments.length !== 2 || candidate.arguments[1].type !== ThisExpression)
            return n;

        let iife_rv_id = b.identifier(`%iife_rv_${this.iife_generator()}`);

        let replacement = b.blockStatement();

        replacement.body.push(b.letDeclaration(iife_rv_id, b.undefinedLit()));

        let body = candidate.arguments[0].object.arguments[1].body;
        body.ejs_iife_rv = iife_rv_id;
        body.fromIIFE = true;

        replacement.body.push(body);

        if (is_intrinsic(n.expression, "%setSlot")) {
            n.expression.arguments[2] = intrinsic(getLocal_id, [iife_rv_id]);
            replacement.body.push(n);
        }
        else if (is_intrinsic(n.expression, "%setGlobal") || is_intrinsic(n.expression, "%setLocal")) {
            n.expression.arguments[1] = intrinsic(getLocal_id, [iife_rv_id]);
            replacement.body.push(n);
        }

        return replacement;
    }
    
    visitExpressionStatement (n) {
        let isMakeClosure = (a) => is_intrinsic(a, "%makeClosure") || is_intrinsic(a, "%makeAnonClosure");

        let candidate;
        // bail out early if we know we aren't in the right place
        if (is_intrinsic(n.expression, "%invokeClosure"))
            candidate = n.expression;
        else if (is_intrinsic(n.expression, "%setSlot"))
            candidate = n.expression.arguments[2];
        else if (is_intrinsic(n.expression, "%setGlobal") || is_intrinsic(n.expression, "%setLocal"))
            candidate = n.expression.arguments[1];
        else
            return n;

        // at this point candidate should only be an invokeClosure intrinsic
        if (!is_intrinsic(candidate, "%invokeClosure")) return n;

        if (isMakeClosure(candidate.arguments[0])) {
            return this.maybeInlineIIFE(candidate, n);
        }
        else if (candidate.arguments[0].type === MemberExpression && isMakeClosure(candidate.arguments[0].object) && candidate.arguments[0].property.name === "call") {
            return this.maybeInlineIIFECall(candidate, n);
        }
        else {
            return n;
        }
    }
}
