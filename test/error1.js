console.log(new EvalError());
console.log(new RangeError());
console.log(new ReferenceError());
console.log(new SyntaxError());
console.log(new TypeError());
console.log(new URIError());
console.log(new Error());

console.log(new EvalError().toString());
console.log(new RangeError().toString());
console.log(new ReferenceError().toString());
console.log(new SyntaxError().toString());
console.log(new TypeError().toString());
console.log(new URIError().toString());
console.log(new Error().toString());

var message = "yolo";
console.log(new EvalError(message).toString());
console.log(new RangeError(message).toString());
console.log(new ReferenceError(message).toString());
console.log(new SyntaxError(message).toString());
console.log(new TypeError(message).toString());
console.log(new URIError(message).toString());
console.log(new Error(message).toString());

console.log(EvalError.prototype === RangeError.prototype);
console.log(EvalError.prototype.toString === RangeError.prototype.toString);

console.log(EvalError.prototype === ReferenceError.prototype);
console.log(EvalError.prototype.toString === ReferenceError.prototype.toString);

console.log(EvalError.prototype === SyntaxError.prototype);
console.log(EvalError.prototype.toString === SyntaxError.prototype.toString);

console.log(EvalError.prototype === TypeError.prototype);
console.log(EvalError.prototype.toString === TypeError.prototype.toString);

console.log(EvalError.prototype === URIError.prototype);
console.log(EvalError.prototype.toString === URIError.prototype.toString);

console.log(EvalError.prototype === Error.prototype);
console.log(EvalError.prototype.toString === Error.prototype.toString);

console.log(EvalError.prototype.toString.call({ name: "YoloError", message: "uh..." }));
