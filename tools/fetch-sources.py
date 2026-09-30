#!/usr/bin/env python3
"""Fetch the pinned open-source libraries, preserving any existing checkout."""
import json
import subprocess
from pathlib import Path
root = Path(__file__).resolve().parents[1]
dependencies = json.loads((root / 'DEPENDENCIES.json').read_text())
for name in ('CommonLibSSE-NG', 'imgui'):
    dep = dependencies[name]
    path = root / 'deps' / name
    if path.exists():
        actual = subprocess.check_output(['git', '-C', str(path), 'rev-parse', 'HEAD'], text=True).strip()
        if actual != dep['commit']:
            raise SystemExit(f'{name}: existing revision differs from the pin; left unchanged')
        print(f'{name}: already at pinned revision')
        continue
    path.mkdir(parents=True)
    subprocess.run(['git', 'init', str(path)], check=True)
    subprocess.run(['git', '-C', str(path), 'remote', 'add', 'origin', dep['url']], check=True)
    subprocess.run(['git', '-C', str(path), 'fetch', '--depth', '1', 'origin', dep['commit']], check=True)
    subprocess.run(['git', '-C', str(path), 'checkout', '--detach', 'FETCH_HEAD'], check=True)
