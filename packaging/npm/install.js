// npm postinstall (release-P2): fetch the platform dist tarball and
// unpack it as ./dist, which bin/ejs.js execs out of.  The wrapper's
// version pins the release tag: v<version> must have uploaded
// echojs-<version>-<short-triple>.tar.gz assets (release-P3 automation
// owns making that true).
//
// EJS_NPM_TARBALL=/path/to/echojs-*.tar.gz overrides the download —
// the pre-release/CI path, and the escape hatch for offline installs.
"use strict";

const fs = require("fs");
const os = require("os");
const path = require("path");
const { spawnSync } = require("child_process");

const version = require("./package.json").version;

const SHORT_TRIPLES = {
    "darwin-arm64": "arm64-macos",
    "linux-arm64": "arm64-linux",
    "linux-x64": "x86_64-linux",
};

function fail(msg) {
    console.error(`echojs install: ${msg}`);
    process.exit(1);
}

async function main() {
    const key = `${process.platform}-${process.arch}`;
    const shortTriple = SHORT_TRIPLES[key];
    if (!shortTriple) {
        fail(
            `no prebuilt echojs for ${key} (supported: ${Object.keys(SHORT_TRIPLES).join(", ")})`
        );
    }

    const work = fs.mkdtempSync(path.join(os.tmpdir(), "echojs-npm-"));
    let tarball = process.env["EJS_NPM_TARBALL"];
    if (tarball) {
        if (!fs.existsSync(tarball)) fail(`EJS_NPM_TARBALL=${tarball} does not exist`);
        console.log(`echojs install: using ${tarball}`);
    } else {
        const url = `https://github.com/toshok/echojs/releases/download/v${version}/echojs-${version}-${shortTriple}.tar.gz`;
        console.log(`echojs install: fetching ${url}`);
        const res = await fetch(url);
        if (!res.ok) {
            fail(
                `download failed (${res.status} ${res.statusText}); ` +
                    `if you are offline or the release is missing, point EJS_NPM_TARBALL at a local tarball`
            );
        }
        tarball = path.join(work, "dist.tar.gz");
        fs.writeFileSync(tarball, Buffer.from(await res.arrayBuffer()));
    }

    const unpack = path.join(work, "unpack");
    fs.mkdirSync(unpack);
    const tar = spawnSync("tar", ["-xzf", tarball, "-C", unpack], { stdio: "inherit" });
    if (tar.status !== 0) fail(`tar extraction failed (${tar.status ?? tar.error})`);

    const entries = fs.readdirSync(unpack).filter((e) => e.startsWith("echojs-"));
    if (entries.length !== 1) fail(`expected one echojs-* directory in the tarball, got [${entries}]`);

    const dist = path.join(__dirname, "dist");
    fs.rmSync(dist, { recursive: true, force: true });
    fs.renameSync(path.join(unpack, entries[0]), dist);
    fs.rmSync(work, { recursive: true, force: true });

    const exe = path.join(dist, "bin", "ejs");
    if (!fs.existsSync(exe)) fail(`unpacked tarball has no bin/ejs`);
    fs.chmodSync(exe, 0o755);

    // dist-info records what the driver needs on top of this package
    // (a matching LLVM major); surface it once at install time
    const info = path.join(dist, "dist-info");
    const major = fs.existsSync(info)
        ? (fs.readFileSync(info, "utf8").match(/^EJS_LLVM_MAJOR=(\d+)$/m) || [])[1]
        : undefined;
    console.log(`echojs install: ${entries[0]} ready`);
    if (major) {
        console.log(
            `echojs install: compiling needs LLVM ${major} (opt/llc) — ` +
                `brew install llvm / https://apt.llvm.org; ejs verifies and fails loudly otherwise`
        );
    }
}

main().catch((e) => fail(e.stack || String(e)));
