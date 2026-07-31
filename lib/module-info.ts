/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

import { sanitize_with_regexp } from "./echo-util";
import type { Literal } from "./estree";

export interface ExportInfo {
    // a const-literal export's folded initializer, when it has one
    constval: Literal | undefined;
    slot_num: number;
    // a hidden slot for a non-exported module-level var (see
    // addPromotedSlot); import resolution and the module-object
    // accessors skip promoted entries
    promoted?: boolean;
}

export abstract class ModuleInfo {
    slot_num = 0;
    exports = new Map<string, ExportInfo>();
    importList: string[] = [];
    has_default = false;
    is_native: boolean;

    // every ModuleInfo names a module and its generated artifacts
    abstract path: string;
    abstract module_name: string;

    constructor(is_native: boolean) {
        this.is_native = is_native;
    }

    setHasDefaultExport(): void {
        this.has_default = true;
    }

    hasDefaultExport(): boolean {
        return this.has_default;
    }

    addExport(ident: string, constval?: Literal): void {
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
    addPromotedSlot(ident: string): void {
        if (this.exports.has(ident)) return;
        this.exports.set(ident, {
            constval: undefined,
            slot_num: this.slot_num,
            promoted: true,
        });
        this.slot_num++;
    }

    addImportSource(source_path: string): void {
        if (this.importList.indexOf(source_path) === -1) this.importList.push(source_path);
    }

    isNative(): boolean {
        return this.is_native;
    }
}

export class JSModuleInfo extends ModuleInfo {
    path: string;
    module_name: string;
    toplevel_function_name: string;

    constructor(path: string) {
        super(false);
        this.path = path;
        const sanitized_path = sanitize_with_regexp(path);
        this.toplevel_function_name = `_ejs_toplevel_${sanitized_path}`;
        this.module_name = `_ejs_module_${sanitized_path}`;
    }
}

export class NativeModuleInfo extends ModuleInfo {
    path: string;
    module_name: string;
    init_function: string;
    link_flags: string;
    module_files: string[];
    ejs_dir: string;

    constructor(
        name: string,
        init_function: string,
        link_flags: string[],
        module_files: string[],
        ejs_dir: string
    ) {
        super(true);
        this.path = name;
        this.module_name = name;
        this.init_function = init_function;
        this.link_flags = link_flags.join(" ");
        this.module_files = module_files;
        this.ejs_dir = ejs_dir;
    }
}
