syntax = (require 'esprima').Syntax
debug = require 'debug'

filter = (x) ->
        rv = []
        for y in x when y
                if y.length
                        rv = rv.concat y
                else
                        rv.push y
        rv
        
hasOwn = Object.prototype.hasOwnProperty

exports.NodeVisitor = class NodeVisitor
        constructor: ->

        shallowCopy: (o) ->
                new_o = {}
                new_o[x] = o[x] for x of o when hasOwn.apply o, [x]
                new_o
                
        visitProgram: (n) ->
                n.body = filter (@visit child for child in n.body)
                n
                
        visitFunction: (n) ->
                n.params = filter (@visit param for param in n.params)
                n.body = @visit n.body
                n

        visitFunctionDeclaration: (n) ->
                @visitFunction n
                
        visitFunctionExpression: (n) ->
                @visitFunction n

        visitBlock: (n) ->
                n.body = filter (@visit child for child in n.body)
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
                n.cases = filter (@visit _case for _case in n.cases)
                n
                
        visitCase: (n) ->
                n.test = @visit n.test
                n.consequent = filter (@visit _con for _con in n.consequent)
                n
                
        visitFor: (n) ->
                n.init = @visit n.init
                n.test = @visit n.test
                n.update = @visit n.update
                n.body = @visit n.body
                n
                
        visitWhile: (n) ->
                n.test = @visit n.test
                n.body = @visit n.body
                n
                
        visitIf: (n) ->
                n.test = @visit n.test
                n.consequent = @visit n.consequent
                n.alternate = @visit n.alternate
                n
                
        visitForIn: (n) ->
                n.left = @visit n.left
                n.right = @visit n.right
                n.body = @visit n.body
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
                        n.handlers = filter (@visit handler for handler in n.handlers)
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
                n.body = @visit n.body
                n
                
        visitVariableDeclaration: (n) ->
                n.declarations = filter (@visit decl for decl in n.declarations)
                n

        visitVariableDeclarator: (n) ->
                n.id = @visit n.id
                n.init = @visit n.init
                n
                                
        visitLabeledStatement: (n) ->
                n.label = @visit n.label
                n.body = @visit n.body
                n
                
        visitAssignmentExpression: (n) ->
                n.left = @visit n.left
                # we don't visit the operator since it's not a separate node
                n.right = @visit n.right
                n
                
        visitConditionalExpression: (n) ->
                n.test = @visit n.test
                n.consequent = @visit n.consequent
                n.alternate = @visit n.alternate
                n
                
        visitLogicalExpression: (n) ->
                n.left = @visit n.left
                # we don't visit the operator since it's not a separate node
                n.right = @visit n.right
                n
                
        visitBinaryExpression: (n) ->
                n.left = @visit n.left
                # we don't visit the operator since it's not a separate node
                n.right = @visit n.right
                n

        visitUnaryExpression: (n) ->
                # we don't visit the operator since it's not a separate node
                n.argument = @visit n.argument
                n

        visitUpdateExpression: (n) ->
                # we don't visit the operator since it's not a separate node
                n.argument = @visit n.argument
                n

        visitMemberExpression: (n) ->
                n.object = @visit n.object
                if n.computed
                        n.property = @visit n.property
                n
                
        visitRelationalExpression: (n) ->
                n.left = @visit n.left
                # we don't visit the operator since it's not a separate node
                n.right = @visit n.right
                n
                
        visitSequenceExpression: (n) ->
                n.expressions = filter (@visit exp for exp in n.expressions)
                n
                
        visitNewExpression: (n) ->
                n.callee = @visit n.callee
                n.arguments = filter (@visit arg for arg in n.arguments)
                n

        visitObjectExpression: (n) ->
                n.properties = filter (@visit property for property in n.properties)
                n

        visitArrayExpression: (n) ->
                n.elements = filter (@visit element for element in n.elements)
                n

        visitProperty: (n) ->
                n.key = @visit n.key
                n.value = @visit n.value
                n
                                
        visitCallExpression: (n) ->
                n.callee = @visit n.callee
                n.arguments = filter (@visit arg for arg in n.arguments)
                n
                
        visit: (n) ->
                debug.log "child is null!>" if not n?
                return null if not n?

                debug.indent()
                debug.log "#{n.type}>"

                new_n = @shallowCopy n
                rv = null
                switch new_n.type
                        when syntax.Program              then rv = @visitProgram new_n
                        when syntax.FunctionDeclaration  then rv = @visitFunctionDeclaration new_n
                        when syntax.FunctionExpression   then rv = @visitFunctionExpression new_n
                        when syntax.LabeledStatement     then rv = @visitLabeledStatement new_n
                        when syntax.BlockStatement       then rv = @visitBlock new_n
                        when syntax.ExpressionStatement  then rv = @visitExpressionStatement new_n
                        when syntax.SwitchStatement      then rv = @visitSwitch new_n
                        when syntax.SwitchCase           then rv = @visitCase new_n
                        when syntax.ForStatement         then rv = @visitFor new_n
                        when syntax.WhileStatement       then rv = @visitWhile new_n
                        when syntax.IfStatement          then rv = @visitIf new_n
                        when syntax.ForInStatement       then rv = @visitForIn new_n
                        when syntax.DoWhileStatement     then rv = @visitDo new_n
                        when syntax.BreakStatement       then rv = @visitBreak new_n
                        when syntax.ContinueStatement    then rv = @visitContinue new_n
                        when syntax.TryStatement         then rv = @visitTry new_n
                        when syntax.CatchClause          then rv = @visitCatchClause new_n
                        when syntax.ThrowStatement       then rv = @visitThrow new_n
                        when syntax.ReturnStatement      then rv = @visitReturn new_n
                        when syntax.WithStatement        then rv = @visitWith new_n
                        when syntax.VariableDeclaration  then rv = @visitVariableDeclaration new_n
                        when syntax.VariableDeclarator   then rv = @visitVariableDeclarator new_n
                        when syntax.LabeledStatement     then rv = @visitLabeledStatement new_n
                        when syntax.AssignmentExpression then rv = @visitAssignmentExpression new_n
                        when syntax.ConditionalExpression then rv = @visitConditionalExpression new_n
                        when syntax.LogicalExpression    then rv = @visitLogicalExpression new_n
                        when syntax.NewExpression        then rv = @visitNewExpression new_n
                        when syntax.ThisExpression       then rv = @visitThisExpression new_n
                        when syntax.BinaryExpression     then rv = @visitBinaryExpression new_n
                        when syntax.UnaryExpression      then rv = @visitUnaryExpression new_n
                        when syntax.UpdateExpression     then rv = @visitUpdateExpression new_n
                        when syntax.MemberExpression     then rv = @visitMemberExpression new_n
                        when syntax.RelationalExpression then rv = @visitRelationalExpression new_n
                        when syntax.SequenceExpression   then rv = @visitSequenceExpression new_n
                        when syntax.ObjectExpression     then rv = @visitObjectExpression new_n
                        when syntax.ArrayExpression      then rv = @visitArrayExpression new_n
                        when syntax.Identifier           then rv = @visitIdentifier new_n
                        when syntax.Literal              then rv = @visitLiteral new_n
                        when syntax.CallExpression       then rv = @visitCallExpression new_n
                        when syntax.Property             then rv = @visitProperty new_n
                        when syntax.EmptyStatement       then rv = @visitEmptyStatement new_n
                        else
                            throw "PANIC: unknown operation #{n.type}"
                        
                debug.log "<#{n.type}"
                debug.unindent()
                rv

