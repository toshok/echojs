/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */
//
// desugars
//
//   for (let x of a) { ... }
//
// to:
//
//   {
//     %forof = a[Symbol.iterator]();
//     while (!(%iter_next = %forof.next()).done) {
//       let x = %iter_next.value;
//       { ... }
//     }
//   }

module b from '../ast-builder';
import { VariableDeclaration } from '../ast-builder';
import { TransformPass } from '../node-visitor';
import { startGenerator } from '../echo-util';
import { Stack } from '../stack-es6';

let forofgen = startGenerator();
let freshForOf = function (ident='') { return `%forof${ident}_${forofgen()}`; };

export class DesugarForOf extends TransformPass {
    constructor (options) {
        super(options);
        this.function_stack = new Stack();
    }

    visitFunction (n) {
        this.function_stack.push(n);
        let rv = super(n);
        this.function_stack.pop();
        return rv;
    }

    visitForOf (n) {
        n.left  = this.visit(n.left);
        n.right = this.visit(n.right);
        n.body  = this.visit(n.body);
        
        let iterable_tmp   = freshForOf('tmp');
        let iter_name      = freshForOf('iter');
        let iter_next_name = freshForOf('next');
        
        let iterable_id  = b.identifier(iterable_tmp);
        let iter_id      = b.identifier(iter_name);
        let iter_next_id = b.identifier(iter_next_name);

        let tmp_iterable_decl = b.letDeclaration(iterable_id, n.right);

        let Symbol_iterator = b.memberExpression(b.identifier("Symbol"), b.identifier("iterator"));
        let get_iterator_stmt = b.letDeclaration(iter_id, b.callExpression(b.memberExpression(iterable_id, Symbol_iterator, true), []));

        let loop_iter_stmt;

        if (n.left.type === VariableDeclaration)
            loop_iter_stmt = b.letDeclaration(n.left.declarations[0].id, // can there be more than 1?
                                              b.memberExpression(iter_next_id, b.identifier("value")));
        else
            loop_iter_stmt = b.expressionStatement(b.assignmentExpression(n.left, "=", b.memberExpression(iter_next_id, b.identifier("value"))));

        let next_decl = b.letDeclaration(iter_next_id, b.undefinedLit());

        let not_done = b.unaryExpression("!", b.memberExpression(b.assignmentExpression(iter_next_id, "=", b.callExpression(b.memberExpression(iter_id, b.identifier("next")))), b.identifier("done")));
        
        let while_stmt = b.whileStatement(not_done, b.blockStatement([loop_iter_stmt, n.body]));

        return b.blockStatement([
            tmp_iterable_decl,
            get_iterator_stmt,
            next_decl,
            while_stmt
        ]);
    }
}
