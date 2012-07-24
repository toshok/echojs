###

  This pass performs two operations:

    1. hoists all vars to the toplevel of their enclosing function and replaces them with lets,
       leaving the assignment to the initializer in the place where the var was.

	 function() {
	   if (test) {
	     var x = 5;
	   }
	 }

	 becomes:

	 function() {
	   let x;
	   if (test) {
	     x = 5;
	   }
	 }

    2. now that there are no vars (and therefor the scope of all variables is determined) we can get rid of IIFE's with the following beta-esque transformation

      (function(x1,x2,...) { body })(y1,y2,...)

      becomes

      { let x1 = y1, x2 = y2,...;  body }

      one thing to note, though, the initializer expressions y1, y2, ... must remain even if x1, x1... aren't referenced in the block.

###

ast = require './echo-ast'

class EchoPrettyPrintVisitor extends ast.NodeVisitor
  constructor: ->
    super
    @reset()

  reset: ->
    @value = ""

  visitIdentifier: (id) ->
    @value += id.name
  visitNumber: (n) ->
    @value += n.value

  visitFunction: (func) ->
    @value += "function "
    @visitNode func.name
    @value += " ("
    @visitNodes func.parameters
    @value += ") "
    @visitNode func.body

  visitExpressionStatement: (exprstmt) ->
    super
    @value += "; "

  visitVar: (decl)   -> @stringifyDecl "var", decl
  visitLet: (decl)   -> @stringifyDecl "let", decl
  visitConst: (decl) -> @stringifyDecl "const", decl

  visitBlock: (block) ->
    @value += " { "
    @visitNodes block.statements, " "
    @value += " } "

  visitAssignment: (assign) ->
    @visitNode assign.lhs
    @value+= " #{assign.op} "
    @visitNode assign.rhs

  stringifyDecl: (t, decl) ->
    @value += "#{t} "
    @visitNode decl.name
    if decl.initializer
      @value += " = "
      @visitNode decl.initializer
    @value += "; "


class DesugarVarToLetPass extends ast.NodeVisitor
  constructor: ->
    @funcStack = []
    super

  visitFunction: (func) ->
    @funcStack.unshift(func)
    super
    @funcStack.shift()

  visitVar: (decl) ->
    currentFunc = @funcStack[0]
    currentFunc.body.prependChild (new ast.Let (decl.name))
    if decl.initializer
      @replaceCurrentNode new ast.ExpressionStatement new ast.Assignment ast.AssignOp.assign, decl.name, @visitNode decl.initializer
    else
      @removeCurrentNode()


class DesugarRemoveIIFEPass extends ast.NodeVisitor
  constructor: -> @classStack = []
  visitCall: (call) ->
    if call.callee instance Function
    else
      super call


testfunc = new ast.Function (new ast.Identifier "testFunction"),
                            [new ast.Identifier "x"],
                            new ast.Block [
                              (new ast.Var (new ast.Identifier "test"), (new ast.Number 5)),
                              (new ast.Block [
                                (new ast.Var (new ast.Identifier "test2"), (new ast.Number 10))
                              ])
                            ]

pp = new EchoPrettyPrintVisitor
pp.reset()
pp.visitNode testfunc

console.log "before #{pp.value}"

desugar = new DesugarVarToLetPass
desugar.visitNode testfunc

pp.reset()
pp.visitNode testfunc

console.log "after #{pp.value}"

console.log "done"