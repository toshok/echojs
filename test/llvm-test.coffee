llvm = require 'llvm'
assert = require 'echo-assert'


#### TYPE TESTS
assert.eq "type1", "#{llvm.Type.getInt8Ty()}",           "i8"
assert.eq "type2", "#{llvm.Type.getInt8Ty().pointerTo}", "i8*"

#### FUNCTIONTYPE TESTS
int8ty = llvm.Type.getInt8Ty()
assert.eq "functype1", "#{llvm.FunctionType.get int8ty, [int8ty]}", "i8 (i8)"
assert.eq "functype2", "#{(llvm.FunctionType.get int8ty, [int8ty]).pointerTo}", "i8 (i8)*"


#### CONSTANT TESTS
assert.eq "constant1", "#{llvm.Constant.getIntegerValue llvm.Type.getInt8Ty(), 0}",    "i8 0"
assert.eq "constant2", "#{llvm.Constant.getIntegerValue llvm.Type.getInt32Ty(), 512}", "i32 512"

#### CONSTANTFP TESTS
assert.eq "constantfp1", "#{llvm.ConstantFP.getDouble 5.21}", "double 5.210000e+00"

#### ALLOCA TESTS
assert.eq "alloca1", "#{llvm.IRBuilder.createAlloca llvm.Type.getInt8Ty(), 'local'}", "%local = alloca i8"

#### STORE TESTS
alloca = llvm.IRBuilder.createAlloca llvm.Type.getInt8Ty(), 'local'
value = llvm.Constant.getIntegerValue llvm.Type.getInt8Ty(), 0
assert.eq "store1", "#{llvm.IRBuilder.createStore value, alloca}", "store i8 0, i8* %local"

#### MODULE TESTS
module = new llvm.Module "test_module"
assert.eq "module1", "#{module}", "; ModuleID = 'test_module'"

# add an extern reference to 'int puts(char*)' to the module
puts = module.getOrInsertExternalFunction "puts", llvm.Type.getInt32Ty(), [llvm.Type.getInt8Ty().pointerTo]

assert.eq "module-extern-func", "#{module}", "; ModuleID = 'test_module'\n\ndeclare i32 @puts(i8*)"

# now create a call to this function
#puts_type = llvm.FunctionType.get llvm.Type.getInt32Ty(), [llvm.Type.getInt8Ty().pointerTo]
puts_call = llvm.IRBuilder.createCall puts, [llvm.Constant.getNull llvm.Type.getInt8Ty().pointerTo], "puts_rv"

assert.eq "module-call-extern-func", "#{puts_call}", "%puts_rv = call i32 @puts(i8* null)"

assert.dumpStats()
