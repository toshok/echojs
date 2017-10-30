(function() {
    exports.Stack = (function() {
        function Stack(initial) {
            this.stack = [];
            if (initial) this.stack.unshift(initial);
        }

        Stack.prototype.push = function(o) {
            this.stack.unshift(o);
        };

        Stack.prototype.pop = function() {
            if (this.stack.length === 0) throw new Error("Stack is empty");
            return this.stack.shift();
        };

        // add a 'top' property to make things a little clearer/nicer to read in the compiler

        Object.defineProperty(Stack.prototype, "top", {
            get: function() {
                if (this.stack.length === 0) throw new Error("Stack is empty");
                return this.stack[0];
            },
        });

        // and a 'depth' property
        Object.defineProperty(Stack.prototype, "depth", {
            get: function() {
                return this.stack.length;
            },
        });

        return Stack;
    })();
})();
