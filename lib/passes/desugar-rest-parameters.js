/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

//
// convert from:
//
//   function name (arg1, arg2, arg3, ...rest) {
//     // body
//   }
//
// to:
//
//   function name (arg1, arg2, arg3) {
//     let rest = %arrayFromRest('rest', 3);
//     // body
//   }
// 

import { TransformPass } from '../node-visitor';
import { arrayFromRest_id } from '../common-ids';
import * as b from '../ast-builder';

export class DesugarRestParameters extends TransformPass {
    visitFunction (n) {
        n = super.visitFunction(n);
        if (n.rest) {
            let rest_declaration = b.letDeclaration(n.rest, b.callExpression(arrayFromRest_id, [b.literal(n.rest.name), b.literal(n.params.length)]));
            n.body.body.unshift(rest_declaration);
            n.rest = undefined;
        }
        return n;
    }
}
