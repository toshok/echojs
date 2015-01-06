/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

export const ArrayExpression = 'ArrayExpression';
export const ArrayPattern = 'ArrayPattern';
export const ArrowFunctionExpression = 'ArrowFunctionExpression';
export const AssignmentExpression = 'AssignmentExpression';
export const BinaryExpression = 'BinaryExpression';
export const BlockStatement = 'BlockStatement';
export const BreakStatement = 'BreakStatement';
export const CallExpression = 'CallExpression';
export const CatchClause = 'CatchClause';
export const ClassBody = 'ClassBody';
export const ClassDeclaration = 'ClassDeclaration';
export const ClassExpression = 'ClassExpression';
export const ClassHeritage = 'ClassHeritage';
export const ComprehensionBlock = 'ComprehensionBlock';
export const ComprehensionExpression = 'ComprehensionExpression';
export const ComputedPropertyKey = 'ComputedPropertyKey';
export const ConditionalExpression = 'ConditionalExpression';
export const ContinueStatement = 'ContinueStatement';
export const DebuggerStatement = 'DebuggerStatement';
export const DoWhileStatement = 'DoWhileStatement';
export const EmptyStatement = 'EmptyStatement';
export const ExportDeclaration = 'ExportDeclaration';
export const ExportBatchSpecifier = 'ExportBatchSpecifier';
export const ExportSpecifier = 'ExportSpecifier';
export const ExpressionStatement = 'ExpressionStatement';
export const ForInStatement = 'ForInStatement';
export const ForOfStatement = 'ForOfStatement';
export const ForStatement = 'ForStatement';
export const FunctionDeclaration = 'FunctionDeclaration';
export const FunctionExpression = 'FunctionExpression';
export const Identifier = 'Identifier';
export const IfStatement = 'IfStatement';
export const ImportDeclaration = 'ImportDeclaration';
export const ImportSpecifier = 'ImportSpecifier';
export const LabeledStatement = 'LabeledStatement';
export const Literal = 'Literal';
export const LogicalExpression = 'LogicalExpression';
export const MemberExpression = 'MemberExpression';
export const MethodDefinition = 'MethodDefinition';
export const ModuleDeclaration = 'ModuleDeclaration';
export const NewExpression = 'NewExpression';
export const ObjectExpression = 'ObjectExpression';
export const ObjectPattern = 'ObjectPattern';
export const Program = 'Program';
export const Property = 'Property';
export const ReturnStatement = 'ReturnStatement';
export const SequenceExpression = 'SequenceExpression';
export const SpreadElement = 'SpreadElement';
export const SwitchCase = 'SwitchCase';
export const SwitchStatement = 'SwitchStatement';
export const TaggedTemplateExpression = 'TaggedTemplateExpression';
export const TemplateElement = 'TemplateElement';
export const TemplateLiteral = 'TemplateLiteral';
export const ThisExpression = 'ThisExpression';
export const ThrowStatement = 'ThrowStatement';
export const TryStatement = 'TryStatement';
export const UnaryExpression = 'UnaryExpression';
export const UpdateExpression = 'UpdateExpression';
export const VariableDeclaration = 'VariableDeclaration';
export const VariableDeclarator = 'VariableDeclarator';
export const WhileStatement = 'WhileStatement';
export const WithStatement = 'WithStatement';
export const YieldExpression = 'YieldExpression';

function isNotNull(n) { if (!n) throw new Error("assertion failed: value is null or undefined"); return true; };
function hasType(n)   { if (!n.type) throw new Error(`assertion failed: value ${JSON.stringify(n)} does not have a 'type:' property`); return true; };

function isast (n) {
    return (isNotNull(n) &&
	        hasType(n) &&
	        n);
}

function isnullableast (n) {
    return ((!n || hasType(n)) &&
	        n);
}

function isastarray (n) {
    if (!Array.isArray(n)) throw new Error("value must be an array");
    for (let el of n)
        isast(el);
    return n;
}

function isnullableastarray (n) {
    if (!Array.isArray(n)) throw new Error("value must be an array");
    for (let el of n)
        isnullableast(el);
    return n;
}

// esprima currently fails to parse arrow functions with default argument values, so we make those function expressions

export function arrayExpression       (els = []) { return { type: ArrayExpression, elements: els }; }
export function assignmentExpression  (l, op, r) { return { type: AssignmentExpression, operator: op, left: isast(l), right: isast(r) }; }
export function binaryExpression      (l, op, r) { return { type: BinaryExpression, operator: op, left: isast(l), right: isast(r) }; }
export function blockStatement        (stmts = []) { return { type: BlockStatement, body: stmts.map(isast) }; }
export function breakStatement        (label) { return { type: BreakStatement, label: isast(label) }; }
export function callExpression        (callee, args = []) { return { type: CallExpression, callee: isast(callee), arguments: args.map(isast)}; }
export function catchClause           (param, body, guard) { return { type: CatchClause, body: body, param: isast(param), guard: isnullableast(guard) }; }
export function conditionalExpression (test, consequent, alternate) { return { type: ConditionalExpression, test: isast(test), consequent: isast(consequent), alternate: isast(alternate) }; }
export function continueStatement     (label)                       { return { type: ContinueStatement, label: isnullableast(label) }; }
export function emptyStatement        () { return { type: EmptyStatement }; }
export function expressionStatement   (exp)                      { return { type: ExpressionStatement, expression: isast(exp) }; }
export function forInStatement        (left, right, body)        { return { type: ForInStatement, left: isast(left), right: isast(right), body: isast(body) }; }
export function forOfStatement        (left, right, body)        { return { type: ForOfStatement, left: isast(left), right: isast(right), body: isast(body) }; }
export function forStatement          (init, test, update, body) { return { type: ForStatement, init: isast(init), test: isast(test), update: isast(update), body: isast(body) }; }
export function functionDeclaration   (id, params, body, defaults=[], rest=null) { return { type: FunctionDeclaration, id: isast(id), params: params.map(isast), body: isast(body), defaults: defaults.map(isnullableast), rest: isnullableast(rest), generator: false, expression: false }; }
export function functionExpression    (id, params, body, defaults=[], rest=null) { return { type: FunctionExpression,  id: isnullableast(id), params: params.map(isast), body: isast(body), defaults: defaults.map(isnullableast), rest: isnullableast(rest), generator: false, expression: false }; }
export function identifier            (name) { return { type: Identifier, name: name }; }
export function ifStatement           (test, consequent, alternate) { return { type: IfStatement, test: isast(test), consequent: isast(consequent), alternate: isnullableast(alternate) }; }
export function labeledStatement      (label, body) { return { type: LabeledStatement, label: isast(label), body: body.map(isast) }; }
export function literal               (val) { return { type: Literal, value: val, raw: typeof(val) === "string" ? `\"${val}\"` : `${val}` }; }
export function logicalExpression     (l, op, r) { return { type: LogicalExpression, left: isast(l), right: isast(r), operator: op }; }
export function memberExpression      (obj, prop, computed = false) { return { type: MemberExpression, object: isast(obj), property: isast(prop), computed: computed }; }
export function methodDefinition      (key, value, kind = "init") { return { type: MethodDefinition, key: key, value: value, kind: kind }; }
export function objectExpression      (properties) { return { type: ObjectExpression, properties: properties.map(isast) }; }
export function property              (key, value, kind = "init") { return { type: Property, key: isast(key), value: isast(value), kind: kind }; }
export function returnStatement       (arg) { return { type: ReturnStatement, argument: isast(arg) }; }
export function sequenceExpression    (expressions) { return { type: SequenceExpression, expressions: expressions.map(isast) }; }
export function spreadElement         (arg) { return { type: SpreadElement, argument: arg }; }
export function switchCase            (test, consequent) { return { type: SwitchCase, test: isnullableast(test), consequent: isastarray(consequent) }; }
export function thisExpression        () { return { type: ThisExpression }; }
export function throwStatement        (arg) { return { type: ThrowStatement, argument: isast(arg) }; }
export function tryStatement          (block, handlers, finalizer) { return { type: TryStatement, block: block, handlers: handlers, guardedHandlers: [], finalizer: finalizer }; }
export function unaryExpression       (op, arg) { return { type: UnaryExpression, operator: op, argument: isast(arg) }; }
export function variableDeclaration   (kind, ...rest) {
    if (Array.isArray(rest[0])) {
        // we assume it's an array of declarators and use it as such
        return { type: VariableDeclaration, kind: kind, declarations: rest[0].map(isast) };
	}
    else {
        // otherwise, we assume it's a list of repeating id+init pairs
        if (rest.length % 2 !== 0)
            throw new Error("variable declarations must have equal numbers of identifiers and initializers");
        let decls = [];
        while (rest.length > 0) {
            decls.push(variableDeclarator(isast(rest.shift()), isnullableast(rest.shift())));
        }
		return { type: VariableDeclaration, kind: kind, declarations: decls };
	}
};
export function constDeclaration (...rest) { return variableDeclaration("const", ...rest); }
export function letDeclaration   (...rest) { return variableDeclaration("let", ...rest); }
export function varDeclaration   (...rest) { return variableDeclaration("var", ...rest); }

export function variableDeclarator (id, init = undefined) { return { type: VariableDeclarator, id: isast(id), init: init }; }
export function whileStatement     (test, body) { return { type: WhileStatement, test: isast(test), body: isast(body) }; }

export function undefinedLit () { return unaryExpression("void", literal(0)); }
export function nullLit      () { return literal(null); }
