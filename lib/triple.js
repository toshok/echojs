import * as os from "@node-compat/os";
import { ABI } from "./abi";
import { SRetABI } from "./sret-abi";

export class Triple {
    constructor(arch, vendor, os) {
        this.arch = arch;
        this.vendor = vendor;
        this.os = os;
    }

    toString() {
        return `${this.arch}-${this.vendor}-${this.os}`;
    }

    isLittleEndian() {
        switch (this.arch) {
            case "x86_64":
            case "x86":
            case "arm":
            case "arm64":
                return true;
            default:
                throw new Error(`unknown endianness for arch: ${this.arch}`);
        }
    }

    pointerSize() {
        switch (this.arch) {
            case "x86_64":
            case "arm64":
                return 64;
            case "x86":
            case "arm":
                return 32;
            default:
                throw new Error(`unknown pointer size for arch: ${this.arch}`);
        }
    }
    
    llcArch() {
        switch (this.arch) {
            case "x86_64":
                return "x86-64";
            case "x86":
                return "x86";
            case "arm64":
                return "arm64";
            case "arm":
                return "arm";
            default:
                throw new Error(`unknown llc arch for arch: ${this.arch}`);
        }
    }

    clangArch() {
        switch (this.arch) {
            case "x86_64":
                return "x86_64";
            case "x86":
                return "i386";
            case "arm64":
                return "arm64";
            case "arm":
                return "armv7";
            default:
                throw new Error(`unknown clang arch for arch: ${this.arch}`);
        }
    }

    abi() {
        switch (this.arch) {
            case "x86_64":
            case "arm64":
                return new ABI();
            case "x86":
            case "arm":
                return new SRetABI();
            default:
                throw new Error(`unknown abi for arch: ${this.arch}`);
        }
    }

    static fromProcess() {
        let vendor = "unknown";;

        let arch = os.arch();
        if (arch === "x64") arch = "x86_64";
        if (arch === "ia32") arch = "x86";

        let platform = os.platform();
        if (platform === "darwin") {
            vendor = "apple";
        }

        return new Triple(arch, vendor, platform);
    }

    static fromString(str) {
        let split = str.split("-");
        let arch, vendor, os;
        if (split.length == 2) {
            arch = "unknown";
            [vendor, os] = split;
        } else if (split.length == 3) {
            [arch, vendor, os] = split;
        } else {
            throw new Error(`invalid triple: ${str}`);
        }
        return new Triple(arch, vendor, os);
    }
}
