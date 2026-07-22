/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// Ambient declarations for the "@llvm" native module (node-llvm when
// node-hosted, ejs-llvm when self-hosted).  The surface here is exactly
// what the compiler uses — extend it as needs grow; do NOT widen types
// to any.
//
// The compiler also hangs bookkeeping properties off llvm objects
// (is_constant on constants, entry_bb/literalAllocas/topScope on
// functions, ...).  Those are declared here, optional, so the habit is
// visible and type-checked rather than smuggled.

declare module "@llvm" {
    // --- values --------------------------------------------------------------

    interface Value {
        setName(name: string): void;
        dump(): void;
        // compiler bookkeeping: values that hold the runtime's boxed-bool
        // encoding (see loadBoolEjsValue / forwardCalleeAttributes)
        _ejs_returns_ejsval_bool?: boolean;
        // compiler bookkeeping: constant tracking (see consts.ts)
        is_constant?: boolean;
        constant_val?: string | number | boolean | number[] | null;
    }

    interface Constant extends Value {}

    const Constant: {
        getNull(type: Type): Constant;
        getAggregateZero(type: Type): Constant;
        getIntegerValue(type: Type, ...val: number[]): Constant;
    };

    const ConstantFP: {
        getDouble(val: number): Constant;
    };

    const ConstantArray: {
        get(type: Type, elements: Constant[]): Constant;
    };

    // --- types ---------------------------------------------------------------

    interface Type {
        pointerTo(): Type;
    }

    interface StructType extends Type {
        setStructBody(elements: Type[]): void;
    }

    interface FunctionType extends Type {}

    const Type: {
        getInt1Ty(): Type;
        getInt8Ty(): Type;
        getInt16Ty(): Type;
        getInt32Ty(): Type;
        getInt64Ty(): Type;
        getDoubleTy(): Type;
        getVoidTy(): Type;
    };

    const StructType: {
        create(name: string, elements: Type[]): StructType;
    };

    const FunctionType: {
        // the 3-arg form is jsllvm's sret shape: the real return value
        // is written through an sret pointer while `ret` is void
        get(ret: Type, params: Type[], sret?: Type): FunctionType;
    };

    const ArrayType: {
        get(elem: Type, count: number): Type;
    };

    // --- functions / globals / blocks -----------------------------------------

    interface Argument extends Value {}

    interface EjsFunction extends Value {
        args: Argument[];
        argSize: number;
        type: FunctionType;
        returnType: Type;
        setInternalLinkage(): void;
        setExternalLinkage(): void;
        setDoesNotThrow(): void;
        setDoesNotAccessMemory(): void;
        setOnlyReadsMemory(): void;
        setStructRet(): void;
        hasStructRetAttr(): boolean;
        setGC(name: string): void;
        setPersonality(fn: Value): void;
        // compiler bookkeeping
        doesNotThrow?: boolean;
        doesNotAccessMemory?: boolean;
        onlyReadsMemory?: boolean;
        returns_ejsval_bool?: boolean;
        takes_builtins?: boolean;
        entry_bb?: BasicBlock;
        literalAllocas?: Record<string, AllocaInst>;
        topScope?: Map<string, Value>;
        bits_alloca?: AllocaInst;
        debug_info?: DISubprogram;
        hasPersonality(): boolean;
    }

    interface BasicBlock {
        parent: EjsFunction;
    }
    const BasicBlock: {
        new (name: string, parent: EjsFunction): BasicBlock;
    };

    interface GlobalVariable extends Value {
        setInitializer(init: Constant): void;
        setAlignment(align: number): void;
    }
    const GlobalVariable: {
        new (
            module: Module,
            type: Type,
            name: string,
            init: Constant | null,
            visible?: boolean
        ): GlobalVariable;
    };

    interface Module {
        setTriple(triple: string): void;
        setDataLayout(layout: string): void;
        getOrInsertFunction(name: string, ret: Type, params: Type[]): EjsFunction;
        getOrInsertExternalFunction(name: string, ret: Type, params: Type[]): EjsFunction;
        getOrInsertGlobal(name: string, type: Type): GlobalVariable;
        getOrInsertIntrinsic(name: string, types?: Type[]): EjsFunction;
        getFunction(name: string): EjsFunction | null;
        writeToFile(path: string): void;
        writeBitcodeToFile(path: string): void;
        dump(): void;
        toString(): string;
    }
    const Module: {
        new (name: string): Module;
    };

    // --- instruction building --------------------------------------------------

    interface CallInst extends Value {
        setOnlyReadsMemory(): void;
        setDoesNotAccessMemory(): void;
        setDoesNotThrow(): void;
        setStructRet(): void;
    }

    interface InvokeInst extends CallInst {}

    interface LandingPad extends Value {
        setCleanup(cleanup: boolean): void;
        addClause(clause: Value): void;
    }

    interface PhiNode extends Value {
        addIncoming(value: Value, block: BasicBlock): void;
    }

    interface AllocaInst extends Value {
        setAlignment(align: number): void;
    }

    interface Switch extends Value {
        addCase(val: Constant, dest: BasicBlock): void;
    }

    const IRBuilder: {
        setInsertPoint(bb: BasicBlock | null): void;
        setInsertPointStartBB(bb: BasicBlock): void;
        getInsertBlock(): BasicBlock | null;
        setCurrentDebugLocation(loc: DebugLoc): void;
        getCurrentDebugLocation(): DebugLoc;

        createAlloca(type: Type, name: string): AllocaInst;
        createBitCast(value: Value, type: Type, name: string): Value;
        createBr(bb: BasicBlock): Value;
        createCondBr(cond: Value, then_bb: BasicBlock, else_bb: BasicBlock): Value;
        createCall(fnType: FunctionType, callee: Value, args: Value[], name: string): CallInst;
        createInvoke(
            fnType: FunctionType,
            callee: Value,
            args: Value[],
            normal: BasicBlock,
            unwind: BasicBlock,
            name: string
        ): InvokeInst;
        createExtractValue(agg: Value, idx: number, name: string): Value;
        createGetElementPointer(type: Type, ptr: Value, idxs: Value[], name: string): Value;
        createInBoundsGetElementPointer(
            type: Type,
            ptr: Value,
            idxs: Value[],
            name: string
        ): Value;
        createGlobalStringPtr(value: string, name: string): Constant;
        createICmpEq(l: Value, r: Value, name: string): Value;
        createFAdd(l: Value, r: Value, name: string): Value;
        createFSub(l: Value, r: Value, name: string): Value;
        createFMul(l: Value, r: Value, name: string): Value;
        createFDiv(l: Value, r: Value, name: string): Value;
        createFCmpOLT(l: Value, r: Value, name: string): Value;
        createICmpSGt(l: Value, r: Value, name: string): Value;
        createICmpUGt(l: Value, r: Value, name: string): Value;
        createICmpULt(l: Value, r: Value, name: string): Value;
        createLandingPad(type: Type, numClauses: number, name: string): LandingPad;
        createLoad(type: Type, ptr: Value, name: string): Value;
        createNswSub(l: Value, r: Value, name: string): Value;
        createOr(l: Value, r: Value, name: string): Value;
        createPhi(type: Type, count: number, name: string): PhiNode;
        createPointerCast(value: Value, type: Type, name: string): Value;
        createPtrToInt(value: Value, type: Type, name: string): Value;
        createRet(value: Value): Value;
        createRetVoid(): Value;
        createSelect(cond: Value, t: Value, f: Value, name: string): Value;
        createStore(value: Value, ptr: Value, name?: string): Value;
        createSwitch(value: Value, dflt: BasicBlock, numCases: number): Switch;
        createTrunc(value: Value, type: Type, name: string): Value;
        createUnreachable(): Value;
        createZExt(value: Value, type: Type, name: string): Value;
    };

    // --- debug info -------------------------------------------------------------

    interface DebugLoc {}
    const DebugLoc: {
        get(line: number, column: number, scope: DIDescriptor): DebugLoc;
    };

    interface DIDescriptor {}
    interface DIFile extends DIDescriptor {}
    interface DISubprogram extends DIDescriptor {}

    interface DIBuilder {
        createFile(filename: string, directory: string): DIFile;
        createCompileUnit(
            filename: string,
            directory: string,
            producer: string,
            optimized: boolean,
            flags: string,
            runtimeVersion: number
        ): DIDescriptor;
        createFunction(
            scope: DIDescriptor,
            name: string,
            displayName: string,
            file: DIFile,
            lineNo: number,
            isLocalToUnit: boolean,
            isDefinition: boolean,
            scopeLine: number,
            flags: number,
            isOptimized: boolean,
            fn: EjsFunction
        ): DISubprogram;
        finalize(): void;
    }
    const DIBuilder: {
        new (module: Module): DIBuilder;
    };
}
