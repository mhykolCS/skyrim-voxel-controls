#!/usr/bin/env python3
"""Build the local Steve model. Mojang's skin is fetched separately, not GPL source."""
import hashlib
import io
import json
import subprocess
import urllib.request
import zipfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]
out = root / 'local' / 'steve'
out.mkdir(parents=True, exist_ok=True)
dep = json.loads((root / 'DEPENDENCIES.json').read_text())['nifly']
nifly = root / 'tools' / 'nifly'
if not nifly.exists():
    subprocess.run(['git', 'init', str(nifly)], check=True)
    subprocess.run(['git', '-C', str(nifly), 'remote', 'add', 'origin', dep['url']], check=True)
    subprocess.run(['git', '-C', str(nifly), 'fetch', '--depth=1', 'origin', dep['commit']], check=True)
    subprocess.run(['git', '-C', str(nifly), 'checkout', '--detach', 'FETCH_HEAD'], check=True)
actual = subprocess.check_output(['git', '-C', str(nifly), 'rev-parse', 'HEAD'], text=True).strip()
if actual != dep['commit']:
    raise SystemExit('Existing nifly checkout differs from the pin; left unchanged')

def download(url):
    with urllib.request.urlopen(url, timeout=60) as response:
        return response.read()

skin = out / 'steve.png'
provenance = out / 'provenance.json'
if not skin.exists():
    manifest = json.loads(download('https://piston-meta.mojang.com/mc/game/version_manifest_v2.json'))
    version = next(v for v in manifest['versions'] if v['id'] == '1.8.9')
    client = json.loads(download(version['url']))['downloads']['client']
    data = download(client['url'])
    if hashlib.sha1(data).hexdigest() != client['sha1']:
        raise SystemExit('Minecraft archive does not match Mojang checksum')
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        texture = archive.read('assets/minecraft/textures/entity/steve.png')
    skin.write_bytes(texture)
    provenance.write_text(json.dumps({'version': '1.8.9', 'url': client['url'], 'sha1': client['sha1'],
        'texture_sha256': hashlib.sha256(texture).hexdigest()}, indent=2) + '\n')

build = root / 'build-nifly'
subprocess.run(['cmake', '-S', str(nifly), '-B', str(build), '-G', 'Ninja', '-DBUILD_TESTING=OFF', '-DCMAKE_BUILD_TYPE=Release'], check=True)
subprocess.run(['cmake', '--build', str(build), '--parallel', '4'], check=True)
generator = build / 'make-steve'
subprocess.run(['c++', '-std=c++17', '-O2', '-I'+str(nifly/'include'), '-I'+str(nifly/'external'),
    str(root/'tools/make-steve.cpp'), str(build/'src/libnifly.a'), '-o', str(generator)], check=True)
subprocess.run([str(generator), str(out/'steve.nif')], check=True)
subprocess.run(['magick', str(skin), '-filter', 'point', '-resize', '1024x1024', '-alpha', 'off',
    '-define', 'dds:compression=dxt1', '-define', 'dds:mipmaps=0', str(out/'steve.dds')], check=True)
subprocess.run(['magick', '-size', '4x4', 'xc:rgb(128,128,255)', '-alpha', 'off',
    '-define', 'dds:compression=dxt1', '-define', 'dds:mipmaps=0', str(out/'steve_n.dds')], check=True)
print('Local Steve assets ready. Mojang texture remains in ignored local/steve/.')
