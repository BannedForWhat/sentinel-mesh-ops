cmake_minimum_required(VERSION 3.22)
project(sentinel_mesh_ops VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic -Werror)

option(ENABLE_ASAN "Enable AddressSanitizer and UBSan" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(ENABLE_ZERO_ALLOC_TEST "Build Zero-Allocation linker wrapping target" OFF)

if(ENABLE_ASAN AND ENABLE_TSAN)
   message(FATAL_ERROR "[FATAL] ASan and TSan are mutually exclusive.")
endif()

if(ENABLE_ZERO_ALLOC_TEST AND (ENABLE_ASAN OR ENABLE_TSAN))
   message(FATAL_ERROR "[FATAL] Zero-Alloc Guard uses linker wrapping and cannot be built with sanitizers.")
endif()

# ТАРГЕТ 1: ThreadSanitizer Concurrency Suite
if(ENABLE_TSAN)
   add_executable(seqlock_tsan_test tests/seqlock_stress_test.cpp)
   target_compile_options(seqlock_tsan_test PRIVATE -fsanitize=thread -g -O1 -Wno-error=tsan)
   target_link_options(seqlock_tsan_test PRIVATE -fsanitize=thread)
   target_link_libraries(seqlock_tsan_test PRIVATE pthread)
endif()

# ТАРГЕТ 2: AddressSanitizer / UBSan Safety Suite
if(ENABLE_ASAN)
   add_executable(mock_sysfs_asan_test tests/mock_sysfs_test.cpp)
   target_compile_options(mock_sysfs_asan_test PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -g -O1)
   target_link_options(mock_sysfs_asan_test PRIVATE -fsanitize=address,undefined)
   target_link_libraries(mock_sysfs_asan_test PRIVATE pthread)
endif()

# ТАРГЕТ 3: Zero-Allocation Proof
if(ENABLE_ZERO_ALLOC_TEST)
   add_executable(zero_alloc_guard_test tests/zero_alloc_guard_test.cpp)
   target_compile_options(zero_alloc_guard_test PRIVATE -O3 -g0)
   target_link_options(zero_alloc_guard_test PRIVATE
       -Wl,--wrap=malloc
       -Wl,--wrap=calloc
       -Wl,--wrap=realloc
       -Wl,--wrap=free
   )
   target_link_libraries(zero_alloc_guard_test PRIVATE pthread)
endif()
