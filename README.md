# sentinel-mesh-ops

[![Platform](https://img.shields.io/badge/Platform-Arch_Linux_%7C_Ubuntu_24.04-blue.svg?logo=linux)](https://github.com/BannedForWhat/sentinel-mesh-ops)
[![CI/CD](https://img.shields.io/badge/CI%2FCD-GitHub_Actions-green.svg?logo=githubactions)](https://github.com/BannedForWhat/sentinel-mesh-ops/actions)
[![Sanitizers](https://img.shields.io/badge/Sanitizers-ASan_%7C_UBSan_%7C_TSan-red.svg)](https://github.com/BannedForWhat/sentinel-mesh-ops)
[![Zero-Alloc](https://img.shields.io/badge/Proof-Zero--Allocation_(-Wl,--wrap)-orange.svg)](https://github.com/BannedForWhat/sentinel-mesh-ops)
[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

Тестовый стенд и конфигурация CI/CD для проверки проектов [amdgpu-sentinel](https://github.com/BannedForWhat/amdgpu-sentinel) и [prompt-mesh](https://github.com/BannedForWhat/prompt-mesh) в GitHub Actions (Ubuntu 24.04).

Репозиторий обеспечивает запуск тестов без физической видеокарты AMD в раннере, проверяет отсутствие динамических аллокаций памяти в основном цикле C++ демона и тестирует синхронизацию Seqlock под санитайзерами компилятора.

## Архитектура конвейера

```text
 +-------------------------------------------------------------------------+
 |                 GitHub Actions Runner (ubuntu-24.04)                   |
 |                                                                         |
 |  [Job 1: Zero-Alloc Proof]        [Job 2: Sanitizers Matrix]            |
 |  • GNU ld: -Wl,--wrap=malloc      • ASan + UBSan (-fno-sanitize=vptr)   |
 |  • Перехват динамической памяти   • TSan: верификация SeqlockChannel    |
 |  • Вызов std::abort() при ошибке    с барьерами acquire/release         |
 +------------------------------------+------------------------------------+
                                      │
                                      ▼
 +-------------------------------------------------------------------------+
 |             [Job 3: Headless Integration (Bubblewrap Sandbox)]          |
 |                                                                         |
 |   /tmp/mock_sysfs/class/drm/card0/ ──(bwrap --ro-bind)──> /sys/class/   |
 |   /tmp/mock_shm/ ────────────────────(bwrap --bind)─────> /dev/shm      |
 |                                                                         |
 |   • Непривилегированные пространства имен (User/Mount Namespaces)       |
 |   • Конфигурация AppArmor для запуска bwrap в Ubuntu 24.04              |
 |   • Сквозная проверка взаимодействия компонентов                        |
 +-------------------------------------------------------------------------+
```

## Методы верификации

1. **Проверка отсутствия обращений к куче (`-Wl,--wrap=malloc`):** вызовы `malloc`, `calloc`, `realloc` и `free` перехватываются компоновщиком GNU ld. В фазе активного реактора любая аллокация в куче фиксируется счетчиком и приводит к завершению программы.
2. **Разделение санитайзеров компилятора:** цели для `AddressSanitizer` и `ThreadSanitizer` изолированы в `CMakeLists.txt`, так как одновременная сборка с обоими флагами не поддерживается. Корректность работы `SeqlockChannel` проверяется под TSan на 100 000 параллельных итераций чтения и записи.
3. **Эмуляция sysfs через Bubblewrap (`bwrap`):** дерево виртуальных файлов монтируется поверх системного пути `/sys` без прав root. Для раннеров Ubuntu 24.04 генерируется локальный профиль AppArmor, снимающий блокировку непривилегированных пространств имен.

## Локальный запуск тестов

```bash
git clone --recursive https://github.com/BannedForWhat/sentinel-mesh-ops.git
cd sentinel-mesh-ops

# 1. Проверка отсутствия динамических аллокаций
cmake -B build-zero -DENABLE_ZERO_ALLOC_TEST=ON
cmake --build build-zero
./build-zero/zero_alloc_guard_test

# 2. Проверка синхронизации под ThreadSanitizer
cmake -B build-tsan -DENABLE_TSAN=ON
cmake --build build-tsan
./build-tsan/seqlock_tsan_test

# 3. Проверка работы с памятью под AddressSanitizer и UBSan
cmake -B build-asan -DENABLE_ASAN=ON
cmake --build build-asan
./build-asan/mock_sysfs_asan_test
```

## Подмодули репозитория

* `submodules/amdgpu-sentinel`: C++ демон телеметрии видеопамяти.
* `submodules/prompt-mesh`: асинхронный сетевой шлюз на Python.
