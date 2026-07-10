/* -*- Mode: js2; indent-tabs-mode: nil; tab-width: 4; js2-indent-offset: 4; js2-basic-offset: 4; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

import { sanitize_with_regexp } from "./echo-util";

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

    hasDefaultExport() {
        return this.has_default;
    }

    addExport(ident, constval) {
        this.exports.set(ident, {
            constval: constval,
            slot_num: this.slot_num,
        });
        this.slot_num++;
    }

    // a hidden slot for a non-exported module-level var: it shares the
    // export slot array (so allocation sizing and GC scanning need no
    // changes) but is private to the module -- import resolution and the
    // module-object accessors skip promoted entries.
    addPromotedSlot(ident) {
        if (this.exports.has(ident)) return;
        this.exports.set(ident, {
            constval: undefined,
            slot_num: this.slot_num,
            promoted: true,
        });
        this.slot_num++;
    }

    addImportSource(source_path) {
        if (this.importList.indexOf(source_path) === -1) this.importList.push(source_path);
    }

    isNative() {
        return this.is_native;
    }
}

export class JSModuleInfo extends ModuleInfo {
    constructor(path) {
        super(false);
        this.path = path;
        let sanitized_path = sanitize_with_regexp(path);
        this.toplevel_function_name = `_ejs_toplevel_${sanitized_path}`;
        this.module_name = `_ejs_module_${sanitized_path}`;
    }

}

export class NativeModuleInfo extends ModuleInfo {
    constructor(name, init_function, link_flags, module_files, ejs_dir) {
        super(true);
        this.path = name;
        this.module_name = name;
        this.init_function = init_function;
        this.link_flags = link_flags.join(" ");
        this.module_files = module_files;
        this.ejs_dir = ejs_dir;
    }
}
