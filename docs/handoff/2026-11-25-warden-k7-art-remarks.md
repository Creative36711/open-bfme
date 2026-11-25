# Замечания D1–D4 по K7: правка `ART-PIPELINE` §3/§4/§8 и ширины `ART` в зоне Godot World

**Дата:** 2026-11-25 · **Решение:** `WOTR-D-046` · **Карточка:** `WOTR-1.1 · K7 · SCCPointer.cur
availability for the retail Godot smoke` (Notion `4216f0cf-5705-815f-8124-ca2aa4f40097`,
Accepted/pass 2026-11-25).

K7 принята по всем четырём критериям, но её материал (Remaster-owned курсор, сгенерированный из
ASCII-исходника в коде и зарегистрированный в install overlay) вскрыл четыре пробела визуального
контракта `docs/tooling/ART-PIPELINE.md` (WOTR-D-015). Coordinator фиксировал замечания D1–D4 без
оформления карточкой — формализация и правка за Warden. Правки применены этим же ходом.

## D1 — §3 «Матрица выбора»: курсор

Строка матрицы предписывала для курсора «SVG с честными paths и `current` palette contract».
Движок потребляет курсор только нативным файлом: `GodotMouse::loadCursorTable` читает
`data/ini/mouse.ini`, образ ищется `FindFileNoCase(<install>/data/cursors, SCCPointer.cur)`
(без `res://`-fallback) и декодируется `DecodeCursorFile`. Для этого случая SVG неприменим, и K7
развела механизм: Remaster-owned ASCII-исходник в коде → runtime-encoded Windows CUR
(32×32, hotspot 2,2 — как в `mouse.ini` BFME2 1.06 и RotWK 2.01), регистрация в install overlay.
**Правка:** строка «курсор» разделена на два случая — движок потребляет SVG (SVG с `current`
palette contract) и движок требует нативный формат (Remaster-owned источник в коде →
runtime-encoded файл; размер/hotspot/SHA-256 записаны). Retail-копия запрещена в обоих случаях.

## D2 — §4 «Style brief до первой генерации»: brief не зависит от image quota

Требование brief было привязано к «до расхода image quota» — процедурная генерация без квоты
(K7: 0 изображений Arena) проходила мимо него формально. **Правка:** brief обязателен для любого
нового визуального ассета независимо от способа получения; для zero-quota процедурных/функциональных
ассетов достаточно минимального brief: назначение, размер/hotspot/форма/палитра, consuming node,
provenance-граница (не retail-копия) и бюджет генерации 0.

## D3 — §8 «Что запрещено»: граница «asset без provenance»

Пункт «коммитить official/retail art, watermark или asset без provenance» не различал Remaster-owned
runtime-encoded ассет с записанным provenance и байтовую копию retail. **Правка:** добавлены три
границы — (а) runtime-encoded Remaster-ассет с записанным provenance (процедура генерации, SHA-256,
проверка собственным декодером движка и preview) не является «asset без provenance»; (б) функциональный
процедурный ассет живёт в дереве исходником (например ASCII в коде) и генерируется в рантайме, его
регистрация идёт в install overlay, а не в дерево; (в) байтовое копирование retail-файла запрещено
в любой каталог, включая install overlay.

## D4 — ширина `ART` в зоне Godot World

Граница «generated art → Scope Technical Artist; интеграция → Godot World Engineer»
(ART-PIPELINE §1, `docs/agents/tools.md`, `docs/agents/roles/specializations.md`) не описывала
малые функциональные процедурные ассеты внутри `godot/wotr_remaster/**`: K7 авторил Godot World
Engineer, а не Technical Artist. **Правка:** зафиксирована ширина ART-скоупа Godot World —
курсоры/маски/маркеры, генерируемые из собственного исходника в коде без image quota и с
минимальным brief §4, Godot World Engineer делает внутри своей карточки; генерация изображений,
иллюстративные наборы и material skins остаются отдельным Scope Technical Artist, интеграция —
у Godot World Engineer.

Попутно устранено подтверждённое расхождение ширины зоны: `specializations.md` показывал
`godot/wotr_remaster/world/**`, тогда как реестр «Роли» («Зона записи» строки Godot World Engineer)
держит `godot/wotr_remaster/**`, и K7 писала в `godot/wotr_remaster/retail/**`. Строка
`specializations.md` приведена к реестру. Пер-карточные evidence-строки
(`docs/evidence/WOTR-1-1/K4/**`, `docs/evidence/WOTR-1-1/K7/**`) остаются по схеме `D-085` —
нарезка новых глобов под карточки не требуется, `ART`-контракт на них не распространяется.

Оракул §8/§9 миссии правками не затронут. Решение: `WOTR-D-046`.
