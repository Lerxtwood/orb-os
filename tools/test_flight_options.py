"""Exercise the actual embedded Flight Options HTML/POST handlers without a device.

Run with --browser to also check the rendered mobile page in headless Edge.
"""
import argparse
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / '.pio' / 'flight-options-test'

STUBS = r'''
#include "flight_display_options.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <map>
#include <string>
#include <vector>
using String = std::string;
#define PSTR(x) x
#define PROGMEM
constexpr int CONTENT_LENGTH_UNKNOWN = -1;
int g_flightRotation = 90;
struct Web {
    std::map<std::string, std::string> args;
    std::string html, location;
    int status = 0;
    void setContentLength(int) {}
    void send(int code, const char *, const char *) { status = code; }
    void sendContent_P(const char *s) { html += s; }
    void sendContent(const char *s, size_t n) { html.append(s, n); }
    void sendHeader(const char *, const char *value) { location = value; }
    bool hasArg(const char *name) { return args.count(name); }
    String arg(const char *name) { return hasArg(name) ? args.at(name) : ""; }
} g_web;
struct Preferences {
    static inline bool open = true, write = true;
    static inline unsigned writes = 0;
    static inline uint32_t stored = 0;
    bool begin(const char *, bool) { return open; }
    size_t putUInt(const char *key, uint32_t value) {
        assert(std::string(key) == "flightDisplay");
        ++writes;
        if (!write) return 0;
        stored = value; return sizeof(value);
    }
    void end() {}
};
namespace radar {
    flight_display::Options options;
    auto flightDisplayOptions() { return options; }
    void setFlightDisplayOptions(const flight_display::Options &o) { options = o; }
    bool autoArrivalCard() { return options.arrivals; }
    bool autoPhoto() { return options.closePasses; }
    double closePassMiles() { return options.miles; }
}
'''

CHECKS = r'''
int main() {
    selection_test::check();
    handleFlightOptions();
    std::ofstream("timed.html") << g_web.html;
    radar::options = {17, true, true, true, 10.0};
    g_web = {}; handleFlightOptions();
    std::ofstream("pinned.html") << g_web.html;

    auto request = [] {
        g_web = {};
        g_web.args = {{"cardMode","stay"},{"seconds","17"},{"arrivals","1"},
                      {"closePasses","1"},{"miles","10.0"},{"smallGa","1"}};
    };
    request(); handleFlightDisplayOptions();
    assert(g_web.status == 303 && g_web.location == "/flight-tracker-options");
    auto saved = flight_display::unpack(Preferences::stored);
    assert(saved.stayVisible && saved.seconds == 17 && saved.arrivals && saved.closePasses && saved.miles == 10);
    assert(saved.smallGa && radar::options.smallGa);
    request(); g_web.args = {{"cardMode","timed"},{"seconds","3"},{"miles","0.1"}};
    handleFlightDisplayOptions();
    assert(g_web.status == 303);
    saved = flight_display::unpack(Preferences::stored);
    assert(!saved.stayVisible && saved.seconds == 3 && !saved.arrivals && !saved.closePasses && saved.miles == 0.1);
    assert(!saved.smallGa && !radar::options.smallGa);
    g_web = {}; handleFlightOptions();
    assert(g_web.html.find("name=smallGa value=1 aria-describedby=gaImageDescription >") != std::string::npos);
    const auto previous = Preferences::stored;
    for (const auto &bad : std::map<std::string,std::string>{{"cardMode","other"},{"seconds","0"},
             {"arrivals","true"},{"closePasses","false"},{"smallGa","50"},{"miles","nan"}}) {
        request(); g_web.args[bad.first] = bad.second;
        const auto writes = Preferences::writes;
        handleFlightDisplayOptions();
        assert(g_web.status == 400 && Preferences::stored == previous && writes == Preferences::writes);
    }
    request(); g_web.args.erase("seconds"); handleFlightDisplayOptions();
    assert(g_web.status == 400);
    request(); Preferences::write = false; handleFlightDisplayOptions();
    assert(g_web.status == 500 && flight_display::pack(radar::options) == previous && g_web.location.empty());
    Preferences::write = true;
    request(); Preferences::open = false; handleFlightDisplayOptions();
    assert(g_web.status == 500 && flight_display::pack(radar::options) == previous);
    Preferences::open = true;
    // Old URLs still update the same atomic options without discarding duration/pinning.
    radar::options = {17, true, true, true, 10.0, false};
    g_web = {}; handleFlightArrivalCard();
    assert(g_web.status == 303 && !radar::options.arrivals && radar::options.stayVisible && radar::options.seconds == 17);
    g_web = {}; g_web.args = {{"miles","5.0"}}; handleFlightClosePass();
    assert(g_web.status == 303 && !radar::options.closePasses && radar::options.stayVisible && radar::options.seconds == 17);
    assert(!radar::options.smallGa); // legacy handlers preserve the image-size choice
}
'''

SELECTION_STUBS = r'''
namespace selection_test {
struct AcInfo { std::string hex; };
struct AcDraw { std::string hex; bool inRange; bool masked; };
std::string s_selHex;
std::vector<AcDraw> s_acs;
bool stockOrb = false, custom = false;
constexpr int ORB_BLIPS = 2;
bool orb() { return stockOrb; }
bool customStyled() { return custom; }
bool ac_masked(const AcDraw &a) { return a.masked; }
void fill_info(const AcDraw &a, AcInfo &out) { out.hex = a.hex; }
'''

SELECTION_CHECKS = r'''
void check() {
    AcInfo out;
    s_selHex = "flight";
    s_acs = {{"flight", true, false}};
    assert(selected(out) && out.hex == "flight");
    s_acs[0].inRange = false;
    assert(!selected(out)); // selected radius shrinks or aircraft flies out
    s_acs[0] = {"flight", true, true};
    assert(!selected(out)); // Keep-out Area hides the aircraft
    s_acs.clear();
    assert(!selected(out)); // leaving the feed cannot retain a stale pinned card
    s_acs = {{"first", true, false}, {"second", true, false}, {"flight", true, false}};
    stockOrb = true;
    assert(!selected(out)); // stock view's visible-aircraft limit
    custom = true;
    assert(selected(out)); // custom view does not apply the stock limit
    s_selHex.clear();
    assert(!selected(out));
}
} // namespace selection_test
'''


def handler(source, name):
    start = source.index('static ', source.index(name) - 12)
    end = source.index('\n}', start) + 2
    return source[start:end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default='g++')
    parser.add_argument('--browser', action='store_true')
    args = parser.parse_args()
    WORK.mkdir(parents=True, exist_ok=True)
    source = (ROOT / 'src/main.cpp').read_text(encoding='utf-8')
    root_head = source[source.index('static const char ROOT_HEAD'):source.index('static const char ROOT_STATUS_HEAD')]
    names = ['handleFlightOptions', 'saveFlightDisplayOptions', 'handleFlightDisplayOptions',
             'handleFlightArrivalCard', 'handleFlightClosePass']
    radar = (ROOT / 'src/radar_view.cpp').read_text(encoding='utf-8')
    start = radar.index('bool selected(AcInfo &out) {')
    selection = radar[start:radar.index('\n}', start) + 2]
    code = (STUBS + root_head + '\n'.join(handler(source, name) for name in names) +
            SELECTION_STUBS + selection + SELECTION_CHECKS + CHECKS)
    cpp = WORK / 'handlers.cpp'
    exe = WORK / 'handlers.exe'
    cpp.write_text(code, encoding='utf-8')
    compiler = [args.compiler] + (['c++'] if 'zig' in Path(args.compiler).stem else [])
    subprocess.run(compiler + ['-std=c++17', '-O2', '-UNDEBUG', '-I' + str(ROOT / 'src'),
                              str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], cwd=WORK, check=True)
    if args.browser:
        from playwright.sync_api import sync_playwright
        with sync_playwright() as p:
            browser = p.chromium.launch(channel='msedge', headless=True)
            page = browser.new_page(viewport={'width': 390, 'height': 844}, device_scale_factor=1)
            for name, mode, seconds in [('timed', 'timed', '5'), ('pinned', 'stay', '17')]:
                page.goto((WORK / (name + '.html')).as_uri())
                form = page.locator('form[action="/flight-display-options"]')
                assert form.count() == 1
                assert form.locator('input[name=cardMode]:checked').input_value() == mode
                assert form.locator('input[name=seconds]').input_value() == seconds
                assert form.locator('fieldset').count() == 4
                assert form.locator('button[type=submit]').count() == 1
                assert form.locator('input[name=arrivals]').is_checked() == (mode == 'stay')
                assert form.locator('input[name=closePasses]').is_checked() == (mode == 'stay')
                assert form.locator('input[name=smallGa]').is_checked()
                assert form.locator('label', has=form.locator('input[name=smallGa]')).inner_text() == 'Show GA aircraft images at 60% size'
                assert page.evaluate('document.documentElement.scrollWidth <= innerWidth')
                page.screenshot(path=str(WORK / (name + '.png')), full_page=True)
            browser.close()
    print('Flight Options submissions, validation, storage failures and card visibility checks passed.')


if __name__ == '__main__':
    main()
