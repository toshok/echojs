// Generates the static-atom tables: reads an EJS_ATOM(...) header and
// emits the ucs2 literals, EJSPrimString/ejsval definitions, and the
// _ejs_init_static_strings() initializer.  Compiled to JS by
// //runtime:gen-atoms-js (tsc) and run under node at build time by the
// //runtime:atoms and //ejs-llvm:atoms genrules.

import * as fs from "fs";

const input = process.argv[2];
if (input == null) {
    console.error("usage: gen-atoms <atoms.h>");
    process.exit(1);
}

const atom_def = fs.readFileSync(input, "utf-8");

const atom_lines = atom_def.split("\n");
const new_lines: string[] = [];
const atom_names: string[] = [];

for (const atom_line of atom_lines) {
    let atom: string | null = null;
    let atom_name: string | null = null;

    const match = atom_line.match(/^EJS_ATOM\((.*)\)$/);
    const match2 = atom_line.match(/^EJS_ATOM2\((.*),(.*)\)$/);
    const match3 = atom_line.match(/^EJS_ATOM2\(,(.*)\)$/);

    if (match) {
        atom = match[1] ?? "";
        atom_name = match[1] ?? "";
    } else if (match2) {
        atom = match2[1] ?? "";
        atom_name = match2[2] ?? "";
    } else if (match3) {
        atom = "";
        atom_name = match3[1] ?? "";
    }

    if (atom === null || atom_name === null) {
        new_lines.push(atom_line);
        continue;
    }

    // output the ucs2 literal for the atom
    let line = `const jschar _ejs_ucs2_${atom_name}[] EJSVAL_ALIGNMENT = { `;
    for (let cn = 0, ce = atom.length; cn < ce; cn++) {
        const code = atom.charCodeAt(cn);
        const hex = code.toString(16);
        if (code < 0x10) line += `0x000${hex}`;
        else if (code < 0x100) line += `0x00${hex}`;
        else if (code < 0x1000) line += `0x0${hex}`;
        else line += `0x${hex}`;

        line += ", ";
    }
    line += "0x0000 };";
    new_lines.push(line);

    new_lines.push(
        `static EJSPrimString _ejs_primstring_${atom_name} EJSVAL_ALIGNMENT = { .gc_header = (EJS_STRING_FLAT<<EJS_GC_USER_FLAGS_SHIFT), .length = ${atom.length}, .data = { .flat = NULL }};`
    );
    new_lines.push(`ejsval _ejs_atom_${atom_name} EJSVAL_ALIGNMENT;`);

    atom_names.push(atom_name);
}

console.log(new_lines.join("\n"));

console.log("void _ejs_init_static_strings() {");
for (const atom of atom_names) {
    console.log(`    _ejs_primstring_${atom}.data.flat = (jschar*)_ejs_ucs2_${atom};`);
    console.log(
        `    _ejs_atom_${atom} = STRING_TO_EJSVAL((EJSPrimString*)&_ejs_primstring_${atom});`
    );
}
console.log("}");
