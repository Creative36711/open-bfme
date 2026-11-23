// Тесты узла сборки бандла (WOTR-D-033). Запуск: node --test tools/relay/*.mjs
//
// Что здесь проверяется и почему именно это. узел ничего не изобретает: он скачивает
// запиненный выпуск, подставляет наши docs и передаёт сборку инструменту транспорта.
// Значит ценны не «счастливые» прогоны, а отказ: негодный пин, чужие байты, проглоченная
// подстановка, отчёт без проверок. Каждый случай ниже — то, что иначе молча дало бы
// допущенную сборку с неверным регламентом внутри.
//
// Тесты герметичны: сеть подменяется fetchImpl, кэш — временным каталогом. Реальный
// прогон сборки делает CI (.github/workflows/relay-bundle.yml), а не этот файл.
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join, relative, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { test } from 'node:test';

import {
  DEFAULT_PIN, DEFAULT_CACHE, META_MARKER, SC_DIR, BUILD_NODE, DIGEST_MAX_CHARS, REPO_ROOT,
  checkDigest, copyToWorkDir, docsCoincide, embeddedDigest, obtainPristineSnapshot, obtainSnapshot,
  parseNodeReport, promptDigest, readBundleFacts, readPin, readProfile, regulationVersion,
  sha256File, sha256Text, snapshotUsable, validatePin, validateProfile, verifyBundle,
  verifyPinnedArtifact, verifyArtifact,
} from './build.mjs';

const RELAY_DIR = dirname(fileURLToPath(import.meta.url));
const PROJECT = resolve(RELAY_DIR, '..', '..');
const PIN = JSON.parse(readFileSync(DEFAULT_PIN, 'utf8'));
const realDigest = readFileSync(join(PROJECT, PIN.inputs.digest), 'utf8');
const realRegulation = readFileSync(join(PROJECT, PIN.inputs.regulation), 'utf8');

/** Бандл-заглушка: те же якоря, что читает узел, без 1.5 МБ настоящего файла. */
function fakeBundle({ version = '1.16.9', runtime = version, digest = 'x', fingerprint = 'a'.repeat(16) } = {}) {
  return [
    '// ==UserScript==',
    `// @version      ${version}`,
    '// ==/UserScript==',
    `const SCRIPT_VERSION = (typeof GM_info !== 'undefined' && GM_info?.script?.version) || "${runtime}";`,
    `  buildFingerprint: "${fingerprint}",`,
    `  regulationDigest: ${JSON.stringify(digest)},`,
  ].join('\n');
}

const relayError = (code, fn) => assert.throws(fn, (error) => {
  assert.equal(error.relayCode, code, `ожидался код ${code}, получен ${error.relayCode}: ${error.message}`);
  return true;
});

test('pin.json в репозитории проходит собственную схему', () => {
  assert.deepEqual(validatePin(PIN), []);
  assert.equal(PIN.transport.artifact.path, `${SC_DIR}/relay.user.js`);
  // Кэш по умолчанию обязан лежать вне рабочего дерева: внутри него узел транспорта
  // принимал бы наше дерево за проверяемое и отказывал на `git status`.
  const rel = relative(REPO_ROOT, DEFAULT_CACHE);
  assert.ok(rel.startsWith('..'), `кэш не должен лежать в репозитории: ${rel}`);
  assert.ok(snapshotUsable(join(REPO_ROOT, 'tools'), PIN) === false);
});

test('пин: битые хеши, чужой kind и побег из репозитория — отказ', () => {
  const cases = [
    [{ transport: { ...PIN.transport, snapshot: { ...PIN.transport.snapshot, sha256: 'deadbeef' } } }, 'sha256 снимка'],
    [{ transport: { ...PIN.transport, artifact: { ...PIN.transport.artifact, sha256: 'X'.repeat(64) } } }, 'hex только в нижнем регистре'],
    [{ transport: { ...PIN.transport, commit: 'ad2e68' } }, 'короткий коммит'],
    [{ transport: { ...PIN.transport, kind: 'drive' } }, 'чужой kind'],
    [{ transport: { ...PIN.transport, kind: 'release', tag: '' } }, 'release без тега'],
    [{ transport: { ...PIN.transport, userscriptVersion: '1.16' } }, 'не plain-semver'],
    [{ transport: { ...PIN.transport, snapshot: { ...PIN.transport.snapshot, url: 'http://example.invalid/a' } } }, 'не https'],
    [{ inputs: { ...PIN.inputs, digest: '../../etc/passwd' } }, 'выход за репозиторий'],
    [{ inputs: { ...PIN.inputs, profile: '/abs/path' } }, 'абсолютный путь'],
    [{ output: { ...PIN.output, bundle: 'engine/relay.user.js' } }, 'артефакт вне build/'],
    [{ format: 'other' }, 'чужой format'],
    [{ version: 2 }, 'неподдержанная version'],
  ];
  for (const [patch, why] of cases) {
    const pin = { ...PIN, ...patch };
    const problems = validatePin(pin);
    assert.ok(problems.length > 0, `пин не отклонён: ${why}`);
  }
});

test('пин читается только валидным; негодный — код 1, а не тихий пропуск', () => {
  const dir = mkdtempSync(join(tmpdir(), 'relay-pin-'));
  try {
    const path = join(dir, 'pin.json');
    writeFileSync(path, JSON.stringify({ ...PIN, transport: { ...PIN.transport, commit: 'nope' } }));
    relayError(1, () => readPin(path));
    writeFileSync(path, '{ not json');
    relayError(1, () => readPin(path));
    relayError(3, () => readPin(join(dir, 'absent.json')));
    writeFileSync(path, readFileSync(DEFAULT_PIN, 'utf8'));
    assert.equal(readPin(path).transport.commit, PIN.transport.commit);
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test('профиль: форма транспорта, а не наш вкус', () => {
  const good = readProfile(join(PROJECT, PIN.inputs.profile));
  assert.equal(good.format, 'scriptcat-project-config');
  const cases = [
    [{ profile: { ...good.profile, id: 'not-a-uuid' } }, 'profile.id'],
    [{ profile: { ...good.profile, requests: '1'.repeat(36) } }, 'profile.requests'],
    [{ profile: { ...good.profile, repos: [] } }, 'пустой repos'],
    [{ profile: { ...good.profile, repos: [{ repo: 'a/b', main: false }] } }, 'без основного репо'],
    [{ format: 'other' }, 'чужой format'],
    [{ version: 7 }, 'неподдержанная version'],
  ];
  for (const [patch, why] of cases) {
    assert.ok(validateProfile({ ...good, ...patch }).length > 0, `профиль не отклонён: ${why}`);
  }
  assert.deepEqual(validateProfile(good), []);
});

test('краткая копия: номер, пустота и предел знаков', () => {
  assert.equal(checkDigest(realDigest, realRegulation).version, regulationVersion(realDigest));
  assert.ok(promptDigest(realDigest).length <= DIGEST_MAX_CHARS);
  assert.ok(!promptDigest(realDigest).includes(META_MARKER), 'служебные заметки не должны попадать в промпт');
  relayError(2, () => checkDigest('# Регламент\n\nникакого номера\n', realRegulation));
  relayError(2, () => checkDigest('Версия 1.4 · x', 'Версия 1.5 · y'));
  relayError(2, () => checkDigest(`Версия 1.5 · ${'z'.repeat(DIGEST_MAX_CHARS)}`, `Версия 1.5 · z`));
  relayError(2, () => checkDigest(`<!-- build:meta -->\nтолько заметки`, `Версия 1.5 · z`));
});

test('вшитый регламент читается из бандла и отсутствует — отказ', () => {
  const text = 'Версия 1.5 · раз\n\n- правило: "важное"';
  const bundle = fakeBundle({ digest: text });
  assert.equal(embeddedDigest(bundle), text);
  relayError(2, () => embeddedDigest('// @version 1.0.0\nнет поля регламента'));
  const facts = readBundleFacts(bundle);
  assert.equal(facts.version, '1.16.9');
  assert.equal(facts.fingerprint, 'a'.repeat(16));
  assert.ok(facts.hasDigestField);
});

test('docsCoincide: сравнение входов, а не надежда', () => {
  const ours = { digestSha256: sha256Text('наш digest'), regulationSha256: sha256Text('наш регламент') };
  assert.ok(docsCoincide({ ours, theirs: { ...ours } }), 'одинаковые входы должны узнаваться как одинаковые');
  assert.ok(!docsCoincide({ ours, theirs: { ...ours, regulationSha256: sha256Text('иной') } }),
    'расхождение полной копии — это уже другой набор входов');
  assert.ok(!docsCoincide({ ours, theirs: { digestSha256: '', regulationSha256: '' } }),
    'пустые тексты в снимке не считаются совпадением');
});

test('verifyBundle: верная подстановка, проглоченная подстановка и чужой регламент', () => {
  const embedded = promptDigest(realDigest);
  const snapshot = { digest: realDigest, regulation: realRegulation };
  const base = {
    pin: PIN,
    digestMd: realDigest,
    regulationMd: realRegulation,
    snapshotDocsText: snapshot,
    nodeReport: { allowed: true, checks: [{ id: 'x', code: 0 }], failedCheck: null, gates: {} },
  };
  // Входы совпали с текстами снимка → сверка с запиненным артефактом обязательна, и
  // подставленный вручную бандл этой сверки не проходит: тихого «ну и ладно» нет.
  const coinciding = verifyBundle({ ...base, bundleText: fakeBundle({ digest: embedded }) });
  assert.ok(coinciding.problems.some((problem) => problem.includes('не равен запиненному артефакту')),
    'при совпадающих docs сверка с запиненным артефактом обязательна');
  assert.equal(coinciding.coincide, true);
  // Регламент в бандле остался чужой (плейсхолдер не подставлен) — это отказ.
  const notSubstituted = verifyBundle({ ...base, bundleText: fakeBundle({ digest: '@@REGULATION_DIGEST@@' }) });
  assert.ok(notSubstituted.problems.some((problem) => problem.includes('подстановка не сработала')));
  // Неверный номер во вшитом тексте — отдельная причина, а не та же строка.
  const wrongVersion = verifyBundle({ ...base, bundleText: fakeBundle({ digest: 'Версия 0.9 · old' }) });
  assert.ok(wrongVersion.problems.some((problem) => problem.includes('номер во вшитом регламенте')));
  // Другой @version = собран не тот выпуск.
  const wrongRelease = verifyBundle({ ...base, bundleText: fakeBundle({ version: '9.9.9', digest: embedded }) });
  assert.ok(wrongRelease.problems.some((problem) => problem.includes('собран не тот выпуск')));
  // Отпечаток обязателен: без него нельзя сказать, какая сборка установлена.
  const noFingerprint = verifyBundle({ ...base, bundleText: `${fakeBundle({ digest: embedded })}`.replace(/buildFingerprint: "[^"]*",/, '') });
  assert.ok(noFingerprint.problems.some((problem) => problem.includes('отпечатка сборки')));
  // Допуск узла транспорта не подменяется нашим «ок».
  const nodeRefused = verifyBundle({
    ...base,
    bundleText: fakeBundle({ digest: embedded }),
    nodeReport: { allowed: false, checks: [{ id: 'test:scriptcat', code: 1 }], failedCheck: 'test:scriptcat', gates: {} },
  });
  assert.ok(nodeRefused.problems.some((problem) => problem.includes('не допустил сборку')));
});

test('отчёт узла транспорта: пустой список проверок и чужие пути — отказ', () => {
  const good = {
    allowed: true,
    checks: [{ id: 'scriptcat:check', code: 0, weight: 'w' }, { id: 'test:scriptcat', code: 0 }],
    gates: { paths: { ok: true, foreign: [] }, pathsSkipped: true, version: { ok: true, why: 'не менялось' } },
    bundle: { sha256: 'a'.repeat(64) },
  };
  const report = parseNodeReport(JSON.stringify(good));
  assert.equal(report.checks.length, 2);
  assert.equal(report.failedCheck, null);
  // Дат и путей каталога в решении быть не должно: они делают манифест недетерминированным.
  assert.ok(!JSON.stringify(report).includes(good.at), 'в решении не должно быть метки времени');
  assert.ok(!JSON.stringify(report).includes('tmp'), 'в решении не должно быть пути временного каталога');
  relayError(2, () => parseNodeReport(JSON.stringify({ ...good, checks: [] })));
  // `allowed:false` — решение узла, которое читает и разъясняет вызывающий код; сам
  // разбор отчёта на нём не падает (иначе причина «какая проверка не прошла» терялась бы).
  const refused = parseNodeReport(JSON.stringify({ ...good, allowed: false, checks: [{ id: 'test:scriptcat', code: 2, counts: { fail: 1 } }] }));
  assert.equal(refused.allowed, false);
  assert.equal(refused.failedCheck, 'test:scriptcat');
  assert.deepEqual(refused.checks[0].counts, { fail: 1 });
  relayError(2, () => parseNodeReport('не JSON'));
  relayError(2, () => parseNodeReport(''));
  relayError(2, () => parseNodeReport(JSON.stringify({ ...good, gates: { ...good.gates, paths: { ok: false, foreign: ['engine/x'] } } })));
  relayError(2, () => parseNodeReport(JSON.stringify({ ...good, gates: { ...good.gates, version: { ok: false, why: 'номер не поднят' } } })));
});

test('снимок: чужие байты не разворачиваются и не попадают в кэш', async () => {
  const dir = mkdtempSync(join(tmpdir(), 'relay-cache-'));
  try {
    const wrong = { ...PIN, transport: { ...PIN.transport, snapshot: { ...PIN.transport.snapshot, sha256: 'f'.repeat(64) } } };
    const body = new Blob(['not an archive']);
    const fetchImpl = async () => ({ ok: true, arrayBuffer: async () => body.arrayBuffer() });
    await assert.rejects(obtainSnapshot({ pin: wrong, cacheDir: dir, fetchImpl }), (error) => {
      assert.equal(error.relayCode, 2);
      assert.match(error.message, /sha256 снимка/);
      return true;
    });
    // HTTP-ошибка — это «источники не получены» (3), а не «сборка не прошла» (2); и
    // при подставном загрузчике второй (настоящий) не подключается: тест не ходит в сеть.
    let realFetchCalls = 0;
    await assert.rejects(obtainSnapshot({
      pin: PIN, cacheDir: dir,
      fetchImpl: async () => { realFetchCalls += 1; return { ok: false, status: 404 }; },
    }), (error) => {
      assert.equal(error.relayCode, 3);
      assert.match(error.message, /HTTP 404/);
      return true;
    });
    assert.equal(realFetchCalls, 1, 'повторной попытки другим загрузчиком быть не должно');
    await assert.rejects(obtainSnapshot({ pin: PIN, cacheDir: dir, offline: true }), (error) => {
      assert.equal(error.relayCode, 3);
      assert.match(error.message, /--offline/);
      return true;
    });
    // В кэше после отказов ничего нет: половину развернутого дерева использовать нельзя.
    assert.deepEqual(readdirSync(dir).filter((name) => !name.startsWith('.')), []);
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test('штамп кэша: переиспользуется только ровно тот снимок, что запинен', () => {
  const dir = mkdtempSync(join(tmpdir(), 'relay-stamp-'));
  const stamp = (fields) => JSON.stringify({ format: 'wotr-relay-cache/1', commit: PIN.transport.commit, sha256: PIN.transport.snapshot.sha256, ...fields });
  const filled = (name, contents) => {
    const snapshotDir = join(dir, name);
    mkdirSync(join(snapshotDir, SC_DIR), { recursive: true });
    mkdirSync(join(snapshotDir, 'tools'), { recursive: true });
    for (const [path, text] of Object.entries(contents)) writeFileSync(join(snapshotDir, path), text);
    return snapshotDir;
  };
  try {
    const good = filled(`relay-wotr-${PIN.transport.commit.slice(0, 12)}`, {
      [`${SC_DIR}/build.mjs`]: '// build',
      [`${BUILD_NODE}`]: '// node',
      '.relay-snapshot.json': stamp(),
    });
    assert.equal(snapshotUsable(good, PIN), true, 'полный снимок с верным штампом обязан переиспользоваться');
    // Тот же каталог, другой архив: штамп не совпал — скачиваем заново, а не верим содержимому.
    writeFileSync(join(good, '.relay-snapshot.json'), stamp({ sha256: 'e'.repeat(64) }));
    assert.equal(snapshotUsable(good, PIN), false);
    writeFileSync(join(good, '.relay-snapshot.json'), stamp({ commit: '0'.repeat(40) }));
    assert.equal(snapshotUsable(good, PIN), false, 'другой коммит под тем же именем — не этот снимок');
    writeFileSync(join(good, '.relay-snapshot.json'), '{ broken');
    assert.equal(snapshotUsable(good, PIN), false);
    rmSync(join(good, '.relay-snapshot.json'));
    writeFileSync(join(good, `${SC_DIR}/build.mjs`), '// build');
    assert.equal(snapshotUsable(good, PIN), false, 'без штампа каталог не снимок');
    // Штамп верный, дерева нет: пустой каталог не проходит.
    assert.equal(snapshotUsable(join(dir, 'empty-snapshot'), PIN), false);
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test('verifyArtifact: собранный бандл сверяется с манифестом и с docs', () => {
  const dir = mkdtempSync(join(tmpdir(), 'relay-verify-'));
  try {
    const bundlePath = join(dir, 'relay.user.js');
    const digestText = promptDigest(realDigest);
    writeFileSync(bundlePath, fakeBundle({ digest: digestText }), 'utf8');
    const first = verifyArtifact({ bundlePath, root: PROJECT, pinPath: DEFAULT_PIN });
    assert.equal(readFileSync(join(dir, 'relay.user.js'), 'utf8'), fakeBundle({ digest: promptDigest(realDigest) }));
    assert.equal(first.ok, true, first.problems.join('; '));
    // Манифест о другом бандле — отказ: правка артефакта после сборки не должна проходить.
    writeFileSync(join(dir, 'build-manifest.json'), JSON.stringify({ schema: 'wotr-relay-manifest/1', artifact: { bundleSha256: 'b'.repeat(64) }, transport: { commit: PIN.transport.commit } }));
    const tampered = verifyArtifact({ bundlePath, root: PROJECT, pinPath: DEFAULT_PIN });
    assert.ok(tampered.problems.some((problem) => problem.includes('изменён после сборки')));
    // Манифест с другим коммитом — тоже отказ, даже если байты бандла верные.
    writeFileSync(join(dir, 'build-manifest.json'), JSON.stringify({ schema: 'wotr-relay-manifest/1', artifact: { bundleSha256: sha256Text(readFileSync(bundlePath, 'utf8')) }, transport: { commit: '0'.repeat(40) } }));
    const otherCommit = verifyArtifact({ bundlePath, root: PROJECT, pinPath: DEFAULT_PIN });
    assert.ok(otherCommit.problems.some((problem) => problem.includes('пин —')));
    // Нет файла — «источники не получены», а не тихое «нечего проверять».
    relayError(3, () => verifyArtifact({ bundlePath: join(dir, 'absent.js'), pinPath: DEFAULT_PIN }));
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test('проверка артефакта ловит устаревший регламент в бандле', () => {
  const dir = mkdtempSync(join(tmpdir(), 'relay-stale-'));
  try {
    const bundlePath = join(dir, 'relay.user.js');
    writeFileSync(bundlePath, fakeBundle({ digest: 'Версия 1.4 · старый текст, уже не тот' }), 'utf8');
    writeFileSync(join(dir, 'build-manifest.json'), '{ broken');
    const result = verifyArtifact({ bundlePath, root: PROJECT, pinPath: DEFAULT_PIN });
    assert.ok(result.problems.some((problem) => problem.includes('вшитый регламент не равен')));
    // Манифест, который не читается, не превращает проверку в «нечего проверять».
    assert.ok(result.problems.some((problem) => problem.includes('build-manifest.json не читается')));
    assert.ok(result.problems.length >= 2);
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test('deploy.json описывает ровно те пути, которые объявлены в пине', () => {
  const deploy = JSON.parse(readFileSync(join(RELAY_DIR, 'deploy.json'), 'utf8'));
  assert.equal(deploy.format, 'wotr-relay-deploy');
  assert.equal(deploy.artifact, PIN.output.bundle);
  assert.equal(deploy.coverage, PIN.output.coverage);
  assert.equal(deploy.manifest, PIN.output.manifest);
  assert.equal(deploy.profile, PIN.inputs.profile);
  assert.equal(deploy.target.kind, 'google-apps-script');
  assert.ok(deploy.target.projectName.length > 0);
  // Секретов и идентификаторов окружения в дереве быть не должно — файл читает CI.
  const text = readFileSync(join(RELAY_DIR, 'deploy.json'), 'utf8');
  assert.ok(!/scriptId|token|apiKey|\/home\/|C:\\/.test(text), 'deploy.json не должен naming окружение или секреты');
});

function fakeTransportSnapshot(dir, { artifact = 'ARTEFACT\n', digest = 'Версия 9.9 · snapshot' } = {}) {
  const base = join(dir, 'tree', 'transport-fake');
  mkdirSync(join(base, SC_DIR, 'contracts'), { recursive: true });
  mkdirSync(join(base, 'tools'), { recursive: true });
  mkdirSync(join(base, 'docs'), { recursive: true });
  writeFileSync(join(base, SC_DIR, 'build.mjs'), '// build');
  writeFileSync(join(base, 'tools', 'relay-build.mjs'), '// node');
  writeFileSync(join(base, SC_DIR, 'relay.user.js'), artifact);
  writeFileSync(join(base, 'docs', 'regulation-digest.md'), `${digest}\n`);
  writeFileSync(join(base, 'docs', 'REGULATION.md'), `${digest}\n`);
  const archive = join(dir, 'snapshot.tar.gz');
  const tar = spawnSync('tar', ['-czf', archive, '-C', join(dir, 'tree'), 'transport-fake'], { encoding: 'utf8' });
  assert.equal(tar.status, 0, tar.stderr);
  const pin = {
    ...PIN,
    transport: {
      ...PIN.transport,
      commit: '1'.repeat(40),
      snapshot: { url: 'https://example.invalid/snapshot.tar.gz', sha256: sha256File(archive) },
      artifact: { path: `${SC_DIR}/relay.user.js`, sha256: sha256Text(artifact), regulationVersion: '9.9' },
    },
  };
  const bytes = readFileSync(archive);
  const fetchImpl = async () => {
    fetchImpl.calls += 1;
    return { ok: true, arrayBuffer: async () => bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength) };
  };
  fetchImpl.calls = 0;
  return { pin, fetchImpl };
}

test('кэш — эталон: артефакт сверяется, испорченный кэш перекачивается ровно один раз', async () => {
  const dir = mkdtempSync(join(tmpdir(), 'relay-pristine-'));
  try {
    const { pin, fetchImpl } = fakeTransportSnapshot(dir);
    const cacheDir = join(dir, 'cache');
    const first = await obtainPristineSnapshot({ pin, cacheDir, fetchImpl });
    assert.equal(first.fromCache, false);
    assert.equal(readFileSync(join(first.dir, SC_DIR, 'relay.user.js'), 'utf8'), 'ARTEFACT\n');
    assert.equal(fetchImpl.calls, 1);
    // Кэш переиспользуется без сети.
    const second = await obtainPristineSnapshot({ pin, cacheDir, fetchImpl });
    assert.equal(second.fromCache, true);
    assert.equal(fetchImpl.calls, 1, 'тёплый кэш не скачивают снова');
    // Кто-то написал в кэш: сборка из него не допускается, но и не навсегда — один re-fetch.
    writeFileSync(join(second.dir, SC_DIR, 'relay.user.js'), 'DIRTY\n');
    const third = await obtainPristineSnapshot({ pin, cacheDir, fetchImpl });
    assert.equal(fetchImpl.calls, 2, 'испорченный кэш лечится перекачиванием');
    assert.equal(third.refetched, true);
    assert.equal(readFileSync(join(third.dir, SC_DIR, 'relay.user.js'), 'utf8'), 'ARTEFACT\n');
    relayError(2, () => verifyPinnedArtifact({ dir: join(dir, 'nowhere'), pin }));
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test('расхождение с пином после перекачивания — отказ, а не «соберём как есть»', async () => {
  const dir = mkdtempSync(join(tmpdir(), 'relay-drift-'));
  try {
    const good = fakeTransportSnapshot(dir);
    const other = fakeTransportSnapshot(join(dir, 'other'), { artifact: 'OTHER\n' });
    // Пин ждёт артефакт из `good`, сервер отдаёт архив с `other`: скачивание не помогает.
    const bytes = readFileSync(join(dir, 'other', 'snapshot.tar.gz'));
    const fetchImpl = async () => ({ ok: true, arrayBuffer: async () => bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength) });
    const pin = { ...good.pin, transport: { ...good.pin.transport, snapshot: { ...other.pin.transport.snapshot } } };
    const cacheDir = join(dir, 'cache');
    // Штамп по sha256 архива совпадёт (архив тот же), поэтому сверка идёт по артефакту.
    await assert.rejects(obtainPristineSnapshot({ pin, cacheDir, fetchImpl }), (error) => {
      assert.equal(error.relayCode, 2);
      assert.match(error.message, /перекачив/);
      return true;
    });
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test('рабочий каталог изолирован: подстановка не трогает кэш', async () => {
  const dir = mkdtempSync(join(tmpdir(), 'relay-work-'));
  try {
    const snapshot = join(dir, 'snapshot');
    mkdirSync(join(snapshot, SC_DIR), { recursive: true });
    writeFileSync(join(snapshot, SC_DIR, 'relay.user.js'), 'ARTEFACT\n');
    const work = copyToWorkDir(snapshot);
    try {
      writeFileSync(join(work.dir, SC_DIR, 'relay.user.js'), 'SUBSTITUTED\n');
      assert.equal(readFileSync(join(snapshot, SC_DIR, 'relay.user.js'), 'utf8'), 'ARTEFACT\n', 'кэш должен остаться эталоном');
    } finally {
      work.cleanup();
    }
    assert.ok(!existsSync(work.scratch), 'рабочий каталог убирается за собой');
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test('узел транспорта обязан быть в снимке: без него доверять нечему', () => {
  assert.equal(BUILD_NODE, 'tools/relay-build.mjs');
  assert.ok(PIN.transport.snapshot.url.includes(PIN.transport.commit));
  // Пин ссылается на URL того же коммита, что и сверяемый артефакт: «скачали другое» невозможно.
  assert.ok(PIN.transport.artifact.path.startsWith(`${SC_DIR}/`));
});
