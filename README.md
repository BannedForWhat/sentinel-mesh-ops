# sentinel-mesh-ops

[![Platform](https://img.shields.io/badge/Platform-Arch_Linux_%7C_Ubuntu_24.04-blue.svg?logo=linux)](https://github.com/BannedForWhat/sentinel-mesh-ops)
[![CI/CD](https://img.shields.io/badge/CI%2FCD-GitHub_Actions-green.svg?logo=githubactions)](https://github.com/BannedForWhat/sentinel-mesh-ops/actions)
[![Sanitizers](https://img.shields.io/badge/Sanitizers-ASan_%7C_UBSan_%7C_TSan-red.svg)](https://github.com/BannedForWhat/sentinel-mesh-ops)
[![Zero-Alloc](https://img.shields.io/badge/Proof-Zero--Allocation_(-Wl,--wrap)-orange.svg)](https://github.com/BannedForWhat/sentinel-mesh-ops)
[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

Автоматизированный конвейер надежности (Reliability & DevSecOps Harness) для двухзвенного комплекса **[amdgpu-sentinel](https://github.com/BannedForWhat/amdgpu-sentinel)** (C++17 bare-metal демон) и **[prompt-mesh](https://github.com/BannedForWhat/prompt-mesh)** (L7 AI Gateway).

Проект решает фундаментальную проблему тестирования аппаратно-зависимого системного ПО в облачных CI/CD раннерах (GitHub Actions `ubuntu-24.04`), где физически отсутствуют графические процессоры AMD Radeon и драйвер `amdgpu`.

---

## Архитектура тестового конвейера

```text
 +-------------------------------------------------------------------------+
 |                 GitHub Actions Runner (ubuntu-24.04)                   |
 |                                                                         |
 |  [Job 1: Zero-Alloc Proof]        [Job 2: Sanitizers Matrix]            |
 |  • GNU ld: -Wl,--wrap=malloc      • ASan + UBSan (-fno-sanitize=vptr)   |
 |  • Атомарный перехват аллокаций   • TSan: SeqlockChannel data race      |
 |  • std::abort() при утечке в кучу   верификация с барьерами acquire/rel |
 +------------------------------------+------------------------------------+
                                      │
                                      ▼
 +-------------------------------------------------------------------------+
 |             [Job 3: Headless Integration (Bubblewrap Sandbox)]          |
 |                                                                         |
 |   /tmp/mock_sysfs/class/drm/card0/ ──(bwrap --ro-bind)──> /sys/class/   |
 |   /tmp/mock_shm/ ────────────────────(bwrap --bind)─────> /dev/shm      |
 |                                                                         |
 |   • Непривилегированные User & Mount Namespaces                         |
 |   • Обход ограничений AppArmor в Ubuntu 24.04                           |
 |   • 100% зеленая сквозная верификация Producer-Consumer пайплайна       |
 +-------------------------------------------------------------------------+
```

---

## Ключевые возможности

1. **Математическое доказательство Zero-Allocation (`-Wl,--wrap=malloc`):**
   * Вместо абстрактных обещаний тест использует директивы линкера GNU ld для перехвата `malloc`, `calloc`, `realloc` и `free`.
   * При переходе в фазу реактора любая непреднамеренная аллокация в куче фиксируется и немедленно роняет сборочный конвейер.
2. **Изоляция санитайзеров памяти (ASan / UBSan vs TSan):**
   * Разруливание несовместимостей: `AddressSanitizer` и `ThreadSanitizer` разведены в изолированные ветки сборочной матрицы.
   * `SeqlockChannel` протестирован под `TSan` на 100 000 итераций конкурентной записи и чтения без ложных срабатываний Data Race.
3. **Headless-эмуляция оборудования через Bubblewrap (`bwrap`):**
   * Синтетическая файловая иерархия DRM монтируется поверх системного `/sys` без необходимости запуска с правами `sudo`.
   * Применение выделенного профиля AppArmor для снятия блокировок `kernel.apparmor_restrict_unprivileged_userns` в Ubuntu 24.04.

---

## Локальная сборка и запуск тестов

```bash
git clone --recursive https://github.com/BannedForWhat/sentinel-mesh-ops.git
cd sentinel-mesh-ops

# 1. Проверка Zero-Allocation
cmake -B build-zero -DENABLE_ZERO_ALLOC_TEST=ON
cmake --build build-zero
./build-zero/zero_alloc_guard_test

# 2. Проверка многопоточности под ThreadSanitizer
cmake -B build-tsan -DENABLE_TSAN=ON
cmake --build build-tsan
./build-tsan/seqlock_tsan_test

# 3. Проверка безопасности памяти под AddressSanitizer/UBSan
cmake -B build-asan -DENABLE_ASAN=ON
cmake --build build-asan
./build-asan/mock_sysfs_asan_test
```

---

## Структура подмодулей

* `submodules/amdgpu-sentinel` — C++17 bare-metal демон телеметрии ядра Linux.
* `submodules/prompt-mesh` — L7 асинхронный платформенный AI-шлюз.
