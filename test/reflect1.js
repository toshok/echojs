function test() {
  var i, names =
    ["apply","construct","defineProperty","deleteProperty","getOwnPropertyDescriptor",
    "getPrototypeOf","has","isExtensible","set","setPrototypeOf"];

  if (typeof Reflect !== "object") {
    return false;
  }
  for (i = 0; i < names.length; i++) {
    if (!(names[i] in Reflect)) {
      return false;
    }
  }
  return true;
}
console.log(test());
