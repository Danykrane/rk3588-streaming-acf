import math
import random
import unittest
from reference import StreamingACF, direct, parameters, estimates


class ACFTests(unittest.TestCase):
    def test_parameters_and_cost(self):
        self.assertEqual(parameters(1), (2000, 0, 500, 0, 2499))
        self.assertEqual(parameters(500), (4, 0, 1, 0, 4))
        self.assertAlmostEqual(parameters(3)[1], 2/3)
        self.assertAlmostEqual(parameters(3)[3], 2/3)
        self.assertEqual(estimates()['direct_ops_per_second'], 140520000)
        self.assertEqual(estimates()['recursive_ops_per_second'], 13000000)
        self.assertEqual(len(set(parameters(f)[:4] for f in range(1,501))), 500)

    def test_stream_against_direct_and_matrix_wrap(self):
        rng = random.Random(124)
        samples = [rng.uniform(-1,1) for _ in range(8300)]
        stream = StreamingACF()
        saved = {}
        checkpoints = {0, 4, 5, 999, 1999, 2000, 2047, 2048, 2498,
                       2499, 2500, 4095, 4096, 6299, 6300, 8191, 8192, 8299}
        for i, value in enumerate(samples):
            row = stream.push(value)
            if i in checkpoints:
                for f, actual in enumerate(row, 1):
                    expected = direct(samples, i, f)
                    if math.isnan(expected):
                        self.assertTrue(math.isnan(actual), (i, f))
                    else:
                        self.assertAlmostEqual(actual, expected, delta=2e-14,
                                               msg=str((i, f)))
            if i in {6300, 8192, 8299}:
                saved[i] = row
        for i, row in saved.items():
            self.assertEqual(stream.row(8299-i), row)
        with self.assertRaises(IndexError):
            stream.row(2000)

    def test_constant_and_zero(self):
        for value in [0.0, 1.0, -1.0]:
            stream = StreamingACF()
            for _ in range(2501):
                row = stream.push(value)
            for f, actual in enumerate(row, 1):
                self.assertAlmostEqual(actual, value*value/f, delta=2e-14)

    def test_impulse(self):
        samples = [0.0]*2700
        samples[1200] = 1.0
        stream = StreamingACF()
        for i, x in enumerate(samples):
            row = stream.push(x)
            for f in (1, 3, 127, 499, 500):
                if i >= parameters(f)[4]:
                    self.assertEqual(row[f-1], 0.0)

    def test_sine_and_step(self):
        samples = [0.7*math.sin(2*math.pi*73*i/2000) if i < 2700 else 0.3
                   for i in range(4200)]
        stream = StreamingACF(rebase_every=257)
        for i, x in enumerate(samples):
            row = stream.push(x)
            if i in (2499, 2699, 2700, 3000, 4199):
                for f in range(1,501):
                    self.assertAlmostEqual(row[f-1], direct(samples,i,f), delta=2e-14)

    def test_reject_bad_input_without_state_change(self):
        stream = StreamingACF()
        for value in (math.nan, math.inf, -math.inf, 1.1):
            with self.assertRaises(ValueError):
                stream.push(value)
            self.assertEqual(stream.index, -1)
        with self.assertRaises(ValueError):
            StreamingACF(0)
        with self.assertRaises(IndexError):
            stream.row()


if __name__ == '__main__':
    unittest.main()
