// xfail: generator support isn't 100%

// "can't use 'this' with 'new'" from kangax
function * generator(){
  yield this.x; yield this.y;
};
try {
  (new generator()).next();
  console.log(false);
}
catch (e) {
  console.log(true);
}
