# Source provenance / Происхождение исходников

Snapshot: **2026-10-17**. Parent application source commit:
`b100753c6fe6ee1d5c370bc8ab8eae1d5b1bd84a`.
The original 2026-10-07 compatibility reference binary remains byte-for-byte
intact. Publication manifests record repository-relative inputs and their hashes.

| Layer | Identity |
| --- | --- |
| Mesa base | `48297c834466` |
| Quest compatibility upstream | `Jbbrack03/Quest-3-Turnip-Driver@83cd874f3449d091fca51d25389f88f520a7f2f3` |
| Baseline binary used by historical builds | `7171e6f718f582f08bb20e57a10c9e066d434241e984085cb3695187f51f90da` |
| Intermediate release driver | `abf6363b138f501b1a813ba5c030735282c307e74a59f14658e021ca9482f117` |
| Published combined compiler driver | `1559bf7493ae397c274c9743befcbcc4bd6cfd6e1232f4f41f0f45e5374c25bb` |
| MSAA2 + VRS driver | `d7cfb0b4df979c105a305a849ae9c6f6fd2af1a82f067317a7eec050a26d1764` |
| Final-map cache driver | `3575c010b189f3f7b715b265fde97ea0091f7b769f8623cadeb5c35cdaf5d756` |
| Compiler profile | `c222722529f8ea1d98b9a1e89a4c91ff96b3d7cd8c8bfafc6836c3f10bdafd2c` |

## English

The baseline patch is a complete diff of the 12 changed tracked source files
between the pinned pristine Mesa archive and the locally preserved baseline.
Before export, baseline files were checked against the original build report's
`originalFiles` fingerprints. It includes upstream Quest compatibility and
later Alyx fixes; it is not attributed entirely to one author.

The historical combined-driver manifest records **32 patches**, including
27 release/backport patches, the VRS patch, three compiler patches, and the
generated profile configuration patch. Patch and profile bytes are preserved;
all listed inputs are checked against the SHA256 recorded in their manifests.
The actual baseline and ordered patches reconstruct the complete modified
source, including new source files introduced by patches. The MSAA2 variant
adds one patch (33 total), and the final-map cache adds another (34 total).
[`variants.json`](../provenance/variants.json) maps each variant to its
manifest and reference binary. [Variant evidence](VARIANTS.md).

The compatibility library matches the application v907 driver's bytes. Historical
`gpuValidated: false` in the build report is deliberately preserved: it
describes that build step, not subsequent application tests. The separate
Quest 3 v907 record reports eight short successful launch/load/render scenarios.
Other models' validation limits are stated in the README.

The additional binaries match the parent project's pinned MSAA/VRS and
cached-map drivers. Their manifests retain historical validation results, including
the inherited `policyTests` fields; those fields are not fresh per-variant
validation. Publication checks use each reconstructed variant's header and
record separate results. The host's 0.185 universal recipe accepts MSAA/VRS;
the final-map cache stays behind private Quest test guards.

Some preserved provenance contains original workstation paths. These are
historical descriptions, not prerequisites for the portable build. The PGO
profile contains compiler execution counts, not the captured game shader files.

Original Mesa license texts and per-file copyright notices are preserved.
The source archive is verified by SHA256 before any patch is applied.
No game assets, application APK, signing keys or unrelated workspace history
are part of this repository.

## Русский

Базовый патч содержит полную разницу 12 изменённых файлов между закреплённым
архивом Mesa и сохранённой базой. Перед экспортом исходники сверены с полем
`originalFiles` исторического отчёта сборки. Патч объединяет исходную
совместимость Quest и дальнейшие исправления Alyx; авторство всех изменений
не приписывается одному разработчику.

Манифест содержит **32 патча**: 27 релизных/перенесённых из Mesa, один VRS,
три для компилятора и один с настройками PGO. Байты патчей и профиля сохранены.
Все входы проверяются по SHA256 из манифестов с путями относительно репозитория.
Применение всей цепочки восстанавливает полное изменённое дерево исходников.
MSAA2 добавляет один патч (33 всего), кэш итоговой карты — ещё один (34 всего).
Соответствие вариантов манифестам и библиотекам записано в
[`variants.json`](../provenance/variants.json). [Описание проверок](VARIANTS.md).

Библиотека `compat` совпадает с драйвером приложения v907. Историческое значение
`gpuValidated: false` сохранено: оно относится к этапу сборки. Более поздние
восемь коротких проверок запуска, загрузки и изображения Quest 3 записаны
отдельно. Ограничения проверки других гарнитур указаны в README.

Новые библиотеки совпадают с закреплёнными MSAA/VRS-вариантами основного проекта.
Исторические поля `policyTests` сохранены без изменения и не выданы за новые
проверки каждого варианта. Проверки публикации используют заголовки реально
восстановленных вариантов и сохраняются отдельно. Сборщик 0.185 допускает
MSAA/VRS в универсальной сборке; новый кэш пока ограничен тестовым путём Quest.

В исторических отчётах встречаются прежние локальные пути; новая сборка
от них не зависит. Профиль PGO содержит счётчики исполнения компилятора,
а не файлы шейдеров игры. Лицензии Mesa и уведомления об авторстве сохранены.
Игровые ресурсы, APK приложения, ключи подписи и посторонняя история рабочего
каталога не входят в репозиторий.
