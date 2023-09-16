function primes(n) {
    function primes_internal(cur, remaining, filter) {
        if (remaining === 0) return;
        else {
            if (!filter(cur)) {
                console.log(cur);
                primes_internal(cur + 1, remaining - 1, function prime_filter(test) {
                    return test % cur === 0 || filter(test);
                });
            } else {
                primes_internal(cur + 1, remaining, filter);
            }
        }
    }

    primes_internal(2, n, function base_filter(test) {
        return false;
    });
}

primes(100);
