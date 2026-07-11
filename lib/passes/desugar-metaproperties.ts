/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

import { reportError } from "../errors";
import { TransformPass, VisitResult } from "../node-visitor";
import { getNewTarget_id } from "../common-ids";
import { intrinsic } from "../echo-util";
import type * as e from "../estree";

export class DesugarMetaProperties extends TransformPass {
    override visitAssignmentExpression(n: e.AssignmentExpression): VisitResult {
        if (n.left.type === "MetaProperty")
            reportError(
                SyntaxError,
                `'${n.left.meta}.${n.left.property}' not permitted on left hand side of assignment`,
                this.filename,
                n.left.loc ?? undefined
            );
        return super.visitAssignmentExpression(n);
    }

    override visitMetaProperty(n: e.MetaProperty): VisitResult {
        if (n.meta === "new" && n.property === "target") return intrinsic(getNewTarget_id, []);
        reportError(
            SyntaxError,
            `unknown meta property '${n.meta}.${n.property}'`,
            this.filename,
            n.loc ?? undefined
        );
    }
}
