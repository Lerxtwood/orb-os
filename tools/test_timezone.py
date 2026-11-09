"""Test timezone migration and DST boundaries using the actual firmware rules.

Requires a C++17 compiler and GNU date (included with Git for Windows).
No device, network, third-party Python packages, or firmware changes are needed.
"""
import argparse
from datetime import datetime
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / '.pio/timezone-tests'

STUBS = r'''
#include "timezone_rules.h"
#include <cassert>
#include <map>
#include <string>
using String = std::string;
String g_tz = "<-06>6", g_tzName;
bool g_tzAuto = true;
unsigned changes = 0, writes = 0;
std::map<std::string, std::string> stored;
struct Preferences {
    bool begin(const char *, bool) { return true; }
    void putBool(const char *key, bool value) { stored[key] = value ? "true" : "false"; ++writes; }
    void putString(const char *key, const String &value) { stored[key] = value; ++writes; }
    void end() {}
};
struct { void printf(const char *, ...) {} } Serial;
int test_setenv(const char *name, const char *value, int) {
    assert(std::string(name) == "TZ" && value == g_tz); ++changes; return 0;
}
void test_tzset() {}
#define setenv test_setenv
#define tzset test_tzset
'''

CHECKS = r'''
int main() {
    apply_automatic_timezone("America/Denver", -7 * 3600);
    assert(g_tz == "MST7MDT,M3.2.0,M11.1.0");
    assert(stored["tzAuto"] == "true" && stored["tzName"] == "America/Denver");
    const unsigned saved = writes;
    apply_automatic_timezone("America/Denver", -6 * 3600);
    assert(writes == saved && changes == 1); // do not rewrite NVS each forecast
    apply_automatic_timezone("", -6 * 3600);
    assert(g_tzName == "America/Denver" && changes == 1); // missing metadata cannot erase DST rules
    g_tzAuto = false;
    apply_automatic_timezone("America/Phoenix", -7 * 3600);
    assert(g_tzName == "America/Denver" && writes == saved); // manual setting takes precedence
    g_tzAuto = true;
    apply_automatic_timezone("", -6 * 3600, true); // changed Studio location: provisional offset
    assert(g_tz == "<-06>6" && g_tzName.empty());
    apply_automatic_timezone("America/Denver", -6 * 3600);
    assert(g_tz == "MST7MDT,M3.2.0,M11.1.0");
    apply_automatic_timezone("America/Phoenix", -7 * 3600);
    assert(g_tz == "<-07>7" && g_tzName == "America/Phoenix");
    apply_automatic_timezone("Africa/Casablanca", 3600);
    assert(g_tz == "<+01>-1");
    apply_automatic_timezone("Africa/Casablanca", 0);
    assert(g_tz == "<+00>0"); // irregular, non-recurring transitions refresh with observations
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default='g++')
    args = parser.parse_args()
    WORK.mkdir(parents=True, exist_ok=True)
    compiler = [args.compiler] + (['c++'] if 'zig' in Path(args.compiler).stem else [])
    env = dict(os.environ)
    env['ZIG_GLOBAL_CACHE_DIR'] = str(ROOT / '.pio/zig-global-cache')
    env['ZIG_LOCAL_CACHE_DIR'] = str(ROOT / '.pio/zig-cache')

    def compile(source, output):
        subprocess.run(compiler + ['-std=c++17', '-O2', '-UNDEBUG', '-I' + str(ROOT / 'src'),
                                  str(source), '-o', str(output)], env=env, check=True)

    unit = WORK / 'rules.exe'
    compile(ROOT / 'tests/timezone_rules_test.cpp', unit)
    subprocess.run([str(unit)], check=True)
    source = (ROOT / 'src/main.cpp').read_text(encoding='utf-8')
    start = source.index('static void apply_automatic_timezone(')
    function = source[start:source.index('\n}', start) + 2]
    harness = WORK / 'migration.cpp'
    harness.write_text(STUBS + function + CHECKS, encoding='utf-8')
    migration = WORK / 'migration.exe'
    compile(harness, migration)
    subprocess.run([str(migration)], check=True)

    date = Path(r'C:\Program Files\Git\usr\bin\date.exe') if os.name == 'nt' else shutil.which('date')
    if not date or not Path(date).is_file():
        raise SystemExit('GNU date is required to exercise DST transitions')
    cases = [
        ('America/Denver', '2026-03-08T08:59:59+00:00', '2026-03-08 01:59:59 -0700'),
        ('America/Denver', '2026-03-08T09:00:00+00:00', '2026-03-08 03:00:00 -0600'),
        ('America/Denver', '2026-11-01T07:59:59+00:00', '2026-11-01 01:59:59 -0600'),
        ('America/Denver', '2026-11-01T08:00:00+00:00', '2026-11-01 01:00:00 -0700'),
        ('America/Denver', '2026-11-09T15:00:00+00:00', '2026-11-09 08:00:00 -0700'),
        ('America/Phoenix', '2026-07-01T15:00:00+00:00', '2026-07-01 08:00:00 -0700'),
        ('America/Phoenix', '2026-11-01T08:00:00+00:00', '2026-11-01 01:00:00 -0700'),
        ('Europe/London', '2026-03-29T00:59:59+00:00', '2026-03-29 00:59:59 +0000'),
        ('Europe/London', '2026-03-29T01:00:00+00:00', '2026-03-29 02:00:00 +0100'),
        ('Europe/London', '2026-10-25T00:59:59+00:00', '2026-10-25 01:59:59 +0100'),
        ('Europe/London', '2026-10-25T01:00:00+00:00', '2026-10-25 01:00:00 +0000'),
        ('Australia/Sydney', '2026-04-04T15:59:59+00:00', '2026-04-05 02:59:59 +1100'),
        ('Australia/Sydney', '2026-04-04T16:00:00+00:00', '2026-04-05 02:00:00 +1000'),
        ('Australia/Sydney', '2026-10-03T15:59:59+00:00', '2026-10-04 01:59:59 +1000'),
        ('Australia/Sydney', '2026-10-03T16:00:00+00:00', '2026-10-04 03:00:00 +1100'),
    ]
    for zone, instant, expected in cases:
        rule = subprocess.check_output([str(unit), zone], text=True).strip()
        timestamp = int(datetime.fromisoformat(instant).timestamp())
        actual = subprocess.check_output([str(date), '-d', '@' + str(timestamp), '+%Y-%m-%d %H:%M:%S %z'],
                                         env={**env, 'TZ': rule}, text=True).strip()
        assert actual == expected, (zone, instant, rule, actual, expected)
    print(f'Timezone lookup/migration tests and {len(cases)} DST boundary cases passed.')


if __name__ == '__main__':
    main()
