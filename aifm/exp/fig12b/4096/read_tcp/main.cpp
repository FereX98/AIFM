extern "C" {
#include <runtime/runtime.h>
}

#include "deref_scope.hpp"
#include "device.hpp"
#include "helpers.hpp"
#include "manager.hpp"
#include "profiler.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>

using namespace far_memory;
using namespace std;

#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))
#define DEFINE_DATA_TYPE(width)                                                \
  struct Data_##width {                                                        \
    uint8_t buf[width];                                                        \
  };                                                                           \
  using Data_t = Data_##width;

DEFINE_DATA_TYPE(4096);

constexpr uint64_t kCacheSize = 256ULL << 20;   // 256 MB.
constexpr uint64_t kFarMemSize = 16ULL << 30;   // 16 GB.
constexpr uint64_t kWorkSetSize = 512ULL << 20; // 512 MB.
constexpr uint64_t kNumEntries = kWorkSetSize / sizeof(Data_t);
constexpr uint32_t kNumGCThreads = 1;
constexpr uint64_t kNumConnections = 600;

unsigned cycles_low_start, cycles_high_start;
unsigned cycles_low_end, cycles_high_end;
//UniquePtr<Data_t> ptrs[kNumEntries];
std::vector<uint64_t> durations;

template <typename T> void print_percentile(int percentile, T *container) {
  auto idx = percentile / 100.0 * container->size();
  cout << percentile << "\t" << (*container)[idx] << endl;
}

#include <numeric>

template <typename T> void print_results(T *container) {
  sort(container->begin(), container->end());
  print_percentile(50, container);
  print_percentile(90, container);
  print_percentile(99.99, container);
  print_percentile(100, container);
  cout << "last one" << "\t" << (*container)[container->size() - 1] << endl;
  auto const count = static_cast<float>(container->size());
  cout << "avg" << "\t" << std::reduce(container->begin(), container->end()) / count << endl;
}

namespace far_memory {
extern bool gc_master_active;
class FarMemTest {
public:
  void test(void *arg) {
    char **argv = (char **)arg;
    std::string ip_addr_port(argv[1]);
    auto raddr = helpers::str_to_netaddr(ip_addr_port);
    std::unique_ptr<FarMemManager> manager =
        std::unique_ptr<FarMemManager>(FarMemManagerFactory::build(
            kCacheSize, kNumGCThreads,
            new TCPDevice(raddr, kNumConnections, kFarMemSize)));

    auto ptrs = manager->allocate_array<Data_t, kNumEntries>();
    //for (uint64_t i = 0; i < kNumEntries; i++) {
    //  ptrs[i] = std::move(manager->allocate_unique_ptr<Data_t>());
    //}
    ptrs.disable_prefetch();
    //auto ptrs2 = manager->allocate_array<Data_t, kNumEntries>();

    for (uint64_t i = 0; i < kNumEntries; i++) {
      DerefScope scope;
      //auto *far_mem_ptr = &ptrs[i];
      //const Data_t *raw_const_ptr = far_mem_ptr->deref(scope);
      //ACCESS_ONCE(*raw_const_ptr);
      auto data = ptrs.at_mut(scope, i);
      ACCESS_ONCE(data);
      //auto data2 = ptrs2.at_mut(scope, i);
      //ACCESS_ONCE(data2);
    }

    for (uint64_t i = 0; i < kNumEntries; i++) {
      //while (ACCESS_ONCE(far_memory::gc_master_active) ||
      //       manager->is_free_cache_low()) {
      //  thread_yield();
      //}
      barrier();
      DerefScope scope;

      //auto data2 = ptrs2.at_mut(scope, i);
      //ACCESS_ONCE(data2);
      //auto *far_mem_ptr = &ptrs[i];
      //Stats::reset_measure_read_object_cycles();
      uint64_t cycles = get_cycles_start();
      {
        //const Data_t *raw_const_ptr = far_mem_ptr->deref(scope);
        //ACCESS_ONCE(*raw_const_ptr);
        auto data = ptrs.at_mut(scope, i);
        ACCESS_ONCE(data);
      }
      //auto read_object_cycles = Stats::get_elapsed_read_object_cycles();
      auto read_object_cycles = get_cycles_end() - cycles;
      if (read_object_cycles) {
        durations.push_back(read_object_cycles);
      }
    }

    print_results(&durations);
    //for (uint64_t i = 0; i < kNumEntries; i++) {
    //  ptrs[i].free();
    //}
  }
} test;

} // namespace far_memory

void do_work(void *arg) { far_memory::test.test(arg); }

int main(int _argc, char *argv[]) {
  int ret;

  if (_argc < 3) {
    std::cerr << "usage: [cfg_file] [ip_addr:port]" << std::endl;
    return -EINVAL;
  }

  char conf_path[strlen(argv[1]) + 1];
  strcpy(conf_path, argv[1]);
  for (int i = 2; i < _argc; i++) {
    argv[i - 1] = argv[i];
  }

  ret = runtime_init(conf_path, do_work, argv);
  if (ret) {
    std::cerr << "failed to start runtime" << std::endl;
    return ret;
  }

  return 0;
}
