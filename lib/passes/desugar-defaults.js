//
// desugars
//
//   function (a, b = a) { ... }
//
// to:
//
//   function (a, b) {
//     a = %getArg(0, undefined);
//     b = %getArg(1, a);
//   }
//

import { reportError } from "../errors";
import { argPresent_id, getArg_id } from "../common-ids";
import { intrinsic } from "../echo-util";
import { TransformPass } from "../node-visitor";
import * as b from "../ast-builder";

export class DesugarDefaults extends TransformPass {
    constructor(options, filename) {
        super(options);
        this.filename = filename;
    }

    visitFunction(n) {
        n = super.visitFunction(n);

        let prepends = [];
        let seen_default = false;

        n.params.forEach((p, i) => {
            let d = n.defaults[i];
            if (d) {
                seen_default = true;
            } else {
                if (seen_default) {
                    reportError(
                        SyntaxError,
                        "Cannot specify non-default parameter after a default parameter",
                        this.filename,
                        p.loc
                    );
                }
                d = b.undefinedLit();
            }
            let let_decl = b.letDeclaration(
                p,
                intrinsic(getArg_id, [
                    b.literal(i),
                    n.defaults[i] != null ? n.defaults[i] : b.undefinedLit(),
                ])
            );
            let_decl.loc = n.body.loc;
            prepends.push(let_decl);
        });
        n.body.body = prepends.concat(n.body.body);
        n.defaults = [];
        return n;
    }
}
