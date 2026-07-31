/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// The parser seam (language-P2): everything upstream of the compiler
// goes through parse() here, so the parser is swappable behind one
// module.  The contract is the ESTree dialect in ./estree — whatever
// parser sits behind this module must produce that shape.

import * as esprima from "../external-deps/esprima/esprima-es6";
import type { Program } from "./estree";

export interface ParseOptions {
    loc?: boolean;
    raw?: boolean;
    sourceType?: "script" | "module";
}

export function parse(source: string, options?: ParseOptions): Program {
    return esprima.parse(source, options);
}
