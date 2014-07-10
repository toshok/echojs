/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */
import { TransformPass } from '../node-visitor';

import { FunctionDeclaration } from '../ast-builder';

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
            if (child.type === FunctionDeclaration) {
                decls.set(child.id.name, this.visit(child));
                n.body.splice(i, 1);
                e = n.body.length;
            }
            else {
                i++;
            }
        }
        n = super(n, decls);
        return n;
    }
}
