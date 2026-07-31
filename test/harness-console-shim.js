// harness-console-shim: the value-based test harness (runtime-P3).
//
// Replaces console.log/warn/error with a serializer OWNED BY THIS FILE.
// The same code runs under node (expected-output generation, via
// harness-run.js) and compiled into each test executable (via the import
// wrapper tester.js generates), so a test's baseline and its output agree
// iff the VALUES it logged agree — no engine's inspect format is in the
// loop, and node upgrades can't drift the baselines.
//
// Rules for editing this file:
//   - conservative ES5 only: it must compile under ejs and run under
//     node byte-identically (including from the esm generator's
//     transpile dir, where it rides along unconverted);
//   - no engine-provided formatting (util.inspect, toISOString, ...);
//     anything observable must be computed here, from values;
//   - it must not rely on ejs-specific or node-specific behavior: any
//     asymmetry becomes a spurious diff in every test.
//
// Known engine gaps deliberately absorbed here (worked around, so the
// harness itself never trips them):
//   - ejs Object.keys(array) omits index keys (node includes them):
//     elements are walked by index, and index-shaped keys are filtered
//     from the named-property pass on both engines;
//   - ejs lacks Date.prototype.toISOString: the ISO string is computed
//     from getTime() with civil-date math.

(function () {
    var origLog = console.log;
    var origError = console.error;

    function isIdentChar(cc, first) {
        if ((cc >= 65 && cc <= 90) || (cc >= 97 && cc <= 122) || cc === 95 || cc === 36) return true;
        return !first && cc >= 48 && cc <= 57;
    }

    function isIdentLike(s) {
        if (s.length === 0) return false;
        for (var i = 0; i < s.length; i++) {
            if (!isIdentChar(s.charCodeAt(i), i === 0)) return false;
        }
        return true;
    }

    function quoteString(s) {
        var out = "'";
        for (var i = 0; i < s.length; i++) {
            var cc = s.charCodeAt(i);
            var ch = s.charAt(i);
            if (ch === "'") out += "\\'";
            else if (ch === "\\") out += "\\\\";
            else if (ch === "\n") out += "\\n";
            else if (ch === "\r") out += "\\r";
            else if (ch === "\t") out += "\\t";
            else if (cc < 32) {
                var hex = cc.toString(16);
                if (hex.length < 2) hex = "0" + hex;
                out += "\\x" + hex;
            } else out += ch;
        }
        return out + "'";
    }

    function numberToString(n) {
        if (n === 0 && 1 / n === -Infinity) return "-0";
        return String(n);
    }

    function pad(n, w) {
        var s = String(n);
        while (s.length < w) s = "0" + s;
        return s;
    }

    // ISO-8601 from epoch millis; civil-from-days per Howard Hinnant.
    function dateToISO(d) {
        var t;
        try {
            t = d.getTime();
        } catch (e) {
            t = NaN;
        }
        if (t !== t) return "Invalid Date";
        var ms = t % 86400000;
        if (ms < 0) ms += 86400000;
        var days = (t - ms) / 86400000;
        var z = days + 719468;
        var era = Math.floor(z / 146097);
        var doe = z - era * 146097;
        var yoe = Math.floor(
            (doe - Math.floor(doe / 1460) + Math.floor(doe / 36524) - Math.floor(doe / 146096)) /
                365
        );
        var y = yoe + era * 400;
        var doy = doe - (365 * yoe + Math.floor(yoe / 4) - Math.floor(yoe / 100));
        var mp = Math.floor((5 * doy + 2) / 153);
        var day = doy - Math.floor((153 * mp + 2) / 5) + 1;
        var m = mp < 10 ? mp + 3 : mp - 9;
        if (m <= 2) y += 1;
        var hh = Math.floor(ms / 3600000);
        ms -= hh * 3600000;
        var mm = Math.floor(ms / 60000);
        ms -= mm * 60000;
        var ss = Math.floor(ms / 1000);
        ms -= ss * 1000;
        return (
            pad(y, 4) + "-" + pad(m, 2) + "-" + pad(day, 2) +
            "T" + pad(hh, 2) + ":" + pad(mm, 2) + ":" + pad(ss, 2) + "." + pad(ms, 3) + "Z"
        );
    }

    // canonical non-negative-integer key, i.e. an array index that the
    // element walk already covered
    function isIndexKey(k) {
        var n = Math.floor(Number(k));
        return n >= 0 && String(n) === k;
    }

    function constructorName(v) {
        try {
            if (Object.getPrototypeOf && Object.getPrototypeOf(v) === null) return null;
            var c = v.constructor;
            if (typeof c === "function" && typeof c.name === "string" && c.name.length > 0)
                return c.name;
        } catch (e) {}
        return "";
    }

    function fmtArrayBody(v, len, seen) {
        var parts = [];
        var emptyRun = 0;
        for (var i = 0; i < len; i++) {
            if (!(i in v)) {
                emptyRun++;
                continue;
            }
            if (emptyRun > 0) {
                parts.push("<" + emptyRun + " empty item" + (emptyRun === 1 ? "" : "s") + ">");
                emptyRun = 0;
            }
            parts.push(fmt(v[i], seen));
        }
        if (emptyRun > 0)
            parts.push("<" + emptyRun + " empty item" + (emptyRun === 1 ? "" : "s") + ">");
        var keys = [];
        try {
            keys = Object.keys(v);
        } catch (e) {}
        for (var j = 0; j < keys.length; j++) {
            var k = keys[j];
            if (k === "length" || isIndexKey(k)) continue;
            parts.push((isIdentLike(k) ? k : quoteString(k)) + ": " + fmt(v[k], seen));
        }
        if (parts.length === 0) return "[]";
        return "[ " + parts.join(", ") + " ]";
    }

    function fmtObject(v, seen) {
        var prefix = "";
        var cn = constructorName(v);
        if (cn === null) prefix = "[Object: null prototype] ";
        else if (cn !== "" && cn !== "Object") prefix = cn + " ";
        var keys = [];
        try {
            keys = Object.keys(v);
        } catch (e) {}
        if (keys.length === 0) return prefix + "{}";
        var parts = [];
        for (var i = 0; i < keys.length; i++) {
            var k = keys[i];
            parts.push((isIdentLike(k) ? k : quoteString(k)) + ": " + fmt(v[k], seen));
        }
        return prefix + "{ " + parts.join(", ") + " }";
    }

    function fmt(v, seen) {
        var t = typeof v;
        if (v === null) return "null";
        if (t === "undefined") return "undefined";
        if (t === "number") return numberToString(v);
        if (t === "boolean") return String(v);
        if (t === "string") return quoteString(v);
        if (t === "symbol") {
            try {
                return v.toString();
            } catch (e) {
                return "Symbol(?)";
            }
        }
        if (t === "function") {
            var fname = "";
            try {
                fname = v.name;
            } catch (e) {}
            return fname ? "[Function: " + fname + "]" : "[Function (anonymous)]";
        }

        if (seen.indexOf(v) !== -1) return "[Circular]";
        seen.push(v);
        var out;
        try {
            out = fmtNonPrimitive(v, seen);
        } catch (e) {
            out = "[unserializable: " + e + "]";
        }
        seen.pop();
        return out;
    }

    function fmtNonPrimitive(v, seen) {
        if (Array.isArray(v)) return fmtArrayBody(v, v.length, seen);
        if (v instanceof Error) {
            var ename = v.name || "Error";
            return v.message ? "[" + ename + ": " + v.message + "]" : "[" + ename + "]";
        }
        if (v instanceof Date) return dateToISO(v);
        if (v instanceof RegExp) return String(v);
        if (typeof Map === "function" && v instanceof Map) {
            var mparts = [];
            v.forEach(function (val, key) {
                mparts.push(fmt(key, seen) + " => " + fmt(val, seen));
            });
            return "Map(" + v.size + ") {" + (mparts.length ? " " + mparts.join(", ") + " " : "") + "}";
        }
        if (typeof Set === "function" && v instanceof Set) {
            var sparts = [];
            v.forEach(function (val) {
                sparts.push(fmt(val, seen));
            });
            return "Set(" + v.size + ") {" + (sparts.length ? " " + sparts.join(", ") + " " : "") + "}";
        }
        if (typeof v.BYTES_PER_ELEMENT === "number" && typeof v.length === "number") {
            var tname = constructorName(v) || "TypedArray";
            var tparts = [];
            for (var i = 0; i < v.length; i++) tparts.push(fmt(v[i], seen));
            return tname + "(" + v.length + ")" + (tparts.length ? " [ " + tparts.join(", ") + " ]" : " []");
        }
        if (v instanceof Number) return "[Number: " + numberToString(v.valueOf()) + "]";
        if (v instanceof String) return "[String: " + quoteString(v.valueOf()) + "]";
        if (v instanceof Boolean) return "[Boolean: " + String(v.valueOf()) + "]";
        return fmtObject(v, seen);
    }

    function fmtTop(v) {
        if (typeof v === "string") return v;
        return fmt(v, []);
    }

    function makeWriter(target) {
        return function () {
            var parts = [];
            for (var i = 0; i < arguments.length; i++) parts.push(fmtTop(arguments[i]));
            target(parts.join(" "));
        };
    }

    console.log = makeWriter(function (s) {
        origLog(s);
    });
    console.warn = makeWriter(function (s) {
        origError(s);
    });
    console.error = makeWriter(function (s) {
        origError(s);
    });
})();
