
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
//     let rest = %gatherRest('rest', 3);
//     // body
//   }
// 

import { TransformPass } from '../node-visitor';
import { gatherRest_id } from '../common-ids';
import { b } from '../ast-builder';

class DesugarRestParameters extends TransformPass {
    visitFunction (n) {
        n = super();
        if (n.rest) {
            let rest_declaration = b.letDeclaration(n.rest, b.callExpression(gatherRest_id, [b.literal(n.rest.name), b.literal(n.params.length)]));
            n.body.body.unshift(rest_declaration);
            n.rest = undefined;
	}
	return n;
    }
}
