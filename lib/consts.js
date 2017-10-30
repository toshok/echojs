/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

import * as types from "./types";
import * as llvm from "@llvm";

export function string(ir, c) {
    let constant = ir.createGlobalStringPtr(c, "strconst");
    constant.is_constant = true;
    constant.constant_val = c;
    return constant;
}

function intConstant(type, ...constant_val) {
    let constant = llvm.Constant.getIntegerValue(type, ...constant_val);
    constant.is_constant = true;
    constant.constant_val = constant_val;
    return constant;
}

export function jschar(c) {
    return intConstant(types.JSChar, c);
}
export function int32(c) {
    return intConstant(types.Int32, c);
}
export function int1(c) {
    return intConstant(types.Int1, c);
}
export function int64(c) {
    return intConstant(types.Int64, c);
}
export function int64_lowhi(ch, cl) {
    return intConstant(types.Int64, ch, cl);
}
export function bool(c) {
    let constant = llvm.Constant.getIntegerValue(types.Bool, c === false ? 0 : 1);
    constant.is_constant = true;
    constant.constant_val = c;
    return constant;
}

export function Null(t) {
    return llvm.Constant.getNull(t);
}

export function True() {
    return bool(true);
}
export function False() {
    return bool(false);
}

export function ejsval_true(is32bit) {
    return int64_lowhi(is32bit ? 0xffffff83 : 0xfff98000, 0x00000001);
}
export function ejsval_false(is32bit) {
    return int64_lowhi(is32bit ? 0xffffff83 : 0xfff98000, 0x00000000);
}
