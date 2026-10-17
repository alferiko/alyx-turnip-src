# Publication validation / Проверка публикации — 2026-10-17

| Variant | Clean source reconstruction | Android NDK r27d build | Reconstructed-header CPU policy |
| --- | --- | --- | --- |
| `compat` | PASS | Historical 2026-10-07 check; not recompiled this time | PASS, ASan/UBSan |
| `msaa-vrs` | PASS | PASS | PASS, ASan/UBSan |
| `msaa-vrs-cache` | PASS | PASS | PASS, ASan/UBSan + final-map regression |

All historical patch/input hashes and prebuilt driver hashes match their
manifests. Both new variants were built in separate fresh source trees using
the published `scripts/build.py`. Rebuilt hashes differ from the historical
reference binaries; the distributed libraries are the original reference
files, not these fresh outputs. No new hardware validation was performed.

The policy runner copies the preserved historical test into a temporary
directory so its quoted include resolves to the reconstructed Mesa header.
The cache regression additionally verifies sample-count separation, sharp
center preservation, explicit MSAA rate limits, owned copies, repeated hits,
unsupported-sample rejection and input invalidation.

Все исходные патчи, входы сборки и готовые библиотеки совпадают с историческими
контрольными суммами. Новые варианты собраны независимо в чистых деревьях.
Их новые хеши отличаются от эталонов; распространяются неизменённые библиотеки
основного проекта. Новое тестирование на гарнитуре не выполнялось.

Тест использует заголовок каждого восстановленного варианта. Для кэша
дополнительно проверены разделение по числу сэмплов, сохранение чёткого центра,
допустимые частоты MSAA, владение копиями, повторные попадания, отклонение
неподдерживаемых режимов и инвалидирование после изменения входной карты.

- [compat preparation](compat-build.json), [policy output](compat-policy.log)
- [MSAA/VRS build](msaa-vrs-build.json), [policy output](msaa-vrs-policy.log)
- [Cached-map build](msaa-vrs-cache-build.json), [policy output](msaa-vrs-cache-policy.log)
