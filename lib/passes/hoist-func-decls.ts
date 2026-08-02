/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// v8 semantics for function declarations: block-level declarations hoist
// to function scope, and same-name redeclarations collapse to the last
// one (the Map keying below).

import { TransformPass, VisitResult } from "../node-visitor";
import type * as e from "../estree";
import type { CompilerOptions } from "../options";

export class HoistFuncDecls extends TransformPass {
    // the current function's hoisted declarations; a stack because
    // functions nest (visitFunction saves/restores around the recursion)
    private decls: Map<string, e.FunctionDeclaration> | null = null;

    // explicit, so tsc doesn't synthesize `constructor() {
    // super(...arguments); }` — the compiler shouldn't gratuitously
    // depend on the runtime's arguments-object iteration
    constructor(options: CompilerOptions) {
        super(options);
    }

    override visitFunction(n: e.Function): VisitResult {
        const saved = this.decls;
        const decls = new Map<string, e.FunctionDeclaration>();
        this.decls = decls;
        n.body = this.visitAs(n.body);
        this.decls = saved;
        if (n.body.type === "BlockStatement") {
            const body = n.body.body;
            decls.forEach((fd) => {
                body.unshift(fd);
            });
        }
        return n;
    }

    override visitBlock(n: e.BlockStatement): VisitResult {
        if (n.body.length === 0) return n;
        const decls = this.decls;
        if (!decls) return super.visitBlock(n);

        let i = 0;
        let end = n.body.length;
        while (i < end) {
            const child = n.body[i]!;
            if (child.type === "FunctionDeclaration") {
                decls.set(child.id.name, this.visitAs(child));
                n.body.splice(i, 1);
                end = n.body.length;
            } else {
                i++;
            }
        }
        return super.visitBlock(n);
    }
}
