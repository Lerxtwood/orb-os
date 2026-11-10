"""Check Settings allocation and actual portal/web-server ownership logic."""
import argparse
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / '.pio/wifi-setup-tests'

STUBS = r'''
#include <atomic>
#include <cassert>
#include <cstdio>
bool portal = false;
struct {
    bool started = false;
    unsigned begins = 0, stops = 0, requests = 0;
    void begin() { assert(!portal && !started); started = true; ++begins; }
    void stop() { assert(started); started = false; ++stops; }
    void handleClient() { assert(!portal && started); ++requests; }
} g_web;
struct {
    bool finish = false;
    unsigned calls = 0;
    bool getConfigPortalActive() { return portal; }
    void process() {
        assert(!portal || !g_web.started);
        ++calls;
        if (finish) { portal = false; finish = false; }
    }
} g_wm;
std::atomic<bool> g_configPortalActive{false};
bool g_webStarted = false;
'''

CHECKS = r'''
int main() {
    portal = true;
    service_config_servers();
    service_config_servers();
    assert(g_configPortalActive && !g_webStarted && g_web.begins == 0);
    assert(g_wm.calls == 2 && g_web.requests == 0);
    // Portal finishes while processing a successful save/exit.
    g_wm.finish = true;
    service_config_servers();
    assert(!g_configPortalActive && !g_webStarted);
    service_config_servers();
    assert(g_webStarted && g_web.begins == 1 && g_web.requests == 1);
    service_config_servers();
    assert(g_web.begins == 1 && g_web.requests == 2);
    // Setup takes ownership again: ordinary web handling must stop.
    portal = true;
    service_config_servers();
    assert(g_configPortalActive && !g_webStarted && g_web.stops == 1);
    assert(g_web.requests == 2);
    portal = false;
    service_config_servers();
    assert(g_webStarted && g_web.begins == 2);
    std::puts("Setup portal/server ownership checks passed");
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
    source = (ROOT / 'src/main.cpp').read_text(encoding='utf-8')
    start = source.index('static void service_config_servers() {')
    function = source[start:source.index('\n}', start) + 2]
    harness = WORK / 'portal.cpp'
    harness.write_text(STUBS + function + CHECKS, encoding='utf-8')
    for name, unit, options in (
        ('portal', harness, []),
        ('memory', ROOT / 'tests/settings_memory_test.cpp',
         ['-DESP_PLATFORM', '-I' + str(ROOT / 'include'),
          '-I' + str(ROOT / 'tests/settings_memory_stubs')]),
    ):
        output = WORK / (name + '.exe')
        subprocess.run(compiler + ['-std=c++17', '-O2', '-UNDEBUG'] + options +
                       [str(unit), '-o', str(output)], env=env, check=True)
        subprocess.run([str(output)], check=True)


if __name__ == '__main__':
    main()
