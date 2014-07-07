esprima = require 'esprima'
escodegen = require 'escodegen'

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
  ComputedPropertyKey,
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

isNotNull = (n) -> throw new Error("assertion failed: value is null or undefined") if not n?
hasType = (n) -> throw new Error("assertion failed: value #{JSON.stringify n} does not have a 'type:' property") if not n.type?

isast = (n) ->
        isNotNull(n)
        hasType(n)
        n
isnullableast = (n) ->
        hasType(n) if n?
        n

exports.arrayExpression = (els = []) -> type: ArrayExpression, elements: els
exports.arrayPattern
exports.arrowFunctionExpression
exports.assignmentExpression = (l, op, r) -> type: AssignmentExpression, operator: op, left: isast(l), right: isast(r)
exports.binaryExpression = (l, op, r) -> type: BinaryExpression, operator: op, left: isast(l), right: isast(r)
exports.blockStatement = (stmts = []) -> type: BlockStatement, body: stmts.map(isast)
exports.breakStatement = (label) -> type: BreakStatement, label: isast(label)
exports.callExpression = (callee, args = []) -> type: CallExpression, callee: isast(callee), arguments: args.map(isast)
exports.catchClause 
exports.classBody
exports.classDeclaration
exports.classExpression
exports.classHeritage
exports.comprehensionBlock
exports.comprehensionExpression
exports.conditionalExpression   = (test, consequent, alternate) -> type: ConditionalExpression, test: isast(test), consequent: isast(consequent), alternate: isast(alternate)
exports.continueStatement       = (label)                       -> type: ContinueStatement, label: isast(label)
exports.computedPropertyKey
exports.debuggerStatement
exports.doWhileStatement
exports.emptyStatement = -> type: EmptyStatement
exports.exportDeclaration
exports.exportBatchSpecifier
exports.exportSpecifier
exports.expressionStatement = (exp)                      -> type: ExpressionStatement, expression: isast(exp)
exports.forInStatement      = (left, right, body)        -> type: ForInStatement, left: isast(left), right: isast(right), body: isast(body)
exports.forOfStatement      = (left, right, body)        -> type: ForOfStatement, left: isast(left), right: isast(right), body: isast(body)
exports.forStatement        = (init, test, update, body) -> type: ForStatement, init: isast(init), test: isast(test), update: isast(update), body: isast(body)
exports.functionDeclaration = (id, params, body, defaults=[], rest=null) -> type: FunctionDeclaration, id: isast(id), params: params.map(isast), body: isast(body), defaults: defaults.map(isnullableast), rest: isnullableast(rest), generator: false, expression: false
exports.functionExpression  = (id, params, body, defaults=[], rest=null) -> type: FunctionExpression,  id: isnullableast(id), params: params.map(isast), body: isast(body), defaults: defaults.map(isnullableast), rest: isnullableast(rest), generator: false, expression: false
exports.identifier          = (name) -> type: Identifier, name: name
exports.ifStatement = (test, consequent, alternate) -> type: IfStatement, test: isast(test), consequent: isast(consequent), alternate: isast(alternate)
exports.importDeclaration
exports.importSpecifier
exports.labeledStatement   = (label, body) -> type: LabeledStatement, label: isast(label), body: body.map(isast)
exports.literal            = (val) ->
        raw = if typeof(val) is "string" then "\"#{val}\"" else "#{val}"
        type: Literal, value: val, raw: raw
exports.logicalExpression  = (l, op, r) -> type: LogicalExpression, left: isast(l), right: isast(r), operator: op
exports.memberExpression   = (obj, prop, computed = false) -> type: MemberExpression, object: isast(obj), property: isast(prop), computed: computed
exports.methodDefinition
exports.moduleDeclaration
exports.newExpression
exports.objectExpression = (properties) -> type: ObjectExpression, properties: properties.map(isast)
exports.objectPattern
exports.program
exports.property         = (key, value, kind = "init") -> type: Property, key: isast(key), value: isast(value), kind: kind
exports.returnStatement  = (arg) -> type: ReturnStatement, argument: isast(arg)
exports.sequenceExpression = (expressions) -> type: SequenceExpression, expressions: expressions.map(isast)
exports.spreadElement
exports.switchCase
exports.switchStatement
exports.taggedTemplateExpression
exports.templateElement
exports.templateLiteral
exports.thisExpression = -> type: ThisExpression
exports.throwStatement = (arg) -> type: ThrowStatement, argument: isast(arg)
exports.tryStatement
exports.unaryExpression = (op, arg) -> type: UnaryExpression, operator: op, argument: isast(arg)
exports.updateExpression
exports.variableDeclaration = (kind, rest...)           ->
        if Array.isArray rest[0]
                # we assume it's an array of declarators and use it as such
                type: VariableDeclaration, kind: kind, declarations: rest[0].map(isast)
        else
                # otherwise, we assume it's a list of repeating id+init pairs
                if rest.length % 2 isnt 0
                        throw new Error("variable declarations must have equal numbers of identifiers and initializers")
                decls = []
                while rest.length > 0
                        decls.push(exports.variableDeclarator(isast(rest.shift()), isast(rest.shift())))
                type: VariableDeclaration, kind: kind, declarations: decls

exports.letDeclaration = (rest...) -> exports.variableDeclaration("let", rest...)
exports.varDeclaration = (rest...) -> exports.variableDeclaration("var", rest...)
                
exports.variableDeclarator  = (id, init = undefined) -> type: VariableDeclarator, id: isast(id), init: init
exports.whileStatement      = (test, body) -> type: WhileStatement, test: isast(test), body: isast(body)
exports.withStatement
exports.yieldExpression

# ugh.  stupid escodegen
exports.undefined = -> exports.unaryExpression("void", exports.literal(0))
exports.null = -> exports.literal(null)
