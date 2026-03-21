// ASL script benchmark resource — scripts.asl
// Used by ScriptEngineBenchmarks: arithmetic, branching, loops, recursion, module calls.

arith := (a, b) => {
    x = a * b + a - b;
    y = x * x - 2 * x + 1;
    return y;
}

fib := (n) => {
    if (n <= 1) { return n; }
    return fib(n - 1) + fib(n - 2);
}

sumLoop := (n) => {
    sum = 0;
    for (i = 0; i < n; i++) {
        sum += i;
    }
    return sum;
}

classify := (score) => {
    if (score >= 90) { return "A"; }
    else if (score >= 80) { return "B"; }
    else if (score >= 70) { return "C"; }
    else { return "F"; }
}

collatz := (n) => {
    steps = 0;
    for (; n != 1;) {
        if (n % 2 == 0) { n = n / 2; }
        else { n = n * 3 + 1; }
        steps++;
    }
    return steps;
}
