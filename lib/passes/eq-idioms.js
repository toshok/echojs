/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */
//
// EqIdioms checks for the following things:
//
// typeof() checks against constant strings.
//
//   For most cases we can inline the test directly into LLVM IR (in
//   compiler.coffee), and in the cases where we can't easily, we can
//   call specialized runtime builtins that don't require us to
//   allocate a string do a comparison.
//
// ==/!= of constants null or undefined
//

import {
    typeofIsObject_id,
    typeofIsFunction_id,
    typeofIsString_id,
    typeofIsSymbol_id,
    typeofIsNumber_id,
    typeofIsBoolean_id,
    isNull_id,
    isUndefined_id,
    isNullOrUndefined_id,
} from "../common-ids";
import * as b from "../ast-builder";
import { is_intrinsic, create_intrinsic } from "../echo-util";
import { TreeVisitor } from "../node-visitor";

function is_typeof(e) {
    return e.type === b.UnaryExpression && e.operator === "typeof";
}
function is_string_literal(e) {
    return e.type === b.Literal && typeof e.value === "string";
}
function is_undefined_literal(e) {
    return e.type === b.Literal && e.value === undefined;
}
function is_null_literal(e) {
    return e.type === b.Literal && e.value === null;
}
function is_null_or_undefined_literal(e) {
    return is_undefined_literal(e) || is_null_literal(e);
}

function eq_neq_op(op) {
    return op === "==" || op === "!=" || op === "===" || op === "!==";
}

function op_coerces(op) {
    return op.length == 2;
}

function maybe_not(op, exp) {
    if (op[0] === "!") {
        return b.unaryExpression("!", exp);
    }
    return exp;
}

const typecheckIntrinsics = {
    object: typeofIsObject_id,
    function: typeofIsFunction_id,
    string: typeofIsString_id,
    symbol: typeofIsSymbol_id,
    number: typeofIsNumber_id,
    boolean: typeofIsBoolean_id,
    null: isNull_id,
    undefined: isUndefined_id,
};

export class EqIdioms extends TreeVisitor {
    visitBinaryExpression(exp) {
        if (!eq_neq_op(exp.operator)) {
            return super.visitBinaryExpression(exp);
        }

        let left = exp.left;
        let right = exp.right;

        // for typeof checks against string literals, both == && === work
        if (
            (is_typeof(left) && is_string_literal(right)) ||
            (is_typeof(right) && is_string_literal(left))
        ) {
            let typecheck = is_typeof(left) ? right.value : left.value;
            let typeofarg = is_typeof(left) ? left.argument : right.argument;

            let intrinsic = typecheckIntrisics[typecheck];
            if (!intrinsic) {
                throw new Error(`invalid typeof check against '${typecheck}'`);
            }

            return maybe_not(exp.operator, create_intrinsic(intrinsic, [typeofarg]));
        }

        // check for null/undefined comparisons
        if (!is_null_or_undefined_literal(left) && !is_null_or_undefined_literal(right)) {
            return super.visitBinaryExpression(exp);
        }

        // one or both of subexpressions are null/undefined literals

        if (op_coerces(exp.operator)) {
            // == or != here, so we need to match both (hence isNullOrUndefined).
            let checkarg = is_null_or_undefined_literal(left) ? right : left;
            return maybe_not(exp.operator, create_intrinsic(isNullOrUndefined_id, [checkarg]));
        }

        // === or !== below here.  at least one of left/right is either null or undefined literal
        if (is_null_literal(left) || is_null_literal(right)) {
            let checkarg = is_null_literal(left) ? right : left;
            return maybe_not(exp.operator, create_intrinsic(isNull_id, [checkarg]));
        }

        // === or !== below here.  at least one of left/right is undefined literal
        let checkarg = is_undefined_literal(left) ? right : left;
        return maybe_not(exp.operator, create_intrinsic(isUndefined_id, [checkarg]));
    }
}
