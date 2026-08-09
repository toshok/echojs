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
    makeGeneratorEIR_id,
    generatorYield_id,
    generatorDelegate_id,
    generatorIsReturnSentinel_id,
    generatorReturnValue_id,
} from "../common-ids";
import { passes } from "../pass-config";
import type * as e from "../estree";

export class DesugarGeneratorFunctions extends TransformPass {
    // innermost generator's %gen identifier first; functions nest.
    // Under -fgen-eir this is the body arrow's generator PARAM (the
    // runtime passes the generator object on every resume), so the body
    // need not capture the wrapper's binding at all.
    private mapping: e.Identifier[] = [];
    // parallel to mapping: the generator's delegation-helper name, and
    // whether a yield* actually referenced it (inject only then).
    // -fgen-eir has no helper: yield* lowers to an inline EIR loop
    // (%generatorDelegate) — a state machine can only suspend its own
    // frame, never a nested helper's.
    private delegateIds: e.Identifier[] = [];
    private usedDelegate: boolean[] = [];
    private genGen = startGenerator();

    override visitFunction(n: e.Function): VisitResult {
        // pair the unshift/shift on THIS function's generator-ness: an
        // unconditional shift would let any non-generator function nested
        // in a generator body pop the generator's own %gen id
        const is_generator = n.generator;
        if (is_generator) {
            this.mapping.unshift(
                b.identifier(passes().genEir ? `%_genp_${this.genGen()}` : `%_gen_${this.genGen()}`)
            );
            this.delegateIds.unshift(b.identifier(`__ejs_genDelegate_${this.genGen()}`));
            this.usedDelegate.unshift(false);
        }
        super.visitFunction(n);
        if (n.generator) {
            const gen_id = this.mapping[0]!;
            // the body wraps in a catch that converts the runtime's
            // .return() sentinel into a normal return: gen.return(v)
            // resumes the suspended yield by throwing the sentinel, so
            // finally blocks run, and this outermost catch completes the
            // generator with v
            const exc_id = b.identifier(`%_genexc_${this.genGen()}`);
            const body_stmts: e.Statement[] = [];
            if (this.usedDelegate[0])
                body_stmts.push(this.delegateHelperDecl(this.delegateIds[0]!.name));
            const old_body = b.blockStatement([
                ...body_stmts,
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
            // parameter-destructuring prologue (tagged by
            // DesugarDestructuring) stays in the wrapper: 9.2.10 binds
            // parameters at call time, so a poisoned iterator throws
            // before the generator object exists.  the body closure
            // captures the hoisted bindings.
            const wrapper_prologue: e.Statement[] = [];
            const try_block = old_body.body[old_body.body.length - 1] as e.TryStatement;
            const gen_body = try_block.block as e.BlockStatement;
            while (
                gen_body.body.length &&
                (gen_body.body[0] as unknown as Record<string, unknown>)["ejs_param_prologue"]
            ) {
                wrapper_prologue.push(gen_body.body.shift()!);
            }
            if (passes().genEir) {
                // the body arrow's signature is the resume protocol: the
                // runtime re-calls it as body(gen, mode, sent) — gen-lower.ts
                // reads the params positionally, the body reads yields'
                // generator operand through the first
                const mode_id = b.identifier(`%_genmode_${this.genGen()}`);
                const sent_id = b.identifier(`%_gensent_${this.genGen()}`);
                const arrow = b.arrowFunctionExpression(
                    [b.identifier(gen_id.name), mode_id, sent_id],
                    old_body
                );
                (arrow as unknown as Record<string, unknown>)["ejs_gen_eir_body"] = true;
                const wrap_id = b.identifier(`%_genobj_${this.genGen()}`);
                n.body = b.blockStatement([
                    ...wrapper_prologue,
                    b.letDeclaration(wrap_id, intrinsic(makeGeneratorEIR_id, [arrow])),
                    b.returnStatement(b.identifier(wrap_id.name)),
                ]);
            } else {
                n.body = b.blockStatement([
                    ...wrapper_prologue,
                    b.letDeclaration(
                        gen_id,
                        intrinsic(makeGenerator_id, [b.arrowFunctionExpression([], old_body)])
                    ),
                    b.returnStatement(b.identifier(gen_id.name)),
                ]);
            }
            n.generator = false;
        }
        if (is_generator) {
            this.mapping.shift();
            this.delegateIds.shift();
            this.usedDelegate.shift();
        }
        return n;
    }

    // yield* x  →  __ejs_genDelegate_N(%gen, x) — a plain call, so it
    // works in any expression position; the generator runs on its own
    // coroutine stack, so the helper yields fine from its nested frame.
    // The helper forwards sent values into the inner iterator's next()
    // and produces the inner return value (both of which the old for-of
    // expansion dropped):
    //
    //   function __ejs_genDelegate_N(g, x) {
    //       let it = x[Symbol.iterator]();
    //       let res = it.next();
    //       while (!res.done) {
    //           let sent;
    //           try { sent = %generatorYield(g, res.value); }
    //           catch (e) { if (it.return != null) it.return(); throw e; }
    //           res = it.next(sent);
    //       }
    //       return res.value;
    //   }
    //
    // the catch is IteratorClose: gen.throw()/gen.return() surface at the
    // suspended yield as a throw (the return sentinel included) — close
    // the inner iterator and let the completion propagate.  Not forwarded
    // to it.throw()/it.return()'s resumption semantics.
    private delegateHelperDecl(name: string): e.FunctionDeclaration {
        const g = b.identifier("g");
        const x = b.identifier("x");
        const it = b.identifier("it");
        const res = b.identifier("res");
        const sent = b.identifier("sent");
        const exc = b.identifier("e");
        const itNext = (arg: e.Expression | null) =>
            b.callExpression(b.memberExpression(b.identifier(it.name), b.identifier("next")), arg ? [arg] : []);
        // built per use: AST nodes must not be shared (node-keyed maps)
        const itReturn = () => b.memberExpression(b.identifier(it.name), b.identifier("return"));
        return b.functionDeclaration(
            b.identifier(name),
            [g, x],
            b.blockStatement([
                b.letDeclaration(
                    it,
                    b.callExpression(
                        b.memberExpression(
                            b.identifier(x.name),
                            b.memberExpression(b.identifier("Symbol"), b.identifier("iterator")),
                            true
                        ),
                        []
                    )
                ),
                b.letDeclaration(res, itNext(null)),
                b.whileStatement(
                    b.unaryExpression(
                        "!",
                        b.memberExpression(b.identifier(res.name), b.identifier("done"))
                    ),
                    b.blockStatement([
                        b.letDeclaration(sent, null),
                        b.tryStatement(
                            b.blockStatement([
                                b.expressionStatement(
                                    b.assignmentExpression(
                                        b.identifier(sent.name),
                                        "=",
                                        intrinsic(generatorYield_id, [
                                            b.identifier(g.name),
                                            b.memberExpression(
                                                b.identifier(res.name),
                                                b.identifier("value")
                                            ),
                                        ])
                                    )
                                ),
                            ]),
                            [
                                b.catchClause(
                                    exc,
                                    b.blockStatement([
                                        b.ifStatement(
                                            b.binaryExpression(itReturn(), "!=", b.nullLit()),
                                            b.expressionStatement(b.callExpression(itReturn(), []))
                                        ),
                                        b.throwStatement(b.identifier(exc.name)),
                                    ])
                                ),
                            ],
                            null
                        ),
                        b.expressionStatement(
                            b.assignmentExpression(
                                b.identifier(res.name),
                                "=",
                                itNext(b.identifier(sent.name))
                            )
                        ),
                    ])
                ),
                b.returnStatement(b.memberExpression(b.identifier(res.name), b.identifier("value"))),
            ])
        );
    }

    override visitYield(n: e.YieldExpression): VisitResult {
        n.argument = this.visitNullable(n.argument);
        if (n.delegate) {
            if (passes().genEir)
                return intrinsic(generatorDelegate_id, [this.mapping[0]!, n.argument!]);
            this.usedDelegate[0] = true;
            return b.callExpression(b.identifier(this.delegateIds[0]!.name), [
                this.mapping[0]!,
                n.argument!,
            ]);
        }
        return intrinsic(generatorYield_id, [this.mapping[0]!, n.argument ?? b.undefinedLit()]);
    }
}
