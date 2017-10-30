/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */
import { foldl } from "./echo-util";

function isNode() {
    return typeof __ejs == "undefined";
}

function getSetNode() {
    let hasOwn = Object.prototype.hasOwnProperty;

    return class Set {
        constructor(arr) {
            if (typeof arr === "undefined") arr = [];

            this.set = Object.create(null);
            for (var i = 0, e = arr.length; i < e; i++) {
                var a = arr[i];
                this.set[a] = a;
            }
        }

        has(el) {
            return hasOwn.call(this.set, el);
        }

        add(el) {
            this.set[el] = el;
        }

        remove(el) {
            delete this.set[el];
        }

        map(f) {
            var result = [];
            for (var p in this.set) result.push(f(p));
            return result;
        }

        forEach(f) {
            for (var p in this.set) f(p);
        }

        keys() {
            return this.values();
        }

        values() {
            return Object.getOwnPropertyNames(this.set);
        }

        union(other_set) {
            if (!other_set) return this;
            let result = new Set();
            for (let p in this.set) result.add(p);
            for (let p in other_set.set) result.add(p);
            return result;
        }

        subtract(other_set) {
            if (!other_set) return this;
            var result = new Set();
            for (var p in this.set) {
                if (!other_set.has(p)) {
                    result.add(p);
                }
            }
            return result;
        }

        intersect(other_set) {
            if (!other_set) return this;
            var result = new Set();
            for (var p in this.set) {
                if (other_set.has(p)) {
                    result.add(p);
                }
            }
            return result;
        }

        toString() {
            var str = "{ ";
            for (var p in this.set) {
                str += p;
                str += " ";
            }
            str += "}";
            return str;
        }

        get size() {
            return this.values().length;
        }

        empty() {
            for (var p in this.set) {
                return false;
            }
            return true;
        }

        /* XXX this should use a rest parameter but there appears to be an esprima bug.  we never see the rest parameter */
        static union() {
            return foldl(
                function(a, b) {
                    return a.union(b);
                },
                new Set(),
                Array.prototype.slice.call(arguments, 0)
            );
        }
    };

    /*
     # Set tests

     s1 = new Set [1, 2, 3, 4]
     s2 = new Set [5, 6, 7, 8]

     console.log "should be { 1 2 3 4 5 6 7 8 }:    #{(s1.union s2).toString()}"


     s3 = new Set [1, 2, 3, 4, 5, 6, 7, 8]
     s4 = new Set [5, 6, 7, 8]

     console.log "should be { 1 2 3 4 }:    #{(s3.subtract s4).toString()}"

     s5 = new Set [1, 2, 3, 4];
     s6 = new Set [3, 4, 5];

     console.log "should be { 3 4 }:    #{(s5.intersect s6).toString()}"

     */
}

function getSetEjs() {
    return Set;
}

function getSet() {
    if (isNode()) {
        return getSetNode();
    }
    return getSetEjs();
}

export default getSet();
