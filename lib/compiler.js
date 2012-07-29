/* vim: set sw=4 ts=4 et tw=78: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Narcissus JavaScript engine.
 *
 * The Initial Developer of the Original Code is
 * Brendan Eich <brendan@mozilla.org>.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shu-Yu Guo <shu@rfrn.org>
 *   Bruno Jouhier
 *   Gregor Richards
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * Narcissus - JS implemented in JS.
 *
 * Compiler to llvm assembly
 */

const esprima = require('esprima');
const llvm = require('llvm');
const tyinfer = require('coffee/typeinfer');

// special key for parent scope when performing lookups
const PARENT_SCOPE_KEY = ":parent:";

const EJS_CONTEXT_NAME = "__ejs_context";
const EJS_THIS_NAME = "__ejs_this";
const EJS_ENV = "__ejs_env";

const EjsContextType = llvm.Type.getInt32Ty().pointerTo;
const EjsValueType = llvm.Type.getInt32Ty().pointerTo;
const EjsFuncType = llvm.Type.getInt32Ty().pointerTo;

const stringType = llvm.Type.getInt8Ty().pointerTo;
const boolType = llvm.Type.getInt8Ty();

const BUILTIN_ARGS = [
  { name: EJS_CONTEXT_NAME, llvm_type: EjsContextType, type: esprima.Syntax.Identifier },
  { name: EJS_THIS_NAME,    llvm_type: EjsValueType, type: esprima.Syntax.Identifier },
  { name: EJS_ENV,          llvm_type: EjsValueType, type: esprima.Syntax.Identifier }
];

function isBlock(n) {
    return n && (n.type === esprima.Syntax.BlockStatement);
}

function isNonEmptyBlock(n) {
    return isBlock(n) && n.children.length > 0;
}

// marks an llvm function using its calling convention -- if ffi = true, we don't insert the 2 special ejs args into the call
function ffi_c(func) {
  func.ffi = true;
  func.ffi_type = "C";
  return func;
}

function ffi_objc(func) {
  func.ffi = true;
  func.ffi_type = "ObjC";
  return func;
}

function NodeVisitor(module) {
  this.module = module;
  this.topScript = true;

  this.current_scope = {};

  // build up our runtime method table
  this.ejs = {
    invoke_closure: [
      ffi_c(module.getOrInsertExternalFunction("_ejs_invoke_closure_0", EjsValueType, EjsContextType, EjsValueType, llvm.Type.getInt32Ty())),
      ffi_c(module.getOrInsertExternalFunction("_ejs_invoke_closure_1", EjsValueType, EjsContextType, EjsValueType, llvm.Type.getInt32Ty(), EjsValueType)),
      ffi_c(module.getOrInsertExternalFunction("_ejs_invoke_closure_2", EjsValueType, EjsContextType, EjsValueType, llvm.Type.getInt32Ty(), EjsValueType, EjsValueType)),
      ffi_c(module.getOrInsertExternalFunction("_ejs_invoke_closure_3", EjsValueType, EjsContextType, EjsValueType, llvm.Type.getInt32Ty(), EjsValueType, EjsValueType, EjsValueType))
    ],

    number_new:      ffi_c(module.getOrInsertExternalFunction("_ejs_number_new", EjsValueType, llvm.Type.getDoubleTy())),
    string_new_utf8: ffi_c(module.getOrInsertExternalFunction("_ejs_string_new_utf8", EjsValueType, stringType)),
    closure_new:     ffi_c(module.getOrInsertExternalFunction("_ejs_closure_new", EjsFuncType, EjsValueType, EjsValueType)),
    print:           ffi_c(module.getOrInsertExternalFunction("_ejs_print", llvm.Type.getVoidTy(), EjsValueType)),
    "binop+":        ffi_c(module.getOrInsertExternalFunction("_ejs_op_add", boolType, EjsValueType, EjsValueType, EjsValueType.pointerTo)),
    "binop-":        ffi_c(module.getOrInsertExternalFunction("_ejs_op_sub", boolType, EjsValueType, EjsValueType, EjsValueType.pointerTo)),
    "logop||":       ffi_c(module.getOrInsertExternalFunction("_ejs_op_or", boolType, EjsValueType, EjsValueType, EjsValueType.pointerTo)),
    "relop===":      ffi_c(module.getOrInsertExternalFunction("_ejs_op_strict_eq", boolType, EjsValueType, EjsValueType, EjsValueType.pointerTo)),
    truthy:          ffi_c(module.getOrInsertExternalFunction("_ejs_truthy", boolType, EjsValueType, boolType.pointerTo)),
    object_setprop:  ffi_c(module.getOrInsertExternalFunction("_ejs_object_setprop", boolType, EjsValueType, EjsValueType, EjsValueType)),
    object_getprop:  ffi_c(module.getOrInsertExternalFunction("_ejs_object_getprop", boolType, EjsValueType, EjsValueType, EjsValueType.pointerTo))
  };
}

NodeVisitor.prototype.pushScope = function(new_scope) {
  new_scope[PARENT_SCOPE_KEY] = this.current_scope;
  this.current_scope = new_scope;
};

NodeVisitor.prototype.popScope = function() {
  this.current_scope = this.current_scope[PARENT_SCOPE_KEY];
};

NodeVisitor.prototype.createAlloca = function (func, type, name) {
  var saved_insert_point = llvm.IRBuilder.getInsertBlock();
  llvm.IRBuilder.setInsertPoint(func.entry_bb);
  var alloca = llvm.IRBuilder.createAlloca (type, name);
  llvm.IRBuilder.setInsertPoint(saved_insert_point);
  return alloca;
};

NodeVisitor.prototype.createAllocas = function (func, names, scope) {
  var allocas = [];

  // the allocas are always allocated in the function entry_bb so the mem2reg opt pass can regenerate the ssa form for us
  var saved_insert_point = llvm.IRBuilder.getInsertBlock();
  llvm.IRBuilder.setInsertPoint(func.entry_bb);

  var j = 0;
  for (var i = 0, e = names.length; i < e; i++) {
    var name = names[i].id.name;
    if (!scope[name]) {
      allocas[j] = llvm.IRBuilder.createAlloca (EjsValueType, "local_"+name);
      scope[name] = allocas[j];
      j++;
    }
  }

  // reinstate the IRBuilder to its previous insert point so we can insert the actual initializations
  llvm.IRBuilder.setInsertPoint (saved_insert_point);

  return allocas;
};


NodeVisitor.prototype.visitProgram = function(n) {
  console.log ("visitProgram");
  console.log(1);
  var top = this.topScript;
  this.topScript = false;

  var funcs = [];
  var nonfunc = [];

  for (var i = 0, j = n.body.length; i < j; i++) {
    if (n.body[i].type === esprima.Syntax.FunctionDeclaration) {
      funcs.push(n.body[i]);
    }
    else {
      nonfunc.push(n.body[i]);
    }
  }

  console.log(2);
  // generate the IR for all the functions first
  for (var i = 0, j = funcs.length; i < j; i++) {
    this.visit(funcs[i]);
  }

  console.log(3);
  var ir_func = top ? this.module.getOrInsertFunction("_ejs_script", BUILTIN_ARGS.length) : this.currentFunction;
  var ir_args = ir_func.args;
  if (top) {
    // XXX this block needs reworking to mirror what happens in visitFunction (particularly the distinction between entry/body_bb)
    this.currentFunction = ir_func;
    // Create a new basic block to start insertion into.
    var entry_bb = new llvm.BasicBlock ("entry", ir_func);
    llvm.IRBuilder.setInsertPoint(entry_bb);
    ir_func.entry_bb = entry_bb;
    var new_scope = {};
    ir_func.topScope = new_scope;
    this.current_scope = new_scope;

    var allocas = [];
    for (var i = 0, j = BUILTIN_ARGS.length; i < j; i++) {
      ir_args[i].setName(BUILTIN_ARGS[i].name);
      allocas[i] = llvm.IRBuilder.createAlloca (EjsValueType, "local_"+BUILTIN_ARGS[i].name);
      new_scope[BUILTIN_ARGS[i].name] = allocas[i];
      llvm.IRBuilder.createStore (ir_args[i], allocas[i]);
    }
  }

  console.log(4);
  for (var i = 0, j = nonfunc.length; i < j; i++) {
    var ir = this.visit(nonfunc[i]);
  }

  console.log(5);
  if (top) {
    // XXX this should be the actual return value for the script itself
    llvm.IRBuilder.createRet(llvm.Constant.getNull(EjsValueType));
  }

  console.log(6);
  return ir_func;
};

NodeVisitor.prototype.visitBlock = function(n) {
  var new_scope = {};

  this.pushScope(new_scope);

  var rv;
  for (var i = 0, j = n.body.length; i < j; i++) {
    rv = this.visit(n.body[i]);
  }

  this.popScope();

  return rv;
};

NodeVisitor.prototype.visitIf = function(n) {

  // first we convert our conditional EJSValue to a boolean
  var truthy_stackalloc = this.createAlloca(this.currentFunction, boolType, "truthy_result");
  llvm.IRBuilder.createCall(this.ejs.truthy, [this.visit(n.condition), truthy_stackalloc], "cond_truthy");
  var cond_truthy = llvm.IRBuilder.createLoad(truthy_stackalloc, "truthy_load");

  var insertBlock = llvm.IRBuilder.getInsertBlock();
  var insertFunc = insertBlock.getParent();

  var then_bb = new llvm.BasicBlock ("then", insertFunc);
  var else_bb = new llvm.BasicBlock ("else", insertFunc);
  var merge_bb = new llvm.BasicBlock ("merge", insertFunc);

  // we invert the test here - check if the condition is false/0
  var cmp = llvm.IRBuilder.createICmpEq (cond_truthy, llvm.Constant.getIntegerValue(boolType, 0), "cmpresult");
  llvm.IRBuilder.createCondBr(cmp, else_bb, then_bb);

  llvm.IRBuilder.setInsertPoint(then_bb);
  var then_val = this.visit(n.thenPart);
  llvm.IRBuilder.createBr(merge_bb);

  llvm.IRBuilder.setInsertPoint(else_bb);
  var else_val = this.visit(n.elsePart);

  llvm.IRBuilder.setInsertPoint(merge_bb);

  return merge_bb;
  /*
  var pn = llvm.IRBuilder.createPhi(EjsValueType, 2, "iftmp");

  pn.addIncoming (then_val, then_bb);
  pn.addIncoming (else_val, else_bb);

  return pn;
*/
};

NodeVisitor.prototype.visitReturn = function(n) {
  return llvm.IRBuilder.createRet(this.visit(n.argument));
};

NodeVisitor.prototype.visitVariableDeclaration = function(n) {
  if (n.kind == "var") {
    // vars are hoisted to the containing function's toplevel scope
    var scope = this.currentFunction.topScope;

    var allocas = this.createAllocas(this.currentFunction, n.declarations, scope);

    for (var i = 0, j = n.declarations.length; i < j; i++) {
      console.log (JSON.stringify(n.declarations[i]));
      var initializer = this.visit(n.declarations[i].init);

      var init_needs_closure = (n.declarations[i].type === esprima.Syntax.FunctionExpression);

      if (init_needs_closure) {
  	initializer = llvm.IRBuilder.createCall(this.ejs.closure_new, [llvm.IRBuilder.createPointerCast(initializer, EjsFuncType, "castfunc"), llvm.Constant.getNull(EjsValueType)/*XXX*/], "closure");
      }

      llvm.IRBuilder.createStore (initializer, allocas[i]);
    }
  }
  else if (n.kind == "let") {
    // lets are not hoisted to the containing function's toplevel, but instead are bound in the lexical block they inhabit
    var scope = this.current_scope;

    var allocas = this.createAllocas(this.currentFunction, n.declarations.length, scope);
    for (var i = 0, j = n.declarations.length; i < j; i++) {
      llvm.IRBuilder.createStore (this.visit(n.declarations[i].init), allocas[i]);
    }
  }
  else if (n.kind === "const") {
    for (var i = 0, j = n.declarations.length; i < j; i++) {
      var u = n.declarations[i];
      var initializer_ir = this.visit (u.init);
      // XXX bind the initializer to u.name in the current basic block and mark it as constant
    }
  }
};

NodeVisitor.prototype.createPropertyStore = function(obj,propname,rhs) {
  // we assume propname is a string/identifier here...
  var pname = this.visitString(propname);
  return llvm.IRBuilder.createCall(this.ejs.object_setprop, [this.visit(obj), pname, rhs], "propstore_"+propname.value);
};

NodeVisitor.prototype.createPropertyLoad = function(obj,propname) {
  // we assume propname is a string/identifier here...
  var pname = this.visitString(propname);
  var result = this.createAlloca(EjsValueType, "result");
  var rv = llvm.IRBuilder.createCall(this.ejs.object_getprop, [this.visit(obj), pname, result], "propload_"+propname.value);
  return llvm.IRBuilder.createLoad(result, "result_propload");
};

NodeVisitor.prototype.visitAssignmentExpression = function(n) {
  var lhs = n.left;
  var rhs = n.right;

  if (lhs.type === esprima.Syntax.Identifier) {
    return llvm.IRBuilder.createStore (this.visit(rhs), findIdentifierInScope(lhs.value, this.current_scope));
  }
  else if (lhs.type === esprima.Syntax.MemberExpression) {
    console.log ("creating propertystore!");

    var rhs_needs_closure = (rhs.type == esprima.Syntax.FunctionExpression);

    rhs = this.visit(rhs);

    if (rhs_needs_closure) {
      console.log ("creating closure");
      rhs = llvm.IRBuilder.createCall(this.ejs.closure_new, [llvm.IRBuilder.createPointerCast(rhs, EjsFuncType, "castfunc"), llvm.Constant.getNull(EjsValueType)/*XXX*/], "closure");
      console.log ("done creating closure");
    }

    return this.createPropertyStore (lhs.object, lhs.property, rhs);
  }
  else {
    throw "unhandled assign lhs";
  }
};

NodeVisitor.prototype.visitFunction = function(n) {
  console.log ("visitFunction, -1");
  // save off the insert point so we can get back to it after generating this function
  var insertBlock = llvm.IRBuilder.getInsertBlock();

  console.log ("visitFunction, -0.5");
  for (var i = 0, j = n.params.length; i < j; i++) {
    if (n.params[i].type !== esprima.Syntax.Identifier) {
      console.log ("we don't handle destructured/defaulted parameters yet");
      throw "we don't handle destructured/defaulted parameters yet";
    }
  }
console.log ("visitFunction, -0.25");

  // XXX this methods needs to be augmented so that we can pass actual types (or the builtin args need
  // to be reflected in jsllvm.cpp too).  maybe we can pass the names to this method and it can do it all
  // there?

  console.log ("visitFunction, 1");
  for (var i = BUILTIN_ARGS.length - 1, j = 0; i >= j; i--) {
    n.params.unshift(BUILTIN_ARGS[i]);
  }

  if (!n.name || n.name == "")
    n.name = "_ejs_anonymous";
  var ir_func = this.module.getOrInsertFunction (n.name, n.params.length);
  this.currentFunction = ir_func;

  var ir_args = ir_func.args;
  for (var i = 0, j = n.params.length; i < j; i++) {
    if (n.params[i].type === esprima.Syntax.Identifier) {
      ir_args[i].setName(n.params[i].name);
    }
    else {
      ir_args[i].setName("__ejs_destructured_param");
    }
  }


  console.log ("visitFunction, 4");
  // Create a new basic block to start insertion into.
  var entry_bb = new llvm.BasicBlock ("entry", ir_func);
  llvm.IRBuilder.setInsertPoint(entry_bb);

  var new_scope = {};

  // we save off the top scope and entry_bb of the function so that we can hoist vars there
  ir_func.topScope = new_scope;
  ir_func.entry_bb = entry_bb;


  var allocas = [];
  // store the arguments on the stack
  for (var i = 0, j = n.params.length; i < j; i++) {
    if (n.params[i].type === esprima.Syntax.Identifier) {
      allocas[i] = llvm.IRBuilder.createAlloca (EjsValueType, "local_"+n.params[i].name);
    }
    else {
      console.log ("we don't handle destructured args at the moment.");
      throw "we don't handle destructured args at the moment.";
    }
  }
  for (var i = 0, j = n.params.length; i < j; i++) {
    // store the allocas in the scope we're going to push onto the scope stack
    new_scope[n.params[i].name] = allocas[i];

    llvm.IRBuilder.createStore (ir_args[i], allocas[i]);
  }

  var body_bb = new llvm.BasicBlock ("body", ir_func);
  llvm.IRBuilder.setInsertPoint(body_bb);

  this.pushScope(new_scope);

  this.visit(n.body);

  this.popScope();

  // XXX more needed here - this lacks all sorts of control flow stuff.
  // Finish off the function.
  llvm.IRBuilder.createRet(llvm.Constant.getNull(EjsValueType));

  // insert an unconditional branch from entry_bb to body here, now that we're
  // sure we're not going to be inserting allocas into the entry_bb anymore.
  llvm.IRBuilder.setInsertPoint(entry_bb);
  llvm.IRBuilder.createBr(body_bb);

  this.currentFunction = null;

  console.log ("returning function");

  llvm.IRBuilder.setInsertPoint(insertBlock);

  return ir_func;
};

NodeVisitor.prototype.visitSemicolon = function(n) {
  return this.visit(n.expression);
};

NodeVisitor.prototype.visitBinaryOperation = function(n) {
  var builtin = "binop" + n.operator;
  var callee = this.ejs[builtin];
  // allocate space on the stack for the result
  var result = this.createAlloca(this.currentFunction, EjsValueType, "result_" + builtin);
  // call the add method
  var rv = llvm.IRBuilder.createCall(callee, [this.visit(n.left), this.visit(n.right), result], "result");
  // load and return the result
  return llvm.IRBuilder.createLoad(result, "result_"+builtin+"_load");
};

NodeVisitor.prototype.createLoadThis = function () {
  var _this = this.current_scope[EJS_THIS_NAME];
  return llvm.IRBuilder.createLoad (this.current_scope[EJS_THIS_NAME], "load_this");
};

NodeVisitor.prototype.createLoadContext = function () {
  if (!this.current_scope[EJS_CONTEXT_NAME])
    console.log ("die die die");
  return llvm.IRBuilder.createLoad (this.current_scope[EJS_CONTEXT_NAME], "load_context");
};

NodeVisitor.prototype.visitCallExpression = function(n) {
  console.log ("visitCall " + JSON.stringify(n));

  var callee_is_closure = callee.type == esprima.Syntax.FunctionExpression;

  var callee = this.visit (n.callee);
  var args = n.arguments;

  if (callee.type && callee.type === esprima.Syntax.MemberExpression) {
    console.log ("creating property load!");
    callee = this.createPropertyLoad (callee.object, callee.property);
  }

  // At this point we assume callee is a function object

  var compiled_arg_start;
  if (callee_is_closure) {
    console.log ("loading closure args");
    compiled_arg_start = 3; // closure invokers take the EJSContext and an arg length
    args.unshift(llvm.Constant.getIntegerValue(llvm.Type.getInt32Ty(), args.length));
    args.unshift(callee);
    args.unshift(this.createLoadContext());
  }
  else if (!callee.ffi) {
    console.log ("loading this and context");
    // we insert the extra BUILTIN_ARGS since we're calling a JS function
    args.unshift(this.createLoadThis());
    args.unshift(this.createLoadContext());

    compiled_arg_start = 2;
    console.log ("done loading this and context");
  }
  else {
    console.log ("ffi function, not loading this and context");
    compiled_arg_start = 0;
  }

  console.log ("callee = " + callee.toString());

  var argv = [];
  for (var i = 0, j = args.length; i < j; i++) {
    console.log ("i = " + i);
    if (i < compiled_arg_start) {
      argv[i] = args[i];
    }
    else {
      console.log ("visiting");
      argv[i] = this.visit(args[i]);
    }
  }

  if (callee_is_closure) {
    console.log (1);
    var invoke_fun = this.ejs.invoke_closure[argv.length-compiled_arg_start];
    console.log ("2 invoke_fun = this.ejs.invoke_closure[" + (argv.length-compiled_arg_start) + "]");
    var rv = llvm.IRBuilder.createCall(invoke_fun, argv, "calltmp");
    console.log (3);
    return rv;
  }
  else {
    // we're dealing with a function here
    if (callee.argSize !== args.length) {
      // this isn't invalid in JS.  if argSize > args.length, the args are undefined.
      // if argSize < args.length, the args are still passed
    }
    return llvm.IRBuilder.createCall(callee, argv, callee.returnType.isVoid() ? "" : "calltmp");
  }
};

NodeVisitor.prototype.visitNewExpression = function(n) {
  var ctor = this.visit (n.callee);
  var args = n.arguments;

  // At this point we assume callee is a function object
  if (callee.argSize !== args.length) {
    // this isn't invalid in JS.  if argSize > args.length, the args are undefined.
    // if argSize < args.length, the args are still passed
  }

  var argv = [];
  for (var i = 0, j = args.length; i < j; i++) {
    argv[i] = this.visit(args[i]);
  }

  var rv = llvm.IRBuilder.createCall(callee, argv, "newtmp");
  return rv;
};

NodeVisitor.prototype.visitPropertyAccess = function(n) {
  console.log ("property access: " + nc[1].value); // NC-USAGE
  return n;
};

NodeVisitor.prototype.visitThisExpression = function(n) {
  console.log ("visitThisExpression");
  return this.createLoadThis();
};

function findIdentifierInScope (ident, scope) {
  while (scope) {
    if (scope[ident])
      return scope[ident];
    scope = scope[PARENT_SCOPE_KEY];
  }
  return null;
}

NodeVisitor.prototype.visitIdentifier = function(n) {
  var val = n.name;
  var alloca = findIdentifierInScope (val, this.current_scope);
  if (alloca) {
    return llvm.IRBuilder.createLoad(alloca, "load_"+val);
  }

  var func = null;

  // we should probably insert a global scope at the toplevel (above the actual user-level global scope) that includes all these renamings/functions?
  if (val == "print") {
    func = this.ejs.print;
  }
  else {
    func = this.module.getFunction(val);
  }

  if (func)
    return func;

  throw "Symbol '" + val + "' not found in current scope";
};


NodeVisitor.prototype.visitLiteral = function(n) {
  console.log(JSON.stringify(n));
  if (typeof (n.value) === "string") {
    console.log ("visitString");
    var c = llvm.IRBuilder.createGlobalStringPtr(n.value, "strconst");
    var call = llvm.IRBuilder.createCall(this.ejs.string_new_utf8, [c], "strtmp");
    return call;
  }
  else if (typeof (n.value) === "number") {
    var c = llvm.ConstantFP.getDouble(n.value);
    var call = llvm.IRBuilder.createCall(this.ejs.number_new, [c], "numtmp");
    return call;
  }
  throw "whuuu";
};

NodeVisitor.prototype.visit = function(n) {
  if (!n)
    return n;

  console.log (n.type);

  const syntax = esprima.Syntax;

  switch (n.type) {
  case syntax.Program: return this.visitProgram(n);
  case syntax.FunctionDeclaration: return this.visitFunction(n); // XXX esprima distinguishes between these two.  hallelujah
  case syntax.FunctionExpression: return this.visitFunction(n);  // XXX
  // XXX case GETTER: return this.visitGetter(n);
  // XXX case SETTER: return this.visitSetter(n);
  // XXX case SCRIPT: return this.visitScript(n);
  case syntax.BlockStatement: return this.visitBlock(n);
  // XXX case LET_BLOCK: return this.visitBlock(n);
  case syntax.SwitchStatement: return this.visitSwitch(n);
  case syntax.SwitchCase: return this.visitCase(n);
  case syntax.ForStatement: return this.visitFor(n);
  case syntax.WhileStatement: return this.visitWhile(n);
  case syntax.IfStatement: return this.visitIf(n);
  case syntax.ForInStatement: return this.visitForIn(n);
  case syntax.DoWhileStatement: return this.visitDo(n);
  case syntax.BreakStatement: return this.visitBreak(n);
  case syntax.ContinueStatement: return this.visitContinue(n);
  case syntax.TryStatement: return this.visitTry(n);
  case syntax.ThrowStatement: return this.visitThrow(n);
  case syntax.ReturnStatement: return this.visitReturn(n);
  case syntax.YieldStatement: return this.visitYield(n);
  case syntax.WithStatement: return this.visitWith(n);
  case syntax.VariableDeclaration: return this.visitVariableDeclaration(n);
  case syntax.LabeledStatement: return this.visitLabeledState(n);
  case syntax.AssignmentExpression: return this.visitAssignmentExpression(n);
  case syntax.LogicalExpression: return this.visitLogicalExpression(n);
  case syntax.NewExpression: return this.visitNewExpression(n);
  case syntax.ThisExpression: return this.visitThisExpression(n);
  case syntax.BinaryExpression: return this.visitBinaryOperation(n);
  case syntax.RelationalExpression: return this.visitRelationalExpression(n);
  case syntax.Identifier: return this.visitIdentifier(n);
  case syntax.Literal: return this.visitLiteral(n);
  case syntax.CallExpression: return this.visitCallExpression(n);
  default:
    throw "PANIC: unknown operation " + n.type;
    /*
  case DELETE:
  case VOID:
  case TYPEOF:
  case NOT:
  case BITWISE_NOT:
  case UNARY_PLUS:
  case UNARY_MINUS:
  case INCREMENT:
  case DECREMENT:
  case DOT: return this.visitPropertyAccess(n);
  case INDEX: break;
  case NEW_WITH_ARGS:
  case ARRAY_INIT:
  case ARRAY_COMP:
  case COMP_TAIL:
  case OBJECT_INIT:
  case NULL:
  case TRUE:
  case FALSE:
  case REGEXP:
  case GROUP:
*/
  }
};

function compile(n) {
/*
  var infer = new tyinfer.TypeInfer(n);

  infer.run ();
*/
  console.log (5);
  var module = new llvm.Module("compiledfoo");
  console.log (6);
  console.log (module);
  console.log (6.5);
  var visitor = new NodeVisitor(module);
  console.log (7);
  var ir = visitor.visit(n);

  console.log (8);
  module.dump();

  //quit(0);
}

exports.compile = compile;
