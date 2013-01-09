###
#
# Now that there are no vars (and therefore all identifiers are
# block-scoped) we can get rid of IIFE's with the following beta-esque
# transformation
#
#      rv = (function(x1,x2,...) { body })(y1,y2,...)
#
# becomes
#
#      { let x1 = y1, x2 = y2,...;  body; rv = %iife_rv }
#
# One thing to note, though, the initializer expressions y1, y2, ... must remain even if x1, x1... aren't referenced in the block.
#
###

esprima = require 'esprima'
escodegen = require 'escodegen'
syntax = esprima.Syntax
debug = require 'debug'

{ NodeVisitor } = require 'nodevisitor'

class VarToLet extends NodeVisitor
        constructor: ->
                @varsStack = []
                super
                
        visitFunction: (n) ->
                func = n
                @varsStack.unshift []
                super
                vars = @varsStack.shift()
                n.body.body = vars.concat n.body.body
                n

        # this is broken for const.
        visitVariableDeclaration: (n) ->
                super

                # if we're already a let or const, don't do anything
                return n if n.kind is "let" or n.kind is "const"

                currentVars = @varsStack[0]

                assignments = []
                top_decl = type: syntax.VariableDeclaration, kind: "let", declarations: []
                for decl in n.declarations
                        top_decl.declarations.push type: syntax.VariableDeclarator, id: {type: syntax.Identifier, name: decl.id.name}, init: null
                        if decl.init?
                                assignments.push type: syntax.ExpressionStatement, expression: {type: syntax.AssignmentExpression, operator: "=", left: {type: syntax.Identifier, name: decl.id.name}, right: decl.init}

                currentVars.push top_decl

                return if assignments.length > 0 then assignments else null


class RemoveIIFE extends NodeVisitor
        visitCallExpression: (n) ->
                if n.callee.type is syntax.FunctionExpression and n.callee.params.length is 0  # let's limit this to 0-arg IIFE's for now
                        body = @visit n.callee.body
                        body.fromIIFE = true
                        body
                else
                        super

exports.desugar = (tree) ->
        debug.log -> escodegen.generate tree

        var_to_let = new VarToLet
        tree = var_to_let.visit tree

        remove_iife = new RemoveIIFE
        tree = remove_iife.visit tree

        return tree
