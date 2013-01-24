#!/usr/bin/env node
var fs = require ("fs");

var atom_def = fs.readFileSync ("ejs-atoms.h", "utf-8");

var atom_lines = atom_def.split ("\n");
var new_lines = [];
var atom_names = [];

for (var i = 0, e = atom_lines.length; i < e; i ++) {
  var atom = null;
  var atom_name = null;

  var match  = atom_lines[i].match(/^EJS_ATOM\((.*)\)$/);
  var match2 = atom_lines[i].match(/^EJS_ATOM2\((.*),(.*)\)$/);
  var match3 = atom_lines[i].match(/^EJS_ATOM2\(,(.*)\)$/);

  if (match) {
    atom = match[1];
    atom_name = match[1];
  }
  else if (match2) {
    atom = match2[1];
    atom_name = match2[2];
  }
  else if (match3) {
    atom = "";
    atom_name = match3[1];
  }

  if (atom != null) {
    // output the ucs2 literal for the atom
    var line = "const jschar _ejs_ucs2_" + atom_name + "[] = { ";
    for (var cn = 0, ce = atom.length; cn < ce; cn ++) {
      line += "0x";
      var code = atom.charCodeAt(cn);
      if (code < 0x10)
	line += "000" + (new Number(code).toString(16));
      else if (code < 0x100)
	line += "00" + (new Number(code).toString(16));
      else if (code < 0x1000)
	line += "0" + (new Number(code).toString(16));
      else
	line += (new Number(code).toString(16));

      line += ", ";
    }
    line += " 0x0000 };";
    new_lines.push(line);

    new_lines.push("static EJSPrimString _ejs_string_" + atom_name + "= { .gc_header = (EJS_STRING_FLAT<<EJS_GC_USER_FLAGS_SHIFT), .length = " + atom.length + ", .data = { .flat = NULL }};");
    new_lines.push("ejsval _ejs_atom_" + atom_name + ";");

    atom_names.push(atom_name);
  }
  else {
    new_lines.push(atom_lines[i]);
  }
}

console.log (new_lines.join('\n'));

console.log ("static void _ejs_init_static_strings() {");
for (var an = 0, ae = atom_names.length; an < ae; an ++) {
  var atom = atom_names[an];
  console.log ("    _ejs_string_" + atom + ".data.flat = (jschar*)_ejs_ucs2_" + atom + ";");
  console.log ("    _ejs_atom_" + atom + " = STRING_TO_EJSVAL((EJSPrimString*)&_ejs_string_" + atom + "); _ejs_gc_add_named_root (_ejs_atom_" + atom + ");");
}
console.log ("}");