try {
  throw "hello world"
}
catch (e) {
  console.log ("caught exception");
  console.log (e);
}
console.log ("done");
