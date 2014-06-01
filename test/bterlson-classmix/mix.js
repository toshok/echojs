function objectUnion(target, objs) {
    objs = [target].concat(objs);

    var proxyHandler = {
        getOwnPropertyDescriptor(t, k) {
            var obj = findProp(k);
            if(obj) return Object.getOwnPropertyDescriptor(obj, k);
        },
        has(t, k) {
            var obj = findProp(k);
            return !!obj;
        },
        get(t, k, r) {
            var obj = findProp(k);
            if(obj) return Reflect.get(obj, k, r);
        }
    }

    return new Proxy(target, proxyHandler);

    function findProp(name) {
/* ejs doesn't support for-of loops yet, so use this version instead of the nicer one below */
        for (var i = 0, e = objs.length; i < e; i ++) {
          var obj = objs[i];
          if(name in obj) return obj;
        }
/*
        for(var obj of objs) {
            if(name in obj) return obj;
        }
*/

        return null;
    }
}

export function mix(... classes) {
    var C = function() {
        classes.forEach(c => this[c.name].apply(this, arguments))
    };

    classes.forEach(c => C.prototype[c.name] = c);

    C.prototype = objectUnion(C.prototype, classes.map(c => c.prototype));

    return objectUnion(C, classes);
}
