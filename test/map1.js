
try {
  var m = new Map([], "foo");
  console.log ("failed - should have thrown");
}
catch (e) {
  console.log ("passed - threw exception in map ctor for !'is' comparator");
}
