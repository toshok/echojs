/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

import { intrinsic, sanitize_with_regexp } from './echo-util';
import { Map } from './map-es6';
import { moduleGetSlot_id, moduleSetSlot_id, env_unused_id, value_id } from './common-ids';
import * as b from './ast-builder';

export class ModuleInfo {
    constructor (path) {
	    this.path = path;
        this.slot_num = 0;
        this.exports = new Map();
        this.importList = [];
        this.has_default = false;
        let sanitized_path = sanitize_with_regexp(path);
        this.toplevel_function_name = `_ejs_toplevel_${sanitized_path}`;
        this.module_name = `_ejs_module_${sanitized_path}`;
    }

    setHasDefaultExport() {
        this.has_default = true;
    }

    hasDefaultExport() { return this.has_default; }

    addExport(ident, constval) {
        this.exports.set(ident, { constval: constval, slot_num: this.slot_num });
        this.slot_num ++;
    }

    getExportGetter(ident) {
        let export_info = this.exports.get(ident);
        let function_id = b.identifier(`get_export_${ident}`);
        if (export_info.constval) {
            return b.functionExpression(function_id, [env_unused_id], b.blockStatement([b.returnStatement(export_info.constval)]));
	    }
        else {
            return b.functionExpression(function_id, [env_unused_id], b.blockStatement([b.returnStatement(intrinsic(moduleGetSlot_id, [b.literal(this.path), b.literal(ident)]))]));
	    }
    }

    getExportSetter(ident) {
        let export_info = this.exports.get(ident);
        let function_id = b.identifier(`set_export_${ident}`);
	    // we shouldn't generate a setter for const exports
        return b.functionExpression(function_id, [env_unused_id, value_id], b.blockStatement([intrinsic(moduleSetSlot_id, [b.literal(this.path), b.literal(ident), value_id])]));
    }
    
    addImportSource(source_path) {
	    if (this.importList.indexOf(source_path) === -1)
            this.importList.push(source_path);
    }
}
