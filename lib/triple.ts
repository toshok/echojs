/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

import * as os from "@node-compat/os";
import { ABI } from "./abi";
import { SRetABI } from "./sret-abi";

export interface TripleParts {
    arch: string;
    vendor: string;
    os: string;
    env?: string | undefined;
}

export class Triple {
    arch: string;
    vendor: string;
    os: string;
    env: string | undefined;

    constructor({ arch, vendor, os, env }: TripleParts) {
        this.arch = arch;
        this.vendor = vendor;
        this.os = os;
        this.env = env;
    }

    toString(): string {
        const envSuffix = this.env ? `-${this.env}` : "";
        return `${this.arch}-${this.vendor}-${this.os}${envSuffix}`;
    }

    // same as toString but we drop the vendor
    toShortString(): string {
        const envSuffix = this.env ? `-${this.env}` : "";
        return `${this.arch}-${this.os}${envSuffix}`;
    }

    isLittleEndian(): boolean {
        switch (this.arch) {
            case "x86_64":
            case "x86":
            case "arm":
            case "arm64":
            case "aarch64":
                return true;
            default:
                throw new Error(`unknown endianness for arch: ${this.arch}`);
        }
    }

    pointerSize(): number {
        switch (this.arch) {
            case "x86_64":
            case "arm64":
            case "aarch64":
                return 64;
            case "x86":
            case "arm":
                return 32;
            default:
                throw new Error(`unknown pointer size for arch: ${this.arch}`);
        }
    }

    // the llvm target triple for emitted modules.  without this (and the
    // data layout below) set on the module, opt folds struct GEPs using
    // llvm's default layout, where i64 is only 4-byte aligned -- which
    // computes different field offsets than the C compiler does for the
    // runtime (e.g. EJSModule.exports), corrupting every module slot
    // access and blinding the GC to module-referenced objects.
    llvmTriple(): string {
        switch (this.os) {
            case "macos":
                return `${this.arch}-apple-macosx`;
            case "ios":
                return `${this.arch}-apple-ios`;
            case "linux":
                return `${this.arch === "arm64" ? "aarch64" : this.arch}-unknown-linux-gnu`;
            default:
                throw new Error(`unknown llvm triple for os: ${this.os}`);
        }
    }

    // must match what clang uses for the runtime's target (see llvmTriple
    // above for why).
    dataLayout(): string {
        if (this.os === "macos" || this.os === "ios") {
            if (this.arch === "arm64" || this.arch === "aarch64")
                return "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-n32:64-S128-Fn32";
            return "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128";
        }
        if (this.arch === "x86_64")
            return "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128";
        return "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128";
    }

    llcArch(): string {
        switch (this.arch) {
            case "x86_64":
                return "x86-64";
            case "x86":
                return "x86";
            case "arm64":
                return "arm64";
            case "aarch64":
                return "aarch64";
            case "arm":
                return "arm";
            default:
                throw new Error(`unknown llc arch for arch: ${this.arch}`);
        }
    }

    clangArch(): string {
        switch (this.arch) {
            case "x86_64":
                return "x86_64";
            case "x86":
                return "i386";
            case "aarch64":
                return "aarch64";
            case "arm64":
                return "arm64";
            case "arm":
                return "armv7";
            default:
                throw new Error(`unknown clang arch for arch: ${this.arch}`);
        }
    }

    abi(): ABI {
        switch (this.arch) {
            case "x86_64":
            case "aarch64":
            case "arm64":
                return new ABI();
            case "x86":
            case "arm":
                return new SRetABI();
            default:
                throw new Error(`unknown abi for arch: ${this.arch}`);
        }
    }

    static fromProcess(): Triple {
        let vendor = "unknown";

        let arch: string = os.arch();
        if (arch === "x64") arch = "x86_64";
        if (arch === "ia32") arch = "x86";

        let _os: string = os.platform();
        if (_os === "darwin") {
            vendor = "apple";
            _os = "macos";
        }

        return new Triple({ arch, vendor, os: _os });
    }

    static fromString(str: string): Triple {
        const split = str.split("-");
        if (split.length === 2) {
            const [vendor, os] = split;
            return new Triple({ arch: "unknown", vendor: vendor!, os: os! });
        } else if (split.length === 3) {
            const [arch, vendor, os] = split;
            return new Triple({ arch: arch!, vendor: vendor!, os: os! });
        } else if (split.length === 4) {
            const [arch, vendor, os, env] = split;
            return new Triple({ arch: arch!, vendor: vendor!, os: os!, env });
        }
        throw new Error(`invalid triple: ${str}`);
    }

    static fromShortString(str: string): Triple {
        const split = str.split("-");
        let arch: string, os: string, env: string | undefined;
        if (split.length === 2) {
            [arch, os] = split as [string, string];
        } else if (split.length === 3) {
            [arch, os, env] = split as [string, string, string];
        } else {
            throw new Error(`invalid triple short string: ${str}`);
        }
        // try and fill in the vendor
        const vendor =
            os === "macos" || os === "ios" || os === "tvos" || os === "watchos"
                ? "apple"
                : "unknown";
        return new Triple({ arch, vendor, os, env });
    }
}
