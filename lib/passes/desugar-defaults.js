//
// desugars
//
//   function (a, b = a) { ... }
//
// to:
//
//   function (a, b) {
//     a = %argPresent(0) ? %getArg(0) : undefined;
//     b = %argPresent(1) ? %getArg(1) : a;
//   }
//

import { reportError } from '../errors';
import { argPresent_id, getArg_id } from '../common-ids';
import { intrinsic } from '../echo-util';
import { TransformPass } from '../node-visitor';
module b from '../ast-builder';

export class DesugarDefaults extends TransformPass {
    constructor (options, filename) {
	this.filename = filename;
        super(options);
    }

    visitFunction (n) {
        n = super(n);

        let prepends = [];
        let seen_default = false;

        n.params.forEach((p, i) => {
            let d = n.defaults[i];
            if (d) {
                seen_default = true;
	    }
            else {
                if (seen_default) {
                    reportError(SyntaxError, "Cannot specify non-default parameter after a default parameter", this.filename, p.loc);
		}
                d = b.undefinedLit();
	    }
            prepends.push(b.expressionStatement(b.assignmentExpression(p, '=', b.conditionalExpression(intrinsic(argPresent_id, [b.literal(i+1)]), intrinsic(getArg_id, [b.literal(i)]), d))));
	});
	n.body.body = prepends.concat(n.body.body);
        n.defaults = [];
        return n;
    }
}
