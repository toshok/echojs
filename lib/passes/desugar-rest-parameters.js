/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
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

import { reportError } from "../errors";
import { TransformPass } from "../node-visitor";
import { arrayFromRest_id } from "../common-ids";
import * as b from "../ast-builder";

export class DesugarRestParameters extends TransformPass {
    visitProperty(n) {
        if (n.kind !== "set") {
            return super.visitProperty(n);
        }
        if (n.value.params.length > 0) {
            let last_param = n.value.params[n.value.params.length - 1];
            if (last_param.type == b.RestElement)
                reportError(
                    SyntaxError,
                    "Setters aren't allowed to have a rest",
                    this.filename,
                    last_param.loc
                );
        }
        n.value = super.visit(n.value);
        return n;
    }

    visitFunction(n) {
        n = super.visitFunction(n);
        if (n.params.length > 0 && n.params[n.params.length - 1].type == b.RestElement) {
            let rest_argument = n.params[n.params.length - 1].argument;
            n.params.pop();
            if (rest_argument.type !== b.Identifier)
                reportError(
                    Error,
                    "we assume rest elements are of the form: ...Identifier",
                    this.filename,
                    rest_argument.argument.loc
                );
            let rest_name = rest_argument.name;
            let rest_declaration = b.letDeclaration(
                b.identifier(rest_argument.name),
                b.callExpression(arrayFromRest_id, [
                    b.literal(rest_argument.name),
                    b.literal(n.params.length),
                ])
            );
            n.body.body.unshift(rest_declaration);
        }
        return n;
    }
}
