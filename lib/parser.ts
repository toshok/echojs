/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// The parser seam: everything upstream of the compiler
// goes through parse() here, so the parser is swappable behind one
// module.  The contract is the ESTree dialect in ./estree — whatever
// parser sits behind this module must produce that shape.
//
// The default parser is acorn (external-deps/acorn, standard ESTree),
// adapted to the dialect below; the esprima fork stays available via
// --parser esprima for bisection.
//
// The adapter also *gates*: syntax acorn parses but the backend does not
// implement (BigInt, dynamic import(), ...) dies here with a clear
// message instead of miscompiling silently.  Gates are deleted as
// lowering support lands.

import * as acorn from "../external-deps/acorn/acorn-es6";
import * as esprima from "../external-deps/esprima/esprima-es6";
import type { Program } from "./estree";

export interface ParseOptions {
    loc?: boolean;
    raw?: boolean;
    sourceType?: "script" | "module";
    // "acorn" (default) or "esprima" (the retained fork, for bisection)
    parser?: string;
}

// acorn nodes, structurally: type plus whatever fields the node kind has
interface Node {
    type: string;
    [key: string]: unknown;
}

class NotSupportedError extends Error {}

let catch_gen = 0;

function notSupported(node: Node, what: string): never {
    const loc = node["loc"] as { start?: { line: number; column: number } } | undefined;
    const where = loc?.start ? `${loc.start.line}:${loc.start.column + 1}: ` : "";
    throw new NotSupportedError(`${where}${what} is not supported yet`);
}

// dialect operator sets (lib/estree.ts unions); anything else is newer
// syntax the emitter has no lowering for
const BINARY_OPS = new Set([
    "==", "!=", "===", "!==", "<", "<=", ">", ">=", "<<", ">>", ">>>",
    "+", "-", "*", "/", "%", "**", "|", "^", "&", "in", "instanceof",
]);
const ASSIGN_OPS = new Set([
    "=", "+=", "-=", "*=", "/=", "%=", "**=", "<<=", ">>=", ">>>=", "|=", "^=", "&=",
    "&&=", "||=", "??=",
]);
const LOGICAL_OPS = new Set(["||", "&&", "??"]);

// in-place fixups on one node, applied before recursing into it
function adaptNode(n: Node): void {
    switch (n.type) {
        case "FunctionDeclaration":
        case "FunctionExpression":
        case "ArrowFunctionExpression": {
            // acorn nests parameter defaults as AssignmentPattern; the
            // dialect wants bare params plus an aligned defaults array
            // (empty when no parameter has a default)
            const params = n["params"] as Node[];
            const defaults: (Node | null)[] = [];
            let sawDefault = false;
            for (let i = 0; i < params.length; i++) {
                const p = params[i]!;
                if (p.type === "AssignmentPattern") {
                    const left = p["left"] as Node;
                    const right = p["right"] as Node;
                    // NamedEvaluation: f(cb = () => {}) names the default
                    if (left.type === "Identifier" && isAnonFn(right))
                        right["ejs_display_name"] = left["name"];
                    params[i] = left;
                    defaults.push(right);
                    sawDefault = true;
                } else {
                    defaults.push(null);
                }
            }
            n["defaults"] = sawDefault ? defaults : [];
            // spec .length (params before the first default or rest),
            // recorded now — the desugar passes rewrite param lists
            let fn_length = 0;
            for (let i = 0; i < params.length; i++) {
                if (params[i]!.type === "RestElement" || defaults[i] != null) break;
                fn_length++;
            }
            n["ejs_fn_length"] = fn_length;
            break;
        }
        case "TryStatement": {
            // dialect: handlers is an array (plus guardedHandlers), like
            // the esprima fork emitted
            const handler = n["handler"] as Node | null;
            n["handlers"] = handler ? [handler] : [];
            n["guardedHandlers"] = [];
            break;
        }
        case "CatchClause":
            // catch { } — synthesize an unused binding (fresh per clause;
            // %-names cannot collide with user code)
            if (n["param"] == null)
                n["param"] = { type: "Identifier", name: `%unused_catch_${catch_gen++}` };
            break;
        case "Property": {
            // NamedEvaluation: an anonymous function/arrow property value
            // is named after its (non-computed) key — { om() {} }, { a: () => {} }
            const key = n["key"] as Node;
            const value = n["value"] as Node;
            if (
                !n["computed"] &&
                isAnonFn(value) &&
                (key.type === "Identifier" || key.type === "Literal")
            ) {
                value["ejs_display_name"] =
                    key.type === "Identifier" ? key["name"] : String(key["value"]);
            }
            break;
        }
        case "PropertyDefinition": {
            // class field initialized with an anonymous function
            const key = n["key"] as Node;
            const value = n["value"] as Node | null;
            if (
                !n["computed"] &&
                value != null &&
                isAnonFn(value) &&
                (key.type === "Identifier" || key.type === "Literal")
            ) {
                value["ejs_display_name"] =
                    key.type === "Identifier" ? key["name"] : String(key["value"]);
            }
            break;
        }
        case "AssignmentPattern": {
            // NamedEvaluation: a destructuring default — [x = () => {}]
            const pleft = n["left"] as Node;
            const pright = n["right"] as Node;
            if (pleft.type === "Identifier" && isAnonFn(pright))
                pright["ejs_display_name"] = pleft["name"];
            break;
        }
        case "VariableDeclarator": {
            // NamedEvaluation: let f = function () {} / () => {}
            const vid = n["id"] as Node;
            const init = n["init"] as Node | null;
            if (vid.type === "Identifier" && init != null && isAnonFn(init))
                init["ejs_display_name"] = vid["name"];
            break;
        }
        case "MetaProperty": {
            // dialect stores the raw names, not Identifier nodes
            const meta = n["meta"] as Node;
            const property = n["property"] as Node;
            if (meta["name"] === "import") notSupported(n, "import.meta");
            n["meta"] = meta["name"];
            n["property"] = property["name"];
            break;
        }
        case "Literal":
            if (n["bigint"] != null) notSupported(n, "BigInt literal syntax");
            break;
        case "ImportExpression":
            notSupported(n, "dynamic import()");
            break;
        case "BinaryExpression":
            if (!BINARY_OPS.has(n["operator"] as string))
                notSupported(n, `the ${n["operator"]} operator`);
            break;
        case "AssignmentExpression": {
            if (!ASSIGN_OPS.has(n["operator"] as string))
                notSupported(n, `the ${n["operator"]} operator`);
            // NamedEvaluation: f = function () {}
            const left = n["left"] as Node;
            const right = n["right"] as Node;
            if (n["operator"] === "=" && left.type === "Identifier" && isAnonFn(right))
                right["ejs_display_name"] = left["name"];
            break;
        }
        case "LogicalExpression":
            if (!LOGICAL_OPS.has(n["operator"] as string))
                notSupported(n, `the ${n["operator"]} operator`);
            break;
        default:
            break;
    }
}

function isNode(v: unknown): v is Node {
    return v != null && typeof v === "object" && typeof (v as Node).type === "string";
}

// an anonymous function-valued node NamedEvaluation applies to
function isAnonFn(n: Node): boolean {
    return (
        (n.type === "FunctionExpression" && n["id"] == null) ||
        n.type === "ArrowFunctionExpression"
    );
}

function adaptTree(n: Node): void {
    adaptNode(n);
    for (const key of Object.keys(n)) {
        if (key === "loc") continue;
        const v = n[key];
        if (Array.isArray(v)) {
            for (const el of v) if (isNode(el)) adaptTree(el);
        } else if (isNode(v)) {
            adaptTree(v);
        }
    }
}

export function parse(source: string, options?: ParseOptions): Program {
    if (options?.parser === "esprima") {
        return esprima.parse(source, {
            loc: options.loc,
            raw: options.raw,
            sourceType: options.sourceType,
        });
    }

    const ast = acorn.parse(source, {
        ecmaVersion: "latest",
        sourceType: options?.sourceType ?? "script",
        locations: true,
    });
    adaptTree(ast);
    return ast as unknown as Program;
}
