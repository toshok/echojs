
module "foo" {
  export let x = "hello world";
}

import { x } from "foo";
console.log (x);
