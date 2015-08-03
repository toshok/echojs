// xfail: node passes ['node','argv1.js'], we return ['argv1.js.exe']

console.log ('length = ' + process.argv.length);
for (var i = 0; i < process.argv.length; i ++)
  console.log ('argv['+i+'] = ' + process.argv[i]);
