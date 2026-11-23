#!/usr/bin/env node
// Сборка бандла ScriptCat силами проекта (WOTR-D-033).
//
// Зачем отдельный узел у нас. Транспорт — одна библиотека на много проектов, и она не
// должна хранить ни байта проектных документов: ровно поэтому правка `docs/REGULATION.md`
// больше не порождает карточку в `Creative36711/relay-wotr`. Но бандл по-прежнему обязан
// нести краткий регламент внутри — агент получает правила вместе с заданием. Значит
// подстановку делает проект: здесь берётся запиненный выпуск транспорта, ЕГО же сборке
// передаётся НАШ `docs/regulation-digest.md`, и результат проверяется по хешам.
//
// С D-003 (их main c38ac37, userscript 1.17.0) вход — параметр `--digest <путь>`: их
// `build.mjs` принимает только `[--check] [--digest <путь>]`, каталога `docs/` в дереве
// транспорта больше нет, и каждая сборка пишет `contracts/build-manifest.json` с шапкой
// и sha256 переданного входа (либо `digest: null` для нейтральной сборки). Поэтому узел
// делает две сборки в двух копиях снимка: нейтральную — чтобы доказать, что запиненный
// артефакт воспроизводится байт-в-байт, и с `--digest` — чтобы получить бандл для агентов.
// Одной копии не хватает: их узел вызывает `build.mjs --check` без входа (список
// проверок фиксирован в их коде), и на дереве с вшитым регламентом он обязан
// отказаться. Профиль проекта в сборку не передаётся: он — импорт-файл рантайма
// ScriptCat, а не вход сборки; здесь он сверяется только с тем, что это наш проект.
//
// Три правила, ради которых этот файл написан, а не заменён `curl`:
//
//  1. **Никакого доверия на слово.** Скачанный снимок сверяется с `pin.json` по sha256
//     архива; нейтральная сборка — по sha256 запиненного артефакта; вшитый регламент — по
//     sha256 нашего входа в их `build-manifest.json`. Не совпало — сборка не допускается,
//     а не «продолжаем с предупреждением». Тот же подход, что у узла транспорта
//     (`tools/relay-build.mjs`): непроверенный бандл к установке не проходит.
//  2. **Мы не переписываем транспорт.** Сборку делает ЕГО `build.mjs`, а фиксированные
//     проверки — ЕГО узел `relay-build.mjs`: список проверок не дублируется у нас и не
//     зависит от того, что напишут в манифесте. Единственный разрешённый способ подать
//     регламент — передать путь его же сборке. Это временный каталог: репозиторий
//     транспорта не меняется ни на байт, и в его снимок мы не пишем ничего.
//  3. **Детерминизм важнее удобства.** Никаких меток времени в манифесте: два запуска на
//     одном пине обязаны дать байт-в-байт одинаковые `relay.user.js`,
//     `prompt-coverage.json` и `build-manifest.json`. Иначе сверять установленный бандл
//     нечем, а «сборка изменилась» и «сборка пахла по-новому» неразличимы.
//
// Запуск (из корня репозитория):
//   node tools/relay/build.mjs                 собрать и проверить
//   node tools/relay/build.mjs --check         то же дважды, сверить байты артефактов
//   node tools/relay/build.mjs --verify [файл] проверить собранный бандл, ничего не скачивая
//   node tools/relay/build.mjs --json          машинный вывод в stdout (человеческий — в stderr)
//   node tools/relay/build.mjs --offline       только из кэша; кэша нет — код 3
//
// Коды возврата — как у узла транспорта: 0 — допущено, 1 — ошибка запуска (неверные
// аргументы или негодный пин), 2 — проверка не пройдена (сборка не допущена),
// 3 — источники или входы не получены.

import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import {
  cpSync, existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, rmSync, writeFileSync,
} from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, isAbsolute, join, relative, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const REPO = resolve(here, '..', '..');

/** Корень репозитория: тестам нужно отличать кэш «рядом» от кэша «внутри дерева». */
export const REPO_ROOT = REPO;

export const SC_DIR = 'tools/script-cat';
export const BUILD_NODE = 'tools/relay-build.mjs';
export const DEFAULT_PIN = join(here, 'pin.json');
// Кэш снимка обязан лежать ВНЕ рабочего дерева: узел транспорта определяет, чьё дерево
// он проверяет, по `git status` в каталоге запуска. Кэш внутри репозитория заставил бы
// его проверять НАШЕ дерево и отказывать («изменения вне разрешённых путей»).
export const DEFAULT_CACHE = join(tmpdir(), 'wotr-relay-cache');
export const META_MARKER = '<!-- build:meta -->';
export const HEX64 = /^[0-9a-f]{64}$/;
export const SEMVER = /^(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)$/;
export const REG_VERSION = /^(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)$/;
export const COMMIT = /^[0-9a-f]{40}$/;
export const OWNER_REPO = /^[\w.-]+\/[\w.-]+$/;
export const TAG = /^[\w.\/-]+$/;
export const UUID = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/;
export const DIGEST_MAX_CHARS = 9000;
export const PROFILE_ID_FIELDS = ['roles', 'missions', 'requests', 'questions'];
export const MANIFEST_ARTIFACTS = ['relay.user.js', 'prompt-coverage.json', 'build-manifest.json'];

export const sha256Text = (text) => createHash('sha256').update(String(text), 'utf8').digest('hex');
export const sha256Buffer = (buffer) => createHash('sha256').update(buffer).digest('hex');
export const sha256File = (path) => sha256Buffer(readFileSync(path));

const run = (command, args, options = {}) => spawnSync(command, args, {
  encoding: 'utf8',
  // Никакой оболочки: argv передаётся как есть, подстановок нет.
  shell: false,
  maxBuffer: 64 * 1024 * 1024,
  ...options,
});

const fail = (code, message) => {
  const error = new Error(message);
  error.relayCode = code;
  throw error;
};

const lastLines = (text, count = 8) => String(text || '').trim().split('\n').slice(-count).join('\n');

/** Путь для сообщения: изнутри репозитория — относительный, снаружи — как есть. */
export const display = (path, root = REPO) => {
  const rel = relative(root, path);
  return rel && !rel.startsWith('..') && !isAbsolute(rel) ? rel : path;
};

/**
 * Текст для промпта: ровно то, что отдаёт их `regulation-oracle.promptDigest` —
 * правила сверяются по одному образцу, иначе «вшитый регламент» и «наш регламент»
 * разошлись бы на служебных строках. Служебные HTML-комментарии (в том числе строка
 * `<!-- build:meta -->`) вырезаются, три и больше пустых строки схлопываются.
 */
export function promptDigest(markdown) {
  return String(markdown || '')
    .split(/\r?\n/)
    .filter((line) => !line.includes(META_MARKER) && !line.trim().startsWith('<!--'))
    .join('\n')
    .replace(/\n{3,}/g, '\n\n')
    .trim();
}

/** Номер регламента из строки «Версия N.N»; без неё файл не считается регламентом. */
export function regulationVersion(text) {
  const match = String(text || '').match(/Версия\s+(\d+\.\d+)/);
  if (!match) fail(2, 'в тексте регламента нет строки «Версия N.N» — сверить номер нечем');
  return match[1];
}

/** Краткая копия обязана укладываться в лимит и не отставать по номеру от полной. */
export function checkDigest(digestMd, regulationMd) {
  const text = promptDigest(digestMd);
  if (text.length === 0) fail(2, 'краткая версия регламента пуста: агент останется без правил');
  if (text.length > DIGEST_MAX_CHARS) {
    fail(2, `краткая версия регламента ${text.length} знаков при пределе ${DIGEST_MAX_CHARS}: вход хода обязан оставаться коротким`);
  }
  const short = regulationVersion(digestMd);
  const full = regulationVersion(regulationMd);
  if (short !== full) {
    fail(2, `краткая версия регламента (${short}) отстала от полной (${full}): обновите docs/regulation-digest.md`);
  }
  return { text, version: short };
}

/**
 * Схема пина. Проверяется всё, на что потом опираются решения: два sha256, коммит,
 * номер userscript, контракт и пути входов. Пустое или кривое поле — отказ, а не
 * «пропустить проверку».
 */
export function validatePin(pin) {
  const problems = [];
  const need = (ok, what) => { if (!ok) problems.push(what); };
  need(pin && typeof pin === 'object' && !Array.isArray(pin), 'pin.json: ожидался объект');
  if (problems.length) return problems;
  need(pin.format === 'wotr-relay-pin', 'pin.json: format должен быть "wotr-relay-pin"');
  need(pin.version === 1, 'pin.json: version поддерживается только 1');
  const t = pin.transport || {};
  need(OWNER_REPO.test(String(t.repo || '')), 'pin.json: transport.repo — "owner/name"');
  need(t.kind === 'commit' || t.kind === 'release', 'pin.json: transport.kind — "commit" или "release"');
  need(COMMIT.test(String(t.commit || '')), 'pin.json: transport.commit — 40 hex');
  if (t.kind === 'release') need(TAG.test(String(t.tag || '')), 'pin.json: transport.tag обязателен при kind=release');
  need(SEMVER.test(String(t.userscriptVersion || '')), 'pin.json: transport.userscriptVersion — MAJOR.MINOR.PATCH');
  need(REG_VERSION.test(String(t.contractVersion || '')) || SEMVER.test(String(t.contractVersion || '')),
    'pin.json: transport.contractVersion — N.N или MAJOR.MINOR.PATCH');
  need(HEX64.test(String(t.snapshot?.sha256 || '')), 'pin.json: transport.snapshot.sha256 — 64 hex');
  need(/^https:\/\/[^\s]+$/.test(String(t.snapshot?.url || '')), 'pin.json: transport.snapshot.url — https без пробелов');
  need(String(t.artifact?.path || '').startsWith(`${SC_DIR}/`), `pin.json: transport.artifact.path должен лежать в ${SC_DIR}/`);
  need(HEX64.test(String(t.artifact?.sha256 || '')), 'pin.json: transport.artifact.sha256 — 64 hex');
  // С D-003 артефакт транспорта обязан быть проект-нейтральным: поле говорит, вшит ли в
  // него чей-то регламент. `true` означало бы, что запиненный выпуск несёт проектный текст,
  // и такой пин не проходит: нейтральность проверяется сборкой, а не обещанием в JSON.
  need(t.artifact?.embedsRegulation === false,
    'pin.json: transport.artifact.embedsRegulation обязан быть false — с D-003 выпуск транспорта не несёт регламента');
  const i = pin.inputs || {};
  for (const key of ['digest', 'regulation', 'profile']) {
    const value = i[key];
    need(typeof value === 'string' && value.length > 0 && !value.startsWith('/') && !value.split('/').includes('..'),
      `pin.json: inputs.${key} — относительный путь внутри репозитория без ".."`);
  }
  const o = pin.output || {};
  for (const key of ['dir', 'bundle', 'coverage', 'manifest']) {
    const value = o[key];
    need(typeof value === 'string' && value.startsWith('build/'), `pin.json: output.${key} должен лежать в build/`);
  }
  need(OWNER_REPO.test(String(pin.project?.repo || '')), 'pin.json: project.repo — "owner/name"');
  return problems;
}

export function readPin(path) {
  if (!existsSync(path)) fail(3, `пин не найден: ${display(path)}`);
  let pin;
  try {
    pin = JSON.parse(readFileSync(path, 'utf8'));
  } catch (error) {
    fail(1, `пин не разобран как JSON: ${error.message}`);
  }
  const problems = validatePin(pin);
  if (problems.length) fail(1, `пин не годится (fail closed):\n  - ${problems.join('\n  - ')}`);
  return pin;
}

/**
 * Схема профиля проекта. Формат задаёт транспорт (`scriptcat-project-config`), поэтому
 * проверяется именно он, а не вкус: набор полей, uuid, "owner/name", основной repo.
 */
export function validateProfile(profile) {
  const problems = [];
  const need = (ok, what) => { if (!ok) problems.push(what); };
  need(profile && typeof profile === 'object' && !Array.isArray(profile), 'профиль: ожидался объект');
  if (problems.length) return problems;
  need(profile.format === 'scriptcat-project-config', 'профиль: format должен быть "scriptcat-project-config"');
  need(profile.version === 1, 'профиль: version поддерживается только 1');
  const p = profile.profile || {};
  need(UUID.test(String(p.id || '')), 'профиль: profile.id — uuid');
  need(typeof p.name === 'string' && p.name.length > 0, 'профиль: profile.name непустой');
  need(OWNER_REPO.test(String(p.repo || '')), 'профиль: profile.repo — "owner/name"');
  for (const field of PROFILE_ID_FIELDS) need(UUID.test(String(p[field] || '')), `профиль: profile.${field} — uuid`);
  need(Array.isArray(p.repos) && p.repos.length > 0, 'профиль: profile.repos — непустой массив');
  if (Array.isArray(p.repos)) {
    need(p.repos.filter((entry) => entry?.main === true).length === 1, 'профиль: ровно один основной repo помечен main:true');
    for (const entry of p.repos) need(OWNER_REPO.test(String(entry?.repo || '')), 'профиль: profile.repos[].repo — "owner/name"');
  }
  return problems;
}

export function readProfile(path) {
  if (!existsSync(path)) fail(3, `профиль проекта не найден: ${display(path)}`);
  let profile;
  try {
    profile = JSON.parse(readFileSync(path, 'utf8'));
  } catch (error) {
    fail(1, `профиль не разобран как JSON: ${error.message}`);
  }
  const problems = validateProfile(profile);
  if (problems.length) fail(1, `профиль не годится (fail closed):\n  - ${problems.join('\n  - ')}`);
  return profile;
}

/** Из отчёта узла транспорта берём только решения по проверкам: дат и путей там нет. */
export function pickNodeReport(report) {
  const checks = (Array.isArray(report?.checks) ? report.checks : []).map((check) => ({
    id: String(check?.id ?? ''),
    code: Number.isInteger(check?.code) ? check.code : -1,
    counts: check?.counts || {},
  }));
  return {
    allowed: Boolean(report?.allowed),
    checks,
    failedCheck: checks.find((check) => check.code !== 0)?.id ?? null,
    bundle: report?.bundle || null,
    gates: {
      paths: report?.gates?.paths ? { ok: Boolean(report.gates.paths.ok), foreign: report.gates.paths.foreign || [] } : null,
      pathsSkipped: Boolean(report?.gates?.pathsSkipped),
      version: report?.gates?.version ? { ok: Boolean(report.gates.version.ok), why: String(report.gates.version.why || '') } : null,
    },
  };
}

export function parseNodeReport(stdout) {
  let parsed;
  try {
    parsed = JSON.parse(String(stdout || ''));
  } catch (error) {
    fail(2, `узел транспорта не вернул разбираемый отчёт: ${error.message}`);
  }
  const report = pickNodeReport(parsed);
  if (report.checks.length === 0) fail(2, 'узел транспорта не сообщил ни одной проверки: доверять допуску нечем');
  const versionGate = report.gates.version;
  if (versionGate && !versionGate.ok) fail(2, `гейт версии транспорта: ${versionGate.why}`);
  if (report.gates.paths && !report.gates.paths.ok) {
    fail(2, `узел транспорта увидел чужие пути: ${report.gates.paths.foreign.join(', ')}`);
  }
  return report;
}

/** Номер userscript и отпечаток: метаданные и рантайм читаются из разных строк. */
export function readBundleFacts(bundleText) {
  const text = String(bundleText || '');
  return {
    version: (text.match(/^\/\/ @version\s+(\S+)\s*$/m) || [])[1] || '',
    runtime: (text.match(/SCRIPT_VERSION\s*=[^;]*?\|\|\s*"([^"]+)"/) || [])[1] || '',
    fingerprint: (text.match(/buildFingerprint:\s*"([0-9a-f]{8,64})"/) || [])[1] || '',
    hasDigestField: /regulationDigest:/.test(text),
    bytes: Buffer.byteLength(text, 'utf8'),
  };
}

/** Что реально достанется агенту: текст вшитого краткого регламента. */
export function embeddedDigest(bundleText) {
  const match = String(bundleText || '').match(/^\s*regulationDigest:\s*(".*?")\s*,?\s*$/m);
  if (!match) fail(2, 'в бандле нет поля regulationDigest: сборка не вшила краткий регламент');
  try {
    return JSON.parse(match[1]);
  } catch (error) {
    fail(2, `вшитый регламент не читается как строка бандла: ${error.message}`);
  }
}

/**
 * Их `contracts/build-manifest.json` — единственное, чем транспорт отвечает, какой
 * регламент он принял. Файл пишется каждой сборкой, поэтому «манифеста нет» означает
 * «сборка не дошла до записи», а не «сверять нечем»: это отказ (2), не пропуск.
 */
export function readTransportManifest({ snapshotRoot }) {
  const path = join(snapshotRoot, SC_DIR, 'contracts', 'build-manifest.json');
  if (!existsSync(path)) {
    fail(2, `сборка транспорта не оставила ${SC_DIR}/contracts/build-manifest.json: принят ли наш вход — неизвестно`);
  }
  const text = readFileSync(path, 'utf8');
  let manifest;
  try {
    manifest = JSON.parse(text);
  } catch (error) {
    fail(2, `${SC_DIR}/contracts/build-manifest.json не разобран как JSON: ${error.message}`);
  }
  if (!manifest || typeof manifest !== 'object' || !('digest' in manifest)) {
    fail(2, 'в build-manifest транспорта нет поля digest: сверять вшитый регламент нечем');
  }
  return { manifest, text };
}

/**
 * Нейтральная сборка (без `--digest`) обязана дать ровно запиненный артефакт: это наша
 * замена проверке «наше зеркало совпадает с их docs». Если байты разошлись — пин описывает
 * не ту сборку, и никакой последующий бандл с вшитым регламентом этому не аргумент.
 */
export function verifyNeutralBundle({ pin, bundleText, manifest }) {
  const problems = [];
  const facts = readBundleFacts(bundleText);
  const bundleSha256 = sha256Text(bundleText);
  if (bundleSha256 !== pin.transport.artifact.sha256) {
    problems.push(`нейтральная сборка не равна запиненному артефакту (${bundleSha256.slice(0, 12)}… ≠ `
      + `${pin.transport.artifact.sha256.slice(0, 12)}…): выпуск транспорта не воспроизводится его же сборкой на запиненном коммите`);
  }
  if (facts.version !== pin.transport.userscriptVersion) {
    problems.push(`@version в нейтральной сборке ${facts.version || '—'}, в пине ${pin.transport.userscriptVersion}`);
  }
  if (manifest.digest !== null) {
    problems.push('build-manifest нейтральной сборки сообщает digest, хотя вход не передавался: '
      + 'их сборка читает регламент из дерева, а не из параметра — D-003 нарушен');
  }
  if (facts.hasDigestField) {
    const embedded = embeddedDigest(bundleText);
    if (embedded !== '') {
      problems.push(`в проект-нейтральном артефакте транспорта лежит регламент (${embedded.length} зн.) — `
        + 'пин ждёт пустой блок (transport.artifact.embedsRegulation: false)');
    }
  }
  return { problems, bundleSha256, facts };
}

/** Проверки собранного бандла: контракт, номер регламента, след подстановки, manifest транспорта. */
export function verifyBundle({ pin, bundleText, digestMd, regulationMd, transportManifest, nodeReport }) {
  const facts = readBundleFacts(bundleText);
  const problems = [];
  if (facts.version !== pin.transport.userscriptVersion) {
    problems.push(`@version в бандле ${facts.version || '—'}, в пине ${pin.transport.userscriptVersion}: собран не тот выпуск транспорта`);
  }
  if (facts.runtime !== facts.version) {
    problems.push(`@version ${facts.version || '—'} и SCRIPT_VERSION ${facts.runtime || '—'} разошлись`);
  }
  if (!facts.fingerprint) problems.push('в бандле нет отпечатка сборки (buildFingerprint)');
  const version = checkDigest(digestMd, regulationMd).version;
  let embedded = '';
  try {
    embedded = embeddedDigest(bundleText);
  } catch (error) {
    problems.push(error.message);
  }
  const expected = promptDigest(digestMd);
  if (embedded !== expected) {
    problems.push(`вшитый регламент не равен ${pin.inputs.digest}: подстановка не сработала `
      + `(в бандле ${embedded.length} знаков, в файле ${expected.length})`);
  }
  const embeddedVersion = (embedded.match(/Версия\s+(\d+\.\d+)/) || [])[1] || '';
  if (embeddedVersion !== version) {
    problems.push(`номер во вшитом регламенте ${embeddedVersion || '—'}, в ${pin.inputs.regulation} — ${version}`);
  }
  if (nodeReport && !nodeReport.allowed) {
    problems.push(`узел транспорта не допустил сборку (проверка ${nodeReport.failedCheck || 'неизвестная'})`);
  }
  // Сверка с их build-manifest: транспорт обязан подтвердить и номер, и байты входа.
  const recorded = transportManifest && transportManifest.digest;
  const inputSha256 = sha256Text(digestMd);
  if (!recorded) {
    problems.push('build-manifest транспорта сообщает digest: null — наш вход не был передан их '
      + 'сборке, вшитый регламент подтвердить нечем');
  } else {
    if (recorded.version !== version) {
      problems.push(`build-manifest транспорта зафиксировал регламент ${recorded.version}, наш вход — ${version}`);
    }
    if (recorded.sha256 !== inputSha256) {
      problems.push(`sha256 входа в build-manifest транспорта ${recorded.sha256?.slice(0, 12)}… ≠ хешу `
        + `${pin.inputs.digest} ${inputSha256.slice(0, 12)}…: файл менялся после сборки`);
    }
  }
  const bundleSha256 = sha256Text(bundleText);
  if (bundleSha256 === pin.transport.artifact.sha256) {
    problems.push('бандл с вшитым регламентом совпал с запиненным нейтральным артефактом: '
      + 'значит подстановка не изменила байты, а не изменила их незаметно');
  }
  return {
    problems, facts, bundleSha256, digestVersion: version, digestSha256: inputSha256,
    transportManifest: recorded || null, embeddedDigest: embedded,
  };
}

const cacheName = (pin) => `${(pin.transport.repo.split('/')[1] || 'transport')}-${pin.transport.commit.slice(0, 12)}`;
const stampPath = (dir) => join(dir, '.relay-snapshot.json');

/** Скачан и развернут ли снимок этого коммита уже в кэше — по штампу с теми же хешами. */
export function snapshotUsable(dir, pin) {
  if (!existsSync(stampPath(dir))) return false;
  let stamp;
  try {
    stamp = JSON.parse(readFileSync(stampPath(dir), 'utf8'));
  } catch {
    return false;
  }
  // Штамп — защита от «тот же каталог, другие байты»: сверяем и коммит, и sha256 архива.
  if (stamp.sha256 !== pin.transport.snapshot.sha256 || stamp.commit !== pin.transport.commit) return false;
  return existsSync(join(dir, SC_DIR, 'build.mjs')) && existsSync(join(dir, BUILD_NODE));
}

/**
 * Скачать архив снимка. Сначала node fetch, при её отказе — `curl`: в средах с
 * собственным TLS-перехватом (корпоративный прокси, песочница CI-агента) у node свой
 * набор корневых сертификатов, и запрос падает, хотя загрузчик в системе есть.
 * Доверие не в канале, а в байтах: sha256 сверяется после скачивания в любом случае.
 */
export async function downloadArchive(url, { fetchImpl = null } = {}) {
  const problems = [];
  const call = fetchImpl || fetch;
  try {
    const response = await call(url, { redirect: 'follow' });
    // HTTP-статус — это ответ сервера, а не поломка канала: второй загрузчик тут ничего
    // не исправит, и 404 не превращается в «попробуем curl, вдруг повезёт».
    if (!response.ok) fail(3, `снимок транспорта не скачан: HTTP ${response.status} с ${url}`);
    return Buffer.from(await response.arrayBuffer());
  } catch (error) {
    problems.push(`fetch: ${error.message}`);
  }
  // Подстановка для тестов обязана оставаться герметичной: второй загрузчик — только
  // настоящему запросу есть чем заменить.
  if (!fetchImpl && run('curl', ['--version']).status === 0) {
    const out = run('curl', ['-sS', '--fail', '--location', '--proto', '=https', '--tlsv1.2', '--max-time', '180', '--output', '-', url], {
      encoding: 'buffer',
      maxBuffer: 64 * 1024 * 1024,
    });
    const body = Buffer.isBuffer(out.stdout) ? out.stdout : Buffer.alloc(0);
    if (out.status === 0 && body.length > 0) return body;
    problems.push(`curl: rc ${out.status} ${lastLines(out.stderr, 2)}`.trim());
  } else {
    problems.push('curl не найден');
  }
  fail(3, `снимок транспорта не скачан (${url}): ${problems.join('; ')}`);
  return null;
}

/**
 * Развернуть запиненный снимок транспорта: сначала sha256 архива, потом распаковка.
 * Несовпадение — удаление кэша и отказ (не перекачиваем «в надежде на удачу»).
 */
export function assertCacheOutsideRepo(cacheDir) {
  const rel = relative(REPO, cacheDir);
  if (rel !== '' && !rel.startsWith('..') && !isAbsolute(rel)) {
    fail(1, `кэш снимка не может лежать в рабочем дереве (${rel}): узел транспорта принял бы наше дерево за проверяемое`);
  }
  return cacheDir;
}

export async function obtainSnapshot({ pin, cacheDir, offline = false, fetchImpl = null }) {
  assertCacheOutsideRepo(cacheDir);
  const dir = join(cacheDir, cacheName(pin));
  if (snapshotUsable(dir, pin)) return { dir, fromCache: true };
  if (offline) {
    fail(3, `в кэше нет запиненного снимка (${display(dir)}), запуск с --offline: сеть не трогали`);
  }
  const archive = await downloadArchive(pin.transport.snapshot.url, { fetchImpl });
  const got = sha256Buffer(archive);
  const want = pin.transport.snapshot.sha256;
  if (got !== want) {
    rmSync(dir, { recursive: true, force: true });
    fail(2, `sha256 снимка ${got} ≠ запиненного ${want}: пин и реальность разошлись, сборка не допущена`);
  }
  const tmp = mkdtempSync(join(tmpdir(), 'relay-fetch-'));
  try {
    const archivePath = join(tmp, 'snapshot.tar.gz');
    writeFileSync(archivePath, archive);
    const extract = join(tmp, 'extract');
    mkdirSync(extract, { recursive: true });
    const tar = run('tar', ['-xzf', archivePath, '-C', extract]);
    if (tar.status !== 0) fail(3, `снимок не развёрнут: ${lastLines(tar.stderr, 4)}`);
    const entries = readdirSync(extract);
    if (entries.length !== 1) fail(3, `в архиве ожидался один корневой каталог, получилось ${entries.length}`);
    const root = join(extract, entries[0]);
    if (!existsSync(join(root, SC_DIR, 'build.mjs'))) fail(3, `в снимке нет ${SC_DIR}/build.mjs: пин указывает не на транспорт`);
    if (!existsSync(join(root, BUILD_NODE))) fail(3, `в снимке нет ${BUILD_NODE}: сверять проверки нечем`);
    rmSync(dir, { recursive: true, force: true });
    mkdirSync(dirname(dir), { recursive: true });
    cpSync(root, dir, { recursive: true });
    writeFileSync(stampPath(dir), `${JSON.stringify({ format: 'wotr-relay-cache/1', commit: pin.transport.commit, sha256: want }, null, 2)}\n`);
    return { dir, fromCache: false };
  } finally {
    rmSync(tmp, { recursive: true, force: true });
  }
}

/** Артефакт в кэше обязан быть дословно тем, что запинено: иначе сверять не с чем. */
export function verifyPinnedArtifact({ dir, pin }) {
  const path = join(dir, pin.transport.artifact.path);
  if (!existsSync(path)) fail(2, `в снимке нет ${pin.transport.artifact.path}: пин указывает не на тот выпуск`);
  const got = sha256File(path);
  if (got !== pin.transport.artifact.sha256) {
    fail(2, `артефакт снимка ${got.slice(0, 12)}… ≠ запиненный ${pin.transport.artifact.sha256.slice(0, 12)}… (${pin.transport.artifact.path})`);
  }
  return got;
}

/**
 * Чистый снимок: скачать (или взять из кэша) и убедиться, что это дословная копия выпуска.
 * Грязный кэш — обычное дело (его мог испортить предыдущий инструмент), поэтому лечится
 * одним перекачиванием. Молча собирать из испорченного кэша нельзя, а зацикливаться на
 * неверном пине тем более: вторая неудача — это уже «пин неверен».
 */
export async function obtainPristineSnapshot({ pin, cacheDir, offline = false, fetchImpl = null }) {
  const first = await obtainSnapshot({ pin, cacheDir, offline, fetchImpl });
  try {
    return { ...first, artifactSha256: verifyPinnedArtifact({ dir: first.dir, pin }) };
  } catch (error) {
    if (offline) fail(2, `${error.message} (и проверить перекачиванием нельзя: запуск с --offline)`);
    rmSync(first.dir, { recursive: true, force: true });
    const second = await obtainSnapshot({ pin, cacheDir, offline, fetchImpl });
    try {
      return { ...second, refetched: true, artifactSha256: verifyPinnedArtifact({ dir: second.dir, pin }) };
    } catch (again) {
      fail(2, `и после перекачивания снимок не совпадает с пином: ${again.message} — править надо pin.json, а не кэш`);
    }
  }
}

/**
 * Рабочий каталог — копия чистого снимка, в которой идёт сборка. Отдельная копия нужна
 * дважды: кэш остаётся эталоном для сверки хеша, а две сборки проекта (агент и CI) не
 * затирают друг друга. Внутри одного запуска копий тоже две — нейтральной сборке и
 * сборке с `--digest` нельзя сидеть в одном дереве: их узел проверяет дерево вызовом
 * `build.mjs --check` без входа, и на дереве с вшитым регламентом он отказал бы.
 */
export function copyToWorkDir(snapshotDir) {
  const scratch = mkdtempSync(join(tmpdir(), 'relay-work-'));
  const dir = join(scratch, 'snapshot');
  cpSync(snapshotDir, dir, { recursive: true });
  return { dir, scratch, cleanup: () => rmSync(scratch, { recursive: true, force: true }) };
}

/** Вход сборки: наш файл лежит в НАШЕМ репозитории, их сборка получает абсолютный путь. */
export function transportInputPath({ root, pin, field = 'digest' }) {
  const path = resolve(root, pin.inputs[field]);
  if (!existsSync(path)) fail(3, `нет входа сборки: ${display(path, root)}`);
  return path;
}

/**
 * Сборку делает транспорт своим `build.mjs`; мы только подаём ему регламент входом.
 * Без `digestPath` сборка нейтральная — ею и сверяется запиненный артефакт.
 */
export function runTransportBuild(snapshotRoot, { digestPath = null } = {}) {
  const args = digestPath === null ? [] : ['--digest', digestPath];
  const out = run(process.execPath, [join(snapshotRoot, SC_DIR, 'build.mjs'), ...args], { cwd: join(snapshotRoot, SC_DIR) });
  if (out.status !== 0) {
    fail(2, `сборка транспорта не прошла (rc ${out.status}):\n${lastLines(out.stderr || out.stdout)}`);
  }
  return lastLines(out.stdout, 2);
}

/** Фиксированные проверки транспорта — его же узлом, чтобы список не дублировать у нас. */
export function runTransportNode(snapshotRoot) {
  const out = run(process.execPath, [join(snapshotRoot, BUILD_NODE), '--repo', snapshotRoot, '--allow-dirty', '--json']);
  const report = parseNodeReport(out.stdout);
  if (out.status !== 0 || !report.allowed) {
    fail(2, `узел транспорта не допустил сборку (rc ${out.status}): ${lastLines(out.stderr || out.stdout, 4)}`);
  }
  return report;
}

/**
 * Один полный цикл: снимок → нейтральная сборка и узел транспорта → сборка с `--digest` →
 * проверки → артефакты проекта. `outDir` позволяет второй запуск ради сверки байт-в-байт;
 * `write:false` ничего не пишет.
 */
export async function buildOnce(options = {}) {
  const {
    pinPath = DEFAULT_PIN, root = REPO, outDir, offline = false,
    cacheDir = DEFAULT_CACHE, fetchImpl,
  } = options;
  const pin = readPin(pinPath);
  const profile = readProfile(resolve(root, pin.inputs.profile));
  const digestMd = readFileSync(resolve(root, pin.inputs.digest), 'utf8');
  const regulationMd = readFileSync(resolve(root, pin.inputs.regulation), 'utf8');
  checkDigest(digestMd, regulationMd);
  if (profile.profile.repo !== pin.project.repo) {
    fail(2, `профиль указывает на ${profile.profile.repo}, пин — на ${pin.project.repo}: это разные проекты`);
  }
  const snapshot = await obtainPristineSnapshot({ pin, cacheDir, offline, fetchImpl });
  const digestPath = transportInputPath({ root, pin, field: 'digest' });
  // (1) Нейтральная сборка на запиненном коммите: она доказывает, что пин описывает
  // воспроизводимый выпуск (запиненный артефакт = его же сборка без входа), и только на
  // таком дереве осмысленен его фиксированный список проверок.
  const neutral = copyToWorkDir(snapshot.dir);
  let nodeReport = null;
  let neutralCheck = null;
  try {
    runTransportBuild(neutral.dir);
    const neutralManifest = readTransportManifest({ snapshotRoot: neutral.dir });
    const neutralBundle = readFileSync(join(neutral.dir, SC_DIR, 'relay.user.js'), 'utf8');
    neutralCheck = verifyNeutralBundle({ pin, bundleText: neutralBundle, manifest: neutralManifest.manifest });
    if (neutralCheck.problems.length) {
      fail(2, `транспорт не воспроизводит запиненный артефакт (fail closed):\n  - ${neutralCheck.problems.join('\n  - ')}`);
    }
    nodeReport = runTransportNode(neutral.dir);
  } finally {
    neutral.cleanup();
  }
  // (2) Та же сборка, но регламент подан входом `--digest`: в дерево транспорта мы не
  // пишем ничего, его каталог docs с D-003 не существует ни у нас, ни в снимке.
  const work = copyToWorkDir(snapshot.dir);
  let result;
  try {
    const buildLine = runTransportBuild(work.dir, { digestPath });
    const transportManifest = readTransportManifest({ snapshotRoot: work.dir });
    const bundleText = readFileSync(join(work.dir, SC_DIR, 'relay.user.js'), 'utf8');
    const coveragePath = join(work.dir, SC_DIR, 'contracts', 'prompt-coverage.json');
    if (!existsSync(coveragePath)) fail(2, 'сборка транспорта не оставила contracts/prompt-coverage.json: покрытие не проверено');
    const coverageText = readFileSync(coveragePath, 'utf8');
    const check = verifyBundle({
      pin, bundleText, digestMd, regulationMd,
      transportManifest: transportManifest.manifest,
      nodeReport,
    });
    if (check.problems.length) fail(2, `бандл не допущен (fail closed):\n  - ${check.problems.join('\n  - ')}`);
    result = { bundleText, coverageText, check, buildLine };
  } finally {
    work.cleanup();
  }
  const { bundleText, coverageText, check, buildLine } = result;
  const manifest = {
    schema: 'wotr-relay-manifest/1',
    transport: {
      repo: pin.transport.repo,
      kind: pin.transport.kind,
      commit: pin.transport.commit,
      userscriptVersion: pin.transport.userscriptVersion,
      contractVersion: pin.transport.contractVersion,
      snapshotSha256: pin.transport.snapshot.sha256,
      pinnedArtifactSha256: pin.transport.artifact.sha256,
      // Хеш нейтральной сборки: он обязан совпасть с pinnedArtifactSha256 — этим
      // доказывается, что пин описывает воспроизводимый выпуск, а не чужой артефакт.
      neutralBundleSha256: neutralCheck.bundleSha256,
      // fromCache и refetched в манифест НЕ пишутся: они меняются от запуска к запуску,
      // а манифест обязан быть байт-стабильным — иначе «детерминизм» проверялся бы по шуму.
    },
    project: {
      repo: pin.project.repo,
      regulationVersion: check.digestVersion,
      digestSha256: sha256File(digestPath),
      regulationSha256: sha256File(resolve(root, pin.inputs.regulation)),
      profileSha256: sha256File(resolve(root, pin.inputs.profile)),
      profileId: profile.profile.id,
      profileName: profile.profile.name,
      // Что транспорт сам записал о принятом входе (их build-manifest): version + sha256.
      // Это то, чем `--verify` сверяет артефакт без сети и без скачивания.
      transportManifest: check.transportManifest,
    },
    artifact: {
      bundle: pin.output.bundle,
      coverage: pin.output.coverage,
      bundleSha256: check.bundleSha256,
      coverageSha256: sha256Text(coverageText),
      bundleBytes: check.facts.bytes,
      fingerprint: check.facts.fingerprint,
      userscriptVersion: check.facts.version,
      digestChars: check.embeddedDigest.length,
    },
    checks: nodeReport.checks,
  };
  const target = resolve(root, outDir || pin.output.dir);
  if (options.write !== false) {
    mkdirSync(target, { recursive: true });
    writeFileSync(join(target, 'relay.user.js'), bundleText, 'utf8');
    writeFileSync(join(target, 'prompt-coverage.json'), coverageText, 'utf8');
    writeFileSync(join(target, 'build-manifest.json'), `${JSON.stringify(manifest, null, 2)}\n`, 'utf8');
    manifest.artifact.outDir = display(target, root) || '.';
  }
  return { ok: true, code: 0, report: manifest, buildLine, pin, profile, snapshot };
}

/** Проверка уже собранного артефакта без сети: шаг CI после сборки, и локальная проверка. */
export function verifyArtifact({ bundlePath, pinPath = DEFAULT_PIN, root = REPO }) {
  const pin = readPin(pinPath);
  const digestMd = readFileSync(resolve(root, pin.inputs.digest), 'utf8');
  const regulationMd = readFileSync(resolve(root, pin.inputs.regulation), 'utf8');
  if (!existsSync(bundlePath)) fail(3, `артефакт не найден: ${display(bundlePath, root)}`);
  const bundleText = readFileSync(bundlePath, 'utf8');
  const facts = readBundleFacts(bundleText);
  const digest = checkDigest(digestMd, regulationMd);
  const embedded = embeddedDigest(bundleText);
  const problems = [];
  if (embedded !== promptDigest(digestMd)) {
    problems.push(`вшитый регламент не равен ${pin.inputs.digest}: артефакт собран из других docs`);
  }
  const embeddedVersion = (embedded.match(/Версия\s+(\d+\.\d+)/) || [])[1] || '';
  if (embeddedVersion !== digest.version) {
    problems.push(`номер во вшитом регламенте ${embeddedVersion || '—'}, в ${pin.inputs.regulation} — ${digest.version}`);
  }
  if (facts.version !== pin.transport.userscriptVersion) {
    problems.push(`@version ${facts.version || '—'} ≠ запиненный ${pin.transport.userscriptVersion}`);
  }
  if (facts.runtime !== facts.version) problems.push(`@version ${facts.version || '—'} и SCRIPT_VERSION ${facts.runtime || '—'} разошлись`);
  const manifestPath = join(dirname(bundlePath), 'build-manifest.json');
  if (existsSync(manifestPath)) {
    let manifest;
    try {
      manifest = JSON.parse(readFileSync(manifestPath, 'utf8'));
    } catch (error) {
      problems.push(`build-manifest.json не читается: ${error.message}`);
      manifest = {};
    }
    if (manifest?.artifact?.bundleSha256 !== sha256Text(bundleText)) {
      problems.push('build-manifest.json описывает другой бандл: артефакт изменён после сборки');
    }
    if (manifest?.transport?.commit !== pin.transport.commit) {
      problems.push(`манифест собран на коммите ${manifest?.transport?.commit || '—'}, пин — ${pin.transport.commit}`);
    }
    // Две опоры, которые появились вместе с D-003: транспорт сам записал, какой регламент
    // он принял (иначе «вшитый текст» ничем не подтверждён), и нейтральная сборка этого
    // запуска совпала с запиненным артефактом (иначе пин описывает невоспроизводимый выпуск).
    const recorded = manifest?.project?.transportManifest;
    if (!recorded) {
      problems.push('в build-manifest.json нет project.transportManifest: артефакт собран до D-003, '
        + 'когда транспорт не подтверждал вход — пересоберите бандл');
    } else {
      if (recorded.version !== digest.version) {
        problems.push(`транспорт зафиксировал регламент ${recorded.version || '—'}, в ${pin.inputs.regulation} — ${digest.version}`);
      }
      if (recorded.sha256 !== sha256Text(digestMd)) {
        problems.push('sha256 входа в build-manifest транспорта ≠ хешу текущего '
          + `${pin.inputs.digest}: артефакт собран из другой копии регламента`);
      }
    }
    if (manifest?.transport?.neutralBundleSha256
        && manifest.transport.neutralBundleSha256 !== pin.transport.artifact.sha256) {
      problems.push('манифест собран из нейтральной сборки, не совпавшей с запиненным артефактом: '
        + 'воспроизводимость выпуска не подтверждена');
    }
  }
  return {
    ok: problems.length === 0, problems, facts, bundlePath: relative(root, bundlePath) || bundlePath,
    digestVersion: digest.version, bundleSha256: sha256Text(bundleText), digestChars: embedded.length,
  };
}

function parseArgs(argv) {
  const out = { pin: DEFAULT_PIN, cache: '', out: '', check: false, verify: '', json: false, offline: false, help: false };
  // Значение принимается и как `--pin путь`, и как `--pin=путь`; `--verify` умеет без
  // значения (тогда путь по умолчанию), поэтому его случай разобран отдельно.
  for (let i = 0; i < argv.length; i += 1) {
    const current = String(argv[i]);
    const arg = current.split('=')[0];
    const inline = current.includes('=') ? current.slice(current.indexOf('=') + 1) : null;
    const take = () => {
      if (inline !== null) return inline;
      i += 1;
      if (i >= argv.length) fail(1, `${arg}: ожидалось значение`);
      return String(argv[i]);
    };
    if (arg === '--pin') out.pin = resolve(REPO, take());
    else if (arg === '--cache') out.cache = resolve(REPO, take());
    else if (arg === '--out') out.out = take();
    else if (arg === '--verify') {
      const following = String(argv[i + 1] ?? '');
      out.verify = inline !== null || !following || following.startsWith('--')
        ? 'build/relay/relay.user.js'
        : take();
      out.verify = resolve(REPO, out.verify);
    }
    else if (arg === '--check') out.check = true;
    else if (arg === '--json') out.json = true;
    else if (arg === '--offline') out.offline = true;
    else if (arg === '--help' || arg === '-h') out.help = true;
    else fail(1, `неизвестный аргумент: ${current}\n  есть: --check, --verify [ФАЙЛ], --json, --offline, --pin, --cache, --out`);
  }
  return out;
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  if (args.help) {
    console.log('node tools/relay/build.mjs [--check | --verify [ФАЙЛ]] [--json] [--offline] [--pin ФАЙЛ] [--cache КАТАЛОГ] [--out КАТАЛОГ]');
    console.log('Сверяет нейтральную сборку запиненного выпуска с пином и собирает бандл: наш');
    console.log('docs/regulation-digest.md передаётся его же сборке флагом --digest, сверка по contracts/build-manifest.json.');
    console.log('Коды: 0 — допущено, 1 — неверный запуск или негодный пин, 2 — проверка не пройдена, 3 — источники не получены.');
    return 0;
  }
  const pin = readPin(args.pin);
  const cacheDir = args.cache || DEFAULT_CACHE;
  if (args.verify) {
    const result = verifyArtifact({ bundlePath: args.verify, pinPath: args.pin });
    if (args.json) console.log(JSON.stringify(result, null, 2));
    else if (!result.ok) console.error(`артефакт не годится:\n  - ${result.problems.join('\n  - ')}`);
    else {
      console.log(`артефакт годится: регламент ${result.digestVersion} (${result.digestChars} зн.), @version ${result.facts.version}, `
        + `отпечаток ${result.facts.fingerprint}, sha256 ${result.bundleSha256.slice(0, 12)}…`);
    }
    return result.ok ? 0 : 2;
  }
  const options = { pinPath: args.pin, cacheDir, offline: args.offline, outDir: args.out || undefined };
  const first = await buildOnce(options);
  const humanLine = `сборка допущена: транспорт ${pin.transport.userscriptVersion} @ ${pin.transport.commit.slice(0, 7)}, `
    + `регламент ${first.report.project.regulationVersion}, отпечаток ${first.report.artifact.fingerprint}, `
    + `sha256 ${first.report.artifact.bundleSha256.slice(0, 12)}… → ${first.report.artifact.outDir}`;
  if (!args.check) {
    if (args.json) console.log(JSON.stringify(first.report, null, 2));
    else console.log(humanLine);
    console.error(`${first.snapshot.refetched ? 'кэш был испорчен — снимок перекачан; ' : ''}${first.buildLine}`);
    return 0;
  }
  // Детерминизм: второй запуск в отдельный каталог, сверка байт-в-байт.
  const tmp = mkdtempSync(join(tmpdir(), 'relay-determinism-'));
  try {
    const second = await buildOnce({ ...options, outDir: tmp });
    const firstDir = resolve(REPO, args.out || pin.output.dir);
    const bad = [];
    for (const name of MANIFEST_ARTIFACTS) {
      const a = readFileSync(join(firstDir, name), 'utf8');
      const b = readFileSync(join(tmp, name), 'utf8');
      if (a !== b) bad.push(`${name}: ${sha256Text(a).slice(0, 12)}… ≠ ${sha256Text(b).slice(0, 12)}…`);
    }
    const payloadEqual = second.report.artifact.bundleSha256 === first.report.artifact.bundleSha256;
    if (!payloadEqual) bad.push('второй запуск вернул другой sha256 бандла');
    const line = bad.length ? `сборка недетерминирована:\n  - ${bad.join('\n  - ')}`
      : `детерминизм ok: ${MANIFEST_ARTIFACTS.join(', ')} байт-в-байт равны в двух запусках`;
    if (args.json) console.log(JSON.stringify({ ok: bad.length === 0, determinism: bad.length ? bad : 'equal', report: first.report }, null, 2));
    else if (bad.length) console.error(line);
    else console.log(line);
    return bad.length ? 2 : 0;
  } finally {
    rmSync(tmp, { recursive: true, force: true });
  }
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  process.exitCode = await main().catch((error) => {
    const code = error.relayCode || 1;
    console.error(`${code === 2 ? 'сборка не допущена' : 'ошибка'}: ${error.message}`);
    return code;
  });
}
