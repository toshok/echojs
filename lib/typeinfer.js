(function() {
  var AnyType, FunctionType, InferVisitor, MultiType, NumberType, StringType, Type, TypeInfer, UndefinedType, VoidType, definitions, lexer, parser, tokens,
    __hasProp = Object.prototype.hasOwnProperty,
    __extends = function(child, parent) { for (var key in parent) { if (__hasProp.call(parent, key)) child[key] = parent[key]; } function ctor() { this.constructor = child; } ctor.prototype = parent.prototype; child.prototype = new ctor; child.__super__ = parent.prototype; return child; },
    __indexOf = Array.prototype.indexOf || function(item) { for (var i = 0, l = this.length; i < l; i++) { if (__hasProp.call(this, i) && this[i] === item) return i; } return -1; };

  lexer = require('./lexer');

  parser = require('./parser');

  definitions = require('./definitions');

  tokens = definitions.tokens;

  Type = (function() {

    function Type() {}

    Type.prototype.isCompatible = function(ty) {
      return false;
    };

    return Type;

  })();

  FunctionType = (function() {

    __extends(FunctionType, Type);

    function FunctionType(return_type, parameter_types) {
      this.return_type = return_type;
      this.parameter_types = parameter_types;
    }

    FunctionType.prototype.isCompatible = function(ty) {
      if (!ty instanceof FunctionType) {
        return false;
      } else if (!this.return_type.isCompatible(ty.return_type)) {
        return false;
      } else if (!parametersCompatible(ty)) {
        return false;
      } else {
        return true;
      }
    };

    FunctionType.prototype.parametersCompatible = function(param_tys) {
      var i;
      if (param_tys.length !== parameter_types.length) return false;
      if ((function() {
        var _ref, _results;
        _results = [];
        for (i = 0, _ref = param_tys.length; 0 <= _ref ? i <= _ref : i >= _ref; 0 <= _ref ? i++ : i--) {
          _results.push(!this.parameter_types[i].isCompatible(param_tys.length[i]));
        }
        return _results;
      }).call(this)) {
        return false;
      }
    };

    return FunctionType;

  })();

  UndefinedType = (function() {

    __extends(UndefinedType, Type);

    function UndefinedType() {
      UndefinedType.__super__.constructor.apply(this, arguments);
    }

    UndefinedType.prototype.isCompatible = function(ty) {
      return false;
    };

    UndefinedType.is = function(ty) {
      return ty(instance in UndefinedType);
    };

    return UndefinedType;

  })();

  VoidType = (function() {

    __extends(VoidType, Type);

    function VoidType() {
      VoidType.__super__.constructor.apply(this, arguments);
    }

    VoidType.prototype.isCompatible = function(ty) {
      return ty instanceof VoidType;
    };

    VoidType.is = function(ty) {
      return ty instanceof VoidType;
    };

    return VoidType;

  })();

  AnyType = (function() {

    __extends(AnyType, Type);

    function AnyType() {
      AnyType.__super__.constructor.apply(this, arguments);
    }

    AnyType.prototype.isCompatible = function(ty) {
      return true;
    };

    AnyType.is = function(ty) {
      return ty instanceof AnyType;
    };

    return AnyType;

  })();

  NumberType = (function() {

    __extends(NumberType, Type);

    function NumberType() {
      NumberType.__super__.constructor.apply(this, arguments);
    }

    NumberType.prototype.isCompatible = function(ty) {
      return ty instanceof NumberType;
    };

    NumberType.is = function(ty) {
      return ty instanceof NumberType;
    };

    return NumberType;

  })();

  StringType = (function() {

    __extends(StringType, Type);

    function StringType() {
      StringType.__super__.constructor.apply(this, arguments);
    }

    StringType.prototype.isCompatible = function(ty) {
      return ty instanceof StringType;
    };

    StringType.is = function(ty) {
      return ty instanceof StringType;
    };

    return StringType;

  })();

  MultiType = (function() {

    __extends(MultiType, Type);

    function MultiType() {
      this.types = [];
    }

    MultiType.prototype.isCompatible = function(ty) {
      var mty, _i, _len, _ref;
      _ref = this.types;
      for (_i = 0, _len = _ref.length; _i < _len; _i++) {
        mty = _ref[_i];
        if (mty(isCompatible(ty))) true;
      }
      return false;
    };

    MultiType.prototype.addType = function(ty) {
      return this.types.unshift(ty);
    };

    return MultiType;

  })();

  InferVisitor = (function() {

    function InferVisitor() {
      this.changed = false;
    }

    InferVisitor.prototype.infer = function(ast, env) {
      this.changed = false;
      return this.inferAst(ast);
    };

    InferVisitor.prototype.isChanged = function() {
      return this.changed;
    };

    InferVisitor.prototype.inferAst = function(ast, env) {
      var arg, argtypes, funtype, node, _i, _j, _len, _len2, _ref, _ref2, _ref3;
      if (ast instanceof parser.prototype.Script) {
        _ref = ast.children;
        for (_i = 0, _len = _ref.length; _i < _len; _i++) {
          node = _ref[_i];
          this.inferAst(node);
        }
      } else if (ast instanceof parser.Number) {
        if (!NumberType.is(ast.type)) assignType(ast, new NumberType);
      } else if (ast instanceof parser.Identifier) {
        if (!ast.type) {
          if (_ref2 = ast.name, __indexOf.call(env, _ref2) >= 0) {
            assignType(ast, env[ast.name]);
          }
        }
      } else if (ast instanceof parser.Decl) {
        if (ast.initializer) env[ast.name] = this.inferAst(ast.initializer, env);
      } else if (ast instanceof parser.Call) {
        funtype = this.infertAst(ast.callee, env);
        _ref3 = ast.arguments;
        for (_j = 0, _len2 = _ref3.length; _j < _len2; _j++) {
          arg = _ref3[_j];
          argtypes = this.inferAst(arg, env);
        }
      }
      return ast.type;
    };

    InferVisitor.prototype.assignType = function(ast, ty) {
      var current_type;
      if (!ast.type) {
        ast.type = ty;
      } else if (ast.type instanceof MultiType) {
        ast.type.addType(ty);
      } else {
        current_type = ast.type;
        ast.type = new MultiType;
        ast.type.addType(current_type);
        ast.type.addType(ty);
      }
      return this.changed = true;
    };

    return InferVisitor;

  })();

  TypeInfer = (function() {

    function TypeInfer(ast) {
      this.ast = ast;
      this.defaultGlobalEnvironment = [
        {
          "print": new FunctionType(new VoidType(), [new AnyType()])
        }
      ];
    }

    TypeInfer.prototype.run = function() {
      var visitor, _results;
      visitor = new InferVisitor;
      visitor.infer(this.ast, this.defaultGlobalEnvironment);
      _results = [];
      while (visitor.isChanged()) {
        _results.push(visitor.infer(this.ast, this.defaultGlobalEnvironment));
      }
      return _results;
    };

    return TypeInfer;

  })();

  exports.TypeInfer = TypeInfer;

}).call(this);
