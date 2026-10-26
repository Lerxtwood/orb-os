"""Exercise settings reset in Edge using simulated flash; never open a real port."""
import json
from pathlib import Path
from playwright.sync_api import sync_playwright

ROOT = Path(__file__).resolve().parents[2]
WEB = ROOT / 'web/companion'
WORK = ROOT / '.pio/companion'
MOCK = '''
export class Transport {
  async disconnect() {}
  async setDTR(value) { window.testSignals.push(['DTR',value]); }
  async setRTS(value) { window.testSignals.push(['RTS',value]); }
}
export class ESPLoader {
  constructor() { this.chip = {CHIP_NAME:'ESP32-S3'}; }
  async main() {} async detectFlashSize() { return '16MB'; }
  async readFlash(address,size) { return window.testFlash.slice(address,address+size); }
  async flashMd5sum() { return 'verified'; }
  async writeFlash(options) {
    if (options.eraseAll) throw Error('Unexpected full erase');
    window.testWrites = options.fileArray.map(p => ({address:p.address,size:p.data.length}));
    for (const p of options.fileArray) window.testFlash.set(p.data,p.address);
  }
}
'''


def main():
    table = (ROOT / '.pio/build/esp32-s3-amoled-175-companion/partitions.bin').read_bytes().ljust(4096, b'\xff')
    with sync_playwright() as playwright:
        browser = playwright.chromium.launch(channel='msedge', headless=True)
        context = browser.new_context(viewport={'width': 1200, 'height': 1000})
        context.add_init_script('''
          window.testWrites = []; window.testSignals = [];
          window.testFlash = new Uint8Array(0x1000000).fill(0x5A);
          Object.defineProperty(navigator, 'serial', {value:{requestPort:async () => ({})}});
          window.testFlash.set(''' + json.dumps(list(table)) + ''', 0x8000);
        ''')

        def serve(route):
            url = route.request.url
            if 'esptool-js' in url:
                route.fulfill(body=MOCK, content_type='text/javascript')
            elif 'spark-md5' in url:
                route.fulfill(body="window.SparkMD5={ArrayBuffer:{hash:()=>'verified'}};", content_type='text/javascript')
            elif url == 'https://installer.test/release-index.json':
                route.fulfill(body='[{"tag":"test","manifest":"unused.json"}]', content_type='application/json')
            elif url.startswith('https://installer.test/'):
                name = url.split('/')[-1] or 'index.html'
                if name not in {'index.html', 'style.css', 'layout.mjs', 'installer.mjs'}:
                    route.abort()
                    return
                kind = 'text/javascript' if name.endswith('.mjs') else 'text/css' if name.endswith('.css') else 'text/html'
                route.fulfill(body=(WEB / name).read_bytes(), content_type=kind)
            else:
                route.abort()

        context.route('**/*', serve)
        page = context.new_page()
        errors = []
        page.on('pageerror', lambda error: errors.append(str(error)))
        page.goto('https://installer.test/')
        page.get_by_role('button', name='Connect device', exact=True).click()
        page.get_by_text('Reset device settings', exact=True).click()
        reset = page.get_by_role('button', name='Reset both firmwares’ settings', exact=True)
        assert reset.is_disabled()
        assert page.evaluate('window.testWrites.length') == 0
        consent = page.get_by_label('I understand that both firmwares’ saved settings will be deleted.', exact=True)
        consent.check()
        assert reset.is_enabled()
        consent.uncheck()
        assert reset.is_disabled()
        consent.check()
        page.screenshot(path=str(WORK / 'settings-reset-desktop.png'), full_page=True)
        reset.click()
        page.wait_for_function("document.getElementById('status').textContent.includes('settings cleared and verified')")
        assert page.evaluate('window.testWrites') == [
            {'address': 0x9000, 'size': 0x5000},
            {'address': 0xED0000, 'size': 0x80000},
            {'address': 0xE000, 'size': 0x2000},
        ]
        assert page.evaluate('window.testSignals') == [['DTR', False], ['RTS', True], ['RTS', False]]
        print('Browser: consent, settings reset, verification, and restart passed')
        page.set_viewport_size({'width': 390, 'height': 844})
        page.get_by_role('button', name='Connect device', exact=True).click()
        page.get_by_text('Reset device settings', exact=True).click()
        assert reset.is_disabled()
        assert page.evaluate('document.documentElement.scrollWidth <= innerWidth')
        page.screenshot(path=str(WORK / 'settings-reset-mobile.png'), full_page=True)
        assert not errors, errors
        print('Browser: mobile layout fits; no JavaScript errors')
        browser.close()


if __name__ == '__main__':
    main()
