llvm = require 'llvm'
assert = require 'echo-assert'

assert.eq "type1", "#{llvm.Type.getInt8Ty()}",           "i8",  assert.XFAIL "(Type.toString seems to be returning `' for everything, even though dump() works)"
assert.eq "type2", "#{llvm.Type.getInt8Ty().pointerTo}", "i8*", assert.XFAIL "(Type.toString seems to be returning `' for everything, even though dump() works)"

# integer constants
assert.eq "constant1", "#{llvm.Constant.getIntegerValue llvm.Type.getInt8Ty(), 0}",    "i8 0"
assert.eq "constant2", "#{llvm.Constant.getIntegerValue llvm.Type.getInt32Ty(), 512}", "i32 512"

# floating point constants
assert.eq "constantfp1", "#{llvm.ConstantFP.getDouble 5.21}", "double 5.210000e+00"

# alloca
assert.eq "alloca1", "#{llvm.IRBuilder.createAlloca llvm.Type.getInt8Ty(), 'local'}", "  %local = alloca i8"

# store
alloca = llvm.IRBuilder.createAlloca llvm.Type.getInt8Ty(), 'local'
value = llvm.Constant.getIntegerValue llvm.Type.getInt8Ty(), 0
assert.eq "store1", "#{llvm.IRBuilder.createStore value, alloca}", "  store i8 0, i8* %local"

assert.dumpStats()