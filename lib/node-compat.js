import { Set } from 'set-es6';

export const modules = {
    fs:   new Set([ "statSync", "readFileSync", "createWriteStream" ]),
    os:   new Set([ "arch", "platform", "tmpdir" ]),
    path: new Set([ "dirname", "basename", "extname", "resolve", "relative", "join" ]),
    child_process: new Set([ "spawn", "stdout", "stderr" ])
};
