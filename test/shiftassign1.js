function t() {
    let a = 12; a |= 1; console.log(a);
    let b = 13; b ^= 2; console.log(b);
    let c = 15; c >>= 1; console.log(c);
    let d = 5; d <<= 2; console.log(d);
    let e = 20; e >>>= 2; console.log(e);
    let f = 7; f &= 5; console.log(f);
}
t();
