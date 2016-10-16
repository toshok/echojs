/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

import { TransformPass } from '../node-visitor';
import * as b from '../ast-builder';

import * as escodegen from '../../external-deps/escodegen/escodegen-es6';

export class NameAnonymousFunctions extends TransformPass {
    visitAssignmentExpression (n) {
        n = super.visitAssignmentExpression(n);
        let lhs = n.left;
        let rhs = n.right;

        // if we have the form
        //   <identifier> = function () { }
        // convert to:
        //   <identifier> = function <identifier> () { }
        // if lhs.type is Identifier and rhs.type is FunctionExpression and not rhs.id?.name
        //        rhs.display = <something pretty about the lhs>
        //
        let rhs_name = null;
        if (rhs.id)
            rhs_name = rhs.id.name;
        if (rhs.type === b.FunctionExpression && !rhs_name)
            rhs.displayName = escodegen.generate(lhs);
        return n;
    }

    visitFunction(n) {
        if (n.id && n.id.name)
            n.displayName = n.id.name;
        return super.visitFunction(n);
    }
}
