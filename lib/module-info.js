/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

import { intrinsic, sanitize_with_regexp } from './echo-util';
import { moduleGetSlot_id, moduleSetSlot_id, env_unused_id, value_id } from './common-ids';
import * as b from './ast-builder';

export class ModuleInfo {
    constructor(is_native) {
        this.slot_num = 0;
        this.exports = new Map();
        this.importList = [];
        this.has_default = false;
        this.is_native = is_native;
    }

    setHasDefaultExport() {
        this.has_default = true;
    }

    hasDefaultExport() { return this.has_default; }

    addExport(ident, constval) {
        this.exports.set(ident, { constval: constval, slot_num: this.slot_num });
        this.slot_num ++;
    }

    addImportSource(source_path) {
        if (this.importList.indexOf(source_path) === -1)
            this.importList.push(source_path);
    }

    isNative() { return this.is_native; }
}

export class JSModuleInfo extends ModuleInfo {
    constructor (path) {
        super(false);
        this.path = path;
        let sanitized_path = sanitize_with_regexp(path);
        this.toplevel_function_name = `_ejs_toplevel_${sanitized_path}`;
        this.module_name = `_ejs_module_${sanitized_path}`;
    }

    getExportGetter(ident) {
        let export_info = this.exports.get(ident);
        let function_id = b.identifier(`get_export_${ident}`);
        let loc = loc = { start: { line: 0, column: 0 } };
        if (export_info.constval) {
            return b.functionExpression(function_id, [env_unused_id], b.blockStatement([b.returnStatement(export_info.constval)], loc), [], null, loc);
        }
        else {
            return b.functionExpression(function_id, [env_unused_id], b.blockStatement([b.returnStatement(intrinsic(moduleGetSlot_id, [b.literal(this.path), b.literal(ident)]))], loc), [], null, loc);
        }
    }

    getExportSetter(ident) {
        let function_id = b.identifier(`set_export_${ident}`);
        // we shouldn't generate a setter for const exports
        return b.functionExpression(function_id, [env_unused_id, value_id], b.blockStatement([intrinsic(moduleSetSlot_id, [b.literal(this.path), b.literal(ident), value_id])]));
    }    
}

export class NativeModuleInfo extends ModuleInfo {
    constructor(name, init_function, link_flags, module_files, ejs_dir) {
        super(true);
        this.path = name;
        this.module_name = name;
        this.init_function = init_function;
        this.link_flags = link_flags.join(' ');
        this.module_files = module_files;
        this.ejs_dir = ejs_dir;
    }
}
