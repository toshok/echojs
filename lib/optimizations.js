/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */
import * as escodegen from '../escodegen/escodegen-es6';

import * as debug from './debug';

import { EqIdioms }         from './passes/eq-idioms'
import { ReplaceUnaryVoid } from './passes/replace-unary-void'

const passes = [
    EqIdioms,
    ReplaceUnaryVoid,
];

export function run (tree) {

    passes.forEach ((passType) => {
        let pass = new passType();
        tree = pass.visit(tree);
        debug.log (2, `after: ${passType.name}`);
        debug.log (2, () => escodegen.generate(tree));
    });
    return tree;
}
