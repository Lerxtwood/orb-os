import {FLASH_SIZE, require, identifyLayout, validateRelease, validateImage, validateCache, flashPlan} from './layout.mjs';
const $ = id => document.getElementById(id);
let releases = [], loader, transport, layout, backup, backupSaved = false, busy = false;
const log = message => { $('log').textContent = ($('log').textContent + message + '\n').slice(-14000); };
function status(message, error = false) { $('status').textContent = message; $('status').classList.toggle('error', error); }
function progress(value) { $('progress').hidden = false; $('progress').value = value; }
function lock(value) {
  busy = value;
  $('connect').disabled = value || !!loader || !releases.length || !('serial' in navigator);
  $('release').disabled = value || !!loader || !releases.length;
  $('disconnect').disabled = value;
  $('install').disabled = value || !layout || (layout !== 'companion' && !backupSaved);
  $('backup').disabled = value;
}
const md5 = bytes => SparkMD5.ArrayBuffer.hash(bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength));
async function readVerified(address, size) {
  const result = await loader.readFlash(address, size);
  require(result.length === size, 'The device returned an incomplete read.');
  require(md5(result) === await loader.flashMd5sum(address, size), 'Device read verification failed. Nothing was written.');
  return result;
}
async function downloadRelease() {
  const entry = releases[Number($('release').value)];
  const url = new URL(entry.manifest, location.href);
  require(url.origin === location.origin, 'Release manifest must be served by this installer.');
  const response = await fetch(url, {cache: 'no-store'});
  require(response.ok, 'Could not load the release manifest.');
  const manifest = await response.json();
  validateRelease(manifest);
  const files = {};
  for (const part of manifest.parts) {
    status('Downloading and checking ' + part.path + '…');
    const response = await fetch(new URL(part.path, url));
    require(response.ok, 'Could not download ' + part.path);
    const bytes = new Uint8Array(await response.arrayBuffer());
    const digest = [...new Uint8Array(await crypto.subtle.digest('SHA-256', bytes))].map(b => b.toString(16).padStart(2, '0')).join('');
    require(bytes.length === part.size && digest === part.sha256, 'Release checksum mismatch: ' + part.path);
    validateImage(part.path, bytes);
    files[part.path] = bytes;
  }
  return files;
}
async function disconnect(reset = true) {
  if (loader && reset) { try { await loader.after('hard_reset'); } catch (error) { log(error.message); } }
  if (transport) { try { await transport.disconnect(); } catch (error) { log(error.message); } }
  loader = transport = layout = backup = undefined;
  backupSaved = false;
  $('ready').hidden = $('disconnect').hidden = true;
  lock(false);
}
$('connect').addEventListener('click', async () => {
  lock(true);
  try {
    // Request the port directly from the click, before asynchronous module loading.
    const port = await navigator.serial.requestPort();
    status('Connecting and checking your device…');
    require(typeof SparkMD5 !== 'undefined', 'Verification library could not load. Reload this page.');
    const {ESPLoader, Transport} = await import('https://cdn.jsdelivr.net/npm/esptool-js@0.7.0/bundle.js');
    transport = new Transport(port);
    loader = new ESPLoader({transport, baudrate: 460800, terminal: {clean() {}, write: log, writeLine: log}});
    await loader.main();
    require(loader.chip.CHIP_NAME === 'ESP32-S3' && await loader.detectFlashSize() === '16MB',
      'This installer requires the 16 MB ESP32-S3 AMOLED 1.75 device.');
    layout = identifyLayout(await readVerified(0x8000, 4096));
    require(layout !== 'unknown', 'This device has another firmware layout. Use its recovery tools before installing Orb Companion. Nothing was changed.');
    if (layout !== 'companion') {
      if (layout === 'orb') validateCache(await readVerified(0x650000, 8192));
      status('Reading your recovery backup. Keep the cable connected…');
      backup = new Uint8Array(FLASH_SIZE);
      // Bound readFlash's internal buffer copying for a full 16 MB backup.
      for (let address = 0; address < FLASH_SIZE; address += 0x40000) {
        backup.set(await readVerified(address, 0x40000), address);
        progress(100 * (address + 0x40000) / FLASH_SIZE);
      }
      if (layout === 'blank') require(backup.every(b => b === 255), 'Unrecognized data on this device. Nothing was changed.');
      $('backup').hidden = $('backup-note').hidden = false;
      $('plan').textContent = layout === 'orb'
        ? 'Ready to add PrintSphere. Your Orb settings, SD themes and cached artwork will be kept. Save the recovery backup to continue.'
        : 'Ready for a first installation of Orb and PrintSphere. Save the recovery backup to continue.';
      $('install').textContent = 'Install both firmwares';
    } else {
      $('backup').hidden = $('backup-note').hidden = true;
      $('plan').textContent = 'Orb + PrintSphere detected. This updates both firmwares and keeps your settings, themes and selected startup firmware.';
      $('install').textContent = 'Update both firmwares';
    }
    $('ready').hidden = $('disconnect').hidden = false;
    $('progress').hidden = true;
    status('Device checked. Ready when you are.');
  } catch (error) {
    await disconnect();
    status(error.name === 'NotFoundError' ? 'No device selected. Connect when you are ready.' : error.message, true);
  } finally { lock(false); }
});
$('backup').addEventListener('click', () => {
  const url = URL.createObjectURL(new Blob([backup], {type: 'application/octet-stream'}));
  const link = document.createElement('a');
  link.href = url; link.download = 'orb-recovery-' + new Date().toISOString().replaceAll(':', '-') + '.bin';
  link.click(); setTimeout(() => URL.revokeObjectURL(url), 60000);
  backupSaved = true; $('backup').textContent = 'Save backup again'; lock(false);
});
$('disconnect').addEventListener('click', async () => { lock(true); await disconnect(); status('Disconnected. Your device is restarting.'); });
$('install').addEventListener('click', async () => {
  lock(true);
  let wrote = false;
  try {
    const files = await downloadRelease();
    require(identifyLayout(await readVerified(0x8000, 4096)) === layout, 'Device layout changed. Reconnect before installing.');
    const plan = flashPlan(layout, files);
    status('Installing both firmwares. Keep this page open and the cable connected…');
    wrote = true;
    await loader.writeFlash({fileArray: plan, flashSize: 'keep', flashMode: 'keep', flashFreq: 'keep',
      eraseAll: false, compress: true, calculateMD5Hash: md5,
      reportProgress: (file, done, total) => progress(100 * (file + done / total) / plan.length)});
    // Check private storage survived an in-place migration before rebooting.
    if (layout === 'orb') {
      require(md5(await readVerified(0x9000, 0x5000)) === md5(backup.slice(0x9000, 0xE000)), 'Orb settings verification failed. Keep your recovery backup.');
    }
    await disconnect();
    progress(100);
    status('Installation complete and verified. Your device is restarting.');
  } catch (error) {
    // Do not boot a partially written image. Reconnect and retry or restore backup.
    await disconnect(!wrote);
    status(error.message + (wrote ? ' Installation did not finish. Reconnect and retry; keep your recovery backup.' : ' Nothing was written.'), true);
  } finally { lock(false); }
});
window.addEventListener('beforeunload', event => { if (busy) { event.preventDefault(); event.returnValue = ''; } });
try {
  const response = await fetch('release-index.json', {cache: 'no-store'});
  require(response.ok, 'Release list is not available yet. Publish a companion release first.');
  releases = await response.json();
  require(Array.isArray(releases) && releases.length, 'No companion releases have been published yet.');
  $('release').replaceChildren(...releases.map((release, index) => {
    const option = document.createElement('option'); option.value = index;
    option.textContent = release.tag + (index === 0 ? ' · Latest' : '') + (release.prerelease ? ' · Preview' : '');
    return option;
  }));
  status('Choose a release, then connect your device.');
} catch (error) { status(error.message, true); }
if (!('serial' in navigator)) {
  $('support').textContent = 'USB installation needs Chrome or Edge on a desktop computer. Open this page there to continue.';
}
lock(false);
