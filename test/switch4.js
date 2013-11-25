
function test(s) {
  switch (s) {
    case "undefined": console.log ("hi"); break;
    default: console.log ("bye"); break;
  }
}

test("undefined");
test("xundefined".substr(1, 15));
test("undef"+"ined");
