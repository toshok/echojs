/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// The --debug-passes / debug.log printer: ESTree (in the compiler's
// old-esprima dialect) → source text, via astring extended with the
// dialect's extras.  Debug fidelity only — nothing downstream consumes
// this output, so a printing gap is a cosmetic bug, never a miscompile.
//
// The dialect extras astring doesn't know (lib/parser.ts produces them,
// the desugar passes preserve them):
//   - function-level `defaults` (array aligned to params) and `rest`
//     (an Identifier) instead of AssignmentPattern/RestElement params;
//   - TryStatement `handlers`/`guardedHandlers` ARRAYS (old-SpiderMonkey
//     `catch (e if cond)`) beside standard `handler`.
// Not covered (prints without its extras, rare in debug output):
// object/class METHOD params with defaults — MethodDefinition still
// routes through astring's stock generator.

import { GENERATOR, generate } from "../external-deps/astring/astring-es6";
import type { AstringGenerator, AstringState } from "../external-deps/astring/astring-es6";
import type * as e from "./estree";

interface DialectFunction {
    params: e.Pattern[];
    defaults?: (e.Node | null)[];
    rest?: e.Identifier | null;
    body: e.Node;
    id?: e.Identifier | null;
    async?: boolean;
    generator?: boolean;
}

function writeDialectParams(state: AstringState, node: DialectFunction): void {
    const params = node.params || [];
    const defaults = node.defaults || [];
    state.write("(");
    for (let i = 0; i < params.length; i++) {
        if (i > 0) state.write(", ");
        const p = params[i]! as unknown as e.Node;
        state.generator[p.type]!(p, state);
        const d = defaults[i];
        if (d) {
            state.write(" = ");
            state.generator[d.type]!(d, state);
        }
    }
    if (node.rest) {
        if (params.length > 0) state.write(", ");
        state.write("...");
        state.generator[node.rest.type]!(node.rest as unknown as e.Node, state);
    }
    state.write(")");
}

function writeCatchClause(g: AstringGenerator, state: AstringState, h: e.CatchClause): void {
    const guard = (h as unknown as { guard?: e.Node | null }).guard;
    if (h.param) {
        state.write(" catch (");
        g[h.param.type]!(h.param as unknown as e.Node, state);
        if (guard) {
            state.write(" if ");
            g[guard.type]!(guard, state);
        }
        state.write(") ");
    } else {
        state.write(" catch ");
    }
    g[h.body.type]!(h.body as unknown as e.Node, state);
}

// `function` (not arrows) throughout: astring's generator functions
// dispatch through `this` bound to the generator table.
const dialectGenerator: AstringGenerator = Object.assign({}, GENERATOR, {
    FunctionDeclaration(node: e.Node, state: AstringState): void {
        const f = node as unknown as DialectFunction;
        state.write(
            (f.async ? "async " : "") +
                (f.generator ? "function* " : "function ") +
                (f.id ? f.id.name : ""),
            node
        );
        writeDialectParams(state, f);
        state.write(" ");
        state.generator[f.body.type]!(f.body, state);
    },
    FunctionExpression(node: e.Node, state: AstringState): void {
        dialectGenerator["FunctionDeclaration"]!(node, state);
    },
    ArrowFunctionExpression(node: e.Node, state: AstringState): void {
        const f = node as unknown as DialectFunction;
        state.write(f.async ? "async " : "", node);
        writeDialectParams(state, f);
        state.write(" => ");
        if (f.body.type === "ObjectExpression") {
            state.write("(");
            state.generator["ObjectExpression"]!(f.body, state);
            state.write(")");
        } else {
            state.generator[f.body.type]!(f.body, state);
        }
    },
    TryStatement(node: e.Node, state: AstringState): void {
        // the dialect's estree.ts already declares `handlers`; standard-ESTree
        // `handler` may still appear on trees that predate the conversion
        const t = node as unknown as e.TryStatement & {
            handler?: e.CatchClause | null;
            handlers?: e.CatchClause[] | null;
            guardedHandlers?: e.CatchClause[] | null;
        };
        state.write("try ");
        state.generator[t.block.type]!(t.block as unknown as e.Node, state);
        const g = state.generator;
        for (const h of t.guardedHandlers || []) writeCatchClause(g, state, h);
        if (t.handler) writeCatchClause(g, state, t.handler);
        for (const h of t.handlers || []) if (h !== t.handler) writeCatchClause(g, state, h);
        if (t.finalizer) {
            state.write(" finally ");
            state.generator[t.finalizer.type]!(t.finalizer as unknown as e.Node, state);
        }
    },
});

/** Render a (possibly dialect-flavored) tree for debug output. */
export function generateDebug(tree: e.Node): string {
    return generate(tree, { generator: dialectGenerator });
}
