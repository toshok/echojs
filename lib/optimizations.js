/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */
import * as escodegen from "../external-deps/escodegen/escodegen-es6";

import * as debug from "./debug";

import { ReplaceUnaryVoid } from "./passes/replace-unary-void";

const passes = [ReplaceUnaryVoid];

export function run(tree) {
    passes.forEach(passType => {
        let pass = new passType();
        tree = pass.visit(tree);
        debug.log(2, `after: ${passType.name}`);
        debug.log(2, () => escodegen.generate(tree));
    });
    return tree;
}
