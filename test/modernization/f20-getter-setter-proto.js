let o = {get x() { return 1; }, __proto__: {z: 9}}; console.log(o.x, o.z);
