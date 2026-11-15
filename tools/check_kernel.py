"""Optional Clang check of OpenCL syntax and scalar FP32 kernel on CPU.

Run from any directory: python3 tools/check_kernel.py.
Does not require OpenCL runtime and does not claim GPU validation.
"""
import ctypes
import math
import random
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from reference import direct


def main():
    subprocess.run(['clang', '-x', 'cl', '-cl-std=CL1.2', '-fsyntax-only',
                    str(ROOT/'acf.cl')], check=True)
    with tempfile.TemporaryDirectory(dir=ROOT/'tmp') as temp:
        lib = Path(temp)/'kernel.so'
        subprocess.run(['clang', '-std=c11', '-O2', '-shared', '-fPIC',
                        str(ROOT/'tests/kernel_host.c'), '-o', str(lib)], check=True)
        run = ctypes.CDLL(str(lib)).run_row
        run.argtypes = [ctypes.POINTER(ctypes.c_float), ctypes.POINTER(ctypes.c_float),
                        ctypes.c_uint, ctypes.c_uint]
        run.restype = None
        rng = random.Random(517)
        signals = {
            'constant': [1.0]*8301,
            'random': [rng.uniform(-1,1) for _ in range(8301)],
            'sine_step': [0.7*math.sin(2*math.pi*73*i/2000) if i<2700 else 0.3
                          for i in range(8301)],
        }
        worst = 0.0
        checks = 0
        for name, data in signals.items():
            data = [ctypes.c_float(x).value for x in data]
            # Poison unavailable history to catch zero-weight invalid reads.
            ring = (ctypes.c_float*4096)(*([math.nan]*4096))
            output = (ctypes.c_float*500)()
            for i, value in enumerate(data):
                ring[i%4096] = value
                if i not in {0,4,5,833,2498,2499,2500,4095,4096,8192,8300}:
                    continue
                run(ring, output, i%4096, min(i+1,4096))
                for f, actual in enumerate(output, 1):
                    expected = direct(data, i, f)
                    checks += 1
                    if math.isnan(expected):
                        assert math.isnan(actual), (name,i,f,actual)
                    else:
                        # Conservative absolute bound for N-term FP32 accumulation
                        # on |x|<=1; scales with the integral window length 1/f.
                        error = abs(actual-expected)
                        assert math.isfinite(actual) and error <= (2e-4/f + 1e-7), (name,i,f,actual,expected)
                        worst = max(worst,error)
        print(f'PASS: OpenCL syntax; {checks} CPU FP32 kernel comparisons; max absolute error {worst:.9g}')
        print('This is not a GPU execution or real-time benchmark.')


if __name__ == '__main__':
    (ROOT/'tmp').mkdir(exist_ok=True)
    main()
