function hello_outer (x) {
  return function () {
      print (x);
  };
}

var hello_inner = hello_outer('hello world');
hello_inner();
