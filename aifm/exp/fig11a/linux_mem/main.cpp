
#include "snappy.h"

#include <chrono>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <numa.h>
#include <streambuf>
#include <string>
#include <unistd.h>
#include <vector>

using namespace std;

struct FileBlock {
  constexpr static uint32_t kSize = 32768;
  uint8_t data[kSize];
};

constexpr uint32_t kUncompressedFileSize = 1000000000;
constexpr uint64_t kUncompressedFileNumBlocks =
    ((kUncompressedFileSize - 1) / FileBlock::kSize) + 1;
constexpr uint32_t kNumUncompressedFiles = 16;
//void *buffers[kNumUncompressedFiles - 1];

alignas(4096) FileBlock file_block;
std::unique_ptr<std::vector<FileBlock>>
    fm_array_ptrs[kNumUncompressedFiles];

template <class F>
static auto finally(F f) noexcept(noexcept(F(std::move(f)))) {
  auto x = [f = std::move(f)](void *) { f(); };
  return std::unique_ptr<void, decltype(x)>(reinterpret_cast<void *>(1),
                                            std::move(x));
}

string read_file_to_string(const string &file_path) {
  ifstream fs(file_path);
  auto guard = finally([&]() { fs.close(); });
  return string((std::istreambuf_iterator<char>(fs)),
                std::istreambuf_iterator<char>());
}

void write_file_to_string(const string &file_path, const string &str) {
  std::ofstream fs(file_path);
  fs << str;
  fs.close();
}

void compress_file(const string &in_file_path, const string &out_file_path) {
  string in_str = read_file_to_string(in_file_path);
  string out_str;
  auto start = chrono::steady_clock::now();
  snappy::Compress(in_str.data(), in_str.size(), &out_str);
  auto end = chrono::steady_clock::now();
  cout << "Elapsed time in microseconds : "
       << chrono::duration_cast<chrono::microseconds>(end - start).count()
       << " µs" << endl;
  write_file_to_string(out_file_path, out_str);
}

void read_files_to_fm_array(const string &in_file_path) {
  int fd = open(in_file_path.c_str(), O_RDONLY | O_DIRECT);
  if (fd == -1) {
    cout << "open file failed" << endl;
    exit(1);
  }
  // Read file and save data into the far-memory array.
  int64_t sum = 0, cur = FileBlock::kSize, tmp;
  while (sum != kUncompressedFileSize) {
    //BUG_ON(cur != FileBlock::kSize);
    cur = 0;
    while (cur < (int64_t)FileBlock::kSize) {
      tmp = read(fd, file_block.data + cur, FileBlock::kSize - cur);
      if (tmp <= 0) {
        break;
      }
      cur += tmp;
    }
    for (uint32_t i = 0; i < kNumUncompressedFiles; i++) {
      (*(fm_array_ptrs[i]))[sum / FileBlock::kSize] = file_block;
    }
    sum += cur;
    /*if ((sum % (1 << 20)) == 0) {
      cerr << "Have read " << sum << " bytes." << endl;
    }*/
  }
  if (sum != kUncompressedFileSize) {
    cout << "size error" << endl;
    exit(1);
  }

  close(fd);
}

void compress_files_bench(const string &in_file_path,
                          const string &out_file_path) {
  //string in_str = read_file_to_string(in_file_path);
  string out_str;
  auto pre_read = chrono::steady_clock::now();
  read_files_to_fm_array(in_file_path);

  //for (uint32_t i = 0; i < kNumUncompressedFiles - 1; i++) {
  //  buffers[i] = numa_alloc_onnode(kUncompressedFileSize, 1);
  //  if (buffers[i] == nullptr) {
  //    cout << "numa_alloc_onnode failed" << endl;
  //    exit(1);
  //  }
  //  memcpy(buffers[i], in_str.data(), in_str.size());
  //}

  auto start = chrono::steady_clock::now();
  cout << "Load time in microseconds : "
       << chrono::duration_cast<chrono::microseconds>(start - pre_read).count()
       << " µs" << endl;
  for (uint32_t i = 0; i < kNumUncompressedFiles; i++) {
    std::cout << "Compressing file " << i << std::endl;
    //if (i == 0) {
    //  snappy::Compress(in_str.data(), in_str.size(), &out_str);
    //} else {
    //  snappy::Compress((const char *)buffers[i - 1], kUncompressedFileSize,
    //                   &out_str);
    //}
    snappy::Compress((const char *)(&(*(fm_array_ptrs[i]))[0]), kUncompressedFileSize, &out_str);
  }
  auto end = chrono::steady_clock::now();
  cout << "Elapsed time in microseconds : "
       << chrono::duration_cast<chrono::microseconds>(end - start).count()
       << " µs" << endl;

  //for (uint32_t i = 0; i < kNumUncompressedFiles - 1; i++) {
  //  numa_free(buffers[i], kUncompressedFileSize);
  //}

  // write_file_to_string(out_file_path, out_str);
}

void do_work(void) {
  for (uint32_t i = 0; i < kNumUncompressedFiles; i++) {
    fm_array_ptrs[i].reset(new std::vector<FileBlock>);
    fm_array_ptrs[i]->resize(kUncompressedFileNumBlocks);
  }
  compress_files_bench("/mnt/ssd/data/enwik9.uncompressed",
                       "/mnt/ssd/data/enwik9.compressed.tmp");
}

int main(int argc, char *argv[]) {
  do_work();

  return 0;
}
