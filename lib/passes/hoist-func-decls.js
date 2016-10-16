/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */
import { TransformPass } from '../node-visitor';

import * as b from '../ast-builder';

export class HoistFuncDecls extends TransformPass {
    visitFunction (n) {
        let decls = new Map();
        n.body = this.visit(n.body, decls);
        decls.forEach((fd) => {
            n.body.body.unshift(fd);
        });
        return n;
    }
    
    visitBlock (n, decls) {
        if (n.body.length === 0) return n;

        let i = 0;
        let e = n.body.length;
        while (i < e) {
            let child = n.body[i];
            if (child.type === b.FunctionDeclaration) {
                decls.set(child.id.name, this.visit(child));
                n.body.splice(i, 1);
                e = n.body.length;
            }
            else {
                i++;
            }
        }
        n = super.visitBlock(n, decls);
        return n;
    }
}
