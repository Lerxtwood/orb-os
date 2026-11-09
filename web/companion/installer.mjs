import {FLASH_SIZE, require, identifyLayout, validateRelease, validateImage, validateCache, flashPlan, settingsResetPlan, resetToFirmware} from './layout.mjs';
const $ = id => document.getElementById(id);
let releases = [], loader, transport, layout, settingsHash, partitionHash, cacheError = '', busy = false;
const connectionLog = $('log');
const connectionDetails = connectionLog.closest('details');
let logScrollFrame;
function followConnectionLog() {
  if (!connectionDetails.open || logScrollFrame !== undefined) return;
  // Follow after layout, including when opening the details or wrapping lines.
  logScrollFrame = requestAnimationFrame(() => {
    logScrollFrame = undefined;
    if (connectionDetails.open) connectionLog.scrollTop = connectionLog.scrollHeight;
  });
}
connectionDetails.addEventListener('toggle', followConnectionLog);
new ResizeObserver(followConnectionLog).observe(connectionLog);
const log = message => {
  connectionLog.textContent = (connectionLog.textContent + message + '\n').slice(-14000);
  followConnectionLog();
};
function status(message, error = false) {
  $('status').textContent = message; $('status').classList.toggle('error', error);
  if (error) log('Error: ' + message);
}
function progress(value) { $('progress').hidden = false; $('progress').value = value; }
function updateReleaseNotes() {
  const entry = releases[Number($('release').value)];
  $('release-notes').hidden = !entry;
  if (!entry) return;
  $('release-notes-title').textContent = 'Release notes · ' + entry.tag;
  // Use text nodes for public release content; never insert release HTML.
  const notes = typeof entry.notes === 'string' && entry.notes.trim()
    ? entry.notes : 'Release notes are available on GitHub using the link below.';
  const blocks = notes.replace(/\r\n/g, '\n').split(/\n\s*\n/).filter(block => block.trim());
  $('release-notes-body').replaceChildren(...blocks.map(block => {
    const heading = /^#{1,6}\s+/.test(block);
    const node = document.createElement(heading ? 'h3' : 'p');
    node.textContent = block.replace(/^#{1,6}\s+/gm, '')
      .replace(/^[-*] /gm, '• ').replace(/\*\*([^*]+)\*\*/g, '$1')
      .replace(/`([^`]+)`/g, '$1');
    return node;
  }));
  const fallback = 'https://github.com/Lerxtwood/orb-os/releases/tag/' + encodeURIComponent(entry.tag);
  let releaseUrl;
  try { releaseUrl = new URL(entry.releaseUrl || fallback); } catch { releaseUrl = new URL(fallback); }
  $('release-notes-link').href = releaseUrl.protocol === 'https:' && releaseUrl.hostname === 'github.com'
    ? releaseUrl.href : fallback;
}
$('release').addEventListener('change', updateReleaseNotes);
function lock(value) {
  busy = value;
  const force = $('confirm-force').checked;
  $('connect').disabled = value || !!loader || !releases.length || !('serial' in navigator);
  $('release').disabled = value || !!loader || !releases.length;
  $('disconnect').disabled = value;
  $('install').disabled = value || !layout || (layout === 'unknown' && !force) ||
    (!force && !!cacheError && !$('rebuild-cache').checked);
  $('confirm-force').disabled = value || !layout;
  $('rebuild-cache').disabled = value || force;
  $('confirm-reset').disabled = value || force;
  $('reset').disabled = value || force || layout !== 'companion' || !$('confirm-reset').checked;
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
  let restarted = false;
  if (transport && reset) {
    try {
      log('Restarting firmware with an explicit DTR/RTS reset pulse...');
      await resetToFirmware(transport);
      restarted = true;
    } catch (error) { log('Automatic restart failed: ' + error.message); }
  }
  if (transport) { try { await transport.disconnect(); } catch (error) { log(error.message); } }
  loader = transport = layout = settingsHash = partitionHash = undefined;
  cacheError = '';
  $('rebuild-cache').checked = false;
  $('confirm-reset').checked = false;
  $('confirm-force').checked = false;
  $('force-install').hidden = true;
  $('force-install').open = false;
  $('reset-settings').hidden = true;
  $('reset-settings').open = false;
  $('ready').hidden = $('disconnect').hidden = true;
  lock(false);
  return restarted;
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
    const table = await readVerified(0x8000, 4096);
    partitionHash = md5(table);
    layout = identifyLayout(table);
    cacheError = '';
    $('rebuild-cache').checked = false;
    $('confirm-force').checked = false;
    if (layout === 'orb') {
      const cache = await readVerified(0x650000, 8192);
      try { validateCache(cache); }
      catch (error) { cacheError = error.message; }
    }
    if (layout === 'blank') {
      // An erased partition table alone does not establish that the device is empty.
      // Check in bounded chunks without retaining or downloading a firmware backup.
      status('Checking that the device is empty...');
      for (let address = 0; address < FLASH_SIZE; address += 0x40000) {
        if (!(await readVerified(address, 0x40000)).every(b => b === 255)) {
          layout = 'unknown';
          log('Erased partition table with existing flash data: clean-install confirmation required.');
          break;
        }
        progress(100 * (address + 0x40000) / FLASH_SIZE);
      }
    }
    $('force-install').hidden = false;
    $('force-install').open = layout === 'unknown';
    updatePlan();
    $('ready').hidden = $('disconnect').hidden = false;
    $('progress').hidden = true;
    status(layout === 'unknown' ? 'Other firmware detected. Confirm Force clean install below to replace it. Nothing has been changed.'
      : cacheError ? cacheError : 'Device checked. Ready when you are.', !!cacheError);
  } catch (error) {
    await disconnect();
    status(error.name === 'NotFoundError' ? 'No device selected. Connect when you are ready.' : error.message, true);
  } finally { lock(false); }
});
function updatePlan() {
  const force = $('confirm-force').checked;
  $('cache-option').hidden = force || !['orb', 'companion'].includes(layout);
  $('reset-settings').hidden = force || layout !== 'companion';
  $('install').textContent = force ? 'Erase and install both firmwares'
    : layout === 'companion' ? 'Update both firmwares' : 'Install both firmwares';
  if (force) {
    $('plan').textContent = 'Ready to erase all internal flash and install Orb + PrintSphere. You will need to set up Wi-Fi again. SD-card files are kept.';
    return;
  }
  if (layout === 'unknown') {
    $('plan').textContent = 'This device has another firmware layout. Force clean install can replace it after you confirm deletion of its internal flash.';
    return;
  }
  const rebuild = $('rebuild-cache').checked;
  $('plan').textContent = layout === 'blank'
    ? 'Ready for a first installation of Orb and PrintSphere.'
    : (layout === 'companion' ? 'Ready to update both firmwares. ' : 'Ready to add PrintSphere. ') +
      (rebuild ? 'Your settings and SD themes will be kept. Cached artwork will be rebuilt on the next boot.'
               : 'Your settings, SD themes and cached artwork will be kept.');
}
$('confirm-force').addEventListener('change', () => {
  if (busy) return;
  $('confirm-reset').checked = false;
  updatePlan(); lock(false);
  status($('confirm-force').checked ? 'Clean install selected. Click Erase and install both firmwares to continue.'
    : layout === 'unknown' ? 'Confirm Force clean install to replace the existing firmware.'
    : cacheError || 'Device checked. Ready when you are.', !$('confirm-force').checked && !!cacheError);
});
$('rebuild-cache').addEventListener('change', () => {
  updatePlan(); lock(false);
  status(cacheError && !$('rebuild-cache').checked ? cacheError : 'Device checked. Ready when you are.',
    !!cacheError && !$('rebuild-cache').checked);
});
$('disconnect').addEventListener('click', async () => {
  lock(true);
  const restarted = await disconnect();
  status(restarted ? 'Disconnected. Restart requested.' : 'Disconnected. Unplug and reconnect USB to restart the device.', !restarted);
});
$('confirm-reset').addEventListener('change', () => { if (!busy) lock(false); });
$('reset').addEventListener('click', async () => {
  if (busy) return;
  lock(true);
  let wrote = false;
  try {
    require($('confirm-reset').checked, 'Confirm deletion of both firmwares’ settings before resetting.');
    const plan = settingsResetPlan(layout);
    require(identifyLayout(await readVerified(0x8000, 4096)) === layout,
      'Device layout changed. Reconnect before resetting.');
    status('Resetting both firmwares’ settings. Keep this page open and the cable connected…');
    wrote = true;
    await loader.writeFlash({fileArray: plan, flashSize: 'keep', flashMode: 'keep', flashFreq: 'keep',
      eraseAll: false, compress: true, calculateMD5Hash: md5,
      reportProgress: (file, done, total) => progress(70 * (file + done / total) / plan.length)});
    const total = plan.reduce((sum, part) => sum + part.data.length, 0);
    let verified = 0;
    for (const part of plan) {
      for (let offset = 0; offset < part.data.length; offset += 0x8000) {
        const size = Math.min(0x8000, part.data.length - offset);
        require((await readVerified(part.address + offset, size)).every(byte => byte === 255),
          'Settings reset verification failed.');
        verified += size;
        progress(70 + 30 * verified / total);
      }
    }
    const restarted = await disconnect();
    progress(100);
    status(restarted ? 'Both firmwares’ settings cleared and verified. Restart requested. Set up Wi-Fi in Orb, then open PrintSphere.'
      : 'Both firmwares’ settings cleared and verified. Unplug and reconnect USB, then set up Wi-Fi in Orb and open PrintSphere.', !restarted);
  } catch (error) {
    await disconnect(!wrote);
    status(error.message + (wrote ? ' Reset did not finish. Reconnect and retry before restarting the device.' : ' Nothing was written.'), true);
  } finally { lock(false); }
});
$('install').addEventListener('click', async () => {
  if (busy) return;
  lock(true);
  let wrote = false;
  try {
    const force = $('confirm-force').checked;
    require(layout && (layout !== 'unknown' || force), 'Confirm Force clean install before replacing other firmware. Nothing was changed.');
    const files = await downloadRelease();
    const currentTable = await readVerified(0x8000, 4096);
    // A blank table with leftover data is treated as unknown during connection.
    require(md5(currentTable) === partitionHash &&
      (identifyLayout(currentTable) === layout || (layout === 'unknown' && identifyLayout(currentTable) === 'blank')),
      'Device layout changed. Reconnect before installing.');
    require(!force || $('confirm-force').checked, 'Clean-install confirmation was cleared. Nothing was changed.');
    const rebuild = $('rebuild-cache').checked;
    if (!force && layout === 'orb' && !rebuild) validateCache(await readVerified(0x650000, 8192));
    if (!force && layout !== 'blank') settingsHash = md5(await readVerified(0x9000, 0x5000));
    const plan = flashPlan(layout, files, rebuild, force);
    status(force ? 'Erasing internal flash and installing both firmwares. Keep this page open and the cable connected…'
      : 'Installing both firmwares. Keep this page open and the cable connected…');
    wrote = true;
    await loader.writeFlash({fileArray: plan, flashSize: 'keep', flashMode: 'keep', flashFreq: 'keep',
      eraseAll: force, compress: true, calculateMD5Hash: md5,
      reportProgress: (file, done, total) => progress(100 * (file + done / total) / plan.length)});
    // Check private storage survived an in-place migration before rebooting.
    if (!force && layout !== 'blank') {
      require(md5(await readVerified(0x9000, 0x5000)) === settingsHash, 'Orb settings verification failed.');
    }
    if (force) require(identifyLayout(await readVerified(0x8000, 4096)) === 'companion',
      'Clean-install partition verification failed. Reconnect and retry before restarting.');
    const restarted = await disconnect();
    progress(100);
    status(restarted ? 'Installation complete and verified. Restart requested.'
      : 'Installation complete and verified, but automatic restart failed. Unplug and reconnect USB to start the device.', !restarted);
  } catch (error) {
    // Do not boot a partially written image. Reconnect and retry.
    await disconnect(!wrote);
    status(error.message + (wrote ? ' Installation did not finish. Reconnect and retry.' : ' Nothing was written.'), true);
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
  updateReleaseNotes();
  status('Choose a release, then connect your device.');
} catch (error) { status(error.message, true); }
if (!('serial' in navigator)) {
  $('support').textContent = 'USB installation needs Chrome or Edge on a desktop computer. Open this page there to continue.';
}
lock(false);
