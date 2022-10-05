#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <cassert>

#ifdef NDEBUG
#undef NDEBUG
#endif

#define NDEBUG

//#define RANDOM_IN_BATCH
#define RANDOM_BATCH_SELECTION

using namespace std;

constexpr static uint64_t kCacheSize = (128ULL << 20);
constexpr static uint64_t kFarMemSize = (8ULL << 30);
constexpr static uint32_t kNumGCThreads = 12;
constexpr static uint32_t kNumConnections = 300;

constexpr static uint32_t kDataSize = 128;
constexpr static uint32_t kBatchSize = 32;
constexpr static uint64_t kArraySize = 4ull << 30;
constexpr static uint64_t kNumElements = kArraySize / kDataSize;
constexpr static uint64_t kNumBatches = kNumElements / kBatchSize;
constexpr static uint32_t kNumPermutations = 10;
constexpr static uint32_t kCPUFreqMHz = 1200;
constexpr static uint32_t kDelayInNs = 1200;

std::unique_ptr<std::mt19937> generator;
std::unique_ptr<std::random_device> rdp;

void random_string(char *data, uint32_t len) {
  std::uniform_int_distribution<int> distribution('a', 'z' + 1);
  for (uint32_t i = 0; i < len; i++) {
    data[i] = char(distribution(*generator));
  }
}

template<uint32_t bound>
uint32_t random_uint_bound_to() {
  std::uniform_int_distribution<uint32_t> distribution(0, bound - 1);
  uint32_t return_value = distribution(*generator);
  assert(return_value < bound);
  return return_value;
}

class DataClass {
public:
  char data[kDataSize];
  DataClass() {
    //random_string(data, kDataSize);
  }
};

std::array<unique_ptr<DataClass>, kNumElements> data_array;

uint32_t permutations[kNumPermutations][kBatchSize];

void generate_random_permutations(uint32_t batch_size, uint32_t num_permutations) {
  for (uint32_t i = 0; i < num_permutations; ++i) {
    for (uint32_t j = 0; j < batch_size; ++j) {
      permutations[i][j] = j;
    }
    std::random_shuffle(std::begin(permutations[i]), std::end(permutations[i]));
  }
}

static inline void my_delay_cycles(int32_t cycles) {
  for (int32_t i = 0; i < cycles; i++) {
    asm("nop");
  }
}

void work_on_data(DataClass &instance) {
  int32_t entire_delay = kDelayInNs / 1000.0 * kCPUFreqMHz;
  int32_t each_delay = entire_delay / 5;
  instance.data[42] = 'a';
  my_delay_cycles(each_delay);
  instance.data[43] = 'n';
  my_delay_cycles(each_delay);
  instance.data[44] = 's';
  my_delay_cycles(each_delay);
  instance.data[45] = 'w';
  my_delay_cycles(each_delay);
  instance.data[46] = 'e';
  my_delay_cycles(each_delay);
  instance.data[47] = 'r';
}

void do_work() {

  printf("array creation\n");

  for (uint64_t i = 0; i < kNumElements; ++i) {
    data_array[i].reset(new DataClass());
  }
  printf("array created\n");

  
#ifdef RANDOM_BATCH_SELECTION
  printf("random batches\n");
  for (uint64_t i = 0; i < kNumBatches; ++i) {
    uint32_t batch_index = random_uint_bound_to<kNumBatches>();
    uint64_t base = batch_index * kBatchSize;
#else // RANDOM_BATCH_SELECTION
  printf("streaming batches\n");
  for (uint64_t i = 0; i < kNumBatches; ++i) {
    uint64_t base = i * kBatchSize;
#endif // RANDOM_BATCH_SELECTION
#ifdef RANDOM_IN_BATCH
    uint32_t perm_to_use_index = random_uint_bound_to<kNumPermutations>();
    for (uint64_t j = 0; j < kBatchSize; ++j) {
      assert(base + permutations[perm_to_use_index][j] < kNumElements);
      work_on_data(*data_array[base + permutations[perm_to_use_index][j]]);
    }
#else // RANDOM_IN_BATCH
    for (uint64_t j = 0; j < kBatchSize; ++j) {
      assert(base + j < kNumElements);
      work_on_data(*data_array[base + j]);
    }
#endif // RANDOM_IN_BATCH
  }
  printf("streaming batches ends\n");
  printf("randomly accessing batches\n");

  cout << "Passed" << endl;
  return;

  cout << "Failed" << endl;
}

int main(int _argc, char *argv[]) {

  //assert(false && "add #define NDEBUG");

  std::random_device rd;
  generator.reset(new std::mt19937(rd()));

  generate_random_permutations(kBatchSize, kNumPermutations);

  do_work();
  return 0;
}
