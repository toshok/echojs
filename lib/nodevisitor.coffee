syntax = (require 'esprima').Syntax
debug = require 'debug'

_ArrayExpression = syntax.ArrayExpression
_ArrayPattern = syntax.ArrayPattern
_ArrowFunctionExpression = syntax.ArrowFunctionExpression
_AssignmentExpression = syntax.AssignmentExpression
_BinaryExpression = syntax.BinaryExpression
_BlockStatement = syntax.BlockStatement
_BreakStatement = syntax.BreakStatement
_CallExpression = syntax.CallExpression
_CatchClause = syntax.CatchClause
_ClassBody = syntax.ClassBody
_ClassDeclaration = syntax.ClassDeclaration
_ClassExpression = syntax.ClassExpression
_ClassHeritage = syntax.ClassHeritage
_ComprehensionBlock = syntax.ComprehensionBlock
_ComprehensionExpression = syntax.ComprehensionExpression
_ConditionalExpression = syntax.ConditionalExpression
_ContinueStatement = syntax.ContinueStatement
_DebuggerStatement = syntax.DebuggerStatement
_DoWhileStatement = syntax.DoWhileStatement
_EmptyStatement = syntax.EmptyStatement
_ExportDeclaration = syntax.ExportDeclaration
_ExportBatchSpecifier = syntax.ExportBatchSpecifier
_ExportSpecifier = syntax.ExportSpecifier
_ExpressionStatement = syntax.ExpressionStatement
_ForInStatement = syntax.ForInStatement
_ForOfStatement = syntax.ForOfStatement
_ForStatement = syntax.ForStatement
_FunctionDeclaration = syntax.FunctionDeclaration
_FunctionExpression = syntax.FunctionExpression
_Identifier = syntax.Identifier
_IfStatement = syntax.IfStatement
_ImportDeclaration = syntax.ImportDeclaration
_ImportSpecifier = syntax.ImportSpecifier
_LabeledStatement = syntax.LabeledStatement
_Literal = syntax.Literal
_LogicalExpression = syntax.LogicalExpression
_MemberExpression = syntax.MemberExpression
_MethodDefinition = syntax.MethodDefinition
_ModuleDeclaration = syntax.ModuleDeclaration
_NewExpression = syntax.NewExpression
_ObjectExpression = syntax.ObjectExpression
_ObjectPattern = syntax.ObjectPattern
_Program = syntax.Program
_Property = syntax.Property
_ReturnStatement = syntax.ReturnStatement
_SequenceExpression = syntax.SequenceExpression
_SpreadElement = syntax.SpreadElement
_SwitchCase = syntax.SwitchCase
_SwitchStatement = syntax.SwitchStatement
_TaggedTemplateExpression = syntax.TaggedTemplateExpression
_TemplateElement = syntax.TemplateElement
_TemplateLiteral = syntax.TemplateLiteral
_ThisExpression = syntax.ThisExpression
_ThrowStatement = syntax.ThrowStatement
_TryStatement = syntax.TryStatement
_UnaryExpression = syntax.UnaryExpression
_UpdateExpression = syntax.UpdateExpression
_VariableDeclaration = syntax.VariableDeclaration
_VariableDeclarator = syntax.VariableDeclarator
_WhileStatement = syntax.WhileStatement
_WithStatement = syntax.WithStatement
_YieldExpression = syntax.YieldExpression


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
                        when _ArrayExpression         then rv = @visitArrayExpression n
                        when _ArrayPattern            then rv = @visitArrayPattern n
                        when _ArrowFunctionExpression then rv = @visitArrowFunctionExpression n
                        when _AssignmentExpression    then rv = @visitAssignmentExpression n
                        when _BinaryExpression        then rv = @visitBinaryExpression n
                        when _BlockStatement          then rv = @visitBlock n
                        when _BreakStatement          then rv = @visitBreak n
                        when _CallExpression          then rv = @visitCallExpression n
                        when _CatchClause             then rv = @visitCatchClause n
                        when _ClassBody               then rv = @visitClassBody n
                        when _ClassDeclaration        then rv = @visitClassDeclaration n
                        when _ClassExpression         then throw new Error "Unhandled AST node type: #{n.type}"
                        when _ClassHeritage           then throw new Error "Unhandled AST node type: #{n.type}"
                        when _ComprehensionBlock      then throw new Error "Unhandled AST node type: #{n.type}"
                        when _ComprehensionExpression then throw new Error "Unhandled AST node type: #{n.type}"
                        when _ConditionalExpression   then rv = @visitConditionalExpression n
                        when _ContinueStatement       then rv = @visitContinue n
                        when _DebuggerStatement       then throw new Error "Unhandled AST node type: #{n.type}"
                        when _DoWhileStatement        then rv = @visitDo n
                        when _EmptyStatement          then rv = @visitEmptyStatement n
                        when _ExportDeclaration       then rv = @visitExportDeclaration n
                        when _ExportBatchSpecifier    then throw new Error "Unhandled AST node type: #{n.type}"
                        when _ExportSpecifier         then throw new Error "Unhandled AST node type: #{n.type}"
                        when _ExpressionStatement     then rv = @visitExpressionStatement n
                        when _ForInStatement          then rv = @visitForIn n
                        when _ForOfStatement          then rv = @visitForOf n
                        when _ForStatement            then rv = @visitFor n
                        when _FunctionDeclaration     then rv = @visitFunctionDeclaration n
                        when _FunctionExpression      then rv = @visitFunctionExpression n
                        when _Identifier              then rv = @visitIdentifier n
                        when _IfStatement             then rv = @visitIf n
                        when _ImportDeclaration       then rv = @visitImportDeclaration n
                        when _ImportSpecifier         then rv = @visitImportSpecifier n
                        when _LabeledStatement        then rv = @visitLabeledStatement n
                        when _Literal                 then rv = @visitLiteral n
                        when _LogicalExpression       then rv = @visitLogicalExpression n
                        when _MemberExpression        then rv = @visitMemberExpression n
                        when _MethodDefinition        then rv = @visitMethodDefinition n
                        when _ModuleDeclaration       then rv = @visitModuleDeclaration n
                        when _NewExpression           then rv = @visitNewExpression n
                        when _ObjectExpression        then rv = @visitObjectExpression n
                        when _ObjectPattern           then throw new Error "Unhandled AST node type: #{n.type}"
                        when _Program                 then rv = @visitProgram n
                        when _Property                then rv = @visitProperty n
                        when _ReturnStatement         then rv = @visitReturn n
                        when _SequenceExpression      then rv = @visitSequenceExpression n
                        when _SpreadElement           then throw new Error "Unhandled AST node type: #{n.type}"
                        when _SwitchCase              then rv = @visitCase n
                        when _SwitchStatement         then rv = @visitSwitch n
                        when _TaggedTemplateExpression then throw new Error "Unhandled AST node type: #{n.type}"
                        when _TemplateElement         then throw new Error "Unhandled AST node type: #{n.type}"
                        when _TemplateLiteral         then throw new Error "Unhandled AST node type: #{n.type}"
                        when _ThisExpression          then rv = @visitThisExpression n
                        when _ThrowStatement          then rv = @visitThrow n
                        when _TryStatement            then rv = @visitTry n
                        when _UnaryExpression         then rv = @visitUnaryExpression n
                        when _UpdateExpression        then rv = @visitUpdateExpression n
                        when _VariableDeclaration     then rv = @visitVariableDeclaration n
                        when _VariableDeclarator      then rv = @visitVariableDeclarator n
                        when _WhileStatement          then rv = @visitWhile n
                        when _WithStatement           then rv = @visitWith n
                        when _YieldExpression         then throw new Error "Unhandled AST node type: #{n.type}"
                        else
                            throw new Error "PANIC: unknown parse node type #{n.type}"
                
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
                
        visitForOf: (n) ->
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

        visitClassDeclaration: (n) ->
                n.body = @visit n.body
                n

        visitClassBody: (n) ->
                n.body = @visitArray n.body
                n

        visitMethodDefinition: (n) ->
                n.value = @visit n.value
                n

        visitModuleDeclaration: (n) ->
                n.id = @visit n.id
                n.body = @visit n.body
                n

        visitExportDeclaration: (n) ->
                n.declaration = @visit n.declaration
                n
                
        visitImportDeclaration: (n) ->
                n.specifiers = @visitArray n.specifiers
                n

        visitImportSpecifier: (n) ->
                n.id = @visit n.id
                n

        visitArrayPattern: (n) ->
                n.elements = @visitArray n.elements
                n

        toString: () -> "TreeVisitor"
