// xfail: we throw an exception when typed array constructors are called as functions.

try { Int8Array(); console.log ("no exception"); } catch (e) { console.log (e.name); }
