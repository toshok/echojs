import { shout, LEVEL } from "./reexport1-lib";
export { shout, LEVEL };
export function twice(s) { return shout(shout(s)); }
