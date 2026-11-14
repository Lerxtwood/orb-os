"""Compile and run the aircraft image classification/scale checks."""
import argparse
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default=str(ROOT / '.pio/build-venv/Scripts/python-zig.exe'))
    args = parser.parse_args()
    work = ROOT / '.pio/flight-icon-scale-tests'
    work.mkdir(parents=True, exist_ok=True)
    compiler = [args.compiler] + (['c++'] if 'zig' in Path(args.compiler).stem else [])
    env = dict(os.environ)
    env['ZIG_GLOBAL_CACHE_DIR'] = str(ROOT / '.pio/zig-global-cache')
    env['ZIG_LOCAL_CACHE_DIR'] = str(ROOT / '.pio/zig-cache')
    for name in ('flight_icon_scale', 'flight_card_lifetime'):
        output = work / (name + '.exe')
        subprocess.run(compiler + ['-std=c++17', '-O2', '-UNDEBUG', '-I' + str(ROOT / 'src'),
                       str(ROOT / 'tests' / (name + '_test.cpp')), '-o', str(output)],
                       env=env, check=True)
        subprocess.run([str(output)], check=True)


if __name__ == '__main__':
    main()
