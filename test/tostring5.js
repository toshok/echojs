// xfail: Date.prototype is an ordinary object in ES2015+ (node throws TypeError on Date.prototype.toString()); ejs still gives it a [[DateValue]].  stale-baseline zombie flushed by runtime-P3

console.log("date");
console.log(Date.prototype.toString());
console.log("object date.proto.tostring");
try {
    console.log({ toString: Date.prototype.toString }.toString());
} catch (e) {
    console.log(e);
}
console.log("number");
console.log(Number.prototype.toString());
console.log("string");
console.log(String.prototype.toString());
console.log("boolean");
console.log(Boolean.prototype.toString());
console.log("regexp");
console.log(RegExp.prototype.toString());
