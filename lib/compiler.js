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

const lexer = require('./lexer');
const parser = require('./parser');
const definitions = require('./definitions');
const tyinfer = require('./typeinfer');
const tokens = definitions.tokens;

// special key for parent scope when performing lookups
const PARENT_SCOPE_KEY = ":parent:";

const EJS_CONTEXT_NAME = "__ejs_context";
const EJS_THIS_NAME = "__ejs_this";

const BUILTIN_ARGS = [
  { name: EJS_CONTEXT_NAME, type: llvm.LLVMType.EjsValueType /* should be EjsContextType */ },
  { name: EJS_THIS_NAME,    type: llvm.LLVMType.EjsValueType }
];

// Set constants in the local scope.
eval(definitions.consts);

function isBlock(n) {
    return n && (n.type === BLOCK);
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
      ffi_c(module.getOrInsertExternalFunction("_ejs_invoke_closure_0", llvm.LLVMType.EjsValueType, llvm.LLVMType.EjsValueType, llvm.LLVMType.EjsValueType, llvm.LLVMType.int32Type)),
      ffi_c(module.getOrInsertExternalFunction("_ejs_invoke_closure_1", llvm.LLVMType.EjsValueType, llvm.LLVMType.EjsValueType, llvm.LLVMType.EjsValueType, llvm.LLVMType.int32Type, llvm.LLVMType.EjsValueType)),
      ffi_c(module.getOrInsertExternalFunction("_ejs_invoke_closure_2", llvm.LLVMType.EjsValueType, llvm.LLVMType.EjsValueType, llvm.LLVMType.EjsValueType, llvm.LLVMType.int32Type, llvm.LLVMType.EjsValueType, llvm.LLVMType.EjsValueType)),
      ffi_c(module.getOrInsertExternalFunction("_ejs_invoke_closure_3", llvm.LLVMType.EjsValueType, llvm.LLVMType.EjsValueType, llvm.LLVMType.EjsValueType, llvm.LLVMType.int32Type, llvm.LLVMType.EjsValueType, llvm.LLVMType.EjsValueType, llvm.LLVMType.EjsValueType))
    ],

    number_new:      ffi_c(module.getOrInsertExternalFunction("_ejs_number_new", llvm.LLVMType.EjsValueType, llvm.LLVMType.doubleType)),
    string_new_utf8: ffi_c(module.getOrInsertExternalFunction("_ejs_string_new_utf8", llvm.LLVMType.EjsValueType, llvm.LLVMType.stringType)),
    closure_new:     ffi_c(module.getOrInsertExternalFunction("_ejs_closure_new", llvm.LLVMType.EjsFuncType, llvm.LLVMType.EjsValueType, llvm.LLVMType.EjsValueType)),
    print:           ffi_c(module.getOrInsertExternalFunction("_ejs_print", llvm.LLVMType.voidType, llvm.LLVMType.EjsValueType)),
    op_add:          ffi_c(module.getOrInsertExternalFunction("_ejs_op_add", llvm.LLVMType.boolType, llvm.LLVMType.EjsValueType, llvm.LLVMType.EjsValueType, llvm.LLVMType.EjsValueType.pointerTo())),
    op_sub:          ffi_c(module.getOrInsertExternalFunction("_ejs_op_sub", llvm.LLVMType.boolType, llvm.LLVMType.EjsValueType, llvm.LLVMType.EjsValueType, llvm.LLVMType.EjsValueType.pointerTo())),
    op_or:           ffi_c(module.getOrInsertExternalFunction("_ejs_op_or", llvm.LLVMType.boolType, llvm.LLVMType.EjsValueType, llvm.LLVMType.EjsValueType, llvm.LLVMType.EjsValueType.pointerTo())),
    op_strict_eq:    ffi_c(module.getOrInsertExternalFunction("_ejs_op_strict_eq", llvm.LLVMType.boolType, llvm.LLVMType.EjsValueType, llvm.LLVMType.EjsValueType, llvm.LLVMType.EjsValueType.pointerTo())),
    truthy:          ffi_c(module.getOrInsertExternalFunction("_ejs_truthy", llvm.LLVMType.boolType, llvm.LLVMType.EjsValueType, llvm.LLVMType.boolType.pointerTo())),
    object_setprop:  ffi_c(module.getOrInsertExternalFunction("_ejs_object_setprop", llvm.LLVMType.boolType, llvm.LLVMType.EjsValueType, llvm.LLVMType.EjsValueType, llvm.LLVMType.EjsValueType)),
    object_getprop:  ffi_c(module.getOrInsertExternalFunction("_ejs_object_getprop", llvm.LLVMType.boolType, llvm.LLVMType.EjsValueType, llvm.LLVMType.EjsValueType, llvm.LLVMType.EjsValueType.pointerTo()))
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
  let saved_insert_point = llvm.IRBuilder.getInsertBlock();
  llvm.IRBuilder.setInsertPoint(func.entry_bb);
  let alloca = llvm.IRBuilder.createAlloca (type, name);
  llvm.IRBuilder.setInsertPoint(saved_insert_point);
  return alloca;
};

NodeVisitor.prototype.createAllocas = function (func, names, scope) {
  let allocas = [];

  // the allocas are always allocated in the function entry_bb so the mem2reg opt pass can regenerate the ssa form for us
  let saved_insert_point = llvm.IRBuilder.getInsertBlock();
  llvm.IRBuilder.setInsertPoint(func.entry_bb);

  for (let i = 0, j = names.length; i < j; i++) {
    let name = names[i].name;
    if (!scope[name]) {
      allocas[i] = llvm.IRBuilder.createAlloca (llvm.LLVMType.EjsValueType, "local_"+name);
      scope[name] = allocas[i];
    }
  }

  // reinstate the IRBuilder to its previous insert point so we can insert the actual initializations
  llvm.IRBuilder.setInsertPoint (saved_insert_point);

  return allocas;
};


NodeVisitor.prototype.visitScript = function(n,nc) {
  let top = this.topScript;
  this.topScript = false;

  let funcs = [];
  let nonfunc = [];

  for (let i = 0, j = nc.length; i < j; i++) {
    if (nc[i].type == FUNCTION) {
      funcs.push(nc[i]);
    }
    else {
      nonfunc.push(nc[i]);
    }
  }

  // generate the IR for all the functions first
  for (let i = 0, j = funcs.length; i < j; i++) {
    this.visit(funcs[i]);
  }

  let ir_func = top ? this.module.getOrInsertFunction("_ejs_script", BUILTIN_ARGS.length) : this.currentFunction;
  let ir_args = ir_func.getArgs();
  if (top) {
    // XXX this block needs reworking to mirror what happens in visitFunction (particularly the distinction between entry/body_bb)
    this.currentFunction = ir_func;
    // Create a new basic block to start insertion into.
    var entry_bb = llvm.BasicBlock.create ("entry", ir_func);
    llvm.IRBuilder.setInsertPoint(entry_bb);
    ir_func.entry_bb = entry_bb;
    var new_scope = {};
    ir_func.topScope = new_scope;
    this.current_scope = new_scope;

    let allocas = [];
    for (let i = 0, j = BUILTIN_ARGS.length; i < j; i++) {
      ir_args[i].setName(BUILTIN_ARGS[i].name);
      allocas[i] = llvm.IRBuilder.createAlloca (llvm.LLVMType.EjsValueType, "local_"+BUILTIN_ARGS[i].name);
      new_scope[BUILTIN_ARGS[i].name] = allocas[i];
      llvm.IRBuilder.createStore (ir_args[i], allocas[i]);
    }
  }

  for (let i = 0, j = nonfunc.length; i < j; i++) {
    var ir = this.visit(nonfunc[i]);
  }

  if (top) {
    // XXX this should be the actual return value for the script itself
    llvm.IRBuilder.createRet(llvm.Constant.getNull(llvm.LLVMType.EjsValueType));
  }

  return ir_func;
};

NodeVisitor.prototype.visitBlock = function(n,nc) {
  var new_scope = {};

  this.pushScope(new_scope);

  let rv;
  for (let i = 0, j = nc.length; i < j; i++) {
    rv = this.visit(nc[i]);
  }

  this.popScope();

  return rv;
};

NodeVisitor.prototype.visitIf = function(n,nc) {

  // first we convert our conditional EJSValue to a boolean
  let truthy_stackalloc = this.createAlloca(this.currentFunction, llvm.LLVMType.boolType, "truthy_result");
  llvm.IRBuilder.createCall(this.ejs.truthy, [this.visit(n.condition), truthy_stackalloc], "cond_truthy");

  let cond_truthy = llvm.IRBuilder.createLoad(truthy_stackalloc, "truthy_load");

  let insertBlock = llvm.IRBuilder.getInsertBlock();
  let insertFunc = insertBlock.getParent();

  var then_bb = llvm.BasicBlock.create ("then", insertFunc);
  var else_bb = llvm.BasicBlock.create ("else", insertFunc);
  var merge_bb = llvm.BasicBlock.create ("merge", insertFunc);

  // we invert the test here - check if the condition is false/0
  let cmp = llvm.IRBuilder.createICmpEq (cond_truthy, llvm.Constant.getIntegerValue(llvm.LLVMType.boolType, 0), "cmpresult");
  llvm.IRBuilder.createCondBr(cmp, else_bb, then_bb);

  llvm.IRBuilder.setInsertPoint(then_bb);
  let then_val = this.visit(n.thenPart);
  llvm.IRBuilder.createBr(merge_bb);

  llvm.IRBuilder.setInsertPoint(else_bb);
  let else_val = this.visit(n.elsePart);

  llvm.IRBuilder.setInsertPoint(merge_bb);

  return merge_bb;
  /*
  let pn = llvm.IRBuilder.createPhi(llvm.LLVMType.EjsValueType, 2, "iftmp");

  pn.addIncoming (then_val, then_bb);
  pn.addIncoming (else_val, else_bb);

  return pn;
*/
};

NodeVisitor.prototype.visitReturn = function(n,nc) {
  return llvm.IRBuilder.createRet(this.visit(n.value));
};

NodeVisitor.prototype.visitVar = function(n,nc) {
  // vars are hoisted to the containing function's toplevel scope
  let scope = this.currentFunction.topScope;

  let allocas = this.createAllocas(this.currentFunction, nc, scope);

  for (let i = 0, j = nc.length; i < j; i++) {
    let initializer = this.visit(nc[i].initializer);

    let init_needs_closure = (initializer.toString() === "[object LLVMFunction]");

    if (init_needs_closure) {
      initializer = llvm.IRBuilder.createCall(this.ejs.closure_new, [llvm.IRBuilder.createPointerCast(initializer, llvm.LLVMType.EjsFuncType, "castfunc"), llvm.Constant.getNull(llvm.LLVMType.EjsValueType)/*XXX*/], "closure");
    }

    llvm.IRBuilder.createStore (initializer, allocas[i]);
  }
};

NodeVisitor.prototype.visitLet = function(n,nc) {
  // lets are not hoisted to the containing function's toplevel, but instead are bound in the lexical block they inhabit
  let scope = this.current_scope;

  let allocas = this.createAllocas(this.currentFunction, nc, scope);

  for (let i = 0, j = nc.length; i < j; i++) {
    llvm.IRBuilder.createStore (this.visit(nc[i].initializer), allocas[i]);
  }
};

NodeVisitor.prototype.visitConst = function(n,nc) {
  for (let i = 0, j = nc.length; i < j; i++) {
    var u = nc[i];
    var initializer_ir = this.visit (u.initializer);
    // XXX bind the initializer to u.name in the current basic block and mark it as constant
  }
};

NodeVisitor.prototype.createPropertyStore = function(obj,propname,rhs) {
  // we assume propname is a string/identifier here...
  let pname = this.visitString(propname);
  return llvm.IRBuilder.createCall(this.ejs.object_setprop, [this.visit(obj), pname, rhs], "propstore_"+propname.value);
};

NodeVisitor.prototype.createPropertyLoad = function(obj,propname) {
  // we assume propname is a string/identifier here...
  let pname = this.visitString(propname);
  let result = this.createAlloca(llvm.LLVMType.EjsValueType, "result");
  let rv = llvm.IRBuilder.createCall(this.ejs.object_getprop, [this.visit(obj), pname, result], "propload_"+propname.value);
  return llvm.IRBuilder.createLoad(result, "result_propload");
};

NodeVisitor.prototype.visitAssign = function(n,nc) {
  var lhs = nc[0];
  var rhs = nc[1];

  if (lhs.type == IDENTIFIER) {
    return llvm.IRBuilder.createStore (this.visit(rhs), findIdentifierInScope(lhs.value, this.current_scope));
  }
  else if (lhs.type == DOT) {
    print ("creating propertystore!");

    rhs = this.visit(rhs);

    let rhs_needs_closure = (rhs.toString() === "[object LLVMFunction]");

    if (rhs_needs_closure) {
      print ("creating closure");
      rhs = llvm.IRBuilder.createCall(this.ejs.closure_new, [llvm.IRBuilder.createPointerCast(rhs, llvm.LLVMType.EjsFuncType, "castfunc"), llvm.Constant.getNull(llvm.LLVMType.EjsValueType)/*XXX*/], "closure");
      print ("done creating closure");
    }

    return this.createPropertyStore (lhs.children[0], lhs.children[1], rhs);
  }
  else {
    throw "unhandled assign lhs";
  }
};

NodeVisitor.prototype.visitFunction = function(n,nc) {
  // save off the insert point so we can get back to it after generating this function
  let insertBlock = llvm.IRBuilder.getInsertBlock();

  for (let i = 0, j = n.params.length; i < j; i++) {
    if (typeof (n.params[i]) !== 'string') {
      print ("we don't handle destructured/defaulted parameters yet");
      throw "we don't handle destructured/defaulted parameters yet";
    }
  }

  // XXX this methods needs to be augmented so that we can pass actual types (or the builtin args need
  // to be reflected in jsllvm.cpp too).  maybe we can pass the names to this method and it can do it all
  // there?

  print ("visitFunction, 1");
  for (let i = BUILTIN_ARGS.length - 1, j = 0; i >= j; i--) {
    n.params.unshift(BUILTIN_ARGS[i].name);
  }

  if (!n.name || n.name == "")
    n.name = "_ejs_anonymous";
  var ir_func = this.module.getOrInsertFunction (n.name, n.params.length);
  this.currentFunction = ir_func;

  var ir_args = ir_func.getArgs();
  for (let i = 0, j = n.params.length; i < j; i++) {
    if (typeof (n.params[i]) === 'string') {
      ir_args[i].setName(n.params[i]);
    }
    else {
      ir_args[i].setName("__ejs_destructured_param");
    }
  }


  print ("visitFunction, 4");
  // Create a new basic block to start insertion into.
  var entry_bb = llvm.BasicBlock.create ("entry", ir_func);
  llvm.IRBuilder.setInsertPoint(entry_bb);

  var new_scope = {};

  // we save off the top scope and entry_bb of the function so that we can hoist vars there
  ir_func.topScope = new_scope;
  ir_func.entry_bb = entry_bb;


  let allocas = [];
  // store the arguments on the stack
  for (let i = 0, j = n.params.length; i < j; i++) {
    if (typeof (n.params[i]) === 'string') {
      allocas[i] = llvm.IRBuilder.createAlloca (llvm.LLVMType.EjsValueType, "local_"+n.params[i]);
    }
    else {
      print ("we don't handle destructured args at the moment.");
      throw "we don't handle destructured args at the moment.";
    }
  }
  for (let i = 0, j = n.params.length; i < j; i++) {
    // store the allocas in the scope we're going to push onto the scope stack
    new_scope[n.params[i]] = allocas[i];

    llvm.IRBuilder.createStore (ir_args[i], allocas[i]);
  }

  var body_bb = llvm.BasicBlock.create ("body", ir_func);
  llvm.IRBuilder.setInsertPoint(body_bb);

  this.pushScope(new_scope);

  this.visit(n.body);

  this.popScope();

  // XXX more needed here - this lacks all sorts of control flow stuff.
  // Finish off the function.
  llvm.IRBuilder.createRet(llvm.Constant.getNull(llvm.LLVMType.EjsValueType));

  // insert an unconditional branch from entry_bb to body here, now that we're
  // sure we're not going to be inserting allocas into the entry_bb anymore.
  llvm.IRBuilder.setInsertPoint(entry_bb);
  llvm.IRBuilder.createBr(body_bb);

  this.currentFunction = null;

  print ("returning function");

  llvm.IRBuilder.setInsertPoint(insertBlock);

  return ir_func;
};

NodeVisitor.prototype.visitSemicolon = function(n,nc) {
  return this.visit(n.expression);
};

NodeVisitor.prototype.visitList = function(n,nc) {
  return nc;
};

NodeVisitor.prototype.visitBinOp = function(n,nc,builtin) {
  let callee = this.module.getFunction(builtin);
  // allocate space on the stack for the result
  let result = this.createAlloca(this.currentFunction, llvm.LLVMType.EjsValueType, "result_" + builtin);
  // call the add method
  let rv = llvm.IRBuilder.createCall(callee, [this.visit(nc[0]), this.visit(nc[1]), result], "result");
  // load and return the result
  return llvm.IRBuilder.createLoad(result, "result_"+builtin+"_load");
};

NodeVisitor.prototype.createLoadThis = function () {
  let _this = this.current_scope[EJS_THIS_NAME];
  return llvm.IRBuilder.createLoad (this.current_scope[EJS_THIS_NAME], "load_this");
};

NodeVisitor.prototype.createLoadContext = function () {
  if (!this.current_scope[EJS_CONTEXT_NAME])
    print ("die die die");
  return llvm.IRBuilder.createLoad (this.current_scope[EJS_CONTEXT_NAME], "load_context");
};

NodeVisitor.prototype.visitCall = function(n,nc) {
  print ("visitCall " + nc[0]);
  let callee = this.visit (nc[0]);
  let args = this.visit (nc[1]);

  if (callee.type && callee.type == DOT) {
    print ("creating property load!");
    callee = this.createPropertyLoad (callee.children[0], callee.children[1]);
  }

  // XXX this needs to be a better instanceof check
  let callee_is_closure = (callee.toString() !== "[object LLVMFunction]");

  // At this point we assume callee is a function object

  let compiled_arg_start;
  if (callee_is_closure) {
    print ("loading closure args");
    compiled_arg_start = 3; // closure invokers take the EJSContext and an arg length
    args.unshift(llvm.Constant.getIntegerValue(llvm.LLVMType.int32Type, args.length));
    args.unshift(callee);
    args.unshift(this.createLoadContext());
  }
  else if (!callee.ffi) {
    print ("loading this and context");
    // we insert the extra BUILTIN_ARGS since we're calling a JS function
    args.unshift(this.createLoadThis());
    args.unshift(this.createLoadContext());

    compiled_arg_start = 2;
    print ("done loading this and context");
  }
  else {
    print ("ffi function, not loading this and context");
    compiled_arg_start = 0;
  }

  print ("callee = " + callee.toString());

  let argv = [];
  for (let i = 0, j = args.length; i < j; i++) {
    if (i < compiled_arg_start)
      argv[i] = args[i];
    else
      argv[i] = this.visit(args[i]);
  }

  if (callee_is_closure) {
    print (1);
    let invoke_fun = this.ejs.invoke_closure[argv.length-compiled_arg_start];
    print ("2 invoke_fun = this.ejs.invoke_closure[" + (argv.length-compiled_arg_start) + "]");
    let rv = llvm.IRBuilder.createCall(invoke_fun, argv, "calltmp");
    print (3);
    return rv;
  }
  else {
    // we're dealing with a function here
    if (callee.argSize() !== args.length) {
      // this isn't invalid in JS.  if argSize > args.length, the args are undefined.
      // if argSize < args.length, the args are still passed
    }
    return llvm.IRBuilder.createCall(callee, argv, callee.returnType().isVoid() ? "" : "calltmp");
  }
};

NodeVisitor.prototype.visitNew = function(n,nc) {
  let ctor = this.visit (nc[0]);
  let args = this.visit (nc[1]);

  // At this point we assume callee is a function object
  if (callee.argSize() !== args.length) {
    // this isn't invalid in JS.  if argSize > args.length, the args are undefined.
    // if argSize < args.length, the args are still passed
  }

  let argv = [];
  for (let i = 0, j = args.length; i < j; i++) {
    argv[i] = this.visit(args[i]);
  }

  let rv = llvm.IRBuilder.createCall(callee, argv, "newtmp");
  return rv;
};

NodeVisitor.prototype.visitPropertyAccess = function(n,nc) {
  print ("property access: " + nc[1].value);
  return n;
};

NodeVisitor.prototype.visitThis = function(n, nc) {
  print ("visitThis");
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

NodeVisitor.prototype.visitIdentifier = function(n,nc) {
  let val = n.value;
  var alloca = findIdentifierInScope (val, this.current_scope);
  if (alloca) {
    return llvm.IRBuilder.createLoad(alloca, "load_"+val);
  }

  let func = null;

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

NodeVisitor.prototype.visitNumber = function(n,nc) {
  let c = llvm.ConstantFP.getDouble(n.value);
  let call = llvm.IRBuilder.createCall(this.ejs.number_new, [c], "numtmp");
  return call;
};

NodeVisitor.prototype.visitString = function(n,nc) {
  print ("visitString");
  let c = llvm.IRBuilder.createGlobalStringPtr(n.value, "strconst");
  let call = llvm.IRBuilder.createCall(this.ejs.string_new_utf8, [c], "strtmp");
  return call;
};

NodeVisitor.prototype.visit = function(n) {
  if (!n)
    return n;

  //print (tokens[n.type]);

  let nc = n.children;
  switch (n.type) {
  case FUNCTION: return this.visitFunction(n,nc);
  case GETTER: return this.visitGetter(n,nc);
  case SETTER: return this.visitSetter(n,nc);
  case SCRIPT: return this.visitScript(n,nc);
  case BLOCK: return this.visitBlock(n,nc);
  case LET_BLOCK: return this.visitBlock(n,nc);
  case SWITCH: return this.visitSwitch(n,nc);
  case FOR: return this.visitFor(n,nc);
  case WHILE: return this.visitWhile(n,nc);
  case IF: return this.visitIf(n,nc);
  case FOR_IN: return this.visitForIn(n,nc);
  case DO: return this.visitDo(n,nc);
  case BREAK: return this.visitBreak(n,nc);
  case CONTINUE: return this.visitContinue(n,nc);
  case TRY: return this.visitTry(n,nc);
  case THROW: return this.visitThrow(n,nc);
  case RETURN: return this.visitReturn(n,nc);
  case YIELD: return this.visitYield(n,nc);
  case GENERATOR: return this.visitGenerator(n,nc);
  case WITH: return this.visitWith(n,nc);
  case LET: return this.visitLet(n,nc);
  case VAR: return this.visitVar(n,nc);
  case CONST: return this.visitConst(n,nc);
  case DEBUGGER: return this.visitDebugger(n,nc);
  case SEMICOLON: return this.visitSemicolon(n,nc);
  case LABEL: return this.visitLabel(n,nc);
  case COMMA: return this.visitComma(n,nc);
  case LIST: return this.visitList(n,nc);
  case ASSIGN: return this.visitAssign(n,nc);
  case HOOK: return this.visitHook(n,nc);
  case OR: return this.visitBinOp(n,nc,"_ejs_op_or");
  case AND: return this.visitAnd(n,nc);
  case BITWISE_OR: return this.visitBitwiseOr(n,nc);
  case BITWISE_XOR: return this.visitBitwiseXor(n,nc);
  case BITWISE_AND: return this.visitBitwiseAnd(n,nc);
  case EQ: return this.visitEq(n,nc);
  case NE: return this.visitNe(n,nc);
  case STRICT_EQ: return this.visitBinOp(n,nc,"_ejs_op_strict_eq");
  case STRICT_NE: return this.visitStrictNe(n,nc);
  case LT: return this.visitLT(n,nc);
  case LE: return this.visitLE(n,nc);
  case GE: return this.visitGE(n,nc);
  case GT: return this.visitGT(n,nc);
  case IN: return this.visitIN(n,nc);
  case INSTANCEOF: return this.visitInstanceOf(n,nc);
  case LSH: return this.visitLSH(n,nc);
  case RSH: return this.visitRSH(n,nc);
  case URSH: return this.visitURSH(n,nc);
  case PLUS: return this.visitBinOp(n,nc,"_ejs_op_add");
  case MINUS: return this.visitBinOp(n,nc,"_ejs_op_sub");
  case IDENTIFIER: return this.visitIdentifier(n,nc);
  case NUMBER: return this.visitNumber(n,nc);
  case STRING: return this.visitString(n,nc);
  case CALL: return this.visitCall(n,nc);
  case NEW: return this.visitNew(n,nc);
  case MUL:
  case DIV:
  case MOD:
  case DELETE:
  case VOID:
  case TYPEOF:
  case NOT:
  case BITWISE_NOT:
  case UNARY_PLUS:
  case UNARY_MINUS:
  case INCREMENT:
  case DECREMENT:
  case DOT: return this.visitPropertyAccess(n,nc);
  case INDEX: break;
  case NEW_WITH_ARGS:
  case ARRAY_INIT:
  case ARRAY_COMP:
  case COMP_TAIL:
  case OBJECT_INIT:
  case NULL:
  case THIS: return this.visitThis(n,nc);
  case TRUE:
  case FALSE:
  case REGEXP:
  case GROUP:
  default:
    throw "PANIC: unknown operation " + tokens[n.type] + " " + n.toSource();
  }
};

function compile(n,nc) {
/*
  var infer = new tyinfer.TypeInfer(n);

  infer.run ();
*/
  var module = new llvm.LLVMModule("compiledfoo");
  var visitor = new NodeVisitor(module);
  var ir = visitor.visit(n);

  module.dump();

  quit(0);
}

exports.compile = compile;
