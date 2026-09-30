#!/usr/bin/env python3
"""Install only VoxelControls, with a reversible file manifest. Never edits saves."""
import argparse
import hashlib
import json
import shutil
from datetime import datetime, timezone
from pathlib import Path

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('game', type=Path, help='Skyrim Special Edition directory')
parser.add_argument('--uninstall', action='store_true')
args = parser.parse_args()
game = args.game.resolve()
if not (game / 'SkyrimSE.exe').is_file():
    parser.error('SkyrimSE.exe was not found')
manifest_path = root / 'local' / 'install-manifest.json'
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
if args.uninstall:
    manifest = json.loads(manifest_path.read_text())
    if Path(manifest['game']) != game:
        parser.error('manifest belongs to a different game installation')
    for entry in manifest['files']:
        target = game / entry['path']
        if target.is_file() and digest(target) != entry['installed_sha256']:
            parser.error(f'{target.name} changed since installation; refusing to overwrite it')
    for entry in manifest['files']:
        target = game / entry['path']
        if target.exists():
            archive = manifest_path.parent / 'uninstalled' / entry['path']
            archive.parent.mkdir(parents=True, exist_ok=True)
            shutil.move(target, archive)
        if entry['backup']:
            shutil.copy2(entry['backup'], target)
    manifest_path.rename(manifest_path.with_name('uninstalled-manifest.json'))
    print('VoxelControls uninstalled. Previous files restored; removed files archived locally.')
else:
    sources = [(root / 'build-windows' / 'VoxelControls.dll', 'Data/SKSE/Plugins/VoxelControls.dll')]
    for name, folder in [('steve.nif', 'meshes'), ('steve.dds', 'textures'), ('steve_n.dds', 'textures')]:
        asset = root / 'local' / 'steve' / name
        if asset.is_file():
            sources.append((asset, f'Data/{folder}/VoxelControls/{name}'))
    if not sources[0][0].is_file():
        parser.error('Build the Windows plugin first')
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    old = json.loads(manifest_path.read_text()) if manifest_path.exists() else {'game': str(game), 'files': []}
    if Path(old['game']) != game:
        parser.error('existing manifest belongs to a different installation')
    entries = {entry['path']: entry for entry in old['files']}
    for source, relative in sources:
        target = game / relative
        if relative in entries and target.is_file() and digest(target) != entries[relative]['installed_sha256']:
            parser.error(f'{relative} changed outside this installer; refusing to replace it')
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    for source, relative in sources:
        target = game / relative
        backup = entries.get(relative, {}).get('backup')
        if target.exists() and relative not in entries:
            backup_path = manifest_path.parent / 'backups' / stamp / relative
            backup_path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(target, backup_path)
            backup = str(backup_path)
        target.parent.mkdir(parents=True, exist_ok=True)
        staged = target.with_name(target.name + '.new')
        shutil.copy2(source, staged)
        staged.replace(target)
        entries[relative] = {'path': relative, 'backup': backup, 'installed_sha256': digest(target)}
        manifest_path.write_text(json.dumps({'game': str(game), 'files': list(entries.values())}, indent=2) + '\n')
        print(f'Installed {target}')
