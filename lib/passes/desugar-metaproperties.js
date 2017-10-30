/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

import { reportError } from "../errors";
import { TransformPass } from "../node-visitor";
import { getNewTarget_id } from "../common-ids";
import { intrinsic } from "../echo-util";
import * as b from "../ast-builder";

export class DesugarMetaProperties extends TransformPass {
    visitAssignmentExpression(n) {
        if (n.left.type === b.MetaProperty)
            reportError(
                SyntaxError,
                `'${n.left.meta}.${n.left.property}' not permitted on left hand side of assignment`,
                this.filename,
                n.left.loc
            );
        return super.visitAssignmentExpression(n);
    }

    visitMetaProperty(n) {
        if (n.meta === "new" && n.property === "target") return intrinsic(getNewTarget_id, []);
        reportError(
            SyntaxError,
            `unknown meta property '${n.meta}.${n.property}'`,
            this.filename,
            n.loc
        );
    }
}
