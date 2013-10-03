syntax = (require 'esprima').Syntax
debug = require 'debug'

exports.TreeVisitor = class TreeVisitor
        # for collections, returns all non-null.  for single item properties,
        # just always returns the item.
        filter = (x) ->
                return x if not Array.isArray x
        
                rv = []
                for y in x when y?
                        if Array.isArray y
                                rv = rv.concat y
                        else
                                rv.push y
                rv

        # a rather disgusting in-place filter+flatten visitor
        visitArray: (arr) ->
                i = 0
                e = arr.length
                while i < e
                        tmp = @visit arr[i]
                        if tmp is null
                                arr.splice i, 1
                                e = arr.length
                        else if Array.isArray tmp
                                tmplen = tmp.length
                                if tmplen > 0
                                        tmp.unshift 1
                                        tmp.unshift i
                                        arr.splice.apply arr, tmp
                                        i += tmplen
                                        e = arr.length
                                else
                                        arr.splice i, 1
                                        e = arr.length
                        else
                                arr[i] = tmp
                                i += 1
                arr
                
                
        visit: (n) ->
                return null if not n?

                return @visitArray n if Array.isArray n

                #debug.indent()
                #debug.log -> "#{n.type}>"

                switch n.type
                        when syntax.Program                 then rv = @visitProgram n
                        when syntax.FunctionDeclaration     then rv = @visitFunctionDeclaration n
                        when syntax.FunctionExpression      then rv = @visitFunctionExpression n
                        when syntax.ArrowFunctionExpression then rv = @visitArrowFunctionExpression n
                        when syntax.LabeledStatement        then rv = @visitLabeledStatement n
                        when syntax.BlockStatement          then rv = @visitBlock n
                        when syntax.ExpressionStatement     then rv = @visitExpressionStatement n
                        when syntax.SwitchStatement         then rv = @visitSwitch n
                        when syntax.SwitchCase              then rv = @visitCase n
                        when syntax.ForStatement            then rv = @visitFor n
                        when syntax.WhileStatement          then rv = @visitWhile n
                        when syntax.IfStatement             then rv = @visitIf n
                        when syntax.ForInStatement          then rv = @visitForIn n
                        when syntax.DoWhileStatement        then rv = @visitDo n
                        when syntax.BreakStatement          then rv = @visitBreak n
                        when syntax.ContinueStatement       then rv = @visitContinue n
                        when syntax.TryStatement            then rv = @visitTry n
                        when syntax.CatchClause             then rv = @visitCatchClause n
                        when syntax.ThrowStatement          then rv = @visitThrow n
                        when syntax.ReturnStatement         then rv = @visitReturn n
                        when syntax.WithStatement           then rv = @visitWith n
                        when syntax.VariableDeclaration     then rv = @visitVariableDeclaration n
                        when syntax.VariableDeclarator      then rv = @visitVariableDeclarator n
                        when syntax.LabeledStatement        then rv = @visitLabeledStatement n
                        when syntax.AssignmentExpression    then rv = @visitAssignmentExpression n
                        when syntax.ConditionalExpression   then rv = @visitConditionalExpression n
                        when syntax.LogicalExpression       then rv = @visitLogicalExpression n
                        when syntax.NewExpression           then rv = @visitNewExpression n
                        when syntax.ThisExpression          then rv = @visitThisExpression n
                        when syntax.BinaryExpression        then rv = @visitBinaryExpression n
                        when syntax.UnaryExpression         then rv = @visitUnaryExpression n
                        when syntax.UpdateExpression        then rv = @visitUpdateExpression n
                        when syntax.MemberExpression        then rv = @visitMemberExpression n
                        when syntax.RelationalExpression    then rv = @visitRelationalExpression n
                        when syntax.SequenceExpression      then rv = @visitSequenceExpression n
                        when syntax.ObjectExpression        then rv = @visitObjectExpression n
                        when syntax.ArrayExpression         then rv = @visitArrayExpression n
                        when syntax.Identifier              then rv = @visitIdentifier n
                        when syntax.Literal                 then rv = @visitLiteral n
                        when syntax.CallExpression          then rv = @visitCallExpression n
                        when syntax.Property                then rv = @visitProperty n
                        when syntax.EmptyStatement          then rv = @visitEmptyStatement n
                        else
                            throw "PANIC: unknown parse node type #{n.type}"
                
                #debug.log -> "<#{n.type}, rv = #{if rv then rv.type else 'null'}"
                #debug.unindent()

                return n if rv is undefined or rv is n
                return rv

        visitProgram: (n) ->
                n.body = @visitArray n.body
                n
                
        visitFunction: (n) ->
                n.params = @visitArray n.params
                n.body   = @visit n.body
                n

        visitFunctionDeclaration: (n) ->
                @visitFunction n
                
        visitFunctionExpression: (n) ->
                @visitFunction n

        visitArrowFunctionExpression: (n) ->
                @visitFunction n

        visitBlock: (n) ->
                n.body = @visitArray n.body
                n

        visitLabeledStatement: (n) ->
                n.body = @visit n.body
                n

        visitEmptyStatement: (n) ->
                n

        visitExpressionStatement: (n) ->
                n.expression = @visit n.expression
                n
                
        visitSwitch: (n) ->
                n.discriminant = @visit n.discriminant
                n.cases        = @visitArray n.cases
                n
                
        visitCase: (n) ->
                n.test       = @visit n.test
                n.consequent = @visit n.consequent
                n
                
        visitFor: (n) ->
                n.init   = @visit n.init
                n.test   = @visit n.test
                n.update = @visit n.update
                n.body   = @visit n.body
                n
                
        visitWhile: (n) ->
                n.test = @visit n.test
                n.body = @visit n.body
                n
                
        visitIf: (n) ->
                n.test       = @visit n.test
                n.consequent = @visit n.consequent
                n.alternate  = @visit n.alternate
                n
                
        visitForIn: (n) ->
                n.left  = @visit n.left
                n.right = @visit n.right
                n.body  = @visit n.body
                n
                
        visitDo: (n) ->
                n.body = @visit n.body
                n.test = @visit n.test
                n
                
        visitIdentifier: (n) -> n
        visitLiteral: (n) -> n
        visitThisExpression: (n) -> n
        visitBreak: (n) -> n
        visitContinue: (n) -> n
                
        visitTry: (n) ->
                n.block = @visit n.block
                if n.handlers?
                        n.handlers = @visit n.handlers
                else
                        n.handlers = null
                n.finalizer = @visit n.finalizer
                n

        visitCatchClause: (n) ->
                n.param = @visit n.param
                n.guard = @visit n.guard
                n.body = @visit n.body
                n
                
        visitThrow: (n) ->
                n.argument = @visit n.argument
                n
                
        visitReturn: (n) ->
                n.argument = @visit n.argument
                n
                
        visitWith: (n) ->
                n.object = @visit n.object
                n.body   = @visit n.body
                n
                
        visitVariableDeclaration: (n) ->
                n.declarations = @visitArray n.declarations
                n

        visitVariableDeclarator: (n) ->
                n.id   = @visit n.id
                n.init = @visit n.init
                n
                                
        visitLabeledStatement: (n) ->
                n.label = @visit n.label
                n.body  = @visit n.body
                n
                
        visitAssignmentExpression: (n) ->
                n.left  = @visit n.left
                n.right = @visit n.right
                n
                
        visitConditionalExpression: (n) ->
                n.test       = @visit n.test
                n.consequent = @visit n.consequent
                n.alternate  = @visit n.alternate
                n
                
        visitLogicalExpression: (n) ->
                n.left  = @visit n.left
                n.right = @visit n.right
                n
                
        visitBinaryExpression: (n) ->
                n.left  = @visit n.left
                n.right = @visit n.right
                n

        visitUnaryExpression: (n) ->
                n.argument = @visit n.argument
                n

        visitUpdateExpression: (n) ->
                n.argument = @visit n.argument
                n

        visitMemberExpression: (n) ->
                n.object = @visit n.object
                if n.computed
                        n.property = @visit n.property
                n
                
        visitRelationalExpression: (n) ->
                n.left  = @visit n.left
                n.right = @visit n.right
                n
                
        visitSequenceExpression: (n) ->
                n.expressions = @visitArray n.expressions
                n
                
        visitNewExpression: (n) ->
                n.callee    = @visit n.callee
                n.arguments = @visitArray n.arguments
                n

        visitObjectExpression: (n) ->
                n.properties = @visitArray n.properties
                n

        visitArrayExpression: (n) ->
                n.elements = @visitArray n.elements
                n

        visitProperty: (n) ->
                n.key   = @visit n.key
                n.value = @visit n.value
                n
                                
        visitCallExpression: (n) ->
                n.callee    = @visit n.callee
                n.arguments = @visitArray n.arguments
                n
                
        toString: () -> "TreeVisitor"
