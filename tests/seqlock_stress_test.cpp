#include <iostream>
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
#include <cassert>
#include <cstring>
#include <cstdint>

struct GpuTelemetryFrame {
   uint32_t power_draw_mw;
   uint32_t temperature_c;
   uint64_t vram_used_bytes;
   uint64_t vram_total_bytes;
   uint32_t gfx_activity_pct;
   uint32_t compute_queue_depth;
};

template <typename T>
class SeqlockChannel {
   static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
   static_assert(sizeof(T) % sizeof(uint64_t) == 0, "T size must be aligned to 64-bit boundaries");

public:
   SeqlockChannel() : sequence_(0) {
       std::memset(storage_, 0, sizeof(storage_));
   }

   void write(const T& source) noexcept {
       const uint64_t seq = sequence_.load(std::memory_order_relaxed);
       // Начало записи: нечетный счетчик с release-семантикой
       sequence_.store(seq + 1, std::memory_order_release);

       const auto* src_words = reinterpret_cast<const std::atomic<uint64_t>*>(&source);
       auto* dst_words = reinterpret_cast<std::atomic<uint64_t>*>(storage_);
       constexpr size_t words_count = sizeof(T) / sizeof(uint64_t);

       for (size_t i = 0; i < words_count; ++i) {
           dst_words[i].store(src_words[i].load(std::memory_order_relaxed), std::memory_order_relaxed);
       }

       // Завершение записи: четный счетчик с release-семантикой
       sequence_.store(seq + 2, std::memory_order_release);
   }

   bool try_read(T& destination) const noexcept {
       const uint64_t seq_before = sequence_.load(std::memory_order_acquire);
       if ((seq_before & 1ULL) != 0) {
           return false;
       }

       auto* dst_words = reinterpret_cast<std::atomic<uint64_t>*>(&destination);
       const auto* src_words = reinterpret_cast<const std::atomic<uint64_t>*>(storage_);
       constexpr size_t words_count = sizeof(T) / sizeof(uint64_t);

       for (size_t i = 0; i < words_count; ++i) {
           dst_words[i].store(src_words[i].load(std::memory_order_relaxed), std::memory_order_relaxed);
       }

       const uint64_t seq_after = sequence_.load(std::memory_order_acquire);
       return (seq_before == seq_after);
   }

   T read_retry() const noexcept {
       T result;
       while (!try_read(result)) {
           #if defined(__x86_64__) || defined(_M_X64)
           __builtin_ia32_pause();
           #endif
       }
       return result;
   }

private:
   alignas(64) std::atomic<uint64_t> sequence_;
   alignas(64) uint8_t storage_[sizeof(T)];
};

int main() {
   std::cout << "[TSAN-AUDIT] Running SeqlockChannel multithreaded validation..." << std::endl;

   SeqlockChannel<GpuTelemetryFrame> channel;
   std::atomic<bool> terminate_flag{false};
   std::atomic<uint64_t> total_reads{0};
   constexpr uint32_t kWriteIterations = 100000;

   std::thread writer([&]() {
       for (uint32_t i = 1; i <= kWriteIterations; ++i) {
           GpuTelemetryFrame frame{
               static_cast<uint32_t>(100000 + (i % 20000)),
               static_cast<uint32_t>(50 + (i % 30)),
               1024ULL * 1024 * i,
               16ULL * 1024 * 1024 * 1024,
               static_cast<uint32_t>(i % 100),
               static_cast<uint32_t>(i % 8)
           };
           channel.write(frame);
       }
       terminate_flag.store(true, std::memory_order_release);
   });

   auto reader_worker = [&](int id) {
       (void)id;
       uint64_t local_count = 0;
       while (!terminate_flag.load(std::memory_order_acquire)) {
           GpuTelemetryFrame frame = channel.read_retry();
           assert(frame.vram_total_bytes == 16ULL * 1024 * 1024 * 1024);
           assert(frame.gfx_activity_pct < 100);
           local_count++;
       }
       total_reads.fetch_add(local_count, std::memory_order_relaxed);
   };

   std::thread reader1(reader_worker, 1);
   std::thread reader2(reader_worker, 2);

   writer.join();
   reader1.join();
   reader2.join();

   std::cout << "[TSAN-AUDIT] Clean execution verified. Successful reads: " 
             << total_reads.load() << std::endl;
   return 0;
}
