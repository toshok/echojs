let escodegen = require('escodegen');

module debug from 'debug';

import { EqIdioms } from 'passes/eq-idioms'
import { ReplaceUnaryVoid } from 'passes/replace-unary-void'

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
