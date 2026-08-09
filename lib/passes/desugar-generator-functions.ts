/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// generator desugar for the EIR state-machine lowering
// (docs/generator-eir-plan.md):
//
//   function* gen() {
//     yield 1;
//   }
//
// becomes
//
//   function gen() {
//     let %genobj = %makeGeneratorEIR((%genp, %mode, %sent) => {
//       try {
//         %generatorYield(%genp, 1);
//       } catch (%exc) {
//         if (%generatorIsReturnSentinel(%exc))
//           return %generatorReturnValue(%genp);
//         throw %exc;
//       }
//     });
//     return %genobj;
//   }
//
// The body arrow's signature is the resume protocol — the runtime
// re-calls it as body(gen, mode, sent) and gen-lower.ts rewrites the
// lowered EIR into a resume-dispatch state machine over a persistent
// env (the marker property below).  The catch converts the runtime's
// .return() sentinel into a normal return: gen.return(v) resumes the
// suspended yield by throwing the sentinel, so finally blocks run, and
// this outermost catch completes the generator with v.  yield* lowers
// through %generatorDelegate to an inline loop in the body — a state
// machine can only suspend its own frame, never a nested helper's.

import { TransformPass, VisitResult } from "../node-visitor";
import * as b from "../ast-builder";
import { intrinsic, startGenerator } from "../echo-util";
import {
    makeGeneratorEIR_id,
    generatorYield_id,
    generatorDelegate_id,
    generatorIsReturnSentinel_id,
    generatorReturnValue_id,
} from "../common-ids";
import type * as e from "../estree";

export class DesugarGeneratorFunctions extends TransformPass {
    // innermost generator's body-arrow generator PARAM, first; functions
    // nest.  Yields reference the param (the runtime passes the
    // generator object on every resume), so the body never captures the
    // wrapper's binding.
    private mapping: e.Identifier[] = [];
    private genGen = startGenerator();

    override visitFunction(n: e.Function): VisitResult {
        // pair the unshift/shift on THIS function's generator-ness: an
        // unconditional shift would let any non-generator function nested
        // in a generator body pop the generator's own %gen id
        const is_generator = n.generator;
        if (is_generator) {
            this.mapping.unshift(b.identifier(`%_genp_${this.genGen()}`));
        }
        super.visitFunction(n);
        if (n.generator) {
            const gen_id = this.mapping[0]!;
            // the body wraps in a catch that converts the runtime's
            // .return() sentinel into a normal return
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
            // the resume-protocol signature: gen-lower.ts reads the
            // params positionally, the body reads yields' generator
            // operand through the first
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
            n.generator = false;
        }
        if (is_generator) {
            this.mapping.shift();
        }
        return n;
    }

    override visitYield(n: e.YieldExpression): VisitResult {
        n.argument = this.visitNullable(n.argument);
        if (n.delegate) return intrinsic(generatorDelegate_id, [this.mapping[0]!, n.argument!]);
        return intrinsic(generatorYield_id, [this.mapping[0]!, n.argument ?? b.undefinedLit()]);
    }
}
