// generator: none

// wrap in an iife so we get an arguments object
(function() {
    console.log(Object.getPrototypeOf(arguments)[Symbol.toStringTag]);
    console.log(Array.prototype[Symbol.toStringTag]);
    console.log(Boolean.prototype[Symbol.toStringTag]);
    console.log(Date.prototype[Symbol.toStringTag]);
    console.log(Error.prototype[Symbol.toStringTag]);
    console.log(Function.prototype[Symbol.toStringTag]);
    console.log(Map.prototype[Symbol.toStringTag]);
    console.log(Number.prototype[Symbol.toStringTag]);
    console.log(Object.prototype[Symbol.toStringTag]);
    console.log(Proxy.prototype[Symbol.toStringTag]);
    console.log(RegExp.prototype[Symbol.toStringTag]);
    console.log(Set.prototype[Symbol.toStringTag]);
    console.log(Symbol.prototype[Symbol.toStringTag]);

    console.log(JSON[Symbol.toStringTag]);
    console.log(Math[Symbol.toStringTag]);

    console.log(new Int8Array()[Symbol.toStringTag]);
    console.log(new Int16Array()[Symbol.toStringTag]);
    console.log(new Int32Array()[Symbol.toStringTag]);
    console.log(new Uint8Array()[Symbol.toStringTag]);
    console.log(new Uint16Array()[Symbol.toStringTag]);
    console.log(new Uint32Array()[Symbol.toStringTag]);
    console.log(new Float32Array()[Symbol.toStringTag]);
    console.log(new Float64Array()[Symbol.toStringTag]);

    console.log(ArrayBuffer.prototype[Symbol.toStringTag]);
    console.log(DataView.prototype[Symbol.toStringTag]);
})();
