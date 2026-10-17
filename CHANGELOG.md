# Changelog / История изменений

## 2026-10-17

### English

- Added the exact MSAA2/VRS reference driver and its final-map cache variant,
  with the two additional source patches, original manifests and build inputs.
- Added explicit `compat`, `msaa-vrs` and `msaa-vrs-cache` build selection.
  The original compatibility binary and default remain unchanged.
- Updated English/Russian documentation to distinguish release recipe usage,
  Quest device acceptance and the experimental cache's integration limits.
- Added per-variant integrity checks and policy tests using reconstructed
  headers; added sample-count isolation and final-map cache regression checks.
- Retained Mesa `48297c834466`, the original PGO profile and project logo.

### Русский

- Добавлены точные библиотеки MSAA2/VRS и варианта с кэшем итоговой карты,
  два новых патча, исходные манифесты и входы сборки.
- В сборщике доступны `compat`, `msaa-vrs` и `msaa-vrs-cache`.
  Прежняя библиотека совместимости и выбор по умолчанию сохранены.
- Обновлена документация на русском и английском: отдельно описаны выбор
  драйвера релизным сборщиком, проверки Quest и ограничения тестового кэша.
- Добавлены проверки целостности всех вариантов и тесты восстановленных
  заголовков, включая разделение кэша по MSAA и сохранение итоговых карт.
- База Mesa `48297c834466`, профиль PGO и логотип сохранены.

## 2026-10-07

Initial private publication: v907 compatibility reference, 32 patches,
compiler profile, source reconstruction and bilingual documentation.

Первая приватная публикация: эталон v907, 32 патча, профиль компилятора,
восстановление исходников и двуязычная документация.
