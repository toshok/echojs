// generator: none
// from MDN

let products = new Proxy({
    browsers: ['Internet Explorer', 'Netscape']
},
{
    get: (obj, prop) => {
	if (prop === 'latestBrowser') {
	    return obj.browsers[obj.browsers.length - 1];
	}

	return obj[prop];
    },
    set: (obj, prop, value) => {
	if (prop === 'latestBrowser') {
	    obj.browsers.push(value);
	    return;
	}

	if (typeof value === 'string') {
	    value = [value];
	}

	obj[prop] = value;
    }
});

console.log(products.browsers);

products.browsers = 'Firefox';
console.log(products.browsers);

products.latestBrowser = 'Chrome';
console.log(products.browsers);

console.log(products.latestBrowser);
