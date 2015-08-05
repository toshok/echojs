// xfail: node outputs {} for console.log(new Number(5)), while SM and JSC output '5'.  we err on the SM/JSC side of things here.
console.log (Number(5));
console.log (new Number(5));
console.log ((new Number(5)).valueOf());
