/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// Typed constructors for the compiler's ESTree dialect (see estree.ts),
// plus the node-type string constants the passes switch over.

import type * as e from "./estree";

export const ArrayExpression = "ArrayExpression" as const;
export const ArrayPattern = "ArrayPattern" as const;
export const AssignmentPattern = "AssignmentPattern" as const;
export const ArrowFunctionExpression = "ArrowFunctionExpression" as const;
export const AssignmentExpression = "AssignmentExpression" as const;
export const BinaryExpression = "BinaryExpression" as const;
export const BlockStatement = "BlockStatement" as const;
export const BreakStatement = "BreakStatement" as const;
export const CallExpression = "CallExpression" as const;
export const CatchClause = "CatchClause" as const;
export const ClassBody = "ClassBody" as const;
export const ClassDeclaration = "ClassDeclaration" as const;
export const ClassExpression = "ClassExpression" as const;
export const ClassHeritage = "ClassHeritage" as const;
export const ComprehensionBlock = "ComprehensionBlock" as const;
export const ComprehensionExpression = "ComprehensionExpression" as const;
export const ConditionalExpression = "ConditionalExpression" as const;
export const ContinueStatement = "ContinueStatement" as const;
export const DebuggerStatement = "DebuggerStatement" as const;
export const DoWhileStatement = "DoWhileStatement" as const;
export const EmptyStatement = "EmptyStatement" as const;
export const ExportAllDeclaration = "ExportAllDeclaration" as const;
export const ExportDefaultDeclaration = "ExportDefaultDeclaration" as const;
export const ExportNamedDeclaration = "ExportNamedDeclaration" as const;
export const ExportSpecifier = "ExportSpecifier" as const;
export const ExpressionStatement = "ExpressionStatement" as const;
export const ForInStatement = "ForInStatement" as const;
export const ForOfStatement = "ForOfStatement" as const;
export const ForStatement = "ForStatement" as const;
export const FunctionDeclaration = "FunctionDeclaration" as const;
export const FunctionExpression = "FunctionExpression" as const;
export const Identifier = "Identifier" as const;
export const IfStatement = "IfStatement" as const;
export const ImportDeclaration = "ImportDeclaration" as const;
export const ImportSpecifier = "ImportSpecifier" as const;
export const ImportDefaultSpecifier = "ImportDefaultSpecifier" as const;
export const ImportNamespaceSpecifier = "ImportNamespaceSpecifier" as const;
export const LabeledStatement = "LabeledStatement" as const;
export const Literal = "Literal" as const;
export const LogicalExpression = "LogicalExpression" as const;
export const MemberExpression = "MemberExpression" as const;
export const MetaProperty = "MetaProperty" as const;
export const MethodDefinition = "MethodDefinition" as const;
export const ModuleDeclaration = "ModuleDeclaration" as const;
export const NewExpression = "NewExpression" as const;
export const ObjectExpression = "ObjectExpression" as const;
export const ObjectPattern = "ObjectPattern" as const;
export const Program = "Program" as const;
export const Property = "Property" as const;
export const RestElement = "RestElement" as const;
export const ReturnStatement = "ReturnStatement" as const;
export const SequenceExpression = "SequenceExpression" as const;
export const SpreadElement = "SpreadElement" as const;
export const Super = "Super" as const;
export const SwitchCase = "SwitchCase" as const;
export const SwitchStatement = "SwitchStatement" as const;
export const TaggedTemplateExpression = "TaggedTemplateExpression" as const;
export const TemplateElement = "TemplateElement" as const;
export const TemplateLiteral = "TemplateLiteral" as const;
export const ThisExpression = "ThisExpression" as const;
export const ThrowStatement = "ThrowStatement" as const;
export const TryStatement = "TryStatement" as const;
export const UnaryExpression = "UnaryExpression" as const;
export const UpdateExpression = "UpdateExpression" as const;
export const VariableDeclaration = "VariableDeclaration" as const;
export const VariableDeclarator = "VariableDeclarator" as const;
export const WhileStatement = "WhileStatement" as const;
export const WithStatement = "WithStatement" as const;
export const YieldExpression = "YieldExpression" as const;

export function arrayExpression(
    els: (e.Expression | e.SpreadElement | null)[] = []
): e.ArrayExpression {
    return { type: ArrayExpression, elements: els };
}

export function arrowFunctionExpression(
    params: e.Pattern[],
    body: e.BlockStatement | e.Expression,
    defaults: (e.Expression | null)[] = [],
    expression = false
): e.ArrowFunctionExpression {
    return {
        type: ArrowFunctionExpression,
        id: null,
        params,
        defaults,
        body,
        generator: false,
        expression,
    };
}

export function assignmentExpression(
    l: e.Expression | e.Pattern,
    op: e.AssignmentOperator,
    r: e.Expression
): e.AssignmentExpression {
    return { type: AssignmentExpression, operator: op, left: l, right: r };
}

export function binaryExpression(
    l: e.Expression,
    op: e.BinaryOperator,
    r: e.Expression
): e.BinaryExpression {
    return { type: BinaryExpression, operator: op, left: l, right: r };
}

export function blockStatement(
    stmts: e.Statement[] = [],
    loc: e.SourceLocation | null = null
): e.BlockStatement {
    return { type: BlockStatement, body: stmts, loc };
}

export function breakStatement(label: e.Identifier | null = null): e.BreakStatement {
    return { type: BreakStatement, label };
}

export function callExpression(
    callee: e.Expression | e.Super,
    args: (e.Expression | e.SpreadElement)[] = []
): e.CallExpression {
    return { type: CallExpression, callee, arguments: args };
}

export function catchClause(
    param: e.Pattern,
    body: e.BlockStatement,
    guard: e.Expression | null = null
): e.CatchClause {
    return { type: CatchClause, body, param, guard };
}

export function conditionalExpression(
    test: e.Expression,
    consequent: e.Expression,
    alternate: e.Expression
): e.ConditionalExpression {
    return { type: ConditionalExpression, test, consequent, alternate };
}

export function continueStatement(label: e.Identifier | null = null): e.ContinueStatement {
    return { type: ContinueStatement, label };
}

export function emptyStatement(): e.EmptyStatement {
    return { type: EmptyStatement };
}

export function expressionStatement(exp: e.Expression): e.ExpressionStatement {
    return { type: ExpressionStatement, expression: exp };
}

export function forInStatement(
    left: e.VariableDeclaration | e.Pattern,
    right: e.Expression,
    body: e.Statement
): e.ForInStatement {
    return { type: ForInStatement, left, right, body };
}

export function forOfStatement(
    left: e.VariableDeclaration | e.Pattern,
    right: e.Expression,
    body: e.Statement
): e.ForOfStatement {
    return { type: ForOfStatement, left, right, body };
}

export function forStatement(
    init: e.VariableDeclaration | e.Expression | null,
    test: e.Expression | null,
    update: e.Expression | null,
    body: e.Statement
): e.ForStatement {
    return { type: ForStatement, init, test, update, body };
}

export function functionDeclaration(
    id: e.Identifier,
    params: e.Pattern[],
    body: e.BlockStatement,
    defaults: (e.Expression | null)[] = []
): e.FunctionDeclaration {
    return {
        type: FunctionDeclaration,
        id,
        params,
        body,
        defaults,
        generator: false,
        expression: false,
    };
}

export function functionExpression(
    id: e.Identifier | null,
    params: e.Pattern[],
    body: e.BlockStatement,
    defaults: (e.Expression | null)[] = []
): e.FunctionExpression {
    return {
        type: FunctionExpression,
        id,
        params,
        body,
        defaults,
        generator: false,
        expression: false,
    };
}

export function identifier(name: string): e.Identifier {
    return { type: Identifier, name };
}

export function ifStatement(
    test: e.Expression,
    consequent: e.Statement,
    alternate: e.Statement | null = null
): e.IfStatement {
    return { type: IfStatement, test, consequent, alternate };
}

export function labeledStatement(label: e.Identifier, body: e.Statement): e.LabeledStatement {
    return { type: LabeledStatement, label, body };
}

export function literal(val: string | number | boolean | null): e.Literal {
    return {
        type: Literal,
        value: val,
        raw: typeof val === "string" ? `'${val}'` : `${val}`,
    };
}

export function logicalExpression(
    l: e.Expression,
    op: "||" | "&&",
    r: e.Expression
): e.LogicalExpression {
    return { type: LogicalExpression, left: l, right: r, operator: op };
}

export function memberExpression(
    obj: e.Expression | e.Super,
    prop: e.Expression,
    computed = false
): e.MemberExpression {
    return { type: MemberExpression, object: obj, property: prop, computed };
}

export function metaProperty(meta: e.Identifier, property: e.Identifier): e.MetaProperty {
    return { type: MetaProperty, meta, property };
}

export function methodDefinition(
    key: e.Expression,
    value: e.FunctionExpression,
    kind: e.MethodDefinition["kind"] = "init"
): e.MethodDefinition {
    return { type: MethodDefinition, key, value, kind };
}

export function objectExpression(properties: e.Property[]): e.ObjectExpression {
    return { type: ObjectExpression, properties };
}

export function property(
    key: e.Expression,
    value: e.Expression | e.Pattern,
    kind: e.Property["kind"] = "init",
    computed = false
): e.Property {
    return { type: Property, key, value, kind, computed };
}

export function restElement(arg: e.Pattern): e.RestElement {
    return { type: RestElement, argument: arg };
}

export function returnStatement(arg: e.Expression | null): e.ReturnStatement {
    return { type: ReturnStatement, argument: arg };
}

export function sequenceExpression(expressions: e.Expression[]): e.SequenceExpression {
    return { type: SequenceExpression, expressions };
}

export function spreadElement(arg: e.Expression): e.SpreadElement {
    return { type: SpreadElement, argument: arg };
}

export function superExpression(): e.Super {
    return { type: Super };
}

export function switchCase(test: e.Expression | null, consequent: e.Statement[]): e.SwitchCase {
    return { type: SwitchCase, test, consequent };
}

export function thisExpression(): e.ThisExpression {
    return { type: ThisExpression };
}

export function throwStatement(arg: e.Expression): e.ThrowStatement {
    return { type: ThrowStatement, argument: arg };
}

export function tryStatement(
    block: e.BlockStatement,
    handlers: e.CatchClause[],
    finalizer: e.BlockStatement | null = null
): e.TryStatement {
    return { type: TryStatement, block, handlers, guardedHandlers: [], finalizer };
}

export function unaryExpression(
    op: e.UnaryExpression["operator"],
    arg: e.Expression
): e.UnaryExpression {
    return { type: UnaryExpression, operator: op, argument: arg };
}

type DeclPair = [e.Pattern, e.Expression | null];

// two call shapes: an array of declarators, or alternating id+init
// arguments (id1, init1, id2, init2, ...)
export function variableDeclaration(
    kind: e.VariableDeclaration["kind"],
    declarations: e.VariableDeclarator[]
): e.VariableDeclaration;
export function variableDeclaration(
    kind: e.VariableDeclaration["kind"],
    ...pairs: (e.Pattern | e.Expression | null)[]
): e.VariableDeclaration;
export function variableDeclaration(
    kind: e.VariableDeclaration["kind"],
    ...rest: (e.VariableDeclarator[] | e.Pattern | e.Expression | null)[]
): e.VariableDeclaration {
    const first = rest[0];
    if (Array.isArray(first)) {
        return { type: VariableDeclaration, kind, declarations: first };
    }
    if (rest.length % 2 !== 0)
        throw new Error(
            "variable declarations must have equal numbers of identifiers and initializers"
        );
    const decls: e.VariableDeclarator[] = [];
    for (let i = 0; i < rest.length; i += 2) {
        const id = rest[i] as e.Pattern;
        const init = (rest[i + 1] as e.Expression | null) ?? null;
        decls.push(variableDeclarator(id, init));
    }
    return { type: VariableDeclaration, kind, declarations: decls };
}

export function constDeclaration(
    ...rest: (e.Pattern | e.Expression | null)[]
): e.VariableDeclaration {
    return variableDeclaration("const", ...rest);
}

export function letDeclaration(
    ...rest: (e.Pattern | e.Expression | null)[]
): e.VariableDeclaration {
    return variableDeclaration("let", ...rest);
}

export function varDeclaration(
    ...rest: (e.Pattern | e.Expression | null)[]
): e.VariableDeclaration {
    return variableDeclaration("var", ...rest);
}

export function variableDeclarator(
    id: e.Pattern,
    init: e.Expression | null | undefined = undefined
): e.VariableDeclarator {
    return { type: VariableDeclarator, id, init };
}

export function whileStatement(test: e.Expression, body: e.Statement): e.WhileStatement {
    return { type: WhileStatement, test, body };
}

export function undefinedLit(): e.UnaryExpression {
    return unaryExpression("void", literal(0));
}

export function nullLit(): e.Literal {
    return literal(null);
}
