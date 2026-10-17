# Driver variants / Варианты драйвера

Updated / Обновлено: **2026-10-17**.

| Variant | Patches | Reference SHA256 |
| --- | --- | --- |
| `compat` | 32 | `1559bf7493ae397c274c9743befcbcc4bd6cfd6e1232f4f41f0f45e5374c25bb` |
| `msaa-vrs` | 33 | `d7cfb0b4df979c105a305a849ae9c6f6fd2af1a82f067317a7eec050a26d1764` |
| `msaa-vrs-cache` | 34 | `3575c010b189f3f7b715b265fde97ea0091f7b769f8623cadeb5c35cdaf5d756` |

## English

All variants share Mesa `48297c834466`, the same Quest baseline, compiler
optimizations and PGO profile. No Mesa 26.2.5 rebase or additional upstream
backport is part of this update. The 32/33/34 counts exclude the common
`patches/0000-quest-baseline.patch` applied first.

**compat:** preserved v907 reference and default of `scripts/build.py`.
Its hardware VRS backend accepts eligible single-sample passes only.

**msaa-vrs:** adds `tests/turnip_vrs/msaa2-vrs.patch`. It accepts ordinary
MSAA2 dynamic rendering with attachment loads/stores and color/depth resolves,
retains full-sized multisample attachments, and clamps hardware rates to the
supported MSAA2 combinations (up to 4x2). Secondary command buffers, suspended
passes, custom resolves and MSRTSS remain excluded. The original cache stores
the unclamped map; each MSAA command-buffer copy is clamped afterward.

The change was integrated by parent commit `be43ff4` on 2026-10-11. The retained
[historical note](../provenance/history/MSAA-VRS.md) describes Quest GPU readback
checks for HDR/D24S8, resolves, eye regions, sharp centers, single-sample 4x4
regression and forced sysmem. Its statement about universal builds reflects
the integration date: the later **0.185 universal snapshot** explicitly admits
this driver's hash and selects it with DirectQuestVrs. Selection by a build
recipe is not a PICO or Quest 2/Pro hardware acceptance result.

**msaa-vrs-cache:** additionally applies `tests/turnip_vrs/msaa2-final-map-cache.patch`.
The cache key includes the sample count and stores the final, clamped MSAA2 map.
Cache hits copy that map without repeatedly reading/modifying GPU-mapped output.
Single-sample and MSAA2 entries cannot alias. Integrated by parent commit
`3a40ab5` on 2026-10-12 in the Quest demo path. The
[integration record](../provenance/history/FOVEAL-AA-MAIN.md) records accepted
v2069 imagery on Quest and host checks. Public, universal and PICO builds still
reject this cache option. Central TAA itself is a host feature, not driver code.

The bundled `.so` files are historical reference binaries, copied without byte
changes. Manifests retain validation fields such as `gpuValidated` and record
repository-relative inputs with matching hashes. Reconstructed sources use a regenerated profile patch for the
new path. Fresh build checks and historical device acceptance are separate;
no new headset test is claimed by this publication.

Use `scripts/build.py --variant NAME` for portable builds. Historical
`scripts/build_msaa_vrs*.py` snapshots require the parent workspace's configured
Mesa environment; they are retained as recorded inputs, not the supported
entry points of this repository.

## Русский

Все три варианта используют Mesa `48297c834466`, общую базу Quest, прежние
оптимизации компилятора и профиль PGO. В это обновление не входит переход
на Mesa 26.2.5. Числа 32/33/34 не включают общий базовый патч, применяемый первым.

**compat** — неизменённый эталон v907 и вариант сборки по умолчанию.
Аппаратный VRS работает только в подходящих проходах без MSAA.

**msaa-vrs** добавляет MSAA 2x вместе с VRS: загрузку/сохранение вложений,
разрешение цвета и глубины, полноразмерные MSAA-буферы и ограничение частоты
затенения до допустимых комбинаций, максимум 4x2. Исключения для вторичных
командных буферов, приостановленных проходов, нестандартных resolve и MSRTSS
сохранены. Патч интегрирован 11 октября; историческая запись описывает GPU-тесты
Quest. Универсальный сборщик 0.185 уже выбирает этот вариант, но это не заменяет
проверку на PICO и Quest 2/Pro.

**msaa-vrs-cache** добавляет кэш уже ограниченной итоговой карты. Ключ включает
число сэмплов: карты без MSAA и MSAA2 не смешиваются. При попадании в кэш карта
копируется без повторной обработки GPU-отображённой памяти. Изменение включено
12 октября в тестовый путь Quest; запись интеграции фиксирует приёмку изображения
v2069. Публичные, универсальные и PICO-сборки пока запрещают эту опцию.
Центральный TAA относится к приложению и не включён в код драйвера.

Библиотеки скопированы без изменения байтов. Манифесты сохраняют результаты
прежних проверок и содержат пути и хеши входов этого репозитория. Для сборки
используйте `scripts/build.py --variant ИМЯ`. Исторические сборщики сохранены
как входы прежних сборок, но требуют окружения основного проекта. Проверка
сборки при публикации и прежняя проверка на гарнитуре — отдельные результаты;
новое тестирование на устройстве этим обновлением не заявляется.
