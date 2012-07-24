
exports.AssignOp =
  assign: "="


# AstNode
#
#  @parent
#  @children

exports.AstNode = class AstNode
  constructor: (@ast_nodes...) ->
    console.log "there are #{@ast_nodes.length} children of #{@}"

    # set @parent on all children
    for n in @ast_nodes
      if n
        console.log "setting parent of #{n} to #{@}"
      n?.parent = @
    
  toString: -> @constructor.name

  getChildren: -> @ast_nodes
  getParent: -> @parent
  getEnclosingScript: ->
    if @parent instanceof Script
      return @parent
    return @parent.getEnclosingScript



exports.Script = class Script extends AstNode
  constructor: (@children) ->
    super(@children...)


exports.Statement = class Statement extends AstNode



exports.ExpressionStatement = class ExpressionStatement extends Statement
  constructor: (@expr) ->
    super



exports.Block = class Block extends Statement
  constructor: (@statements) ->
    super(@statements...)

  prependChild: (stmt) -> @statements.unshift stmt
  replaceChild: (stmt, withStmt) ->
    @statements[@statements.indexOf(stmt)] = withStmt



exports.Expression = class Expression extends AstNode

exports.Identifier = class Identifier extends Expression

exports.Number = class Number extends Expression
  constructor: (@value) ->
    super



exports.BinOp = class BinOp extends Expression
  constructor: (@op, @lhs, @rhs) ->
    super



exports.Assignment = class Assignment extends Expression
  constructor: (@op, @lhs, @rhs) ->
    super


#
# Decl objects have the following properties
#
# name        - the current name of the variable
# initializer - optional (in non-Const subclasses) initializer expression
#
exports.Decl = class Decl extends Statement
  constructor: (@name, @initializer) ->
    super

exports.Const = class Const extends Decl
  constructor: (name, initializer) ->
    throw new Error "Const declaration must have an initializer" unless initializer
    super

exports.Let = class Let extends Decl
  constructor: (name, initializer) ->
    super

exports.Var = class Var extends Decl
  constructor: (name, initializer) ->
    super

#
# Function objects have the following properties
#
# name         - the current name of the function (can be different than declaredName if lifted and/or conflicts)
# declaredName - the name given to the function in the source file
# parameters   - an array of Parameter objects
# body         - the AST subtree of the function's body
#
exports.Function = class Function extends Expression
  constructor: (@declaredName, @parameters, @body) ->
    throw new Error "Function::body must be a Block" unless @body instanceof Block
    @name = @declaredName # we start with the same name
    super

exports.Call = class Call extends Expression
  constructor: (@callee, @args) ->
    super
  isIIFE: () ->
	# an immediately invoked function expression (IIFE) looks like this in JS:
	# (function(a,b,c,...)(d,e,f, ...))
	# the important bit is that it's a call with a Function node in the callee position
    @callee instanceof Function




exports.NodeReplaced = class NodeReplaced
  
exports.NodeVisitor = class NodeVisitor
  constructor: ->
    @currentNodeStack = []

  visitNode: (node) ->
    @currentNodeStack.unshift node
    matched = false
    match = (t,v) ->
      if not matched
        if node instanceof t
          v()
          matched = true
    unmatched = (v) ->
      v() unless matched

    match Var,                 => @visitVar node
    match Let,                 => @visitLet node
    match Const,               => @visitConst node
    match Function,            => @visitFunction node
    match Identifier,          => @visitIdentifier node
    match Number,              => @visitNumber node
    match Block,               => @visitBlock node
    match Assignment,          => @visitAssignment node
    match ExpressionStatement, => @visitExpressionStatement node
    unmatched                  => throw new Error "unrecognized node type #{node}"

    @currentNodeStack.shift()


  visitNodes: (nodes) ->
    while true
      try
        @visitNode n for n in nodes
        break
      catch error
        if error instanceof NodeReplaced
          continue
        throw error

  replaceCurrentNode: (newnode) ->
    currentNode = @currentNodeStack.shift()
    console.log "currentNode = #{currentNode}"
    currentNode.parent.replaceChild currentNode, newnode
    throw new NodeReplaced

  removeCurrentNode: ->
    currentNode = @currentNodeStack.shift()
    currentNode.parent.removeChild currentNode
    

  visitIdentifier: (id) ->
  visitNumber: (id) ->

  visitFunction: (func) ->
    console.log "func = #{func}"
    @visitNode func.name
    @visitNodes func.parameters
    @visitNode func.body

  visitExpressionStatement: (exprstmt) ->
    @visitNode exprstmt.expr

  visitDecl: (decl) ->
    @visitNode decl.name
    if decl.initializer
      @visitNode decl.initializer
  visitVar: (decl) ->
    @visitDecl decl
  visitLet: (decl) -> @visitDecl decl
  visitConst: (decl) -> @visitDecl decl
  visitBlock: (block) -> @visitNodes block.statements
  visitAssignment: (assign) ->
    @visitNode assign.rhs
    @visitNode assign.lhs

exports.buildAST = (n) ->
	


###
# some tests
console.log "script"
s = new Script null

console.log "expression"
e = new Expression
e.setParent s
if e.getParent() isnt s
  console.log "getParent() failed"
if e.getEnclosingScript() isnt s
  console.log "getEnclosingScript() failed"

console.log "call"
f = new Function "iife-test", [], null
c = new Call f, []
if not c.isIIFE()
  console.log "iife test failed"

console.log "done"
###