/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// coroutine-style generator desugar:
//
//   function* gen() {
//     yield 1;
//     yield 2;
//     yield 3;
//   }
//
// becomes
//
//   function gen() {
//     let %gen = %makeGenerator(() => {
//       %generatorYield(%gen, 1);
//       %generatorYield(%gen, 2);
//       %generatorYield(%gen, 3);
//     });
//     return %gen;
//   }
//
// the body closure runs on its own stack (runtime ucontext switch); the
// wrapper's catch converts the runtime's .return() sentinel into a
// normal return (see _ejs_Generator_prototype_return).

import { TransformPass, VisitResult } from "../node-visitor";
import * as b from "../ast-builder";
import { intrinsic, startGenerator } from "../echo-util";
import {
    makeGenerator_id,
    generatorYield_id,
    generatorIsReturnSentinel_id,
    generatorReturnValue_id,
} from "../common-ids";
import type * as e from "../estree";

export class DesugarGeneratorFunctions extends TransformPass {
    // innermost generator's %gen identifier first; functions nest
    private mapping: e.Identifier[] = [];
    private genGen = startGenerator();

    override visitFunction(n: e.Function): VisitResult {
        if (n.generator) this.mapping.unshift(b.identifier(`%_gen_${this.genGen()}`));
        super.visitFunction(n);
        if (n.generator) {
            const gen_id = this.mapping[0]!;
            // the body wraps in a catch that converts the runtime's
            // .return() sentinel into a normal return: gen.return(v)
            // resumes the suspended yield by throwing the sentinel, so
            // finally blocks run, and this outermost catch completes the
            // generator with v
            const exc_id = b.identifier(`%_genexc_${this.genGen()}`);
            const old_body = b.blockStatement([
                b.tryStatement(
                    n.body as e.BlockStatement,
                    [
                        b.catchClause(
                            exc_id,
                            b.blockStatement([
                                b.ifStatement(
                                    intrinsic(generatorIsReturnSentinel_id, [
                                        b.identifier(exc_id.name),
                                    ]),
                                    b.returnStatement(
                                        intrinsic(generatorReturnValue_id, [
                                            b.identifier(gen_id.name),
                                        ])
                                    ),
                                    b.throwStatement(b.identifier(exc_id.name))
                                ),
                            ])
                        ),
                    ],
                    null
                ),
            ]);
            n.body = b.blockStatement([
                b.letDeclaration(
                    gen_id,
                    intrinsic(makeGenerator_id, [b.arrowFunctionExpression([], old_body)])
                ),
                b.returnStatement(b.identifier(gen_id.name)),
            ]);
            n.generator = false;
        }
        this.mapping.shift();
        return n;
    }

    // yield* x  →  for (let %_yield of x) %generatorYield(%gen, %_yield);
    // (n.argument must already be visited)
    private delegateLoop(n: e.YieldExpression): e.ForOfStatement {
        const yield_id = b.identifier(`%_yield_${this.genGen()}`);
        return b.forOfStatement(
            b.letDeclaration(yield_id, null),
            n.argument!,
            b.blockStatement([
                b.expressionStatement(
                    intrinsic(generatorYield_id, [this.mapping[0]!, yield_id])
                ),
            ])
        );
    }

    // statement-position yield* replaces the whole ExpressionStatement
    // with the for-of loop, keeping the AST well-formed
    override visitExpressionStatement(n: e.ExpressionStatement): VisitResult {
        if (n.expression.type === "YieldExpression" && n.expression.delegate) {
            n.expression.argument = this.visitNullable(n.expression.argument);
            return this.delegateLoop(n.expression);
        }
        return super.visitExpressionStatement(n);
    }

    override visitYield(n: e.YieldExpression): VisitResult {
        n.argument = this.visitNullable(n.argument);
        if (n.delegate) {
            return this.delegateLoop(n);
        }
        return intrinsic(generatorYield_id, [this.mapping[0]!, n.argument ?? b.undefinedLit()]);
    }
}
