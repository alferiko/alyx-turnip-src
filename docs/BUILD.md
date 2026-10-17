# Building Alyx Turnip / Сборка Alyx Turnip

## English

Use Linux x86-64 or WSL2, Python 3.12+, Git, Ninja, a C/C++ host compiler,
Python venv support and **Android NDK r27d (27.3.13750724)**. Extract the NDK
from the [official archive](https://developer.android.com/ndk/downloads/older_releases).
The pinned download URL and digest are also in
[`upstream-inputs.json`](../provenance/upstream-inputs.json).

```sh
sudo apt-get install git python3-venv ninja-build build-essential bison flex pkg-config
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements-build.txt
python scripts/verify.py
python scripts/build.py --out "$HOME/alyx-turnip-build-01" \
  --variant msaa-vrs --ndk /path/to/android-ndk-r27d --jobs 8
```

Run from the repository root. Replace `/path/to/android-ndk-r27d` with your
extracted NDK directory. The output directory must not already exist. The
builder never modifies an existing Mesa checkout or installs anything on a
headset. Internet access is needed for Mesa and Meson's pinned fallback
dependencies (Expat, libarchive and libxml2). `--archive /path/to/mesa.tar.gz`
supplies a cached Mesa archive with the matching hash; a fully offline compile
also needs the packages specified by Mesa's `subprojects/*.wrap` cached in
the prepared tree's `subprojects/packagecache/`.

Select `--variant compat`, `--variant msaa-vrs`, or `--variant msaa-vrs-cache`.
Omitting the option retains the historical `compat` default. Each variant
needs a separate new output directory. [Variant status](VARIANTS.md).

Outputs:

- `mesa/`: the complete patched Mesa source tree and its build directory.
- `libalyx_turnip.so`: the rebuilt Android ARM64 library.
- `build-report.json`: input identity, output hash and reference comparison.
- `compiler-profile.patch`: regenerated PGO patch for your checkout path.

To reconstruct sources without compiling:

```sh
python3 scripts/build.py --variant msaa-vrs --out /tmp/alyx-turnip-source --prepare-only
```

The reference binary is pinned, but a build at another path or with different
tools is not promised to be byte-identical. The script reports the actual hash;
it does not silently claim equivalence. Successful compilation is not a GPU
or headset validation result.

The publication check on 2026-10-07 successfully reconstructed and compiled
the sources with NDK r27d. Its binary hash differs from the historical
reference; see [`publication-build.json`](../provenance/publication-build.json).
The repository ships the historical reference, not this untested rebuild.

On 2026-10-17 both new variants were fully rebuilt from clean sources with
NDK r27d. All three variants passed policy tests using their reconstructed
headers under ASan/UBSan. Reports and test output are in
[`validation-2026-10-17`](../provenance/validation-2026-10-17/README.md).
The new rebuilds also have different hashes from the reference binaries;
the published libraries remain the unchanged parent-project references.

### Source chain

1. Verify/extract Mesa `48297c834466` using its recorded archive SHA256.
2. Apply `patches/0000-quest-baseline.patch` and verify the source fingerprints
   recorded by the historical compiler build.
3. Apply the selected variant's ordered patches, with its manifest selected
   from `provenance/variants.json` (32, 33 or 34 patches).
4. Regenerate the PGO patch using the original helper and profile; do not apply
   the historical absolute-path `compiler-profile.patch` directly.
5. Configure the Android API29 toolchain and compile Turnip with Meson/Ninja.

Historical builder snapshots in `dependencies/` and `scripts/build_msaa_vrs*.py`
are evidence, not standalone
entry points. Use `scripts/build.py` for a relocated checkout. Historical
reference documents may mention files or application workflows outside this
driver-only distribution.

### CPU policy test

```sh
python3 scripts/test_policy.py --mesa /tmp/alyx-turnip-source/mesa
```

This tests the reconstructed variant's actual header under ASan/UBSan, not
the unchanged baseline header next to the historical test. The cache variant
also tests sample-count isolation, final-map ownership and MSAA rate limits.
These are CPU tests, not rendered-pixel or new headset tests.

### Updating this repository

After reviewing changes, run `python3 scripts/update_checksums.py` and then
`python3 scripts/verify.py`. The verifier checks the full published inventory,
all three historical manifests and all three reference binary hashes.

## Русский

Нужны Linux x86-64 или WSL2, Python 3.12+, Git, Ninja, системный компилятор
C/C++, поддержка Python venv и **Android NDK r27d (27.3.13750724)**.
Скачайте и распакуйте NDK из [официального архива](https://developer.android.com/ndk/downloads/older_releases).
Точная ссылка и контрольная сумма сохранены в
[`upstream-inputs.json`](../provenance/upstream-inputs.json).

В корне репозитория выполните команды из раздела English выше, заменив
`/path/to/android-ndk-r27d` своим путём. Каталог `--out` должен быть новым.
Выберите `--variant compat`, `--variant msaa-vrs` или `--variant msaa-vrs-cache`.
Без параметра сохраняется `compat`. Для каждого варианта нужен отдельный каталог.
Скрипт не изменяет существующие деревья Mesa и не устанавливает драйвер на
гарнитуру. Для использования готового архива Mesa передайте
`--archive /путь/к/mesa.tar.gz`; контрольная сумма всё равно проверяется.
При полной компиляции Meson также загружает закреплённые зависимости Expat,
libarchive и libxml2. Для полностью автономной сборки их архивы из описаний
`subprojects/*.wrap` нужны в `subprojects/packagecache/` подготовленного дерева.

После сборки в каталоге результата появятся полные исходники `mesa/`,
библиотека `libalyx_turnip.so`, отчёт `build-report.json` и локальный патч
настроек PGO. Флаг `--prepare-only` восстанавливает исходники без компиляции.

Последовательность: проверка исходного архива Mesa → базовые изменения Quest
→ проверка отпечатков исходников → патчи релизного драйвера, VRS и компилятора
→ генерация PGO-патча под текущий путь → сборка Meson/Ninja.

При изменении окружения или пути сборки побайтовое совпадение с эталоном
не гарантируется: фактическая сумма записывается в отчёт. Успешная компиляция
не заменяет проверку на GPU и гарнитуре. Команда CPU-теста выше проверяет
логику преобразования VRS по заголовку выбранного восстановленного варианта,
но не изображение. Для варианта с кэшем также проверяются разделение по числу
сэмплов, владение итоговой картой и ограничения MSAA. Используется ASan/UBSan.

07.10.2026 проверены восстановление исходников и полная компиляция с NDK r27d.
Хеш новой библиотеки отличается от исторического; результат сохранён в
[`publication-build.json`](../provenance/publication-build.json). В репозитории
размещена исходная проверенная библиотека, а не эта новая сборка.

17.10.2026 оба новых варианта полностью пересобраны с NDK r27d. Все три
варианта прошли тесты политики по восстановленным заголовкам с ASan/UBSan.
См. [отчёты](../provenance/validation-2026-10-17/README.md). Хеши новых сборок
отличаются от эталонов; опубликованные библиотеки взяты без изменения байтов
из основного проекта. Новые проверки на гарнитуре не выполнялись.

Исторические скрипты в `dependencies/` и `scripts/build_msaa_vrs*.py` сохранены для прослеживаемости.
Для новой сборки используйте `scripts/build.py`. Исторические документы
могут ссылаться на файлы и сценарии приложения вне этого репозитория.
После правок репозитория обновите суммы через `scripts/update_checksums.py`
и выполните `scripts/verify.py` для проверки всех трёх вариантов.
