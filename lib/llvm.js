
// test llvm binding

exports.Function = {
  create: function (num_args, name, module) {
  }
};

exports.ConstantFP = {
  getDouble: function (context, val) {
  }
};

exports.BasicBlock = {
  create: function (context, label, func) {
  }
};

exports.Builder = {
  setInsertPoint: function (basicBlock) {
  },

  createRet: function (retVal) {
  },

  createFAdd: function (left, right, tmpname) {
  }
};