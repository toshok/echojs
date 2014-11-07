/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

import * as os from            '@node-compat/os';
import * as path from          '@node-compat/path';
import * as fs from            '@node-compat/fs';
import * as child_process from '@node-compat/child_process';

let spawn = child_process.spawn;

import * as debug        from './lib/debug';
import { Set }           from './lib/set-es6';
import { compile }       from './lib/compiler';
import { dumpModules, getAllModules, gatherAllModules } from './lib/passes/gather-imports';

import { bold, reset, genFreshFileName } from './lib/echo-util';

// if we're running under coffee/node, argv will be ["coffee", ".../ejs", ...]
// if we're running the compiled ejs.exe, argv will be [".../ejs.js.exe", ...]
let slice_count = typeof(__ejs) === "undefined" ? 2 : 1;
let argv = process.argv.slice(slice_count);

let temp_files = [];

let host_arch = os.arch();
if (host_arch === "x64")
    host_arch = "x86-64"; // why didn't we just standardize on 'amd64'?  sigh
if (host_arch === "ia32")
    host_arch = "x86";

let host_platform = os.platform();

let options = {
    // our defaults:
    debug_level: 0,
    debug_passes: new Set(),
    warn_on_undeclared: false,
    frozen_global: false,
    record_types: false,
    output_filename: null,
    show_help: false,
    leave_temp_files: false,
    target_arch: host_arch,
    target_platform: host_platform,
    external_modules: [],
    extra_clang_args: "",
    ios_sdk: "7.1",
    ios_min: "7.0",
    target_pointer_size: 64
};

function add_external_module (modinfo) {
    let [library,module_name,module_entrypoint,link_flags] = modinfo.split(',');
    options.external_modules.push ({ library, module_name, module_entrypoint, link_flags });
}

let arch_info = {
    "x86-64": { pointer_size: 64, little_endian: true, llc_arch: "x86-64",  clang_arch: "x86_64" },
    x86:      { pointer_size: 32, little_endian: true, llc_arch: "x86",     clang_arch: "i386" },
    arm:      { pointer_size: 32, little_endian: true, llc_arch: "arm",     clang_arch: "armv7" },
    aarch64:  { pointer_size: 64, little_endian: true, llc_arch: "aarch64", clang_arch: "aarch64" }
};

function set_target_arch(arch) {
    if (options.target)
        throw new Error("--arch and --target cannot be specified at the same time");

    // we accept some arch aliases

    if (arch === "amd64")  arch = "x86-64";
    if (arch === "x86_64") arch = "x86-64";
    if (arch === "i386")   arch = "x86";

    if (! (arch in arch_info))
        throw new Error(`invalid arch '${arch}'.`);
    
    options.target_arch         = arch;
    options.target_pointer_size = arch_info[arch].pointer_size;
}

function set_target(platform, arch) {
    options.target_platform     = platform;
    options.target_arch         = arch;
    options.target_pointer_size = arch_info[arch].pointer_size;
}

function set_target_alias(alias) {
    const target_aliases = {
        linux_amd64: { platform: "linux",  arch: "x86-64" },
        osx:         { platform: "darwin", arch: "x86-64" },
        sim:         { platform: "darwin", arch: "x86" },
        dev:         { platform: "darwin", arch: "arm" }
    };

    if (!(alias in target_aliases))
        throw new Error(`invalid target alias '${alias}'.`);

    options.target = alias;
    set_target(target_aliases[alias].platform, target_aliases[alias].arch);
}

function set_extra_clang_args(arginfo) {
    options.extra_clang_args = arginfo;
}

function increase_debug_level() {
    options.debug_level += 1;
}

function add_debug_after_pass(passname) {
    options.debug_passes.add(passname);
}

let args = {
    "-q": {
        flag:    "quiet",
        help:    "don't output anything during compilation except errors."
    },
    "-d": {
        handler: increase_debug_level,
        handlerArgc: 0,
        help:    "debug output.  more instances of this flag increase the amount of spew."
    },
    "--debug-after": {
        handler: add_debug_after_pass,
        handlerArgc: 1,
        help:    "dump the IR tree after the named pass"
    },
    "-o": {
        option:  "output_filename",
        help:    "name of the output file."
    },
    "--leave-temp": {
        flag:    "leave_temp_files",
        help:    "leave temporary files in $TMPDIR from compilation"
    },
    "--module": {
        handler: add_external_module,
        handlerArgc: 1,
        help:    "--module library.a,module-name,module_init,link_flags"
    },
    "--help": {
        flag:    "show_help",
        help:    "output this help info."
    },
    "--extra-clang-args": {
        handler: set_extra_clang_args,
        handlerArgc: 1,
        help:    "extra arguments to pass to the clang command (used to compile the .s to .o)"
    },
    "--record-types": {
        flag:    "record_types",
        help:    "generates an executable which records types in a format later used for optimizations."
    },
    "--frozen-global": {
        flag:    "frozen_global",
        help:    "compiler acts as if the global object is frozen after initialization, allowing for faster access."
    },
    "--warn-on-undeclared": {
        flag:    "warn_on_undeclared",
        help:    "accesses to undeclared identifiers result in warnings (and global accesses).  By default they're an error."
    },
    "--arch": {
        handler: set_target_arch,
        handlerArgc: 1,
        help:    "--arch x86-64|x86|arm|aarch64"
    },
    "--target": {
        handler: set_target_alias,
        handlerArgc: 1,
        help:    "--target linux_amd64|osx|sim|dev"
    },
    "--ios-sdk": {
        option:  "ios_sdk",
        help:    "the version of the ios sdk to use.  useful if more than one is installed.  Default is 7.0."
    },
    "--ios-min": {
        option:  "ios_min",
        help:    "the minimum version of ios to support.  Default is 7.0."
    }
};

function output_usage() {
    console.warn('Usage:');
    console.warn('   ejs [options] file1.js file2.js file.js ...');
}

function output_options() {
    console.warn('Options:');
    for (let a of args.keys())
        console.warn(`   ${a}:  ${args[a].help}`);
}

// default to the host platform/arch
set_target(host_platform, host_arch);

let file_args;

if (argv.length > 0) {
    let skipNext = 0;
    for (let ai = 0, ae = argv.length; ai < ae; ai ++) {
        if (skipNext > 0) {
            skipNext -= 1;
        }
        else {
            if (args[argv[ai]]) {
                let o = args[argv[ai]];
                if (o.flag) {
                    options[o.flag] = true;
                }
                else if (o.option) {
                    options[o.option] = argv[++ai];
                    skipNext = 1;
                }
                else if (o.handler) {
                    let handler_args = [];
                    for (let i = 0, e = o.handlerArgc; i < e; i ++)
                        handler_args.push(argv[++ai]);
                    o.handler.apply(null, handler_args);
                }
            }
            else {
                // end of options signals the rest of the array is files
                file_args = argv.slice(ai);
                break;
            }
        }
    }
}

if (options.show_help) {
    output_usage();
    console.warn('');
    output_options();
    process.exit(0);
}

if (!file_args || file_args.length === 0) {
    output_usage();
    process.exit(0);
}

if (!options.quiet) {
    console.log(`running on ${host_platform}-${host_arch}`);
    console.log(`generating code for ${options.target_platform}-${options.target_arch}`);
}

debug.setLevel(options.debug_level);

let files_remaining = 0;

let o_filenames = [];

let base_filenames = file_args.map(path.basename);

let compiled_modules = [];

let sim_base="/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform";
let dev_base="/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform";

let sim_bin=`${sim_base}/Developer/usr/bin`;
let dev_bin=`${dev_base}/Developer/usr/bin`;

function target_llc_args(platform, arch) {
    let args = [`-march=${arch_info[options.target_arch].llc_arch}`, "-disable-fp-elim" ];
    if (arch === "arm")
        args = args.concat(["-mtriple=thumbv7-apple-ios", "-mattr=+v6", "-relocation-model=pic", "-soft-float" ]);
    if (arch === "aarch64")
        args = args.concat(["-mtriple=thumbv7s-apple-ios", "-mattr=+fp-armv8", "-relocation-model=pic" ]);
    return args;
}

let target_linker = "clang++";

function target_link_args(platform, arch) {
    let args = [ "-arch", arch_info[options.target_arch].clang_arch ];

    if (platform === "linux") {
        // on ubuntu 14.04, at least, clang spits out a warning about this flag being unused (presumably because there's no other arch)
        if (arch === "x86-64") return [];
        return args;
    }

    if (platform === "darwin") {
        if (arch === "x86-64") return args;
        if (arch === "x86")
            return args.concat([ "-isysroot", `${sim_base}/Developer/SDKs/iPhoneSimulator${options.ios_sdk}.sdk`, `-miphoneos-version-min=${options.ios_min}` ]);
        return args.concat(["-isysroot", `${dev_base}/Developer/SDKs/iPhoneOS${options.ios_sdk}.sdk`, `-miphoneos-version-min=${options.ios_min}` ]);
    }

    return [];
}


function target_libraries(platform, arch) {
    if (platform === "linux") return [ "-lpthread", "-luv" ];

    if (platform === "darwin") {
        let rv = [ "-framework", "Foundation" ];

        // for osx we only need Foundation and AppKit
        if (arch === "x86-64") return rv.concat([ "-framework" , "AppKit" ]);

        // for any other darwin we're dealing with ios, so...
        return rv.concat([ "-framework", "UIKit", "-framework", "GLKit", "-framework", "OpenGLES", "-framework", "CoreGraphics" ]);
    }
    return [];
}


function target_libecho(platform, arch) {
    if (platform === "darwin") {
        if (arch === "x86-64") return "runtime/libecho.a";

        return "runtime/libecho.a.ios";
    }

    return "runtime/libecho.a";
}


function target_extra_libs(platform, arch) {
    if (platform === "linux")   return "external-deps/pcre-linux/.libs/libpcre16.a";

    if (platform === "darwin") {
        if (arch === "x86-64")  return "external-deps/pcre-osx/.libs/libpcre16.a";
        if (arch === "x86")     return "external-deps/pcre-iossim/.libs/libpcre16.a";
        if (arch === "arm")     return "external-deps/pcre-iosdev/.libs/libpcre16.a";
        if (arch === "aarch64") return "external-deps/pcre-iosdevaarch64/.libs/libpcre16.a";
    }

    throw new Error("no pcre for this platform");
}

function target_path_prepend (platform, arch) {
    if (platform === "darwin") {
        if (arch === "x86") return sim_bin;
        if (arch === "arm" || arch === "aarch64") return dev_bin; 
    }
    return "";
}

let llvm_commands = {};
for (let x of ["opt", "llc", "llvm-as"])
    llvm_commands[x]=`${x}${process.env.LLVM_SUFFIX || ''}`;

function compileFile(filename, parse_tree, modules) {
    let base_filename = genFreshFileName(path.basename(filename));

    if (!options.quiet) {
        let suffix = options.debug_level > 0 ? ` -> ${base_filename}` : '';
        console.warn (`${bold()}COMPILE${reset()} ${filename}${suffix}`);
    }

    let compiled_module;
//    try {
        compiled_module = compile(parse_tree, base_filename, filename, modules, options);
//    }
//    catch (e) {
//        console.warn(`${e}`);
//        if (options.debug_level == 0) process.exit(-1);
//        throw e;
//    }

    let ll_filename     = `${os.tmpdir()}/${base_filename}-${options.target_platform}-${options.target_arch}.ll`;
    let bc_filename     = `${os.tmpdir()}/${base_filename}-${options.target_platform}-${options.target_arch}.bc`;
    let ll_opt_filename = `${os.tmpdir()}/${base_filename}-${options.target_platform}-${options.target_arch}.ll.opt`;
    let o_filename      = `${os.tmpdir()}/${base_filename}-${options.target_platform}-${options.target_arch}.o`;

    temp_files.push(ll_filename, bc_filename, ll_opt_filename, o_filename);
    
    let llvm_as_args = [`-o=${bc_filename}`, ll_filename];
    let opt_args     = ["-O2", "-strip-dead-prototypes", "-S", `-o=${ll_opt_filename}`, bc_filename];
    let llc_args     = target_llc_args(options.target_platform,options.target_arch).concat(["-filetype=obj", `-o=${o_filename}`, ll_opt_filename]);

    debug.log (1, `writing ${ll_filename}`);
    compiled_module.writeToFile(ll_filename);
    debug.log (1, `done writing ${ll_filename}`);

    compiled_modules.push({ filename: options.basename ? path.basename(filename) : filename, module_toplevel: compiled_module.toplevel_name });

    // in ejs spawn is synchronous.
    spawn(llvm_commands["llvm-as"], llvm_as_args);
    spawn(llvm_commands["opt"], opt_args);
    spawn(llvm_commands["llc"], llc_args);
    o_filenames.push(o_filename);
}

function relative_to_ejs_exe(n) {
    return path.resolve(path.dirname(process.argv[typeof(__ejs) === 'undefined' ? 1 : 0]), n);
}


function generate_import_map (modules) {
    let sanitize = (filename, c_callable) => {
        let sfilename = filename.replace(/\.js$/, "");
        if (c_callable)
            sfilename = sfilename.replace(/[.,-\/\\]/g, "_"); // this is insanely inadequate
        return sfilename;
    };
    let map_path = `${os.tmpdir()}/${genFreshFileName(path.basename(main_file))}-import-map.cpp`;
    let map = fs.createWriteStream(map_path);
    map.write (`#include "${relative_to_ejs_exe('runtime/ejs-module.h')}"\n`);
    map.write ('extern "C" {\n');

    modules.forEach( (module) => {
        map.write(`extern EJSModule ${module.module_name};\n`);
        map.write(`extern ejsval ${module.toplevel_function_name} (ejsval env, ejsval _this, uint32_t argc, ejsval *args);\n`);
    });


    map.write ("EJSModule* _ejs_modules[] = {\n");
    modules.forEach ( (module) => {
        map.write(`  &${module.module_name},\n`);
    });
    map.write ("};\n");

    map.write('ejsval (*_ejs_module_toplevels[])(ejsval, ejsval, uint32_t, ejsval*) = {\n');
    modules.forEach ( (module) => {
        map.write(`  ${module.toplevel_function_name},\n`);
    });
    map.write ("};\n");
    map.write('int _ejs_num_modules = sizeof(_ejs_modules) / sizeof(_ejs_modules[0]);\n\n');

    options.external_modules.forEach ((module) => {
        map.write(`extern ejsval ${module.module_entrypoint} (ejsval exports);\n`);
    });

    map.write('EJSExternalModule _ejs_external_modules[] = {\n');
    options.external_modules.forEach ( (module) => {
        map.write(`  { \"${module.module_name}\", ${module.module_entrypoint}, 0 },\n`);
    });
    map.write('};\n');
    map.write('int _ejs_num_external_modules = sizeof(_ejs_external_modules) / sizeof(_ejs_external_modules[0]);\n');

    let entry_module = file_args[0];
    if (entry_module.lastIndexOf(".js") == entry_module.length - 3)
        entry_module = entry_module.substring(0, entry_module.length-3);
    map.write(`const EJSModule* entry_module = &${modules.get(entry_module).module_name};\n`);

    map.write("};");
    map.end();

    temp_files.push(map_path);
    
    return map_path;
}


function do_final_link(main_file, modules) {
    let map_filename = generate_import_map(modules);

    process.env.PATH = `${target_path_prepend(options.target_platform,options.target_arch)}:${process.env.PATH}`;

    let output_filename = options.output_filename || `${main_file}.exe`;
    let clang_args = target_link_args(options.target_platform, options.target_arch).concat([`-DEJS_BITS_PER_WORD=${options.target_pointer_size}`, "-o", output_filename].concat(o_filenames));
    if (arch_info[options.target_arch].little_endian)
        clang_args.unshift("-DIS_LITTLE_ENDIAN=1");
    
    // XXX we shouldn't need this, but build is failing while compiling the require map
    clang_args.push("-I.");
    
    clang_args.push(map_filename);
    
    clang_args.push(relative_to_ejs_exe(target_libecho(options.target_platform, options.target_arch)));
    clang_args.push(relative_to_ejs_exe(target_extra_libs(options.target_platform, options.target_arch)));
    
    let seen_external_modules = new Set();
    options.external_modules.forEach ((extern_module) => {
        // don't include external modules more than once
        if (seen_external_modules.has(extern_module.library)) return;

        seen_external_modules.add(extern_module.library);
                
        clang_args.push(extern_module.library);
        // very strange, not sure why we need this \n
        clang_args = clang_args.concat(extern_module.link_flags.replace('\n', ' ').split(" "));
    });

    clang_args = clang_args.concat(target_libraries(options.target_platform, options.target_arch));

    if (!options.quiet) console.warn(`${bold()}LINK${reset()} ${output_filename}`);
    
    debug.log (1, `executing '${target_linker} ${clang_args.join(' ')}'`);
    
    if (typeof(__ejs) != "undefined") {
        spawn(target_linker, clang_args);
        // we ignore leave_tmp_files here
        if (!options.quiet) console.warn(`${bold()}done.${reset()}`);
    }
    else {
        let clang = spawn(target_linker, clang_args);
        clang.stderr.on("data", (data) => { console.warn(`${data}`); });
        clang.on("exit", (code) => {
            if (!options.leave_temp_files) {
                cleanup ( () => {
                    if (!options.quiet) console.warn(`${bold()}done.${reset()}`);
                });
            }
        });
    }
}

function cleanup(done) {
    let files_to_delete = temp_files.length;
    temp_files.forEach ( (filename) => {
        fs.unlink(filename, (err) => {
            files_to_delete = files_to_delete - 1;
            if (files_to_delete === 0) done();
        });
    });
}

let main_file = file_args[0];

let files = gatherAllModules(file_args, options);
debug.log (1, () => dumpModules());
let allModules = getAllModules();

// now compile them
//
for (let f of files)
    compileFile(f.file_name, f.file_ast, allModules);
do_final_link(main_file, allModules);
