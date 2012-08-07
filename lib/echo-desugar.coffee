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

syntax = (require 'esprima').Syntax
{ NodeVisitor } = require 'nodevisitor'

exports.VarToLet = class VarToLet extends NodeVisitor
        constructor: ->
                @funcStack = []
                super

        visitFunction: (n) ->
                func = n
                @varsStack.unshift []
                super
                vars = @varsStack.shift()
                n.body = vars.concat n.body
                n

        visitVariableDeclaration: (n) ->
                if n.kind is "let"
                        return n

                decls = n.decls
                currentVars = @varsStack[0]
                
                currentVars.unshift

                if decl.initializer
                        @replaceCurrentNode new ast.ExpressionStatement new ast.Assignment ast.AssignOp.assign, decl.name, @visitNode decl.initializer
                else
                        @removeCurrentNode()


exports.RemoveIIFE = class RemoveIIFE extends NodeVisitor
