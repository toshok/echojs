
//
// desugars
//
//   [1, 2, ...foo, 3, 4]
// 
//   o.foo(1, 2, ...foo, 3, 4)
//
// to:
//
//   %arrayFromSpread([1, 2], foo, [3, 4])
//
//   foo.apply(o, %arrayFromSpread([1, 2], foo, [3, 4])
//

import { TransformPass } from '../node-visitor';
import { b, SpreadElement, ArrayExpression } from '../ast-builder';
import { intrinsic, is_intrinsic } from '../echo-util';
import { arrayFromSpread_id } from '../common-ids';

class DesugarSpread extends TransformPass {
    visitArrayExpression (n) {
        n = super();
        let needs_desugaring = false;
        for (let el of n.elements) {
	    if (el.type === SpreadElement) {
		needs_desugaring = true;
		break;
	    }
	}

	if (!needs_desugaring)
	    return n;

        let new_args = [];
        let current_elements = [];
        for (let el of n.elements) {
            if (el.type === SpreadElement) {
                if (current_elements.length === 0) {
                    // just push the spread argument into the new args
                    new_args.push(el.argument);
                }
                else {
                    // push the current_elements as an array literal, then the spread.
                    // also reset current_elements to []
                    new_args.push(b.arrayExpression(current_elements));
                    new_args.push(el.argument);
                    current_elements = [];
                }
            }
            else {
                current_elements.push(el);
            }
        }
        if (current_elements.length > 0)
            new_args.push(b.arrayExpression(current_elements));

        // check to see if we've just created an array of nothing but array literals, and flatten them all
        // into one and get rid of the spread altogether
        let all_arrays = true;
        for (let a of new_args) {
	    if (a.type !== ArrayExpression)
                all_arrays = false;
	}
        
        if (all_arrays) {
            let na = [];
            for (let a of new_args)
                na = na.concat(a.elements);
            n.elements = na;
            return n;
        }
        else {
            return intrinsic(arrayFromSpread_id, new_args);
        }
    }

    visitCallExpression (n) {
        n = super();
        let needs_desugaring = false;
        for (let el of n.arguments) {
            if (el.type === SpreadElement) {
                needs_desugaring = true;
		break;
	    }
	}

	if (!needs_desugaring)
	    return n;

        let new_args = [];
        let current_elements = [];
        for (let el of n.arguments) {
            if (is_intrinsic(el, "%arrayFromSpread")) {
                // flatten spreads
                new_args.concat(el.arguments);
            }
            else if (el.type === SpreadElement) {
                if (current_elements.length === 0) {
                    // just push the spread argument into the new args
                    new_args.push(el.argument);
                }
                else {
                    // push the current_elements as an array literal, then the spread.
                    // also reset current_elements to []
                    new_args.push(b.arrayExpression(current_elements));
                    new_args.push(el.argument);
                    current_elements = [];
                }
            }
            else {
                current_elements.push(el);
            }
        }

        if (current_elements.length > 0)
            new_args.push(b.arrayExpression(current_elements));

        // check to see if we've just created an array of nothing but array literals, and flatten them all
        // into one and get rid of the spread altogether
        let all_arrays = true;
        for (let a of new_args) {
	    if (a.type !== ArrayExpression) {
                all_arrays = false;
		break;
	    }
	}
        if (all_arrays) {
            let na = [];
            for (let a of new_args)
                na = na.concat(a.elements);

            n.arguments = na;
	}
        else {
	    let receiver;

            if (n.callee.type === MemberExpression)
                receiver = n.callee.object;
            else
                receiver = b.null();

            n.callee = b.memberExpression(n.callee, b.identifier("apply"));
            n.arguments = [receiver, intrinsic(arrayFromSpread_id, new_args)];
	}
    }
}
