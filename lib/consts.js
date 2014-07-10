module types from './types';
let llvm = require('llvm');

export function string(ir, c) {
    let constant = ir.createGlobalStringPtr(c, "strconst");
    constant.is_constant = true;
    constant.constant_val = c;
    return constant;
}
        
export function jschar(c) {
    constant = llvm.Constant.getIntegerValue(types.jschar, c);
    constant.is_constant = true;
    constant.constant_val = c;
    return constant;
}

export function int32(c) {
    constant = llvm.Constant.getIntegerValue(types.int32, c);
    constant.is_constant = true;
    constant.constant_val = c;
    return constant;
}

export function int1(c) {
    constant = llvm.Constant.getIntegerValue(types.int1, c);
    constant.is_constant = true;
    constant.constant_val = c;
    return constant;
}
                                
export function int64(c) {
    constant = llvm.Constant.getIntegerValue(types.int64, c);
    constant.is_constant = true;
    constant.constant_val = c;
    return constant;
}

export function int64_lowhi(ch, cl) {
    constant = llvm.Constant.getIntegerValue(types.int64, ch, cl);
    constant.is_constant = true;
    constant.constant_val = [ ch, cl ];
    return constant;
}

export function nullConst(t) {
    return llvm.Constant.getNull(t);
}

export function trueConst() {
    return llvm.Constant.getIntegerValue(types.bool, 1);
}

export function falseConst() {
    return llvm.Constant.getIntegerValue(types.bool, 0);
}

export function bool(c) {
    let constant = llvm.Constant.getIntegerValue(types.bool, c === false ? 0 : 1);
    constant.is_constant = true;
    constant.constant_val = c;
    return constant;
}

export function ejsval_true() {
    return int64_lowhi(0xfff98000, 0x00000001);
}
export function ejsval_false() {
    return int64_lowhi(0xfff98000, 0x00000000);
}
