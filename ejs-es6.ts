/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

import * as os from "@node-compat/os";
import * as path from "@node-compat/path";
import * as fs from "@node-compat/fs";
import * as child_process from "@node-compat/child_process";
import type { CompilerOptions } from "./lib/options";
import type { Triple as TripleT } from "./lib/triple";
import type { ModuleInfo, JSModuleInfo, NativeModuleInfo } from "./lib/module-info";
import type { Program } from "./lib/estree";

import * as debug from "./lib/debug";
import { compile } from "./lib/compiler";
import { dumpModules, getAllModules, gatherAllModules } from "./lib/passes/gather-imports";

import { bold, reset, genFreshFileName, Writer } from "./lib/echo-util";
import { Triple } from "./lib/triple";

import {
    LLVM_SUFFIX as DEFAULT_LLVM_SUFFIX,
    RUNLOOP_IMPL as DEFAULT_RUNLOOP_IMPL,
} from "./lib/host-config";

const spawn = child_process.spawn;

// the self-hosted runtime exposes a global marker object
declare const __ejs: object | undefined;

function isNode(): boolean {
    return typeof __ejs == "undefined";
}

let argv: string[];
if (!isNode()) {
    // argv is ['.../ejs', ...], get rid of the first arg
    argv = process.argv.slice(1);
} else {
    // argv is ['node', '.../ejs-es6.js', ...], get rid of the first two args
    argv = process.argv.slice(2);
}

let ejs_dirname: string | undefined;
function ejs_exe_dirname(): string {
    if (ejs_dirname) return ejs_dirname;
    const argv0 = process.argv[isNode() ? 1 : 0]!;
    let cwd = process.cwd();

    let full_path_to_exe: string | undefined;
    if (argv0.indexOf("/") != -1) {
        // either relative or absolute.  don't both searching path.
        let ejs_path = path.resolve(cwd, argv0);
        try {
            if (fs.statSync(ejs_path).isFile()) {
                full_path_to_exe = ejs_path;
            }
        } catch (e) {
            // an exception while stat'ing is the same as the file not existing.
        }
    } else {
        // not qualified at all, search over PATH
        for (const p of (process.env["PATH"] || "").split(":")) {
            let ejs_path = path.resolve(cwd, p, argv0);
            try {
                if (fs.statSync(ejs_path).isFile()) {
                    full_path_to_exe = ejs_path;
                    break;
                }
            } catch (e) {
                // we treat an exception while stat'ing the same as the file not existing.
            }
        }
    }
    if (!full_path_to_exe) {
        throw new Error("could not locate ejs executable");
    }

    ejs_dirname = path.dirname(full_path_to_exe);
    return ejs_dirname;
}

function relative_to_ejs_exe(n: string): string;
function relative_to_ejs_exe(n: string[]): string[];
function relative_to_ejs_exe(n: string | string[]): string | string[] {
    const was_array = Array.isArray(n);
    const list = was_array ? n : [n];

    const rv = isNode()
        ? list.map((el) => path.resolve(ejs_exe_dirname(), "../..", el))
        : list.map((el) => path.resolve(ejs_exe_dirname(), el));

    if (was_array) return rv;
    return rv[0]!;
}

const temp_files: string[] = [];

const host_triple = Triple.fromProcess();
let target_triple = host_triple; // a reasonable default. we're compiling for _this_ triple.

const options: CompilerOptions = {
    // our defaults:
    opt_level: 2,
    debug: false,
    debug_level: 0,
    debug_passes: new Set(),
    warn_on_undeclared: false,
    frozen_global: false,
    record_types: false,
    types: false,
    types_dump: false,
    output_filename: null,
    show_help: false,
    leave_temp_files: false,
    native_module_dirs: [],
    extra_clang_args: "",
    ios_sdk: "9.2",
    ios_min: "8.0",
    osx_min: "11.0",
    import_variables: [],
    srcdir: false,
    stdout_writer: new Writer(process.stdout),
};

function add_native_module_dir(dir: string): void {
    options.native_module_dirs.push(dir);
}

function set_target(str: string): void {
    let triple: TripleT;
    switch (str) {
        case "linux_x86_64":
            triple = new Triple({ arch: "x86_64", vendor: "unknown", os: "linux" });
            break;
        case "macos":
            triple = new Triple({ arch: "arm64", vendor: "apple", os: "macos" });
            break;
        case "iossim":
            triple = new Triple({ arch: "arm64", vendor: "apple", os: "ios", env: "simulator" });
            break;
        case "iosdev":
            triple = new Triple({ arch: "arm64", vendor: "apple", os: "ios" });
            break;
        default:
            triple = Triple.fromString(str);
            break;
    }
    target_triple = triple;
}

function set_extra_clang_args(arginfo: string): void {
    options.extra_clang_args = arginfo;
}

function increase_debug_level(): void {
    options.debug_level += 1;
}

function add_debug_after_pass(passname: string): void {
    options.debug_passes.add(passname);
}

function add_import_variable(arg: string): void {
    let equal_idx = arg.indexOf("=");
    if (equal_idx == -1) throw new Error("-I flag requires <name>=<value>");

    options.import_variables.push({
        variable: arg.substring(0, equal_idx),
        value: arg.substring(equal_idx + 1),
    });
}

interface ArgSpec {
    // sets options[flag] = true
    flag?: keyof CompilerOptions & string;
    // consumes one argument into options[option]
    option?: keyof CompilerOptions & string;
    handler?: (...args: string[]) => void;
    handlerArgc?: number;
    help: string;
}

const args: Record<string, ArgSpec | undefined> = {
    "-O0": {
        handler: () => (options.opt_level = 0),
        help: "Optimization level 0.",
    },
    "-O1": {
        handler: () => (options.opt_level = 1),
        help: "Optimization level 1. Similar to clang -O1",
    },
    "-O2": {
        handler: () => (options.opt_level = 2),
        help: "Optimization level 2. Similar to clang -O2 (default)",
    },
    "-O3": {
        handler: () => (options.opt_level = 3),
        help: "Optimization level 3. Similar to clang -O3",
    },
    "-g": {
        flag: "debug",
        help: "enable debugging of generated code",
    },
    "-q": {
        flag: "quiet",
        help: "don't output anything during compilation except errors.",
    },
    "-I": {
        handler: add_import_variable,
        handlerArgc: 1,
        help: "add a name=value mapping used to resolve module references.",
    },
    "-d": {
        handler: increase_debug_level,
        handlerArgc: 0,
        help: "debug output.  more instances of this flag increase the amount of spew.",
    },
    "--dump-after": {
        handler: add_debug_after_pass,
        handlerArgc: 1,
        help: "dump the AST after the named pass; `--dump-after eir` dumps the lowered EIR module(s), `--dump-after eir-opt` the optimized EIR",
    },
    "--debug-after": {
        handler: add_debug_after_pass,
        handlerArgc: 1,
        help: "deprecated alias for --dump-after",
    },
    "-o": {
        option: "output_filename",
        help: "name of the output file.",
    },
    "--leave-temp": {
        flag: "leave_temp_files",
        help: "leave temporary files in $TMPDIR from compilation",
    },
    "--moduledir": {
        handler: add_native_module_dir,
        handlerArgc: 1,
        help: "--module path-to-search-for-modules",
    },
    "--help": {
        flag: "show_help",
        help: "output this help info.",
    },
    "--extra-clang-args": {
        handler: set_extra_clang_args,
        handlerArgc: 1,
        help: "extra arguments to pass to the clang command (used to compile the .s to .o)",
    },
    "--record-types": {
        flag: "record_types",
        help: "generates an executable which records types in a format later used for optimizations.",
    },
    "--types": {
        flag: "types",
        help: "run the MAAM type-analysis probe over each module and log its stats (consumes nothing yet).",
    },
    "--types-dump": {
        flag: "types_dump",
        help: "with the MAAM analysis, print each binding's inferred type (implies --types).",
    },
    "--frozen-global": {
        flag: "frozen_global",
        help: "compiler acts as if the global object is frozen after initialization, allowing for faster access.",
    },
    "--warn-on-undeclared": {
        flag: "warn_on_undeclared",
        help: "accesses to undeclared identifiers result in warnings (and global accesses).  By default they're an error.",
    },
    "--target": {
        handler: set_target,
        handlerArgc: 1,
        help: "--target linux_x86_64|macos|iossim|iosdev",
    },
    "--ios-sdk": {
        option: "ios_sdk",
        help: "the version of the ios sdk to use.  useful if more than one is installed.  Default is 7.0.",
    },
    "--ios-min": {
        option: "ios_min",
        help: "the minimum version of iOS to support.  Default is 8.0.",
    },
    "--osx-min": {
        option: "osx_min",
        help: "the minimum version of OSX to support.  Default is 11.0.",
    },
    "--srcdir": {
        flag: "srcdir",
        help: "internal flag.  if set, will look for libecho/libpcre/etc from source directory locations.",
    },
};

function output_usage() {
    console.warn("Usage:");
    console.warn("   ejs [options] file1.js file2.js file.js ...");
}

function output_options() {
    console.warn("Options:");
    for (const a of Object.keys(args)) {
        console.warn(`   ${a}:  ${args[a]!.help}`);
    }
}

let file_args: string[] | undefined;

if (argv.length > 0) {
    for (let ai = 0, ae = argv.length; ai < ae; ai++) {
        const o = args[argv[ai]!];
        if (o) {
            const opts = options as unknown as Record<string, string | boolean>;
            if (o.flag) {
                opts[o.flag] = true;
            } else if (o.option) {
                opts[o.option] = argv[++ai]!;
            } else if (o.handler) {
                const handler_args: string[] = [];
                for (let i = 0, e = o.handlerArgc ?? 0; i < e; i++) handler_args.push(argv[++ai]!);
                o.handler.apply(null, handler_args);
            }
        } else {
            // end of options signals the rest of the array is files
            file_args = argv.slice(ai);
            break;
        }
    }
}

if (options.show_help) {
    output_usage();
    console.warn("");
    output_options();
    process.exit(0);
}

if (!file_args || file_args.length === 0) {
    output_usage();
    process.exit(0);
}

if (!options.quiet) {
    console.log(`host: ${host_triple.toShortString()}, target: ${target_triple.toShortString()}`);
}

debug.setLevel(options.debug_level);

const o_filenames: string[] = [];

const compiled_modules: { filename: string; module_toplevel: string }[] = [];

let sim_base = "/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform";
let dev_base = "/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform";

let sim_bin = `${sim_base}/Developer/usr/bin`;
let dev_bin = `${dev_base}/Developer/usr/bin`;

function target_llc_args(triple: TripleT): string[] {
    let args = [`-march=${triple.llcArch()}`];
    switch (triple.os) {
        case "macos":
            args = args.concat([
                `-mtriple=arm64-apple-macosx${options.osx_min}.0`,
                "-mattr=+fp-armv8",
                "--relocation-model=pic",
            ]);
            break;
        case "ios":
            args = args.concat([`-mtriple=arm64-apple-ios${options.ios_min}.0`]);
            break;
        case "linux":
            args = args.concat(["--relocation-model=pic"]);
            break;
    }

    return args;
}

const target_linker = process.env["CXX"] || "clang++";

function target_link_args(triple: TripleT): string[] {
    let args = ["-arch", triple.clangArch()];

    if (triple.os === "linux") {
        // -arch is a Darwin-only clang flag.  -no-pie keeps the data
        // segment (static atom strings get NaN-boxed by address) below
        // the 47-bit ejsval payload limit; PIE ASLR on aarch64 maps it
        // above 2^47.
        return ["-no-pie"];
    }

    if (triple.os === "macos") {
        return args;
    }

    if (triple.os === "ios") {
        if (triple.env === "simulator") {
            return args.concat([
                "-isysroot",
                `${sim_base}/Developer/SDKs/iPhoneSimulator${options.ios_sdk}.sdk`,
                `-miphoneos-version-min=${options.ios_min}`,
            ]);
        }

        return args.concat([
            "-isysroot",
            `${dev_base}/Developer/SDKs/iPhoneOS${options.ios_sdk}.sdk`,
            `-miphoneos-version-min=${options.ios_min}`,
        ]);
    }

    return [];
}

function target_libraries(triple: TripleT): string[] {
    if (triple.os === "linux") {
        if (DEFAULT_RUNLOOP_IMPL == "noop") return ["-lunwind", "-lpthread"];
        return ["-lunwind", "-lpthread", "-luv"];
    }

    if (triple.os === "macos") {
        // for macos we only need Foundation and AppKit
        return ["-framework", "Foundation", "-framework", "AppKit"];
    }

    if (triple.os === "ios") {
        return [
            "-framework",
            "Foundation",
            "-framework",
            "UIKit",
            "-framework",
            "GLKit",
            "-framework",
            "OpenGLES",
            "-framework",
            "CoreGraphics",
        ];
    }
    return [];
}

function target_libecho(triple: TripleT): string {
    if (options.srcdir) {
        return path.join("runtime", "out", `${triple}`, "libecho.a");
    } else {
        return path.join(relative_to_ejs_exe(`../lib/${triple}`), "libecho.a");
    }
}

function target_extra_libs(triple: TripleT): string[] {
    if (options.srcdir) {
        if (triple.os === "linux")
            return [
                "external-deps/double-conversion-linux/double-conversion/libdouble-conversion.a",
                "external-deps/pcre-linux/.libs/libpcre16.a",
            ];

        if (triple.os === "macos") {
            return [
                "external-deps/double-conversion-macos/double-conversion/libdouble-conversion.a",
                "external-deps/pcre-macos/.libs/libpcre16.a",
            ];
        }

        if (triple.os === "ios") {
            if (triple.env === "simulator") {
                return [
                    "external-deps/double-conversion-iossim/double-conversion/libdouble-conversion.a",
                    "external-deps/pcre-iossim/.libs/libpcre16.a",
                ];
            }

            return [
                "external-deps/double-conversion-iosdev/double-conversion/libdouble-conversion.a",
                "external-deps/pcre-iosdev/.libs/libpcre16.a",
            ];
        }

        throw new Error("no pcre for this platform");
    } else {
        return ["libdouble-conversion.a", "libpcre16.a"].map((lib) =>
            path.join(relative_to_ejs_exe(`../lib/${triple}`), lib)
        );
    }
}

function target_path_prepend(triple: TripleT): string {
    if (triple.os === "ios") {
        if (triple.env === "simulator") {
            return sim_bin;
        }
        return dev_bin;
    }
    return "";
}

const llvm_suffix = process.env["LLVM_SUFFIX"] || DEFAULT_LLVM_SUFFIX;
const llvm_commands = {
    opt: `opt${llvm_suffix}`,
    llc: `llc${llvm_suffix}`,
    "llvm-as": `llvm-as${llvm_suffix}`,
} as const;

function compileFile(
    filename: string,
    parse_tree: Program,
    modules: Map<string, ModuleInfo>,
    files_count: number,
    cur_file: number,
    compileCallback: () => void
): void {
    let base_filename = genFreshFileName(path.basename(filename));

    if (!options.quiet) {
        let suffix = options.debug_level > 0 ? ` -> ${base_filename}` : "";

        // loop over import variables, replacing their values with
        // their names for output
        let output_name = filename;
        for (let ivar of options.import_variables) {
            output_name = output_name.replace(ivar.value, `$${ivar.variable}`);
        }
        options.stdout_writer.write(
            `[${cur_file}/${files_count}] ${bold()}COMPILE${reset()} ${output_name}${suffix}`
        );
    }

    let compiled_module: import("@llvm").Module;
    try {
        compiled_module = compile(
            parse_tree,
            base_filename,
            filename,
            modules,
            options,
            target_triple
        );
    } catch (e) {
        console.warn(`${e}`);
        if (options.debug_level == 0) process.exit(-1);
        throw e;
    }

    function tmpfile(suffix: string): string {
        return `${os.tmpdir()}/${base_filename}-${target_triple.arch}-${target_triple.os}${suffix}`;
    }
    let ll_filename = tmpfile(".ll");
    let bc_filename = tmpfile(".bc");
    let ll_opt_filename = tmpfile(".ll.opt");
    let o_filename = tmpfile(".o");

    temp_files.push(ll_filename, bc_filename, ll_opt_filename, o_filename);

    let opt_level = options.opt_level > 0 ? `default<O${options.opt_level}>,` : "";

    let llvm_as_args = [`-o=${bc_filename}`, ll_filename];
    let opt_args = [
        `-passes=${opt_level}strip-dead-prototypes`,
        "-S",
        `-o=${ll_opt_filename}`,
        bc_filename,
    ];
    let llc_args = target_llc_args(target_triple).concat([
        "-filetype=obj",
        `-o=${o_filename}`,
        ll_opt_filename,
    ]);

    debug.log(1, `writing ${ll_filename}`);
    compiled_module.writeToFile(ll_filename);
    debug.log(1, `done writing ${ll_filename}`);

    // debug.log (1, `writing ${bc_filename}`);
    // compiled_module.writeBitcodeToFile(bc_filename);
    // debug.log (1, `done writing ${bc_filename}`);

    compiled_modules.push({
        filename: filename,
        module_toplevel: (compiled_module as unknown as { toplevel_name: string }).toplevel_name,
    });

    if (!isNode()) {
        // in ejs spawn is synchronous.
        spawn(llvm_commands["llvm-as"], llvm_as_args);
        spawn(llvm_commands["opt"], opt_args);
        spawn(llvm_commands["llc"], llc_args);
        o_filenames.push(o_filename);
        compileCallback();
    } else {
        let llvm_as = spawn(llvm_commands["llvm-as"], llvm_as_args);
        llvm_as.stderr.on("data", (data) => console.warn(`${data}`));
        llvm_as.on("error", (err) => {
            console.warn(`error executing ${llvm_commands["llvm-as"]}: ${err}`);
            process.exit(-1);
        });
        llvm_as.on("exit", (/* XXX code*/) => {
            debug.log(1, `executing '${llvm_commands["opt"]} ${opt_args.join(" ")}'`);
            let opt = spawn(llvm_commands["opt"], opt_args);
            opt.stderr.on("data", (data) => console.warn(`${data}`));
            opt.on("error", (err) => {
                console.warn(`error executing #{llvm_commands['opt']}: ${err}`);
                process.exit(-1);
            });
            opt.on("exit", (/* XXX code*/) => {
                debug.log(1, `executing '${llvm_commands["llc"]} ${llc_args.join(" ")}'`);
                let llc = spawn(llvm_commands["llc"], llc_args);
                llc.stderr.on("data", (data) => console.warn(`${data}`));
                llc.on("error", (err) => {
                    console.warn(`error executing ${llvm_commands["llc"]}: ${err}`);
                    process.exit(-1);
                });
                llc.on("exit", (/* XXX code*/) => {
                    o_filenames.push(o_filename);
                    compileCallback();
                });
            });
        });
    }
}

function generate_import_map(
    js_modules: Map<string, JSModuleInfo>,
    native_modules: Map<string, NativeModuleInfo>
): string {
    let map_path = `${os.tmpdir()}/${genFreshFileName(path.basename(main_file))}-import-map.cpp`;

    let map_contents = "";
    map_contents += `#include "ejs-module.h"\n`;
    map_contents += 'extern "C" {\n';

    js_modules.forEach((module) => {
        map_contents += `extern EJSModule ${module.module_name};\n`;
        map_contents += `extern ejsval ${module.toplevel_function_name} (ejsval env, ejsval _this, uint32_t argc, ejsval *args);\n`;
    });

    map_contents += "EJSModule* _ejs_modules[] = {\n";
    js_modules.forEach((module) => {
        map_contents += `  &${module.module_name},\n`;
    });
    map_contents += "};\n";

    map_contents += "ejsval (*_ejs_module_toplevels[])(ejsval, ejsval, uint32_t, ejsval*) = {\n";
    js_modules.forEach((module) => {
        map_contents += `  ${module.toplevel_function_name},\n`;
    });
    map_contents += "};\n";
    map_contents += "int _ejs_num_modules = sizeof(_ejs_modules) / sizeof(_ejs_modules[0]);\n\n";

    native_modules.forEach((module) => {
        map_contents += `extern ejsval ${module.init_function} (ejsval exports);\n`;
    });

    map_contents += "EJSExternalModule _ejs_external_modules[] = {\n";
    native_modules.forEach((module) => {
        map_contents += `  { "@${module.module_name}", ${module.init_function}, 0 },\n`;
    });
    map_contents += "};\n";
    map_contents +=
        "int _ejs_num_external_modules = sizeof(_ejs_external_modules) / sizeof(_ejs_external_modules[0]);\n";

    let entry_module = file_args![0]!;
    if (entry_module.lastIndexOf(".js") == entry_module.length - 3)
        entry_module = entry_module.substring(0, entry_module.length - 3);
    map_contents += `const EJSModule* entry_module = &${
        js_modules.get(entry_module)!.module_name
    };\n`;

    map_contents += "};";
    fs.writeFileSync(map_path, map_contents);

    temp_files.push(map_path);

    return map_path;
}

function do_final_link(main_file: string, modules: Map<string, ModuleInfo>): void {
    const js_modules = new Map<string, JSModuleInfo>();
    const native_modules = new Map<string, NativeModuleInfo>();
    modules.forEach((m, k) => {
        if (m.isNative()) {
            native_modules.set(k, m as NativeModuleInfo);
        } else {
            js_modules.set(k, m as JSModuleInfo);
        }
    });

    let map_filename = generate_import_map(js_modules, native_modules);

    process.env["PATH"] = `${target_path_prepend(target_triple)}:${process.env["PATH"]}`;

    let output_filename = options.output_filename || `${main_file}.exe`;
    let clang_args = target_link_args(target_triple).concat(
        [`-DEJS_BITS_PER_WORD=${target_triple.pointerSize()}`, "-o", output_filename].concat(
            o_filenames
        )
    );
    if (target_triple.isLittleEndian()) clang_args.unshift("-DIS_LITTLE_ENDIAN=1");

    clang_args.push(`-I${relative_to_ejs_exe(options.srcdir ? "./runtime" : "../include")}`);

    clang_args.push(map_filename);

    clang_args = clang_args.concat(relative_to_ejs_exe(target_libecho(target_triple)));
    clang_args = clang_args.concat(relative_to_ejs_exe(target_extra_libs(target_triple)));

    const seen_native_modules = new Set<string>();
    native_modules.forEach((module) => {
        // don't include native modules more than once
        module.module_files.forEach((mf) => {
            if (seen_native_modules.has(mf)) return;

            seen_native_modules.add(mf);

            clang_args.push(
                path.resolve(
                    module.ejs_dir,
                    options.srcdir ? "." : `${target_triple.arch}-${target_triple.os}`,
                    mf
                )
            );
        });

        clang_args = clang_args.concat(module.link_flags.replace("\n", " ").split(" "));
    });

    clang_args = clang_args.concat(target_libraries(target_triple));

    if (!options.quiet) options.stdout_writer.write(`${bold()}LINK${reset()} ${output_filename}`);

    debug.log(1, `executing '${target_linker} ${clang_args.join(" ")}'`);

    if (typeof __ejs != "undefined") {
        spawn(target_linker, clang_args);
        // we ignore leave_tmp_files here
        if (!options.quiet) console.warn(`${bold()}done.${reset()}`);
    } else {
        let clang = spawn(target_linker, clang_args);
        clang.stderr.on("data", (data) => console.warn(`${data}`));
        clang.on("exit", (/* XXX code*/) => {
            if (!options.leave_temp_files) {
                cleanup(() => {
                    if (!options.quiet) console.warn(`${bold()}done.${reset()}`);
                });
            }
        });
    }
}

function cleanup(done: () => void): void {
    let files_to_delete = temp_files.length;
    temp_files.forEach((filename) => {
        fs.unlink(filename, (/* XXX err*/) => {
            files_to_delete = files_to_delete - 1;
            if (files_to_delete === 0) done();
        });
    });
}

const main_file = file_args[0]!;

if (!options.srcdir) options.native_module_dirs.push(relative_to_ejs_exe("../lib"));
let files = gatherAllModules(file_args, options, target_triple);
debug.log(1, () => {
    dumpModules();
    return "";
});
let allModules = getAllModules();

// now compile them
//
// reverse the list so the main program is the first thing we compile
files.reverse();
let files_count = files.length;
const compileNextFile = (): void => {
    if (files.length === 0) {
        do_final_link(main_file, allModules);
        return;
    }
    const f = files.pop()!;
    compileFile(
        f.file_name,
        f.file_ast,
        allModules,
        files_count,
        files_count - files.length,
        compileNextFile
    );
};
compileNextFile();
