/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// The compiler's ESTree dialect: what the esprima fork produces, plus
// the properties our passes hang off the nodes.  Dialect notes:
//   - functions carry `defaults` (old-esprima parameter defaults) and
//     may carry `rest`;
//   - TryStatement has `handlers` (an array) and `guardedHandlers`;
//   - CatchClause has a SpiderMonkey-era `guard`;
//   - gather-imports adds `source_path` to import/export declarations;
//   - EIR integration tags the toplevel function with eir_module /
//     eir_main / ir_func and friends.

import type { EjsFunction, DISubprogram } from "@llvm";
import type { Module as EIRModule } from "./eir/ir";

export interface Position {
    line: number;
    column: number;
}

export interface SourceLocation {
    start: Position;
    end?: Position;
}

interface BaseNode {
    loc?: SourceLocation | null;
}

// --- expressions ------------------------------------------------------------

export interface ArrayExpression extends BaseNode {
    type: "ArrayExpression";
    // elisions (holes) are null elements
    elements: (Expression | SpreadElement | null)[];
}

export interface ObjectExpression extends BaseNode {
    type: "ObjectExpression";
    properties: Property[];
}

export interface Property extends BaseNode {
    type: "Property";
    key: Expression;
    value: Expression | Pattern;
    kind: "init" | "get" | "set";
    computed: boolean;
    method?: boolean;
    shorthand?: boolean;
}

export interface Identifier extends BaseNode {
    type: "Identifier";
    name: string;
}

export interface Literal extends BaseNode {
    type: "Literal";
    value: string | number | boolean | null | RegExp;
    raw?: string;
}

export interface TemplateLiteral extends BaseNode {
    type: "TemplateLiteral";
    quasis: TemplateElement[];
    expressions: Expression[];
}

export interface TemplateElement extends BaseNode {
    type: "TemplateElement";
    value: { cooked: string; raw: string };
    tail: boolean;
}

export interface TaggedTemplateExpression extends BaseNode {
    type: "TaggedTemplateExpression";
    tag: Expression;
    quasi: TemplateLiteral;
}

export interface FunctionBase extends BaseNode {
    id: Identifier | null;
    params: Pattern[];
    defaults: (Expression | null)[];
    rest?: Identifier | null;
    body: BlockStatement | Expression;
    generator: boolean;
    expression: boolean;
    // --- compiler extensions -------------------------------------------------
    // set by insert_toplevel_func on the synthetic module toplevel
    toplevel?: boolean;
    displayName?: string;
    // set by collectEIRToplevel
    eir_module?: EIRModule;
    eir_main?: string;
    // set by compile() for the toplevel wrapper
    ir_name?: string;
    ir_func?: EjsFunction & { debug_info?: DISubprogram };
}

export interface FunctionDeclaration extends FunctionBase {
    type: "FunctionDeclaration";
    id: Identifier;
    body: BlockStatement;
}

export interface FunctionExpression extends FunctionBase {
    type: "FunctionExpression";
    body: BlockStatement;
}

export interface ArrowFunctionExpression extends FunctionBase {
    type: "ArrowFunctionExpression";
}

export interface UnaryExpression extends BaseNode {
    type: "UnaryExpression";
    operator: "-" | "+" | "!" | "~" | "typeof" | "void" | "delete";
    prefix?: boolean;
    argument: Expression;
}

export interface UpdateExpression extends BaseNode {
    type: "UpdateExpression";
    operator: "++" | "--";
    argument: Expression;
    prefix: boolean;
}

export type BinaryOperator =
    | "==" | "!=" | "===" | "!=="
    | "<" | "<=" | ">" | ">="
    | "<<" | ">>" | ">>>"
    | "+" | "-" | "*" | "/" | "%"
    | "|" | "^" | "&"
    | "in" | "instanceof";

export interface BinaryExpression extends BaseNode {
    type: "BinaryExpression";
    operator: BinaryOperator;
    left: Expression;
    right: Expression;
}

export type AssignmentOperator =
    | "=" | "+=" | "-=" | "*=" | "/=" | "%="
    | "<<=" | ">>=" | ">>>=" | "|=" | "^=" | "&=";

export interface AssignmentExpression extends BaseNode {
    type: "AssignmentExpression";
    operator: AssignmentOperator;
    left: Expression | Pattern;
    right: Expression;
}

export interface LogicalExpression extends BaseNode {
    type: "LogicalExpression";
    operator: "||" | "&&";
    left: Expression;
    right: Expression;
}

export interface MemberExpression extends BaseNode {
    type: "MemberExpression";
    object: Expression | Super;
    property: Expression;
    computed: boolean;
}

export interface ConditionalExpression extends BaseNode {
    type: "ConditionalExpression";
    test: Expression;
    consequent: Expression;
    alternate: Expression;
}

export interface CallExpression extends BaseNode {
    type: "CallExpression";
    callee: Expression | Super;
    arguments: (Expression | SpreadElement)[];
}

export interface NewExpression extends BaseNode {
    type: "NewExpression";
    callee: Expression;
    arguments: (Expression | SpreadElement)[];
}

export interface SequenceExpression extends BaseNode {
    type: "SequenceExpression";
    expressions: Expression[];
}

export interface SpreadElement extends BaseNode {
    type: "SpreadElement";
    argument: Expression;
}

export interface YieldExpression extends BaseNode {
    type: "YieldExpression";
    argument: Expression | null;
    delegate: boolean;
}

export interface ThisExpression extends BaseNode {
    type: "ThisExpression";
}

export interface Super extends BaseNode {
    type: "Super";
}

export interface MetaProperty extends BaseNode {
    type: "MetaProperty";
    // dialect: the esprima fork stores the raw NAMES here, not
    // Identifier nodes
    meta: string;
    property: string;
}

// --- patterns ---------------------------------------------------------------

export interface ObjectPattern extends BaseNode {
    type: "ObjectPattern";
    properties: Property[];
}

export interface ArrayPattern extends BaseNode {
    type: "ArrayPattern";
    // dialect: declaration-position rests parse as SpreadElement,
    // assignment-position ones as RestElement
    elements: (Pattern | SpreadElement | null)[];
}

export interface RestElement extends BaseNode {
    type: "RestElement";
    argument: Pattern;
}

export interface AssignmentPattern extends BaseNode {
    type: "AssignmentPattern";
    left: Pattern;
    right: Expression;
}

// --- statements -------------------------------------------------------------

export interface Program extends BaseNode {
    type: "Program";
    body: Statement[];
    sourceType?: "script" | "module";
}

export interface ExpressionStatement extends BaseNode {
    type: "ExpressionStatement";
    expression: Expression;
}

export interface BlockStatement extends BaseNode {
    type: "BlockStatement";
    body: Statement[];
}

export interface EmptyStatement extends BaseNode {
    type: "EmptyStatement";
}

export interface DebuggerStatement extends BaseNode {
    type: "DebuggerStatement";
}

export interface WithStatement extends BaseNode {
    type: "WithStatement";
    object: Expression;
    body: Statement;
}

export interface ReturnStatement extends BaseNode {
    type: "ReturnStatement";
    argument: Expression | null;
}

export interface LabeledStatement extends BaseNode {
    type: "LabeledStatement";
    label: Identifier;
    body: Statement;
}

export interface BreakStatement extends BaseNode {
    type: "BreakStatement";
    label: Identifier | null;
}

export interface ContinueStatement extends BaseNode {
    type: "ContinueStatement";
    label: Identifier | null;
}

export interface IfStatement extends BaseNode {
    type: "IfStatement";
    test: Expression;
    consequent: Statement;
    alternate: Statement | null;
}

export interface SwitchStatement extends BaseNode {
    type: "SwitchStatement";
    discriminant: Expression;
    cases: SwitchCase[];
}

export interface SwitchCase extends BaseNode {
    type: "SwitchCase";
    test: Expression | null;
    consequent: Statement[];
}

export interface ThrowStatement extends BaseNode {
    type: "ThrowStatement";
    argument: Expression;
}

export interface TryStatement extends BaseNode {
    type: "TryStatement";
    block: BlockStatement;
    handlers: CatchClause[];
    guardedHandlers: CatchClause[];
    finalizer: BlockStatement | null;
}

export interface CatchClause extends BaseNode {
    type: "CatchClause";
    param: Pattern;
    guard: Expression | null;
    body: BlockStatement;
}

export interface WhileStatement extends BaseNode {
    type: "WhileStatement";
    test: Expression;
    body: Statement;
}

export interface DoWhileStatement extends BaseNode {
    type: "DoWhileStatement";
    body: Statement;
    test: Expression;
}

export interface ForStatement extends BaseNode {
    type: "ForStatement";
    init: VariableDeclaration | Expression | null;
    test: Expression | null;
    update: Expression | null;
    body: Statement;
}

export interface ForInStatement extends BaseNode {
    type: "ForInStatement";
    left: VariableDeclaration | Pattern;
    right: Expression;
    body: Statement;
}

export interface ForOfStatement extends BaseNode {
    type: "ForOfStatement";
    left: VariableDeclaration | Pattern;
    right: Expression;
    body: Statement;
}

export interface VariableDeclaration extends BaseNode {
    type: "VariableDeclaration";
    kind: "var" | "let" | "const";
    declarations: VariableDeclarator[];
}

export interface VariableDeclarator extends BaseNode {
    type: "VariableDeclarator";
    id: Pattern;
    init: Expression | null | undefined;
}

// --- classes ----------------------------------------------------------------

export interface ClassBase extends BaseNode {
    id: Identifier | null;
    superClass: Expression | null;
    body: ClassBody;
}

export interface ClassDeclaration extends ClassBase {
    type: "ClassDeclaration";
    id: Identifier;
}

export interface ClassExpression extends ClassBase {
    type: "ClassExpression";
}

export interface ClassBody extends BaseNode {
    type: "ClassBody";
    body: MethodDefinition[];
}

export interface MethodDefinition extends BaseNode {
    type: "MethodDefinition";
    key: Expression;
    value: FunctionExpression;
    kind: "init" | "constructor" | "method" | "get" | "set";
    computed?: boolean;
    static?: boolean;
}

// --- modules ----------------------------------------------------------------

export interface ModuleSpecifierBase extends BaseNode {
    local: Identifier;
}

export interface ImportSpecifier extends ModuleSpecifierBase {
    type: "ImportSpecifier";
    imported: Identifier;
    // legacy alias some paths still consult
    id?: Identifier;
}

export interface ImportDefaultSpecifier extends ModuleSpecifierBase {
    type: "ImportDefaultSpecifier";
    id?: Identifier;
}

export interface ImportNamespaceSpecifier extends ModuleSpecifierBase {
    type: "ImportNamespaceSpecifier";
    id?: Identifier;
}

export interface ImportDeclaration extends BaseNode {
    type: "ImportDeclaration";
    specifiers: (ImportSpecifier | ImportDefaultSpecifier | ImportNamespaceSpecifier)[];
    source: Literal;
    // added by gather-imports: the resolved module path literal
    source_path?: Literal & { value: string };
}

export interface ExportSpecifier extends BaseNode {
    type: "ExportSpecifier";
    local: Identifier;
    exported: Identifier;
}

export interface ExportNamedDeclaration extends BaseNode {
    type: "ExportNamedDeclaration";
    declaration: Statement | null;
    specifiers: ExportSpecifier[];
    source: Literal | null;
    source_path?: Literal & { value: string };
}

export interface ExportDefaultDeclaration extends BaseNode {
    type: "ExportDefaultDeclaration";
    declaration: Expression | FunctionDeclaration | VariableDeclaration;
}

export interface ExportAllDeclaration extends BaseNode {
    type: "ExportAllDeclaration";
    source: Literal;
    source_path?: Literal & { value: string };
}

// --- unions -----------------------------------------------------------------

export type Function = FunctionDeclaration | FunctionExpression | ArrowFunctionExpression;

export type Class = ClassDeclaration | ClassExpression;

export type ModuleDeclarationNode =
    | ImportDeclaration
    | ExportNamedDeclaration
    | ExportDefaultDeclaration
    | ExportAllDeclaration;

export type Pattern =
    | Identifier
    | ObjectPattern
    | ArrayPattern
    | RestElement
    | AssignmentPattern
    | MemberExpression; // assignment-position targets

export type Expression =
    | ArrayExpression
    | ObjectExpression
    | Identifier
    | Literal
    | TemplateLiteral
    | TaggedTemplateExpression
    | FunctionExpression
    | ArrowFunctionExpression
    | UnaryExpression
    | UpdateExpression
    | BinaryExpression
    | AssignmentExpression
    | LogicalExpression
    | MemberExpression
    | ConditionalExpression
    | CallExpression
    | NewExpression
    | SequenceExpression
    | SpreadElement
    | YieldExpression
    | ThisExpression
    | Super
    | MetaProperty
    | ClassExpression
    | ObjectPattern
    | ArrayPattern;

export type Statement =
    | ExpressionStatement
    | BlockStatement
    | EmptyStatement
    | DebuggerStatement
    | WithStatement
    | ReturnStatement
    | LabeledStatement
    | BreakStatement
    | ContinueStatement
    | IfStatement
    | SwitchStatement
    | ThrowStatement
    | TryStatement
    | WhileStatement
    | DoWhileStatement
    | ForStatement
    | ForInStatement
    | ForOfStatement
    | VariableDeclaration
    | FunctionDeclaration
    | ClassDeclaration
    | ModuleDeclarationNode;

export type Node =
    | Program
    | Statement
    | Expression
    | Pattern
    | Property
    | SwitchCase
    | CatchClause
    | VariableDeclarator
    | TemplateElement
    | ClassBody
    | MethodDefinition
    | ImportSpecifier
    | ImportDefaultSpecifier
    | ImportNamespaceSpecifier
    | ExportSpecifier;

export type NodeType = Node["type"];
