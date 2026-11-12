// SPDX-License-Identifier: MIT
// Source maintenance only. Never compiles or changes the installed keyboard.
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import crypto from 'node:crypto';
import {fileURLToPath} from 'node:url';
import {execFileSync} from 'node:child_process';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const manifestPath = path.join(root, 'packaging/upstream.json');
const patchDir = path.join(root, 'packaging/patches');
const hash = bytes => crypto.createHash('sha256').update(bytes).digest('hex');
const run = (program, args, cwd = root) => execFileSync(program, args, {cwd, maxBuffer: 16 * 1024 * 1024});
const safe = name => typeof name === 'string' && !path.isAbsolute(name) && !name.split('/').some(p => p === '..' || p === '') && !name.includes('\0');
const names = [
    '0001-build-settings-and-identity.patch',
    '0002-keyboard-interface-and-input.patch',
    '0003-laptop-and-native-integration.patch',
    '0004-optional-tools-and-data.patch',
    '0005-native-regression-coverage.patch',
];
const production = name => ['src/', 'kcm/', 'kwin/', 'autotests/', 'tests/', 'tools/', 'plasmoid/', 'data/', 'po/', 'LICENSES/'].some(p => name.startsWith(p)) || ['CMakeLists.txt', 'LICENSE', 'REUSE.toml'].includes(name);
const group = name => {
    if (name.startsWith('autotests/') || name.startsWith('tests/')) return 4;
    if (name.startsWith('kwin/') || name.startsWith('src/laptop/') || name.startsWith('plasmoid/')) return 2;
    if (name === 'CMakeLists.txt' || name.endsWith('/CMakeLists.txt') || name.startsWith('kcm/') || /src\/org\..*\.(desktop|xml)$/.test(name)) return 0;
    if (name.startsWith('src/')) {
        if (/^src\/(clipboard|prediction|swipekb|stt|theme)\//.test(name)) return 3;
        return 1;
    }
    if (production(name)) return 3;
    throw Error('Unclassified source path: ' + name);
};

function readManifest() {
    const m = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
    if (m.schema !== 1 || !/^6\.[0-9]+\.[0-9]+$/.test(m.upstream.version) || !/^[a-f0-9]{40}$/.test(m.upstream.commit) || !/^[a-f0-9]{64}$/.test(m.upstream.sha256)) throw Error('Invalid upstream pin');
    if (m.patches && JSON.stringify(m.patches.map(p => p.name)) !== JSON.stringify(names)) throw Error('Unexpected patch order');
    return m;
}

function check(m) {
    if (!m.patches || !m.production_files) throw Error('Refresh the reviewed patch series first');
    const series = fs.readFileSync(path.join(patchDir, 'series'), 'utf8').trim().split('\n');
    if (JSON.stringify(series) !== JSON.stringify(names)) throw Error('Series order differs');
    for (const patch of m.patches) {
        if (!safe(patch.name) || hash(fs.readFileSync(path.join(patchDir, patch.name))) !== patch.sha256) throw Error('Patch digest differs: ' + patch.name);
    }
    for (const file of [...m.production_files, ...m.assets]) {
        if (!safe(file.path) || !fs.lstatSync(path.join(root, file.path)).isFile()) throw Error('Unsafe source path');
        if (hash(fs.readFileSync(path.join(root, file.path))) !== file.sha256) throw Error('Refresh the patch after reviewing: ' + file.path);
    }
    const actual = [];
    const walk = (directory, prefix) => {
        for (const item of fs.readdirSync(directory, {withFileTypes: true})) {
            const name = path.posix.join(prefix, item.name);
            if (item.isDirectory()) walk(path.join(directory, item.name), name);
            else if (item.isFile()) actual.push(name);
            else throw Error('Non-regular production source: ' + name);
        }
    };
    for (const name of ['src', 'kcm', 'kwin', 'autotests', 'tests', 'tools', 'plasmoid', 'data', 'po', 'LICENSES']) {
        if (fs.existsSync(path.join(root, name))) walk(path.join(root, name), name);
    }
    for (const name of ['CMakeLists.txt', 'LICENSE', 'REUSE.toml']) if (fs.existsSync(path.join(root, name))) actual.push(name);
    const expected = [...m.production_files, ...m.assets].map(f => f.path).sort();
    if (JSON.stringify(actual.sort()) !== JSON.stringify(expected)) throw Error('Production file set changed; review and refresh the series');
    const spec = fs.readFileSync(path.join(root, 'packaging/uke-plasma-keyboard-custom.spec'), 'utf8');
    if (!spec.includes('%global upstream_version ' + m.upstream.version) || !spec.includes('%autosetup -S git -n plasma-keyboard-%{upstream_version} -p1')) throw Error('Recipe must use the original pinned source and Git patches');
    const recipePatches = [...spec.matchAll(/^Patch[0-9]+:\s+(\S+)$/gm)].map(p => p[1]);
    if (JSON.stringify(recipePatches) !== JSON.stringify(names)) throw Error('Recipe patch order differs');
    console.log('Upstream pin, five ordered patches, production sources and retained asset digests passed');
}

function unpack(m, archive, task) {
    if (hash(fs.readFileSync(archive)) !== m.upstream.sha256) throw Error('Official archive digest differs');
    const prefix = `plasma-keyboard-${m.upstream.version}`;
    const members = run('tar', ['-tJf', archive]).toString().trim().split('\n');
    if (members.some(n => !n.startsWith(prefix + '/') || !safe(n.replace(/\/$/, '')))) throw Error('Unsafe upstream archive member');
    const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'uke-keyboard-source-'));
    fs.chmodSync(temp, 0o700);
    try {
        run('tar', ['--no-same-owner', '--no-same-permissions', '-xJf', archive, '-C', temp]);
        return task(path.join(temp, prefix));
    } finally {
        fs.rmSync(temp, {recursive: true, force: true});
    }
}

const mode = process.argv[2] || '--check';
const m = readManifest();
if (mode === '--check') {
    check(m);
} else if (mode === '--verify-base') {
    check(m);
    const archive = path.resolve(process.argv[3] || '');
    unpack(m, archive, base => {
        for (const patch of m.patches) {
            const file = path.join(patchDir, patch.name);
            run('git', ['apply', '--check', '--whitespace=nowarn', file], base);
            run('git', ['apply', '--whitespace=nowarn', file], base);
        }
        for (const file of m.production_files) {
            const target = path.join(base, file.path);
            if (!fs.existsSync(target) || hash(fs.readFileSync(target)) !== file.sha256) throw Error('Patch round trip differs: ' + file.path);
        }
        console.log('Official source plus the ordered series reproduces every reviewed production file');
    });
} else if (mode === '--refresh') {
    const archive = path.resolve(process.argv[3] || '');
    const assetPaths = new Set(m.assets.map(a => a.path));
    for (const file of m.assets) {
        if (!safe(file.path) || hash(fs.readFileSync(path.join(root, file.path))) !== file.sha256) throw Error('Retained asset changed: ' + file.path);
    }
    const files = run('git', ['ls-files', '--cached', '--others', '--exclude-standard', '-z']).toString().split('\0').filter(Boolean).filter(production).filter(n => !assetPaths.has(n)).sort();
    if (new Set(files).size !== files.length || files.some(n => !safe(n))) throw Error('Duplicate or unsafe source paths');
    unpack(m, archive, base => {
        run('git', ['init', '-q', '--initial-branch=source'], base);
        run('git', ['add', '-f', '--', '.'], base);
        const baseline = run('git', ['write-tree'], base).toString().trim();
        for (const name of files) {
            const from = path.join(root, name), to = path.join(base, name);
            const stat = fs.lstatSync(from);
            if (!stat.isFile()) throw Error('Only regular production files are accepted: ' + name);
            fs.mkdirSync(path.dirname(to), {recursive: true});
            fs.copyFileSync(from, to); fs.chmodSync(to, stat.mode & 0o777);
        }
        run('git', ['add', '-f', '--', ...files], base);
        fs.mkdirSync(patchDir, {recursive: true});
        m.patches = names.map((name, index) => {
            const paths = files.filter(file => group(file) === index);
            const bytes = run('git', ['diff', '--cached', '--binary', '--full-index', baseline, '--', ...paths], base);
            if (!bytes.length) throw Error('Empty feature patch: ' + name);
            fs.writeFileSync(path.join(patchDir, name), bytes);
            return {name, sha256: hash(bytes)};
        });
        m.production_files = files.map(name => ({path: name, sha256: hash(fs.readFileSync(path.join(root, name)))}));
        fs.writeFileSync(path.join(patchDir, 'series'), names.join('\n') + '\n');
        fs.writeFileSync(manifestPath, JSON.stringify(m, null, 2) + '\n');
        console.log('Refreshed five feature patches from the reviewed expanded source');
    });
} else {
    throw Error('Use --check, --refresh ARCHIVE, or --verify-base ARCHIVE');
}
