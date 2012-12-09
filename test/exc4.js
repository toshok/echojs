function f () {
  try {
    return "goodbye world";
  }
  finally {
    console.log("hello world");
  }
}

console.log (f());
