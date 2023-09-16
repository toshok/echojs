var llvm = require("llvm");
var assert = require("assert");

describe("LLVM bindings", function () {
    describe("Type", function () {
        it("llvm.Type.getInt8Ty()", function () {
            assert.equal("i8", llvm.Type.getInt8Ty().toString());
            assert.equal("i8*", llvm.Type.getInt8Ty().pointerTo().toString());
        });
    });

    describe("FunctionType", function () {
        var int8ty = llvm.Type.getInt8Ty();
        it("llvm.FunctionType.get()", function () {
            assert.equal("i8 (i8)", llvm.FunctionType.get(int8ty, [int8ty]).toString());
            assert.equal(
                "i8 (i8)*",
                llvm.FunctionType.get(int8ty, [int8ty]).pointerTo().toString()
            );
        });
    });

    describe("Constant", function () {
        it("llvm.Constant.getIntegerValue()", function () {
            assert.equal(
                "i8 0",
                llvm.Constant.getIntegerValue(llvm.Type.getInt8Ty(), 0).toString()
            );
            assert.equal(
                "i32 512",
                llvm.Constant.getIntegerValue(llvm.Type.getInt32Ty(), 512).toString()
            );
        });
    });

    describe("ConstantFP", function () {
        it("llvm.Constant.getDouble()", function () {
            assert.equal("double 5.210000e+00", llvm.ConstantFP.getDouble(5.21));
        });
    });

    describe("Alloca", function () {
        it("llvm.IRBuilder.createAlloca()", function () {
            assert.equal(
                "%local = alloca i8",
                llvm.IRBuilder.createAlloca(llvm.Type.getInt8Ty(), "local")
            );
        });
    });

    describe("Stores", function () {
        it("llvm.IRBuilder.createStore()", function () {
            var alloca = llvm.IRBuilder.createAlloca(llvm.Type.getInt8Ty(), "local");
            var value = llvm.Constant.getIntegerValue(llvm.Type.getInt8Ty(), 0);

            assert.equal("store i8 0, i8* %local", llvm.IRBuilder.createStore(value, alloca));
        });
    });

    describe("Modules", function () {
        it("llvm.Module#ctor", function () {
            var module = new llvm.Module("test_module");
            assert.equal("; ModuleID = 'test_module'", module);
        });

        it("should add an extern reference to the module", function () {
            var module = new llvm.Module("test_module");
            module.getOrInsertExternalFunction("puts", llvm.Type.getInt32Ty(), [
                llvm.Type.getInt8Ty().pointerTo(),
            ]);
            assert.equal("; ModuleID = 'test_module'\n\ndeclare i32 @puts(i8*)", module);
        });
    });

    describe("Calls", function () {
        it("llvm.IRBuilder#createCall", function () {
            var module = new llvm.Module("test_module");
            var puts = module.getOrInsertExternalFunction("puts", llvm.Type.getInt32Ty(), [
                llvm.Type.getInt8Ty().pointerTo(),
            ]);

            var puts_call = llvm.IRBuilder.createCall(
                puts,
                [llvm.Constant.getNull(llvm.Type.getInt8Ty().pointerTo())],
                "puts_rv"
            );
            assert.equal("%puts_rv = call i32 @puts(i8* null)", puts_call);
        });
    });
});
