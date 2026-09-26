#!/usr/bin/env python3
"""Audit the exact public Arduino library scope (not a board compilation)."""
import json
from pathlib import Path, PurePosixPath
import re
import sys


def safe_file(root, name):
    path = PurePosixPath(name)
    if not name or str(path) != name or path.is_absolute() or '..' in path.parts or any(c in name for c in '*?[]\\'):
        raise ValueError(f'Invalid policy path: {name}')
    current = root
    for part in path.parts:
        current = current / part
        if current.is_symlink():
            raise ValueError(f'Symlink rejected: {name}')
    if not current.is_file():
        raise ValueError(f'Missing regular file: {name}')
    return current


def policy(root):
    spec = json.loads(safe_file(root, 'scripts/public-files.json').read_text())
    sync, local = spec['sync'], spec['local']
    all_names = sync + local
    if len(all_names) != len(set(all_names)):
        raise ValueError('Duplicate policy entries')
    for name in sync:
        if not (name.startswith('src/') or name.startswith('examples/') or name == 'keywords.txt'):
            raise ValueError(f'Not a promotable library file: {name}')
    for name in all_names:
        safe_file(root, name)
    return spec


def verify(root):
    root = root.resolve()
    spec = policy(root)
    allowed = set(spec['sync'] + spec['local'])
    def walk(directory):
        for item in directory.iterdir():
            if directory == root and item.name == '.git':
                continue
            name = item.relative_to(root).as_posix()
            if item.is_symlink():
                raise ValueError(f'Symlink rejected: {name}')
            if item.is_dir():
                if not any(entry.startswith(name + '/') for entry in allowed):
                    raise ValueError(f'Unexpected directory: {name}')
                walk(item)
            elif name not in allowed or not item.is_file():
                raise ValueError(f'Unexpected public file: {name}')
    walk(root)
    properties = dict(line.split('=', 1) for line in (root / 'library.properties').read_text().splitlines() if '=' in line)
    if properties.get('name') != 'IOSignal' or not re.fullmatch(r'\d+\.\d+\.\d+', properties.get('version', '')):
        raise ValueError('Invalid library name/version')
    print(f'Public scope verified: {len(allowed)} files, library {properties["version"]}')
    return spec


if __name__ == '__main__':
    verify(Path(sys.argv[1]) if len(sys.argv) == 2 else Path(__file__).resolve().parents[1])
