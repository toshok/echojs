let esprima = require('esprima');
let debug = require('debug');

let { ArrayExpression,
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
  ComputedPropertyKey,
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
  YieldExpression } = esprima.Syntax;

export class TreeVisitor {
    visitArray (arr, ...args) {
        let i = 0;
        let e = arr.length;
	
        while (i < e) {
            let tmp = this.visit(arr[i], ...args);
	    if (!tmp) {
                arr.splice(i, 1);
                e = arr.length;
	    }
	    else if (Array.isArray(tmp)) {
                let tmplen = tmp.length;
                if (tmplen > 0) {
		    tmp.unshift(1);
		    tmp.unshift(i);
		    arr.splice.apply(arr, tmp);
		    i += tmplen;
		    e = arr.length;
		}
                else {
		    arr.splice(i, 1);
		    e = arr.length;
		}
	    }
	    else {
                arr[i] = tmp;
		i += 1;
	    }
	}
        return arr;
    }
                
                
    visit (n, ...args) {
	if (!n) return null;
	if (Array.isArray(n)) return this.visitArray(n, ...args);

        //debug.indent();
        //debug.log(() => `${n.type}>`;

        let rv = null;
        switch (n.type) {
        case ArrayExpression:          	   rv = this.visitArrayExpression(n, ...args);         break;
        case ArrayPattern:             	   rv = this.visitArrayPattern(n, ...args);            break;
        case ArrowFunctionExpression:  	   rv = this.visitArrowFunctionExpression(n, ...args); break;
        case AssignmentExpression:     	   rv = this.visitAssignmentExpression(n, ...args);    break;
        case BinaryExpression:         	   rv = this.visitBinaryExpression(n, ...args);        break;
        case BlockStatement: 	   	   rv = this.visitBlock(n, ...args);                   break;
        case BreakStatement: 	   	   rv = this.visitBreak(n, ...args);                   break;
        case CallExpression: 	   	   rv = this.visitCallExpression(n, ...args);          break;
        case CatchClause:    	   	   rv = this.visitCatchClause(n, ...args);             break;
        case ClassBody:      	   	   rv = this.visitClassBody(n, ...args);               break;
        case ClassDeclaration: 	   	   rv = this.visitClassDeclaration(n, ...args);        break;
        case ClassExpression:  	           rv = this.visitClassExpression(n, ...args);         break;
        case ClassHeritage:    	           throw new Error(`Unhandled AST node type: ${n.type}, ${JSON.stringify(n)}`);
        case ComprehensionBlock:           throw new Error(`Unhandled AST node type: ${n.type}, ${JSON.stringify(n)}`);
        case ComprehensionExpression:      throw new Error(`Unhandled AST node type: ${n.type}, ${JSON.stringify(n)}`);
        case ConditionalExpression:    	   rv = this.visitConditionalExpression(n, ...args); break;
        case ContinueStatement:        	   rv = this.visitContinue(n, ...args); break;
        case ComputedPropertyKey:      	   rv = this.visitComputedPropertyKey(n, ...args); break;
        case DebuggerStatement: 	   throw new Error(`Unhandled AST node type: ${n.type}, ${JSON.stringify(n)}`);
        case DoWhileStatement:  	   rv = this.visitDo(n, ...args); break;
        case EmptyStatement:    	   rv = this.visitEmptyStatement(n, ...args); break;
        case ExportDeclaration: 	   rv = this.visitExportDeclaration(n, ...args); break;
        case ExportBatchSpecifier: 	   throw new Error(`Unhandled AST node type: ${n.type}, ${JSON.stringify(n)}`);
        case ExportSpecifier:      	   throw new Error(`Unhandled AST node type: ${n.type}, ${JSON.stringify(n)}`);
        case ExpressionStatement:  	   rv = this.visitExpressionStatement(n, ...args); break;
        case ForInStatement: 	   	   rv = this.visitForIn(n, ...args); break;
        case ForOfStatement: 	   	   rv = this.visitForOf(n, ...args); break;
        case ForStatement:   	   	   rv = this.visitFor(n, ...args); break;
        case FunctionDeclaration: 	   rv = this.visitFunctionDeclaration(n, ...args); break;
        case FunctionExpression:  	   rv = this.visitFunctionExpression(n, ...args); break;
        case Identifier:  		   rv = this.visitIdentifier(n, ...args); break;
        case IfStatement: 		   rv = this.visitIf(n, ...args); break;
        case ImportDeclaration: 	   rv = this.visitImportDeclaration(n, ...args); break;
        case ImportSpecifier:   	   rv = this.visitImportSpecifier(n, ...args); break;
        case LabeledStatement:  	   rv = this.visitLabeledStatement(n, ...args); break;
        case Literal:           	   rv = this.visitLiteral(n, ...args); break;
        case LogicalExpression: 	   rv = this.visitLogicalExpression(n, ...args); break;
        case MemberExpression:  	   rv = this.visitMemberExpression(n, ...args); break;
        case MethodDefinition:  	   rv = this.visitMethodDefinition(n, ...args); break;
        case ModuleDeclaration: 	   rv = this.visitModuleDeclaration(n, ...args); break;
        case NewExpression:     	   rv = this.visitNewExpression(n, ...args); break;
        case ObjectExpression:  	   rv = this.visitObjectExpression(n, ...args); break;
        case ObjectPattern:     	   rv = this.visitObjectPattern(n, ...args); break;
        case Program:           	   rv = this.visitProgram(n, ...args); break;
        case Property:           	   rv = this.visitProperty(n, ...args); break;
        case ReturnStatement:    	   rv = this.visitReturn(n, ...args); break;
        case SequenceExpression: 	   rv = this.visitSequenceExpression(n, ...args); break;
        case SpreadElement:            	   rv = this.visitSpreadElement(n, ...args); break;
        case SwitchCase:               	   rv = this.visitCase(n, ...args); break;
        case SwitchStatement:          	   rv = this.visitSwitch(n, ...args); break;
        case TaggedTemplateExpression: 	   rv = this.visitTaggedTemplateExpression(n, ...args); break;
        case TemplateElement: 	   	   rv = this.visitTemplateElement(n, ...args); break;
        case TemplateLiteral: 	   	   rv = this.visitTemplateLiteral(n, ...args); break;
        case ThisExpression: 	   	   rv = this.visitThisExpression(n, ...args); break;
        case ThrowStatement: 	   	   rv = this.visitThrow(n, ...args); break;
        case TryStatement:                 rv = this.visitTry(n, ...args); break;
        case UnaryExpression:     	   rv = this.visitUnaryExpression(n, ...args); break;
        case UpdateExpression:    	   rv = this.visitUpdateExpression(n, ...args); break;
        case VariableDeclaration: 	   rv = this.visitVariableDeclaration(n, ...args); break;
        case VariableDeclarator:  	   rv = this.visitVariableDeclarator(n, ...args); break;
        case WhileStatement:  	   	   rv = this.visitWhile(n, ...args); break;
        case WithStatement:   	   	   rv = this.visitWith(n, ...args); break;
        case YieldExpression: 	   	   rv = this.visitYield(n, ...args); break;
	default:
            throw new Error(`PANIC: unknown parse node type ${n.type}, ${JSON.stringify(n)}`);
	}
        
        //debug.log -> "<#{n.type}, rv = #{if rv then rv.type else 'null'}"
        //debug.unindent()

	if (!rv) return n;
	return rv;
    }

    visitProgram (n, ...args) {
        n.body = this.visitArray(n.body, ...args);
        return n;
    }
                
    visitFunction (n, ...args) {
        n.params = this.visitArray(n.params, ...args);
        n.body   = this.visit(n.body, ...args);
        return n;
    }

    visitFunctionDeclaration (n, ...args) {
	return this.visitFunction(n, ...args);
    }

    visitFunctionExpression (n, ...args) {
        return this.visitFunction(n, ...args);
    }

    visitArrowFunctionExpression (n, ...args) {
        return this.visitFunction(n, ...args);
    }

    visitBlock (n, ...args) {
        n.body = this.visitArray(n.body, ...args);
        return n;
    }

    visitEmptyStatement (n) { return n; }

    visitExpressionStatement (n, ...args) {
        n.expression = this.visit(n.expression, ...args);
        return n;
    }
                
    visitSwitch (n, ...args) {
        n.discriminant = this.visit(n.discriminant, ...args);
        n.cases        = this.visitArray(n.cases, ...args);
        return n;
    }
                
    visitCase (n, ...args) {
        n.test       = this.visit(n.test, ...args);
        n.consequent = this.visit(n.consequent, ...args);
        return n;
    }
                
    visitFor (n, ...args) {
        n.init   = this.visit(n.init, ...args);
        n.test   = this.visit(n.test, ...args);
        n.update = this.visit(n.update, ...args);
        n.body   = this.visit(n.body, ...args);
        return n;
    }
                
    visitWhile (n, ...args) {
        n.test = this.visit(n.test, ...args);
        n.body = this.visit(n.body, ...args);
        return n;
    }
                
    visitIf (n, ...args) {
        n.test       = this.visit(n.test, ...args);
        n.consequent = this.visit(n.consequent, ...args);
        n.alternate  = this.visit(n.alternate, ...args);
        return n;
    }
                
    visitForIn (n, ...args) {
        n.left  = this.visit(n.left, ...args);
        n.right = this.visit(n.right, ...args);
        n.body  = this.visit(n.body, ...args);
        return n;
    }
                
    visitForOf (n, ...args) {
        n.left  = this.visit(n.left, ...args);
        n.right = this.visit(n.right, ...args);
        n.body  = this.visit(n.body, ...args);
        return n;
    }
                
    visitDo (n, ...args) {
        n.body = this.visit(n.body, ...args);
        n.test = this.visit(n.test, ...args);
        return n;
    }
                
    visitIdentifier (n)     { return n; }
    visitLiteral (n)        { return n; }
    visitThisExpression (n) { return n; }
    visitBreak (n)          { return n; }
    visitContinue (n)       { return n; }
                
    visitTry (n, ...args) {
        n.block = this.visit(n.block, ...args);
        if (n.handlers)
            n.handlers = this.visit(n.handlers, ...args);
        else
            n.handlers = null;
        n.finalizer = this.visit(n.finalizer, ...args);
        return n;
    }

    visitCatchClause (n, ...args) {
        n.param = this.visit(n.param, ...args);
        n.guard = this.visit(n.guard, ...args);
        n.body  = this.visit(n.body, ...args);
        return n;
    }
                
    visitThrow (n, ...args) {
        n.argument = this.visit(n.argument, ...args);
        return n;
    }
    
    visitReturn (n, ...args) {
        n.argument = this.visit(n.argument, ...args);
        return n;
    }
                
    visitWith (n, ...args) {
        n.object = this.visit(n.object, ...args);
        n.body   = this.visit(n.body, ...args);
        return n;
    }

    visitYield (n, ...args) {
        n.argument = this.visit(n.argument, ...args);
        return n;
    }
                
    visitVariableDeclaration (n, ...args) {
        n.declarations = this.visitArray(n.declarations, ...args);
        return n;
    }

    visitVariableDeclarator (n, ...args) {
        n.id   = this.visit(n.id, ...args);
        n.init = this.visit(n.init, ...args);
        return n;
    }
                                
    visitLabeledStatement (n, ...args) {
        n.label = this.visit(n.label, ...args);
        n.body  = this.visit(n.body, ...args);
        return n;
    }
                
    visitAssignmentExpression (n, ...args) {
        n.left  = this.visit(n.left, ...args);
        n.right = this.visit(n.right, ...args);
        return n;
    }
                
    visitConditionalExpression (n, ...args) {
        n.test       = this.visit(n.test, ...args);
        n.consequent = this.visit(n.consequent, ...args);
        n.alternate  = this.visit(n.alternate, ...args);
        return n;
    }
                
    visitLogicalExpression (n, ...args) {
        n.left  = this.visit(n.left, ...args);
        n.right = this.visit(n.right, ...args);
        return n;
    }
                
    visitBinaryExpression (n, ...args) {
        n.left  = this.visit(n.left, ...args);
        n.right = this.visit(n.right, ...args);
        return n;
    }

    visitUnaryExpression (n, ...args) {
        n.argument = this.visit(n.argument, ...args);
        return n;
    }

    visitUpdateExpression (n, ...args) {
        n.argument = this.visit(n.argument, ...args);
        return n;
    }

    visitMemberExpression (n, ...args) {
        n.object = this.visit(n.object, ...args);
        if (n.computed)
            n.property = this.visit(n.property, ...args);
        return n;
    }
                
    visitSequenceExpression (n, ...args) {
        n.expressions = this.visitArray(n.expressions, ...args);
        return n;
    }

    visitSpreadElement (n, ...args) {
        n.arguments = this.visit(n.argument, ...args);
        return n;
    }

    visitNewExpression (n, ...args) {
        n.callee    = this.visit(n.callee, ...args);
        n.arguments = this.visitArray(n.arguments, ...args);
        return n;
    }

    visitObjectExpression (n, ...args) {
        n.properties = this.visitArray(n.properties, ...args);
        return n;
    }

    visitArrayExpression (n, ...args) {
        n.elements = this.visitArray(n.elements, ...args);
        return n;
    }

    visitProperty (n, ...args) {
        n.key   = this.visit(n.key, ...args);
        n.value = this.visit(n.value, ...args);
        return n;
    }
                                
    visitComputedPropertyKey (n, ...args) {
        n.expression   = this.visit(n.expression, ...args);
        return n;
    }
                
    visitCallExpression (n, ...args) {
        n.callee    = this.visit(n.callee, ...args);
        n.arguments = this.visitArray(n.arguments, ...args);
        return n;
    }

    visitClassDeclaration (n, ...args) {
	return this.visitClass(n, ...args);
    }

    visitClassExpression (n, ...args) {
	return this.visitClass(n, ...args);
    }

    visitClass (n, ...args) {
        n.body = this.visit(n.body, ...args);
        return n;
    }

    visitClassBody (n, ...args) {
        n.body = this.visitArray(n.body, ...args);
        return n;
    }

    visitMethodDefinition (n, ...args) {
        n.value = this.visit(n.value, ...args);
        return n;
    }

    visitModuleDeclaration (n, ...args) {
        n.id   = this.visit(n.id, ...args);
        n.body = this.visit(n.body, ...args);
        return n;
    }

    visitExportDeclaration (n, ...args) {
        n.declaration = this.visit(n.declaration, ...args);
        return n;
    }
                
    visitImportDeclaration (n, ...args) {
        n.specifiers = this.visitArray(n.specifiers, ...args);
        return n;
    }

    visitImportSpecifier (n, ...args) {
        n.id = this.visit(n.id, ...args);
        return n;
    }

    visitArrayPattern (n, ...args) {
        n.elements = this.visitArray(n.elements, ...args);
        return n;
    }

    visitObjectPattern (n, ...args) {
        n.properties = this.visitArray(n.properties, ...args);
        return n;
    }

    visitTaggedTemplateExpression (n, ...args) {
        n.quasi = this.visit(n.quasi, ...args);
        return n;
    }

    visitTemplateLiteral (n, ...args) {
        n.quasis      = this.visitArray(n.quasis, ...args);
        n.expressions = this.visitArray(n.expressions, ...args);
        return n;
    }

    visitTemplateElement (n, ...args) { return n; }

    toString () { return "TreeVisitor"; }
}

export class TransformPass extends TreeVisitor {
        constructor (options) {
	    this.options = options;
	}
}
                
