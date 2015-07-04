/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

import { foldl } from './echo-util';

let hasOwn = Object.prototype.hasOwnProperty;

export class Map {
    constructor() {
        this.map = Object.create(null);
        this.map_size = 0;
    }

    has (key) {
        return hasOwn.call(this.map, "%map" + JSON.stringify(key));
    }

    set (key, val) {
        var entry_key = "%map" + JSON.stringify(key);
        var had_before = hasOwn.call(this.map, entry_key);
        this.map[entry_key] = { key: key, val: val };
        if (!had_before) this.map_size++;
    }

    get (key, val) {
        var entry_key = "%map" + JSON.stringify(key);
        if (!hasOwn.call(this.map, entry_key)) return undefined;
        return this.map[entry_key].val;
    }

    remove (key) {
        var entry_key = "%map" + JSON.stringify(key);
        var had_before = hasOwn.call(this.map, entry_key);

        delete this.map[entry_key];
        if (had_before) this.map_size--;
    }

    clear () {
        this.map = Object.create(null);
        this.size = 0;
    }

    forEach (f) {
        for (var p in this.map) {
            if (!hasOwn.call(this.map, p))
                continue;
            var entry = this.map[p];
            f (entry.val, entry.key, this);
        }
    }

    keys () {
        var result = [];
        for (var p in this.map) {
            if (!this.has (p))
                continue;
            var entry = this.map[p];
            result.push (entry.key);
        }
        return result;
    }
    values () {
        var result = [];
        for (var p in this.map) {
            if (!this.has (p))
                continue;
            var entry = this.map[p];
            result.push (entry.value);
        }
        return result;
    }

    get size () {
        return this.map_size;
    }
}

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
