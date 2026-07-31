let p = new Proxy({}, {get: () => 7}); console.log(p.anything);
