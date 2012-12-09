function f () {
  try {
    return "goodbye world";
  }
  catch (e) {
    console.log (e);
  }
  finally {
    console.log("hello world");
  }
}

console.log (f());
