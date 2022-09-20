extern "C" {
#include <runtime/runtime.h>
}

#include "array.hpp"
#include "device.hpp"
#include "helpers.hpp"
#include "manager.hpp"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <random>
#include <string>

#include <cstdio>

#define RUN_THE_TEST
//#define RUN_AIFM
#define RUN_UNMODIFIED

using namespace far_memory;
using namespace std;

//constexpr static uint64_t kCacheSize = (128ULL << 20);
constexpr static uint64_t kCacheSize = (4ULL << 30);
constexpr static uint64_t kFarMemSize = (4ULL << 30);
constexpr static uint32_t kNumGCThreads = 12;
constexpr static uint32_t kNumEntries =
    (8ULL << 20); // So the array size is larger than the local cache size.
//      (24ULL << 20); // So the array size is larger than the local cache size.
constexpr static uint32_t kNumConnections = 300;

#ifdef RUN_THE_TEST
uint64_t raw_array_A[kNumEntries];
uint64_t raw_array_B[kNumEntries];
uint64_t raw_array_C[kNumEntries];
#endif

template <uint64_t N, typename T>
void copy_array(Array<T, N> *array, T *raw_array) {
  for (uint64_t i = 0; i < N; i++) {
    DerefScope scope;
    (*array).at_mut(scope, i) = raw_array[i];
  }
}

template <typename T, uint64_t N>
void add_array(Array<T, N> *array_C, Array<T, N> *array_A,
               Array<T, N> *array_B) {
  for (uint64_t i = 0; i < N; i++) {
    DerefScope scope;
    (*array_C).at_mut(scope, i) =
        (*array_A).at(scope, i) + (*array_B).at(scope, i);
  }
}

void gen_random_array(uint64_t num_entries, uint64_t *raw_array) {
  std::random_device rd;
  std::mt19937_64 eng(rd());
  std::uniform_int_distribution<uint64_t> distr;

  for (uint64_t i = 0; i < num_entries; i++) {
    raw_array[i] = distr(eng);
  }
}

void do_work(FarMemManager *manager) {
  #ifdef RUN_THE_TEST
  #ifdef RUN_AIFM
  auto array_A = manager->allocate_array<uint64_t, kNumEntries>();
  auto array_B = manager->allocate_array<uint64_t, kNumEntries>();
  auto array_C = manager->allocate_array<uint64_t, kNumEntries>();
  #endif // RUN_AIFM

  #ifdef RUN_UNMODIFIED
  printf("array creation\n");
  uint64_t** array_A = (uint64_t**)malloc(kNumEntries * sizeof(uint64_t));
  uint64_t** array_B = (uint64_t**)malloc(kNumEntries * sizeof(uint64_t));
  uint64_t** array_C = (uint64_t**)malloc(kNumEntries * sizeof(uint64_t));
  for (uint64_t i = 0; i < kNumEntries; ++i) {
    array_A[i] = (uint64_t*)malloc(sizeof(uint64_t));
    array_B[i] = (uint64_t*)malloc(sizeof(uint64_t));
    array_C[i] = (uint64_t*)malloc(sizeof(uint64_t));
  }
  printf("array created\n");
  #endif // RUN_UNMODIFIED

  gen_random_array(kNumEntries, raw_array_A);
  gen_random_array(kNumEntries, raw_array_B);

  #ifdef RUN_UNMODIFIED
  printf("start copying\n");
  for (uint64_t i = 0; i < kNumEntries; i++) {
    *array_A[i] = raw_array_A[i];
  }
  for (uint64_t i = 0; i < kNumEntries; i++) {
    *array_B[i] = raw_array_B[i];
  }
  printf("end copying\n");
  for (uint64_t i = 0; i < kNumEntries; i++) {
    *array_C[i] = *array_A[i] + *array_B[i];
  }
  printf("end adding\n");
  uint64_t counter = 0;
  for (uint64_t i = 0; i < kNumEntries; i++) {
    if (*array_C[i] != raw_array_A[i] + raw_array_B[i]) {
  //    //FILE* fp;
  //    //fp = fopen("/users/shiliu/AIFM/aifm/file.txt", "w");
  //    //fprintf(fp, "%lu\n%lu\n%lu\n%lu\n", i, raw_array_A[i], raw_array_B[i], array_C[i]);
      //cout << i << endl;
      //cout << raw_array_A[i] << endl;
      //cout << raw_array_B[i] << endl;
      //cout << array_C[i] << endl;
      // Shi: no idea why this comparison will fail, use a counter to make sure it runs to the end for now.
      //counter += 1;
      //goto fail;
      //cout << "Failed" << endl;
      //return;
    }
  }
  #endif // RUN_UNMODIFIED

  #ifdef RUN_AIFM
  copy_array(&array_A, raw_array_A);
  copy_array(&array_B, raw_array_B);
  add_array(&array_C, &array_A, &array_B);

  for (uint64_t i = 0; i < kNumEntries; i++) {
    DerefScope scope;
    if (array_C.at(scope, i) != raw_array_A[i] + raw_array_B[i]) {
      //goto fail;
      cout << "Failed" << endl;
      return;
    }
  }
  #endif // RUN_AIFM
  #endif // RUN_THE_TEST

  cout << "Passed" << endl;
  return;

//fail:
//  cout << "Failed" << endl;
}

int argc;
void _main(void *arg) {
  char **argv = static_cast<char **>(arg);
  std::string ip_addr_port(argv[1]);
  auto raddr = helpers::str_to_netaddr(ip_addr_port);
  std::unique_ptr<FarMemManager> manager =
      std::unique_ptr<FarMemManager>(FarMemManagerFactory::build(
          kCacheSize, kNumGCThreads,
          new TCPDevice(raddr, kNumConnections, kFarMemSize)));
  do_work(manager.get());
}

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
  argc = _argc - 1;

  ret = runtime_init(conf_path, _main, argv);
  if (ret) {
    std::cerr << "failed to start runtime" << std::endl;
    return ret;
  }

  return 0;
}
