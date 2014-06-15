try {
  var x = 'y';
  console.log (({ [x]: 1 })['y'] === 1);
}
catch(e) {
  console.log(e);
}
