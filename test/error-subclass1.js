
class MyError extends Error {
    constructor() {
        super();
    }
}
MyError.prototype[Symbol.toStringTag] = "MyOwnError";


console.log (typeof Error);

var e = new MyError();

try {
  throw e;
}
catch (exc) {
  console.log(Object.prototype.toString.call(exc));
}
