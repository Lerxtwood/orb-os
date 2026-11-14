"""Exercise the shell's actual switch and restart functions with fake LVGL/NVS."""
import argparse
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / '.pio/settings-resume-tests'

STUBS = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <map>
std::map<std::string, std::string> store;
struct Preferences {
    bool begin(const char *, bool) { return true; }
    void end() {}
    void putString(const char *k, const char *v) { store[k] = v; }
    void getString(const char *k, char *v, size_t n) {
        if (store.count(k)) snprintf(v, n, "%s", store[k].c_str());
    }
    void remove(const char *k) { store.erase(k); }
};
struct App {
    void *screen = nullptr;
    const char *name = "Renamed by theme";
    bool capture = false, hidden = false;
    unsigned lastUsed = 0;
    void (*onEnter)() = nullptr;
    void (*onExit)() = nullptr;
};
App s_apps[16];
int s_cur = 0, s_count = app_shell::APP_COUNT;
int s_settingsReturn = app_shell::APP_CLOCK;
unsigned s_useTick = 0;
bool s_captured = false, buildOk = true;
void evict(int) {}
bool ensure_built(int) { return buildOk; }
void *lv_scr_act() { return nullptr; }
using lv_scr_load_anim_t = int;
const int LV_SCR_LOAD_ANIM_MOVE_LEFT = 1, LV_SCR_LOAD_ANIM_MOVE_RIGHT = 2, ANIM_MS = 250;
void lv_scr_load_anim(void *, int, int, int, bool) {}
void lv_scr_load(void *) {}
struct { template<class... T> void printf(const char *, T...) {} } Serial;
namespace diag { template<class... T> void log(const char *, T...) {} }
'''

CHECKS = r'''
int main() {
    using namespace app_shell;
    // Opening/reopening Settings preserves the last committed app.
    load(APP_INTEL, false, true);
    load(APP_SETTINGS, false, true);
    load(APP_SETTINGS, false, true);
    prepareSettingsRestart();
    assert(store["resumeApp"] == "headlines");
    s_cur = APP_CLOCK; s_settingsReturn = APP_CLOCK; // reboot
    restoreAfterSettingsRestart(true);
    assert(s_cur == APP_INTEL && store.empty());
    s_cur = APP_FLIGHT;
    restoreAfterSettingsRestart(true); // consumed; normal boot is unaffected
    assert(s_cur == APP_FLIGHT);

    // Failed screen construction does not change the remembered source app.
    buildOk = false;
    load(APP_SETTINGS, false, true);
    assert(s_cur == APP_FLIGHT);
    buildOk = true;
    load(APP_SETTINGS, false, true);
    prepareSettingsRestart();
    assert(store["resumeApp"] == "flight");
    restoreAfterSettingsRestart(false); // setup/card notice wins, request consumed
    assert(s_cur == APP_SETTINGS && store.empty());

    // A new theme may disable the old app: fall back to its first visible app.
    s_apps[APP_INTEL].hidden = true;
    store["resumeApp"] = "headlines";
    restoreAfterSettingsRestart(true);
    assert(s_cur == APP_CLOCK);
    s_apps[APP_CLOCK].hidden = true;
    store["resumeApp"] = "unavailable-app";
    restoreAfterSettingsRestart(true);
    assert(s_cur == APP_FLIGHT);

    // Web settings resume the app currently running, without entering Settings.
    prepareSettingsRestart();
    assert(store["resumeApp"] == "flight");
    s_cur = APP_CLOCK;
    restoreAfterSettingsRestart(true);
    assert(s_cur == APP_FLIGHT && store.empty());
    std::puts("Settings restart return checks passed");
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default=str(ROOT / '.pio/build-venv/Scripts/python-zig.exe'))
    args = parser.parse_args()
    WORK.mkdir(parents=True, exist_ok=True)
    header = (ROOT / 'src/app_shell.h').read_text(encoding='utf-8')
    enum_start = header.index('    enum Slot {')
    enum = header[enum_start:header.index('    };', enum_start) + 6]
    declarations = ('namespace app_shell {\n' + enum + '\n'
                    'void selectApp(int); bool hiddenAt(int);\n'
                    'void prepareSettingsRestart(); void restoreAfterSettingsRestart(bool);\n}\n')
    source = (ROOT / 'src/app_shell.cpp').read_text(encoding='utf-8')
    load_start = source.index('    void load(int idx, bool animate, bool forward) {')
    load = source[load_start:source.index('\n    }', load_start) + 6]
    resume_start = source.index('namespace {\n    // Stable across build rosters')
    resume = source[resume_start:source.index('\nint         app_shell::count()', resume_start)]
    harness = WORK / 'resume.cpp'
    harness.write_text(declarations + STUBS + load + '''
void app_shell::selectApp(int idx) { load(idx, false, true); }
bool app_shell::hiddenAt(int idx) { return s_apps[idx].hidden; }
''' + resume + CHECKS, encoding='utf-8')
    compiler = [args.compiler] + (['c++'] if 'zig' in Path(args.compiler).stem else [])
    env = dict(os.environ)
    env['ZIG_GLOBAL_CACHE_DIR'] = str(ROOT / '.pio/zig-global-cache')
    env['ZIG_LOCAL_CACHE_DIR'] = str(ROOT / '.pio/zig-cache')
    for launch in (0, 1):
        for companion in (0, 1):
            output = WORK / f'resume-{launch}-{companion}.exe'
            subprocess.run(compiler + ['-std=c++17', '-O2', '-UNDEBUG', '-DARDUINO',
                           f'-DAPPS_LAUNCH_ONE={launch}', f'-DORB_COMPANION={companion}',
                           str(harness), '-o', str(output)], env=env, check=True)
            subprocess.run([str(output)], check=True)


if __name__ == '__main__':
    main()
