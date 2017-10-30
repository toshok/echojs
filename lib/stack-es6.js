/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */
export class Stack {
    constructor(initial) {
        this.stack = [];
        if (initial) this.stack.unshift(initial);
    }

    push(o) {
        this.stack.unshift(o);
    }

    pop() {
        if (this.stack.length === 0) throw new Error("Stack is empty");
        return this.stack.shift();
    }

    // add a 'top' property to make things a little clearer/nicer to read in the compiler
    get top() {
        if (this.stack.length === 0) throw new Error("Stack is empty");
        return this.stack[0];
    }

    get depth() {
        return this.stack.length;
    }
}
