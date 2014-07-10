/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */
import { TreeVisitor } from '../node-visitor';
import { Literal } from '../ast-builder';
import { builtinUndefined_id } from '../common-ids';
import { create_intrinsic } from '../echo-util';

export class ReplaceUnaryVoid extends TreeVisitor {
    visitUnaryExpression (n) {
        if (n.operator === "void" && n.argument.type === Literal && n.argument.value === 0)
            return create_intrinsic(builtinUndefined_id, []);
        return n;
    }
}
