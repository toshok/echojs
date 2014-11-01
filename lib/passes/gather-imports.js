// this class does two things
//
// 1. rewrites all sources to be relative to this.toplevel_path.  i.e. if
//    the following directory structure exists:
//
//    externals/
//      ext1.js
//    root/
//      main.js      (contains: import { foo } from "modules/foo" )
//      modules/
//        foo1.js    (contains: module ext1 from "../../externals/ext1")
//
//    $PWD = root/
//
//      $ ejs main.js
//
//    ejs will rewrite module paths such that main.js is unchanged, and
//    foo1.js's module declaration reads:
//
//      "../externals/ext1"
// 
// 2. builds up a list (this.importList) containing the list of all
//    imported modules
//

import * as node_compat from '../node-compat'

import * as path from '@node-compat/path';
import { TreeVisitor } from '../node-visitor';
import { startGenerator, is_intrinsic, is_string_literal, underline } from '../echo-util';
import { ModuleInfo } from '../module-info';
import * as b, { FunctionDeclaration, ClassDeclaration, VariableDeclaration, VariableDeclarator, Literal } from '../ast-builder';

let hasOwn = Object.prototype.hasOwnProperty;

function isInternalModule (source) {
    return source[0] === "@";
}

let allModules = new Map();

class GatherImports extends TreeVisitor {
    constructor (filename, path, toplevel_path) {
        super();
        this.filename = filename;
        this.path = path;
        this.toplevel_path = toplevel_path;

        this.importList = [];
        // remove our .js suffix since all imports are suffix-free
        if (this.filename.lastIndexOf(".js") === this.filename.length - 3)
            this.filename = this.filename.substring(0, this.filename.length-3);

	this.moduleInfo = new ModuleInfo(this.filename);
	allModules.set(this.filename, this.moduleInfo);
    }
    
    addSource (n) {
        if (!n.source) return n;
        
        if (!is_string_literal(n.source)) throw new Error("import sources must be strings");

        if (isInternalModule(n.source.value)) {
            if (n.source.value.indexOf('@node-compat/') === 0) {
                // add the exports here for node-compat modules
                let module_name = n.source.value.slice('@node-compat/'.length);
                if (!node_compat.modules[module_name])
                    throw new Error(`@node-compat module ${module_name} not found`);

                if (!allModules.has(n.source.value)) {
                    let internal_module = new ModuleInfo(n.source.value);
                    node_compat.modules[module_name].forEach ((v) => internal_module.addExport(v));
		}

                n.source_path = b.literal(n.source.value);
	    }
            // otherwise we just strip off the @
            else {
                n.source_path = b.literal(n.source.value.slice(1));
	    }
	    return n;
        }
        
        let source_path = (n.source[0] === "/") ? n.source.value : path.resolve(this.toplevel_path, `${this.path}/${n.source.value}`);

        if (source_path.indexOf(process.cwd()) === 0)
            source_path = path.relative(process.cwd(), source_path);

        if (this.importList.indexOf(source_path) === -1)
            this.importList.push(source_path);
        this.moduleInfo.addImportSource(source_path);

        n.source_path = b.literal(source_path);
        return n;
    }

    addDefaultExport (path) {
        this.moduleInfo.addExport("default");
        this.moduleInfo.setHasDefaultExport();
    }
    
    addExportIdentifier (id, constval) {
        if (id === 'default')
            this.moduleInfo.setHasDefaultExport();
        this.moduleInfo.addExport(id, constval);
    }
    
    visitImportDeclaration (n) {
        return this.addSource(n);
    }
    
    visitExportDeclaration (n) {
        n = this.addSource(n);

        if (n.default) {
            this.addDefaultExport();
        }
        else if (n.source) {
            for (let spec of n.specifiers)
                this.addExportIdentifier(spec.name ? spec.name.name : spec.id && spec.id.name);
        }
        else if (Array.isArray(n.declaration)) {
            for (let decl of n.declaration)
                this.addExportIdentifier(decl.id.name);
        }
        else if (n.declaration.type === FunctionDeclaration) {
            this.addExportIdentifier(n.declaration.id.name) ;
        }
        else if (n.declaration.type === ClassDeclaration) {
            this.addExportIdentifier(n.declaration.id.name) ;
        }
        else if (n.declaration.type === VariableDeclaration) {
            for (let decl of n.declaration.declarations)
                this.addExportIdentifier(decl.id.name, n.declaration.kind === 'const' && decl.init.type === Literal ? decl.init : undefined);
        }
        else if (n.declaration.type === VariableDeclarator) {
            this.addExportIdentifier(n.declaration.id.name);
        }
        else {
            throw new Error("unhandled case in visitExportDeclaration");
        }

        return n;
    }

    visitModuleDeclaration (n) {
        return this.addSource(n);
    }
}

export function getAllModules () { return allModules; }
export function dumpModules () {
    console.log(underline('modules'));
    allModules.forEach ((m) => {
        console.log(m.path);
        console.log(`   has default: ${m.hasDefaultExport()}`);
        if (m.exports.size() > 0) {
            console.log('   slots:');
            m.exports.forEach ((v, k) => {
		console.log(`      ${k}: ${v.slot_num}`);
	    });
	}
    });
}

export function gatherImports (filename, path, top_path, tree) {
    let visitor = new GatherImports(filename, path, top_path);
    visitor.visit(tree);
    return visitor.importList;
}

