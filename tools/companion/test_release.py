import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from build_site import mirror, validate
from release_assets import FILES


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.payload = b'release-fixture'
        self.manifest = {'schema': 1, 'layout': 'orb-printsphere-v1', 'chip': 'ESP32-S3',
            'flashSize': 0x1000000, 'parts': [
                {'path': name, 'offset': offset, 'size': len(self.payload),
                 'sha256': hashlib.sha256(self.payload).hexdigest()}
                for name, (_, offset, _) in FILES.items()]}

    def test_only_public_images_are_mirrored(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp)
            entry = mirror(path, 'v1.2.3', json.dumps(self.manifest).encode(), lambda _: self.payload)
            self.assertEqual(set(p.name for p in (path / 'releases/v1.2.3').iterdir()),
                             {*FILES, 'companion-release.json'})
            self.assertEqual(entry['manifest'], 'releases/v1.2.3/companion-release.json')

    def test_rejects_modified_download(self):
        with tempfile.TemporaryDirectory() as temp, self.assertRaises(ValueError):
            mirror(Path(temp), 'v1.2.3', json.dumps(self.manifest).encode(), lambda _: b'bad')

    def test_rejects_path_traversal(self):
        with tempfile.TemporaryDirectory() as temp, self.assertRaises(ValueError):
            mirror(Path(temp), '../bad', json.dumps(self.manifest).encode(), lambda _: self.payload)

    def test_rejects_private_asset_and_wrong_address(self):
        bad = copy.deepcopy(self.manifest)
        bad['parts'][0]['path'] = 'orb-before-dualboot.bin'
        with self.assertRaises(ValueError):
            validate(bad)
        bad = copy.deepcopy(self.manifest)
        bad['parts'][0]['offset'] = 0x9000
        with self.assertRaises(ValueError):
            validate(bad)


if __name__ == '__main__':
    unittest.main()
