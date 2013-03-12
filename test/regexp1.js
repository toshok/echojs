
var re = new RegExp("hi");

console.log (re.test("hi there"));
console.log (re.test("ih there"));
console.log (re.test("there there, hi"));

console.log (re);
console.log (re.toString());

re = /hi/;

console.log (re.test("hi there"));
console.log (re.test("ih there"));
console.log (re.test("there there, hi"));

console.log (re);
console.log (re.toString());
