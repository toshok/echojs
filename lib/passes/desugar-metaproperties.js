/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

import { reportError } from '../errors';
import { TransformPass } from '../node-visitor';
import { getNewTarget_id } from '../common-ids';
import * as b from '../ast-builder';

export class DesugarMetaProperties extends TransformPass {
    visitAssignmentExpression (n) {
	if (n.left.type === b.MetaProperty)
	    reportError(SyntaxError, `'${n.left.meta}.${n.left.property}' not permitted on left hand side of assignment`, this.filename, n.left.loc);
	return super.visitAssignmentExpression(n);
    }

    visitMetaProperty (n) {
	if (n.meta === "new" && n.prop === "target")
	    return intrinsic(getNewTarget_id, []);
	reportError(SyntaxError, `unknown meta property '${n.meta}.${n.property}'`, this.filename, n.loc);
    }
}
