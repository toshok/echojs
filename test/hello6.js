function hello_outer (x) {
  function hello_inner () {
      print (x);
  }

  hello_inner();
}

hello_outer('hello world');
