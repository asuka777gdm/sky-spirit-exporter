"""Package only native DLLs, Russian instructions and license notices."""
from pathlib import Path
import hashlib
import zipfile
ROOT = Path(__file__).resolve().parent
archive = ROOT / 'dist/native/SkySpiritExporter-v0.3.0-native-windows-x64.zip'
files = {
 'powrprof.dll': 'dist/native/powrprof.dll',
 'mods/SkySpiritBridge.dll': 'dist/native/SkySpiritBridge.dll',
 'УСТАНОВКА.txt': 'УСТАНОВКА.txt',
 'README.md': 'README.md',
 'LICENSE': 'LICENSE',
 'THIRD_PARTY.md': 'THIRD_PARTY.md',
 'licenses/MinHook.txt': 'vendor/minhook/LICENSE.txt',
 'licenses/ImGui.txt': 'vendor/imgui/LICENSE.txt',
 'licenses/nlohmann-json.txt': 'vendor/nlohmann/LICENSE',
}
for source in files.values():
 if not (ROOT / source).is_file(): raise SystemExit(f'Missing: {source}')
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as output:
 for name, source in files.items(): output.write(ROOT / source, name)
digest = hashlib.sha256(archive.read_bytes()).hexdigest()
(ROOT / 'dist/native/SHA256SUMS-v0.3.0.txt').write_text(f'{digest}  {archive.name}\n')
print(archive)
