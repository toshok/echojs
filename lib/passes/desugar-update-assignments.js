/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */
// we split up assignment operators +=/-=/etc into their
// component operator + assignment so we can mark lhs as
// setLocal/setGlobal/etc, and rhs getLocal/getGlobal/etc

import { TransformPass } from '../node-visitor';
import { startGenerator } from '../echo-util';
import * as b from '../ast-builder';
import { reportError } from '../errors';

let updateGen = startGenerator();
let freshUpdate = () => `%update_${updateGen()}`;

export class DesugarUpdateAssignments extends TransformPass {
    
    constructor (options) {
        super(options);
        this.debug = true;
        this.updateGen = startGenerator();
    }

    visitProgram (n) {
        n.prepends = [];
        n = super.visitProgram(n, n);
        if (n.prepends.length > 0)
            n.body = n.prepends.concat(n.body);
        return n;
    }
    
    visitBlock (n) {
        n.prepends = [];
        n = super.visitBlock(n, n);
        if (n.prepends.length > 0)
            n.body = n.prepends.concat(n.body);
        return n;
    }
    
    visitAssignmentExpression (n, parentBlock) {
        n = super.visitAssignmentExpression(n, parentBlock);

        // we only care about the $= operators, where $ = *,/,-,+,%,etc
        if (n.operator.length === 1)
            return n;

        if (n.left.type === b.Identifier) {
            // for identifiers we just expand a += b to a = a + b
            n.right = b.binaryExpression(n.left, n.operator[0], n.right);
            n.operator = '=';
            return n;
        }

        if (n.left.type === b.MemberExpression) {
            let complex_exp = (n) => {
                if (!n)                    return false;
                if (n.type === b.Literal)    return false;
                if (n.type === b.Identifier) return false;
                return true;
            };

            let prepend_update = () => {
                let update_id = b.identifier(freshUpdate());
                parentBlock.prepends.unshift(b.letDeclaration(update_id, b.undefinedLit()));
                return update_id;
            };
            
            let object_exp = n.left.object;
            let prop_exp = n.left.property;

            let expressions = [];

            if (complex_exp(object_exp)) {
                let update_id = prepend_update();
                expressions.push(b.assignmentExpression(update_id, '=', object_exp));
                n.left.object = update_id;
            }

            if (complex_exp(prop_exp)) {
                let update_id = prepend_update();
                expressions.push(b.assignmentExpression(update_id, '=', prop_exp));
                n.left.property = update_id;
            }

            n.right = b.binaryExpression(b.memberExpression(n.left.object, n.left.property, n.computed), n.operator[0], n.right);
            n.operator = "=";
            
            if (expressions.length !== 0) {
                expressions.push(n);
                return b.sequenceExpression(expressions);
            }

            return n;
        }

        reportError(Error, `unexpected expression type ${n.left.type} in update assign expression.`, this.filename, n.left.loc);
    }
}
