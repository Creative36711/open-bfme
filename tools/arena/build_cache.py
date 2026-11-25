#!/usr/bin/env python3
"""Checkout-local iteration snapshots; never an acceptance gate."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tarfile
import tempfile

REPO = Path(__file__).resolve().parents[2]
ROOTS = ('engine/build', 'godot/bin', 'godot/.godot')
CHECK = REPO / 'tools/arena/build_engine.sh'


def output(*args):
    return subprocess.check_output(args, cwd=REPO, text=True).strip()


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def identity():
    output('git', 'diff', '--quiet', 'HEAD', '--')
    output('git', 'diff', '--cached', '--quiet', 'HEAD', '--')
    mode, kind, pin = output('git', 'ls-tree', 'HEAD', 'engine/thirdparty/godot-cpp').split()[0:3]
    if (mode, kind) != ('160000', 'commit') or os.environ.get('GODOTCPP_REV', pin) != pin:
        raise ValueError('expected the committed godot-cpp gitlink')
    compiler = shlex.split(os.environ.get('CXX') or 'c++')
    return {
        'schema': 1, 'checkout': str(REPO), 'platform': list(os.uname())[:1] + [os.uname().machine],
        'tree': output('git', 'rev-parse', 'HEAD^{tree}'), 'godot_cpp': pin,
        'build_type': os.environ.get('BUILD_TYPE') or 'Release',
        'flags': os.environ.get('OPENBFME_EXTRA_CXX_FLAGS') or '-include cstddef',
        'launcher': os.environ.get('OPENBFME_CXX_LAUNCHER') or '',
        'compiler': compiler, 'compiler_version': output(*compiler, '--version'),
    }


def inspect(root):
    paths = ['engine/build/libopenbfme_core.a', 'engine/build/openbfme_tests']
    paths.extend(str(p.relative_to(root)) for p in sorted((root / 'godot/bin').iterdir())
                 if p.is_file() and (p.name == 'libopenbfme.so' or p.name.startswith('openbfme.linux')))
    if len(paths) == 2 or not os.access(root / paths[1], os.X_OK):
        raise ValueError('expected extension and executable tests')
    return {name: sha(root / name) for name in paths}


def check_outputs():
    """build_engine.sh --check exits 0 even for missing outputs, so read its report."""
    report = output(CHECK, '--check')
    failed = [line.strip() for line in report.splitlines() if '\u2717' in line]
    if failed:
        raise ValueError('build_engine.sh --check: ' + '; '.join(failed))
    print(f'build_engine.sh --check: {report}', flush=True)


def verify_build_settings(config):
    cache = dict(line.split('=', 1) for line in (REPO / ROOTS[0] / 'CMakeCache.txt').read_text().splitlines()
                 if '=' in line and not line.startswith(('#', '//')))
    compiler = next(value for name, value in cache.items() if name.startswith('CMAKE_CXX_COMPILER:'))
    launcher = next((value for name, value in cache.items() if name.startswith('CMAKE_CXX_COMPILER_LAUNCHER')), '')
    if (cache.get('CMAKE_BUILD_TYPE:STRING') != config['build_type']
            or cache.get('CMAKE_CXX_FLAGS:STRING') != config['flags']
            or launcher != config['launcher']
            or output(compiler, '--version') != config['compiler_version']):
        raise ValueError('configured build settings do not match the key')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['key', 'save', 'restore'])
    parser.add_argument('store', type=Path, nargs='?')
    args = parser.parse_args()
    config = identity()
    key = hashlib.sha256(json.dumps(config, sort_keys=True).encode()).hexdigest()
    print(f'cache key: {key}', flush=True)
    if args.action == 'key':
        return
    print('key inputs: ' + json.dumps(config, sort_keys=True), flush=True)
    if args.store is None or args.store.resolve().is_relative_to(REPO):
        parser.error('provide a cache directory outside the checkout')
    archive = args.store.resolve() / f'{key}.tar.gz'
    if args.action == 'save':
        if (REPO / ROOTS[0] / '.arena-cache-key').read_text().strip() != f'cache key: {key}':
            raise ValueError('build provenance does not match; run build_engine.sh --clean --tests')
        check_outputs()
        verify_build_settings(config)
        if not (REPO / 'engine/thirdparty/godot-cpp/src').is_dir():
            raise ValueError('godot-cpp sources are missing')
        binaries = inspect(REPO)
        print('before:', json.dumps(binaries, sort_keys=True), flush=True)
        archive.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=archive.parent) as temp:
            temp = Path(temp)
            manifest = temp / 'manifest.json'
            manifest.write_text(json.dumps({'config': config, 'binaries': binaries}))
            with tarfile.open(temp / 'snapshot', 'w:gz', compresslevel=1) as bundle:
                bundle.add(manifest, 'manifest.json')
                for name in ROOTS:
                    bundle.add(REPO / name, name)
            os.link(temp / 'snapshot', archive)  # Publish atomically without overwriting.
    else:
        # CMake/Ninja embed absolute paths. Restore only to the original checkout.
        for name in ROOTS:
            if (REPO / name).exists() or (REPO / name).is_symlink():
                raise ValueError(f'move existing output aside first: {name}')
        with tempfile.TemporaryDirectory(dir=REPO) as temp:
            stage = Path(temp)
            with tarfile.open(archive, 'r:gz') as bundle:
                manifest = json.load(bundle.extractfile('manifest.json'))
                if manifest['config'] != config:
                    raise ValueError('snapshot key mismatch')
                members = bundle.getmembers()
                for member in members:
                    path = Path(member.name)
                    if (path.is_absolute() or '..' in path.parts
                            or not (member.name == 'manifest.json' or any(path.is_relative_to(r) for r in ROOTS))
                            or not (member.isfile() or member.isdir())):
                        raise ValueError(f'unsupported archive entry: {member.name}')
                if hasattr(tarfile, 'data_filter'):  # Python 3.12+; 3.11 takes no filter argument
                    bundle.extractall(stage, members=members, filter='data')
                else:
                    bundle.extractall(stage, members=members)
            print('before:', json.dumps(manifest['binaries'], sort_keys=True), flush=True)
            if (stage / ROOTS[0] / '.arena-cache-key').read_text().strip() != f'cache key: {key}':
                raise ValueError('staged build provenance mismatch')
            if not (REPO / 'engine/thirdparty/godot-cpp/src').is_dir():
                raise ValueError('populate pinned godot-cpp before restoring')
            if inspect(stage) != manifest['binaries']:
                raise ValueError('staged binary SHA-256 mismatch')
            installed = []
            try:
                for name in ROOTS:
                    (stage / name).rename(REPO / name)
                    installed.append(name)
                check_outputs()
                verify_build_settings(config)
                binaries = inspect(REPO)
                print('after:', json.dumps(binaries, sort_keys=True), flush=True)
                if binaries != manifest['binaries']:
                    raise ValueError('restored binary SHA-256 mismatch')
            except BaseException:
                for name in reversed(installed):
                    (REPO / name).rename(stage / name)
                raise
    print(f'archive SHA-256: {sha(archive)}', flush=True)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError, tarfile.TarError, subprocess.CalledProcessError) as error:
        raise SystemExit(f'build cache: {error}') from error
