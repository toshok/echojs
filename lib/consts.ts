/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

import * as types from "./types";
import * as llvm from "@llvm";

// the IRBuilder surface string() needs (avoids importing the whole thing)
interface StringBuilder {
    createGlobalStringPtr(value: string, name: string): llvm.Constant;
}

export function string(ir: StringBuilder, c: string): llvm.Constant {
    const constant = ir.createGlobalStringPtr(c, "strconst");
    constant.is_constant = true;
    constant.constant_val = c;
    return constant;
}

function intConstant(type: llvm.Type, ...constant_val: number[]): llvm.Constant {
    const constant = llvm.Constant.getIntegerValue(type, ...constant_val);
    constant.is_constant = true;
    constant.constant_val = constant_val;
    return constant;
}

export function jschar(c: number): llvm.Constant {
    return intConstant(types.JSChar, c);
}
export function int32(c: number): llvm.Constant {
    return intConstant(types.Int32, c);
}
export function int1(c: number): llvm.Constant {
    return intConstant(types.Int1, c);
}
export function int64(c: number): llvm.Constant {
    return intConstant(types.Int64, c);
}
export function int64_lowhi(ch: number, cl: number): llvm.Constant {
    return intConstant(types.Int64, ch, cl);
}
export function bool(c: boolean): llvm.Constant {
    const constant = llvm.Constant.getIntegerValue(types.Bool, c === false ? 0 : 1);
    constant.is_constant = true;
    constant.constant_val = c;
    return constant;
}

export function Null(t: llvm.Type): llvm.Constant {
    return llvm.Constant.getNull(t);
}

export function True(): llvm.Constant {
    return bool(true);
}
export function False(): llvm.Constant {
    return bool(false);
}

export function ejsval_true(is32bit: boolean): llvm.Constant {
    return int64_lowhi(is32bit ? 0xffffff83 : 0xfff98000, 0x00000001);
}
export function ejsval_false(is32bit: boolean): llvm.Constant {
    return int64_lowhi(is32bit ? 0xffffff83 : 0xfff98000, 0x00000000);
}
