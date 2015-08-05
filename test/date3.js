// xfail: the first date is off by an hour.  timegm/localtime_r screwup?

console.log (new Date(2000, 8));
console.log (new Date(2000, 0));

