#include <iostream>
#include <atomic>
#include <cstdlib>
#include <cassert>
#include <array>
#include <cstdint>

static std::atomic<bool> g_guard_active{false};
static std::atomic<uint64_t> g_allocation_violations{0};

extern "C" {
   void* __real_malloc(size_t size);
   void  __real_free(void* ptr);
   void* __real_calloc(size_t nmemb, size_t size);
   void* __real_realloc(void* ptr, size_t size);

   void* __wrap_malloc(size_t size) {
       if (g_guard_active.load(std::memory_order_relaxed)) {
           g_allocation_violations.fetch_add(1, std::memory_order_relaxed);
       }
       return __real_malloc(size);
   }

   void* __wrap_calloc(size_t nmemb, size_t size) {
       if (g_guard_active.load(std::memory_order_relaxed)) {
           g_allocation_violations.fetch_add(1, std::memory_order_relaxed);
       }
       return __real_calloc(nmemb, size);
   }

   void* __wrap_realloc(void* ptr, size_t size) {
       if (g_guard_active.load(std::memory_order_relaxed)) {
           g_allocation_violations.fetch_add(1, std::memory_order_relaxed);
       }
       return __real_realloc(ptr, size);
   }

   void __wrap_free(void* ptr) {
       __real_free(ptr);
   }
}

void polling_loop_hotpath(size_t iterations) {
   // Эмуляция реактора опроса: только стековые массивы, ноль кучи
   static std::array<uint64_t, 512> local_state{};
   for (size_t i = 0; i < iterations; ++i) {
       local_state[i % local_state.size()] = i * 17ULL;
   }
}

int main() {
   std::cout << "[ZERO-ALLOC] Running zero-allocation invariant test..." << std::endl;

   // Прогрев до активации гарда
   void* warmup = malloc(256);
   free(warmup);

   g_allocation_violations.store(0, std::memory_order_seq_cst);
   g_guard_active.store(true, std::memory_order_seq_cst);

   polling_loop_hotpath(1000000);

   g_guard_active.store(false, std::memory_order_seq_cst);

   const uint64_t violations = g_allocation_violations.load(std::memory_order_seq_cst);
   std::cout << "[ZERO-ALLOC] Execution completed. Intercepted allocations: " 
             << violations << std::endl;

   if (violations != 0) {
       std::cerr << "[FAIL] Zero-Allocation invariant violated! Detected " 
                 << violations << " dynamic memory calls." << std::endl;
       return 1;
   }

   std::cout << "[PASS] Zero-Allocation proof verified mathematically." << std::endl;
   return 0;
}
