"""Package only the two installable DLLs and public instructions."""
from pathlib import Path
import hashlib
import zipfile

ROOT = Path(__file__).resolve().parent
archive = ROOT / 'dist/SkySpiritExporter-v0.2.0-full-windows-x64.zip'
files = {
    'dxgi.dll': 'dist/dxgi.dll',
    'mods/SkySpiritBridge.dll': 'dist/SkySpiritBridge.dll',
    'УСТАНОВКА.txt': 'УСТАНОВКА.txt',
    'README.md': 'README.md',
    'LICENSE': 'LICENSE',
    'MinHook-LICENSE.txt': 'vendor/minhook/LICENSE.txt',
}
for source in files.values():
    if not (ROOT / source).is_file():
        raise SystemExit(f'Missing: {source}')
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as output:
    for name, source in files.items():
        output.write(ROOT / source, name)
digest = hashlib.sha256(archive.read_bytes()).hexdigest()
(ROOT / 'dist/SHA256SUMS-v0.2.0.txt').write_text(f'{digest}  {archive.name}\n')
print(archive)
