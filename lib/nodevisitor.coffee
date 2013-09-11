syntax = (require 'esprima').Syntax
debug = require 'debug'

exports.TreeVisitor = class TreeVisitor
        visitProgram: (n) ->
                @visit n.body
                
        visitFunction: (n) ->
                @visit n.params

        visitFunctionDeclaration: (n) ->
                @visitFunction n
                
        visitFunctionExpression: (n) ->
                @visitFunction n

        visitBlock: (n) ->
                @visit n.body

        visitLabeledStatement: (n) ->
                @visit n.body

        visitEmptyStatement: (n) ->

        visitExpressionStatement: (n) ->
                @visit n.expression
                
        visitSwitch: (n) ->
                @visit n.discriminant
                @visit _case for _case in n.cases
                
        visitCase: (n) ->
                @visit n.test
                @visit _con for _con in n.consequent
                
        visitFor: (n) ->
                @visit n.init
                @visit n.test
                @visit n.update
                @visit n.body
                
        visitWhile: (n) ->
                @visit n.test
                @visit n.body
                
        visitIf: (n) ->
                @visit n.test
                @visit n.consequent
                @visit n.alternate
                
        visitForIn: (n) ->
                @visit n.left
                @visit n.right
                @visit n.body
                
        visitDo: (n) ->
                @visit n.body
                @visit n.test
                
        visitIdentifier: (n) ->
        visitLiteral: (n) ->
        visitThisExpression: (n) ->
        visitBreak: (n) ->
        visitContinue: (n) ->
                
        visitTry: (n) ->
                @visit n.block
                if n.handlers?
                        @visit n.handlers
                @visit n.finalizer

        visitCatchClause: (n) ->
                @visit n.param
                @visit n.guard
                @visit n.body
                
        visitThrow: (n) ->
                @visit n.argument
                
        visitReturn: (n) ->
                @visit n.argument
                
        visitWith: (n) ->
                @visit n.object
                @visit n.body
                
        visitVariableDeclaration: (n) ->
                @visit n.declarations

        visitVariableDeclarator: (n) ->
                @visit n.id
                @visit n.init
                                
        visitLabeledStatement: (n) ->
                @visit n.label
                @visit n.body
                
        visitAssignmentExpression: (n) ->
                @visit n.left
                @visit n.right
                
        visitConditionalExpression: (n) ->
                @visit n.test
                @visit n.consequent
                @visit n.alternate
                
        visitLogicalExpression: (n) ->
                @visit n.left
                @visit n.right
                
        visitBinaryExpression: (n) ->
                @visit n.left
                @visit n.right

        visitUnaryExpression: (n) -> @visit n.argument

        visitUpdateExpression: (n) -> @visit n.argument

        visitMemberExpression: (n) ->
                @visit n.object
                if n.computed
                        @visit n.property
                
        visitRelationalExpression: (n) ->
                @visit n.left
                @visit n.right
                
        visitSequenceExpression: (n) ->
                @visit n.expressions
                
        visitNewExpression: (n) ->
                @visit n.callee
                @visit n.arguments

        visitObjectExpression: (n) ->
                @visit n.properties

        visitArrayExpression: (n) ->
                @visit n.elements

        visitProperty: (n) ->
                @visit n.key
                @visit n.value
                                
        visitCallExpression: (n) ->
                @visit n.callee
                @visit n.arguments
                
        visit: (n) ->
                if not n?
                        debug.log "child is null!>"
                        return null

                if Array.isArray n
                        return n.map (el) => @visit el
                        
                debug.indent()
                debug.log "#{n.type}>"

                rv = null
                switch n.type
                        when syntax.Program               then rv = @visitProgram n
                        when syntax.FunctionDeclaration   then rv = @visitFunctionDeclaration n
                        when syntax.FunctionExpression    then rv = @visitFunctionExpression n
                        when syntax.LabeledStatement      then rv = @visitLabeledStatement n
                        when syntax.BlockStatement        then rv = @visitBlock n
                        when syntax.ExpressionStatement   then rv = @visitExpressionStatement n
                        when syntax.SwitchStatement       then rv = @visitSwitch n
                        when syntax.SwitchCase            then rv = @visitCase n
                        when syntax.ForStatement          then rv = @visitFor n
                        when syntax.WhileStatement        then rv = @visitWhile n
                        when syntax.IfStatement           then rv = @visitIf n
                        when syntax.ForInStatement        then rv = @visitForIn n
                        when syntax.DoWhileStatement      then rv = @visitDo n
                        when syntax.BreakStatement        then rv = @visitBreak n
                        when syntax.ContinueStatement     then rv = @visitContinue n
                        when syntax.TryStatement          then rv = @visitTry n
                        when syntax.CatchClause           then rv = @visitCatchClause n
                        when syntax.ThrowStatement        then rv = @visitThrow n
                        when syntax.ReturnStatement       then rv = @visitReturn n
                        when syntax.WithStatement         then rv = @visitWith n
                        when syntax.VariableDeclaration   then rv = @visitVariableDeclaration n
                        when syntax.VariableDeclarator    then rv = @visitVariableDeclarator n
                        when syntax.LabeledStatement      then rv = @visitLabeledStatement n
                        when syntax.AssignmentExpression  then rv = @visitAssignmentExpression n
                        when syntax.ConditionalExpression then rv = @visitConditionalExpression n
                        when syntax.LogicalExpression     then rv = @visitLogicalExpression n
                        when syntax.NewExpression         then rv = @visitNewExpression n
                        when syntax.ThisExpression        then rv = @visitThisExpression n
                        when syntax.BinaryExpression      then rv = @visitBinaryExpression n
                        when syntax.UnaryExpression       then rv = @visitUnaryExpression n
                        when syntax.UpdateExpression      then rv = @visitUpdateExpression n
                        when syntax.MemberExpression      then rv = @visitMemberExpression n
                        when syntax.RelationalExpression  then rv = @visitRelationalExpression n
                        when syntax.SequenceExpression    then rv = @visitSequenceExpression n
                        when syntax.ObjectExpression      then rv = @visitObjectExpression n
                        when syntax.ArrayExpression       then rv = @visitArrayExpression n
                        when syntax.Identifier            then rv = @visitIdentifier n
                        when syntax.Literal               then rv = @visitLiteral n
                        when syntax.CallExpression        then rv = @visitCallExpression n
                        when syntax.Property              then rv = @visitProperty n
                        when syntax.EmptyStatement        then rv = @visitEmptyStatement n
                        else
                            throw "PANIC: unknown parse node type #{n.type}"
                        
                debug.log "<#{n.type}"
                debug.unindent()
                rv

exports.TreeTransformer = class TreeTransformer extends TreeVisitor
        shallowCopy = (o) ->
                return null if not o?
                
                new_o = Object.create Object.getPrototypeOf o
                new_o[x] = o[x] for x in Object.getOwnPropertyNames o
                new_o

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

        visit: (n) ->
                # for non-array n's, just return the transformed node
                return super shallowCopy n unless Array.isArray n

                # for array n's, we need to copy them all before visiting them
                return filter super n.map shallowCopy
        
        visitProgram: (n) ->
                n.body = @visit n.body
                n
                
        visitFunction: (n) ->
                n.params = @visit n.params
                n.body   = @visit n.body
                n

        visitFunctionDeclaration: (n) ->
                @visitFunction n
                
        visitFunctionExpression: (n) ->
                @visitFunction n

        visitBlock: (n) ->
                n.body = @visit n.body
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
                n.cases        = @visit n.cases
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
                n.declarations = @visit n.declarations
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
                n.expressions = @visit n.expressions
                n
                
        visitNewExpression: (n) ->
                n.callee    = @visit n.callee
                n.arguments = @visit n.arguments
                n

        visitObjectExpression: (n) ->
                n.properties = @visit n.properties
                n

        visitArrayExpression: (n) ->
                n.elements = @visit n.elements
                n

        visitProperty: (n) ->
                n.key   = @visit n.key
                n.value = @visit n.value
                n
                                
        visitCallExpression: (n) ->
                n.callee    = @visit n.callee
                n.arguments = @visit n.arguments
                n
