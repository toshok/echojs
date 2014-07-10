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

let isNotNull = (n) => { if (!n) throw new Error("assertion failed: value is null or undefined"); return true;};
let hasType = (n) => { if (!n.type) throw new Error(`assertion failed: value ${JSON.stringify(n)} does not have a 'type:' property`); return true; };

function isast (n) {
    return (isNotNull(n) &&
	        hasType(n) &&
	        n);
}

function isnullableast (n) {
    return ((!n || hasType(n)) &&
	        n);
}

// esprima currently fails to parse arrow functions with default argument values, so we make those function expressions

export let arrayExpression      = function (els = []) { return { type: ArrayExpression, elements: els }; };
export let assignmentExpression = (l, op, r) => ({ type: AssignmentExpression, operator: op, left: isast(l), right: isast(r) });
export let binaryExpression     = (l, op, r) => ({ type: BinaryExpression, operator: op, left: isast(l), right: isast(r) });
export let blockStatement       = function (stmts = []) { return { type: BlockStatement, body: stmts.map(isast) }; };
export let breakStatement       = (label) => ({ type: BreakStatement, label: isast(label) });
export let callExpression       = (callee, args = []) => ({ type: CallExpression, callee: isast(callee), arguments: args.map(isast)});
export let conditionalExpression   = (test, consequent, alternate) => ({ type: ConditionalExpression, test: isast(test), consequent: isast(consequent), alternate: isast(alternate) });
export let continueStatement       = (label)                       => ({ type: ContinueStatement, label: isast(label) });
export let emptyStatement          = () => ({ type: EmptyStatement });
export let expressionStatement = (exp)                      => ({ type: ExpressionStatement, expression: isast(exp) });
export let forInStatement      = (left, right, body)        => ({ type: ForInStatement, left: isast(left), right: isast(right), body: isast(body) });
export let forOfStatement      = (left, right, body)        => ({ type: ForOfStatement, left: isast(left), right: isast(right), body: isast(body) });
export let forStatement        = (init, test, update, body) => ({ type: ForStatement, init: isast(init), test: isast(test), update: isast(update), body: isast(body) });
export let functionDeclaration = (id, params, body, defaults=[], rest=null) => ({ type: FunctionDeclaration, id: isast(id), params: params.map(isast), body: isast(body), defaults: defaults.map(isnullableast), rest: isnullableast(rest), generator: false, expression: false });
export let functionExpression  = (id, params, body, defaults=[], rest=null) => ({ type: FunctionExpression,  id: isnullableast(id), params: params.map(isast), body: isast(body), defaults: defaults.map(isnullableast), rest: isnullableast(rest), generator: false, expression: false });
export let identifier          = (name) => ({ type: Identifier, name: name });
export let ifStatement = (test, consequent, alternate) => ({ type: IfStatement, test: isast(test), consequent: isast(consequent), alternate: isast(alternate) });
export let labeledStatement   = (label, body) => ({ type: LabeledStatement, label: isast(label), body: body.map(isast) });
export let literal            = (val) => ({ type: Literal, value: val, raw: typeof(val) === "string" ? `\"${val}\"` : `${val}` });
export let logicalExpression  = (l, op, r) => ({ type: LogicalExpression, left: isast(l), right: isast(r), operator: op });
export let memberExpression   = (obj, prop, computed = false) => ({ type: MemberExpression, object: isast(obj), property: isast(prop), computed: computed });
export let objectExpression = (properties) => ({ type: ObjectExpression, properties: properties.map(isast) });
export let property         = (key, value, kind = "init") => ({ type: Property, key: isast(key), value: isast(value), kind: kind });
export let returnStatement  = (arg) => ({ type: ReturnStatement, argument: isast(arg) });
export let sequenceExpression = (expressions) => ({ type: SequenceExpression, expressions: expressions.map(isast) });
export let thisExpression = () => ({ type: ThisExpression });
export let throwStatement = (arg) => ({ type: ThrowStatement, argument: isast(arg) });
export let unaryExpression = (op, arg) => ({ type: UnaryExpression, operator: op, argument: isast(arg) });
export let variableDeclaration = (kind, ...rest)           => {
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
            decls.push(variableDeclarator(isast(rest.shift()), isast(rest.shift())));
		    return { type: VariableDeclaration, kind: kind, declarations: decls };
	    }
	}
};
export let letDeclaration = (...rest) => variableDeclaration("let", ...rest);
export let varDeclaration = (...rest) => variableDeclaration("var", ...rest);

export let variableDeclarator  = (id, init = undefined) => ({ type: VariableDeclarator, id: isast(id), init: init });
export let whileStatement      = (test, body) => ({ type: WhileStatement, test: isast(test), body: isast(body) });

export let undefinedLit = () => unaryExpression("void", literal(0));
export let nullLit = () => literal(null);
