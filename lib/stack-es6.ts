/* -*- Mode: typescript; indent-tabs-mode: nil; tab-width: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=typescript:
 */

export class Stack<T> {
    stack: T[] = [];

    constructor(initial?: T) {
        if (initial !== undefined) this.stack.unshift(initial);
    }

    push(o: T): void {
        this.stack.unshift(o);
    }

    pop(): T {
        const top = this.stack.shift();
        if (top === undefined) throw new Error("Stack is empty");
        return top;
    }

    // a 'top' property makes things a little clearer/nicer to read
    get top(): T {
        if (this.stack.length === 0) throw new Error("Stack is empty");
        return this.stack[0]!;
    }

    get depth(): number {
        return this.stack.length;
    }
}
