/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
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

import { typeofIsObject_id, typeofIsFunction_id, typeofIsString_id, typeofIsSymbol_id, typeofIsNumber_id, typeofIsBoolean_id, isNull_id, isUndefined_id, isNullOrUndefined_id } from '../common-ids';
import * as b from '../ast-builder';
import { is_intrinsic, create_intrinsic } from '../echo-util';
import { TreeVisitor } from '../node-visitor';

function is_typeof(e) { return e.type === b.UnaryExpression && e.operator === "typeof"; }
function is_string_literal(e) { return e.type === b.Literal && typeof e.value === "string"; }
function is_undefined_literal(e) { return e.type === b.Literal && e.value === undefined; }
function is_null_literal(e) { return e.type === b.Literal && e.value === null; }

export class EqIdioms extends TreeVisitor {

    visitBinaryExpression (exp) {
        if (exp.operator !== "==" && exp.operator !== "===" && exp.operator !== "!=" && exp.operator !== "!==")
            return super(exp);

        let left = exp.left;
        let right = exp.right;

        // for typeof checks against string literals, both == && === work
        if ((is_typeof(left) && is_string_literal(right)) || (is_typeof(right) && is_string_literal(left))) {
            let typecheck;
            let typeofarg;

            if (is_typeof(left)) {
                typecheck = right.value;
                typeofarg = left.argument;
            }
            else {
                typecheck = left.value;
                typeofarg = right.argument;
            }
            
            let intrinsic;
            switch (typecheck) {
            case "object":    intrinsic = typeofIsObject_id; break;
            case "function":  intrinsic = typeofIsFunction_id; break;
            case "string":    intrinsic = typeofIsString_id; break;
            case "symbol":    intrinsic = typeofIsSymbol_id; break;
            case "number":    intrinsic = typeofIsNumber_id; break;
            case "boolean":   intrinsic = typeofIsBoolean_id; break;
            case "null":      intrinsic = isNull_id; break;
            case "undefined": intrinsic = isUndefined_id; break;
            default:
                throw new Error(`invalid typeof check against '${typecheck}'`);
            }

            let rv = create_intrinsic(intrinsic, [typeofarg]);
            if (exp.operator[0] === '!')
                rv = b.unaryExpression('!', rv);
            return rv;
        }

        if (exp.operator === "==" || exp.operator === "!=") {
            if (is_null_literal(left) || is_null_literal(right) || is_undefined_literal(left) || is_undefined_literal(right)) {
                
                let checkarg = is_null_literal(left) || is_undefined_literal(left) ? right : left;
                
                let rv = create_intrinsic(isNullOrUndefined_id, [checkarg]);
                if (exp.operator === "!=")
                    rv = b.unaryExpression('!', rv);
                return rv;
            }
        }

        if (exp.operator === "===" || exp.operator === "!==") {
            if (is_null_literal(left) || is_null_literal(right)) {
                
                let checkarg = is_null_literal(left) ? right : left;

                let rv = create_intrinsic(isNull_id, [checkarg]);
                if (exp.operator === "!==")
                    rv = b.unaryExpression('!', rv);
                return rv;
            }
            if (is_undefined_literal(left) || is_undefined_literal(right)) {
                
                let checkarg = is_undefined_literal(left) ? right : left;

                let rv = create_intrinsic(isUndefined_id, [checkarg]);
                if (exp.operator === "!==")
                    rv = b.unaryExpression('!', rv);
                return rv;
            }
        }
        return super(exp);
    }
}
