
class TestClass {
  constructor () {
    console.log ("Hello world");
  }

  sayGoodbye() { console.log ("Goodbye world"); }

  static saySomethingStatically() { console.log ("hello?"); }

  get prop() { return "hi"; }
  static get prop() { return "static hi"; }
}


let t = new TestClass();
t.sayGoodbye();
TestClass.saySomethingStatically();
console.log (t.prop);
console.log (TestClass.prop);
