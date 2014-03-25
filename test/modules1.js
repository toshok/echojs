
import methodInFoo1 from "modules1/foo1"
import methodInFoo2 from "modules1/foo2"
import foo3 from "modules1/foo3"

methodInFoo1();
methodInFoo2();
foo3.methodInFoo3_1();
foo3.methodInFoo3_2();
foo3.methodInFoo3_3();

if (foo3.internalMethodInFoo3_3)
  console.log ("failure is always an option");
console.log ("hi");
