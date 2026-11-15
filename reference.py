"""Executable mathematical model; Python binary64, no third-party packages.

Not a real-time driver or a GPU benchmark. Input is finite, scaled to [-1, 1].
Missing history produces NaN. Matrix rows are read newest first.
"""
from array import array
from math import fsum, isfinite, nan

FS = 2000
FREQUENCIES = range(1, 501)
HISTORY = 4096
ROWS = 2000


def parameters(f):
    if not isinstance(f, int) or not 1 <= f <= 500:
        raise ValueError("frequency must be an integer in [1, 500]")
    n, rem = divmod(FS, f)
    q, lag_rem = divmod(500, f)
    beta, alpha = rem / f, lag_rem / f
    oldest = n - (rem == 0) + q + (lag_rem != 0)
    return n, beta, q, alpha, oldest


def direct(samples, index, f):
    """Independent oracle using absolute indices and linear interpolation."""
    n, beta, q, alpha, oldest = parameters(f)
    if index < oldest:
        return nan
    def product(k):
        j = index - k
        delayed = samples[j-q]
        if alpha:
            delayed += alpha * (samples[j-q-1] - delayed)
        return samples[j] * delayed
    terms = [product(k) for k in range(n)]
    if beta:
        terms.append(beta * product(n))
    return fsum(terms) / FS


class StreamingACF:
    """O(500) update; bounded buffers; periodic rebase limits sum drift."""
    def __init__(self, rebase_every=2048):
        if not isinstance(rebase_every, int) or rebase_every < 1:
            raise ValueError("rebase_every must be a positive integer")
        self.rebase_every = rebase_every
        self.history = array('d', [0.0]) * HISTORY
        self.matrix = array('d', [nan]) * (ROWS * 500)
        self.sums = [0.0] * 500
        self.params = [parameters(f) for f in FREQUENCIES]
        self.index = -1

    def push(self, value):
        value = float(value)
        if not isfinite(value) or abs(value) > 1:
            raise ValueError("input must be finite and normalized to [-1, 1]")
        self.index += 1
        i = self.index
        self.history[i % HISTORY] = value
        row = []
        for c, (n, beta, q, alpha, oldest) in enumerate(self.params):
            def product(j):
                # Before acquisition starts, internal state is zero padded.
                def x(k):
                    return self.history[k % HISTORY] if k >= 0 else 0.0
                return x(j) * ((1-alpha) * x(j-q) + alpha * x(j-q-1))
            old = product(i-n)
            if i % self.rebase_every == 0:
                self.sums[c] = fsum(product(i-k) for k in range(n))
            else:
                self.sums[c] += product(i) - old
            result = (self.sums[c] + beta * old) / FS if i >= oldest else nan
            row.append(result)
        start = (i % ROWS) * 500
        self.matrix[start:start+500] = array('d', row)
        return row

    def row(self, age=0):
        if not isinstance(age, int) or not 0 <= age < min(self.index+1, ROWS):
            raise IndexError("row is not available")
        start = ((self.index-age) % ROWS) * 500
        return list(self.matrix[start:start+500])


def estimates():
    total = sum(FS // f for f in FREQUENCIES)
    return {
        'sum_integer_windows': total,
        'direct_ops_per_second': FS * (5*total + 7*500),
        'recursive_ops_per_second': FS * 500 * 13,
        'matrix_fp32_bytes': ROWS * 500 * 4,
        'matrix_fp64_bytes': ROWS * 500 * 8,
        'input_bytes_per_second': FS * 8,
    }


if __name__ == '__main__':
    import json
    print(json.dumps(estimates(), indent=2))
