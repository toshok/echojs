esprima = require 'esprima'
debug = require 'debug'

{ ArrayExpression,
  ArrayPattern,
  ArrowFunctionExpression,
  AssignmentExpression,
  BinaryExpression,
  BlockStatement,
  BreakStatement,
  CallExpression,
  CatchClause,
  ClassBody,
  ClassDeclaration,
  ClassExpression,
  ClassHeritage,
  ComprehensionBlock,
  ComprehensionExpression,
  ConditionalExpression,
  ContinueStatement,
  DebuggerStatement,
  DoWhileStatement,
  EmptyStatement,
  ExportDeclaration,
  ExportBatchSpecifier,
  ExportSpecifier,
  ExpressionStatement,
  ForInStatement,
  ForOfStatement,
  ForStatement,
  FunctionDeclaration,
  FunctionExpression,
  Identifier,
  IfStatement,
  ImportDeclaration,
  ImportSpecifier,
  LabeledStatement,
  Literal,
  LogicalExpression,
  MemberExpression,
  MethodDefinition,
  ModuleDeclaration,
  NewExpression,
  ObjectExpression,
  ObjectPattern,
  Program,
  Property,
  ReturnStatement,
  SequenceExpression,
  SpreadElement,
  SwitchCase,
  SwitchStatement,
  TaggedTemplateExpression,
  TemplateElement,
  TemplateLiteral,
  ThisExpression,
  ThrowStatement,
  TryStatement,
  UnaryExpression,
  UpdateExpression,
  VariableDeclaration,
  VariableDeclarator,
  WhileStatement,
  WithStatement,
  YieldExpression } = esprima.Syntax

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
                        when ArrayExpression         then rv = @visitArrayExpression n
                        when ArrayPattern            then rv = @visitArrayPattern n
                        when ArrowFunctionExpression then rv = @visitArrowFunctionExpression n
                        when AssignmentExpression    then rv = @visitAssignmentExpression n
                        when BinaryExpression        then rv = @visitBinaryExpression n
                        when BlockStatement          then rv = @visitBlock n
                        when BreakStatement          then rv = @visitBreak n
                        when CallExpression          then rv = @visitCallExpression n
                        when CatchClause             then rv = @visitCatchClause n
                        when ClassBody               then rv = @visitClassBody n
                        when ClassDeclaration        then rv = @visitClassDeclaration n
                        when ClassExpression         then throw new Error "Unhandled AST node type: #{n.type}"
                        when ClassHeritage           then throw new Error "Unhandled AST node type: #{n.type}"
                        when ComprehensionBlock      then throw new Error "Unhandled AST node type: #{n.type}"
                        when ComprehensionExpression then throw new Error "Unhandled AST node type: #{n.type}"
                        when ConditionalExpression   then rv = @visitConditionalExpression n
                        when ContinueStatement       then rv = @visitContinue n
                        when DebuggerStatement       then throw new Error "Unhandled AST node type: #{n.type}"
                        when DoWhileStatement        then rv = @visitDo n
                        when EmptyStatement          then rv = @visitEmptyStatement n
                        when ExportDeclaration       then rv = @visitExportDeclaration n
                        when ExportBatchSpecifier    then throw new Error "Unhandled AST node type: #{n.type}"
                        when ExportSpecifier         then throw new Error "Unhandled AST node type: #{n.type}"
                        when ExpressionStatement     then rv = @visitExpressionStatement n
                        when ForInStatement          then rv = @visitForIn n
                        when ForOfStatement          then rv = @visitForOf n
                        when ForStatement            then rv = @visitFor n
                        when FunctionDeclaration     then rv = @visitFunctionDeclaration n
                        when FunctionExpression      then rv = @visitFunctionExpression n
                        when Identifier              then rv = @visitIdentifier n
                        when IfStatement             then rv = @visitIf n
                        when ImportDeclaration       then rv = @visitImportDeclaration n
                        when ImportSpecifier         then rv = @visitImportSpecifier n
                        when LabeledStatement        then rv = @visitLabeledStatement n
                        when Literal                 then rv = @visitLiteral n
                        when LogicalExpression       then rv = @visitLogicalExpression n
                        when MemberExpression        then rv = @visitMemberExpression n
                        when MethodDefinition        then rv = @visitMethodDefinition n
                        when ModuleDeclaration       then rv = @visitModuleDeclaration n
                        when NewExpression           then rv = @visitNewExpression n
                        when ObjectExpression        then rv = @visitObjectExpression n
                        when ObjectPattern           then rv = @visitObjectPattern n
                        when Program                 then rv = @visitProgram n
                        when Property                then rv = @visitProperty n
                        when ReturnStatement         then rv = @visitReturn n
                        when SequenceExpression      then rv = @visitSequenceExpression n
                        when SpreadElement           then throw new Error "Unhandled AST node type: #{n.type}"
                        when SwitchCase              then rv = @visitCase n
                        when SwitchStatement         then rv = @visitSwitch n
                        when TaggedTemplateExpression then throw new Error "Unhandled AST node type: #{n.type}"
                        when TemplateElement         then throw new Error "Unhandled AST node type: #{n.type}"
                        when TemplateLiteral         then throw new Error "Unhandled AST node type: #{n.type}"
                        when ThisExpression          then rv = @visitThisExpression n
                        when ThrowStatement          then rv = @visitThrow n
                        when TryStatement            then rv = @visitTry n
                        when UnaryExpression         then rv = @visitUnaryExpression n
                        when UpdateExpression        then rv = @visitUpdateExpression n
                        when VariableDeclaration     then rv = @visitVariableDeclaration n
                        when VariableDeclarator      then rv = @visitVariableDeclarator n
                        when WhileStatement          then rv = @visitWhile n
                        when WithStatement           then rv = @visitWith n
                        when YieldExpression         then throw new Error "Unhandled AST node type: #{n.type}"
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

        visitObjectPattern: (n) ->
                n.properties = @visitArray n.properties
                n

        toString: () -> "TreeVisitor"
