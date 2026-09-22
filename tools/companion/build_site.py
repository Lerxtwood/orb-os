"""Mirror compatible GitHub release assets onto Pages for same-origin downloads."""
import argparse
import hashlib
import json
import os
import re
import shutil
import urllib.request
from pathlib import Path
from package import ROOT, require
from release_assets import FILES


def request(url, token=None, binary=False):
    headers = {'User-Agent': 'orb-companion-installer', 'Accept': 'application/octet-stream' if binary else 'application/vnd.github+json'}
    if token:
        headers['Authorization'] = 'Bearer ' + token
    with urllib.request.urlopen(urllib.request.Request(url, headers=headers), timeout=120) as response:
        return response.read()


def validate(manifest):
    require(manifest.get('schema') == 1 and manifest.get('layout') == 'orb-printsphere-v1', 'Unsupported release')
    require(manifest.get('chip') == 'ESP32-S3' and manifest.get('flashSize') == 0x1000000, 'Unsupported hardware')
    parts = manifest.get('parts', [])
    require(len(parts) == len(FILES) and {p['path'] for p in parts} == set(FILES), 'Incomplete release')
    for part in parts:
        _, offset, capacity = FILES[part['path']]
        require(part['offset'] == offset and 0 < part['size'] <= capacity and
                re.fullmatch('[a-f0-9]{64}', part['sha256']), 'Invalid firmware metadata')


def mirror(site, tag, manifest_bytes, fetch):
    require(re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]*', tag) is not None, 'Unsafe release tag')
    manifest = json.loads(manifest_bytes)
    validate(manifest)
    destination = site / 'releases' / tag
    destination.mkdir(parents=True, exist_ok=True)
    for part in manifest['parts']:
        data = fetch(part['path'])
        require(len(data) == part['size'] and hashlib.sha256(data).hexdigest() == part['sha256'],
                f"Release checksum mismatch: {tag}/{part['path']}")
        (destination / part['path']).write_bytes(data)
    (destination / 'companion-release.json').write_bytes(manifest_bytes)
    return {'tag': tag, 'manifest': f'releases/{tag}/companion-release.json'}


def build(site, repository, limit=8, local=None, tag=None):
    site.mkdir(parents=True, exist_ok=True)
    shutil.copytree(ROOT / 'web/companion', site, dirs_exist_ok=True)
    entries = []
    if local:
        require(bool(tag), '--tag is required with --local')
        entries.append(mirror(site, tag, (local / 'companion-release.json').read_bytes(),
                              lambda name: (local / name).read_bytes()))
    else:
        token = os.environ.get('GH_TOKEN') or os.environ.get('GITHUB_TOKEN')
        releases = json.loads(request(f'https://api.github.com/repos/{repository}/releases?per_page=100', token))
        releases.sort(key=lambda release: release['prerelease'])
        for release in releases:
            if release['draft']:
                continue
            assets = {asset['name']: asset['url'] for asset in release['assets']}
            if not {'companion-release.json', *FILES}.issubset(assets):
                continue
            fetch = lambda name: request(assets[name], token, binary=True)
            entry = mirror(site, release['tag_name'], fetch('companion-release.json'), fetch)
            entry['prerelease'] = release['prerelease']
            entries.append(entry)
            if len(entries) >= limit:
                break
    require(bool(entries), 'No compatible public companion releases found')
    (site / 'release-index.json').write_text(json.dumps(entries, indent=2) + '\n')
    (site / '.nojekyll').touch()
    print(f'Prepared installer with {len(entries)} release(s): {site}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--site', type=Path, required=True)
    parser.add_argument('--repo', default=os.environ.get('GITHUB_REPOSITORY', 'Lerxtwood/orb-os'))
    parser.add_argument('--limit', type=int, default=8)
    parser.add_argument('--local', type=Path)
    parser.add_argument('--tag')
    args = parser.parse_args()
    build(args.site, args.repo, args.limit, args.local, args.tag)
