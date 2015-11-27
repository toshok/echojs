/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

// this pass flattens all declaration AST nodes such that they're 1
// declarator per declaration.
//
// i.e. from:
//    {
//       ....
//       var x = 5, y = 10, z = x;
//       ....
//    }
// to:
//    {
//       ....
//       var x = 5;
//       var y = 10;
//       var z = x;
//       ....
//    }
//
// with one exception.  we don't flatten the decls inside of a for
// loop init, since this violates the rules of the AST, and blows up
// our traversal routines.

import * as b from '../ast-builder';
import { TransformPass } from '../node-visitor';
import { Stack } from '../stack-es6';
import Set  from '../set-es6';
import { reportWarning } from '../errors';

export class FlattenDecls extends TransformPass {
    constructor (options, filename) {
        super(options);
        this.filename = filename;
    }

    visitFor(n, ...args) {
        this.in_for = true;
        n.init   = super.visit(n.init, ...args);
        this.in_for = false;
        n.test   = super.visit(n.test, ...args);
        n.update = super.visit(n.update, ...args);
        n.body   = super.visit(n.body, ...args);
        return n;
    }

    visitVariableDeclaration (n) {
        if (this.in_for) return super.visitVariableDeclaration(n);

        if (n.declarations.length === 1)
            return super.visitVariableDeclaration(n);

        let rv = [];
        n.declarations.forEach((declarator) => {
            let new_declaration = b.variableDeclaration(n.kind, [declarator]);

            new_declaration.loc = declarator.loc;
            
            rv.push(new_declaration);
        });
        return rv;
    }
}
