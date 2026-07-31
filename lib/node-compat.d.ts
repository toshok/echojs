/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

// The "@node-compat/*" modules resolve to node's own os/path/fs/... when
// node-hosted (the babel step rewrites the specifier) and to the
// node-compat native module when self-hosted.  Their surface is node's.

declare module "@node-compat/os" {
    const os: typeof import("os");
    export = os;
}
declare module "@node-compat/path" {
    const path: typeof import("path");
    export = path;
}
declare module "@node-compat/fs" {
    const fs: typeof import("fs");
    export = fs;
}
declare module "@node-compat/child_process" {
    const child_process: typeof import("child_process");
    export = child_process;
}
