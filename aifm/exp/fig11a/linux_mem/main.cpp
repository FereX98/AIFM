
#include "snappy.h"

#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <numa.h>
#include <streambuf>
#include <string>
#include <unistd.h>
#include <chrono>

using namespace std;

constexpr uint32_t kUncompressedFileSize = 1000000000;
constexpr uint32_t kNumUncompressedFiles = 16;
void *buffers[kNumUncompressedFiles - 1];

string read_file_to_string(const string &file_path) {
  ifstream fs(file_path);
  // No need to explicitly close the file as the destructor will do it.
  //auto guard = helpers::finally([&]() { fs.close(); });
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

void compress_files_bench(const string &in_file_path,
                          const string &out_file_path) {
  auto before_string_loading = chrono::steady_clock::now();
  string in_str = read_file_to_string(in_file_path);
  string out_str;
  auto before_buffer_copying = chrono::steady_clock::now();
  for (uint32_t i = 0; i < kNumUncompressedFiles - 1; i++) {
    buffers[i] = numa_alloc_onnode(kUncompressedFileSize, 0);
    if (buffers[i] == nullptr) {
      std::cerr << "failed to allocate buffer " << i << std::endl;
      exit(-1);
    }
    memcpy(buffers[i], in_str.data(), in_str.size());
  }

  auto start = chrono::steady_clock::now();
  for (uint32_t i = 0; i < kNumUncompressedFiles; i++) {
    std::cout << "Compressing file " << i << std::endl;
    if (i == 0) {
      snappy::Compress(in_str.data(), in_str.size(), &out_str);
    } else {
      snappy::Compress((const char *)buffers[i - 1], kUncompressedFileSize,
                       &out_str);
    }
  }
  auto end = chrono::steady_clock::now();
  cout << "Elapsed time in microseconds : "
       << chrono::duration_cast<chrono::microseconds>(end - start).count()
       << " µs" << endl
       << "String loading time in microseconds : "
       << chrono::duration_cast<chrono::microseconds>(before_buffer_copying - before_string_loading).count()
       << " µs" << endl
       << "Buffer copying time in microseconds : "
       << chrono::duration_cast<chrono::microseconds>(start - before_buffer_copying).count()
       << " µs" << endl;

  for (uint32_t i = 0; i < kNumUncompressedFiles - 1; i++) {
    numa_free(buffers[i], kUncompressedFileSize);
  }

  // write_file_to_string(out_file_path, out_str);
}

void do_work(void *arg) {
  compress_files_bench("/mnt/ssd/data/enwik9.uncompressed",
                       "/mnt/ssd/data/enwik9.compressed.tmp");
}

int main(int argc, char *argv[]) {
  /*int ret;

  if (argc < 2) {
    std::cerr << "usage: [cfg_file]" << std::endl;
    return -EINVAL;
  }

  ret = runtime_init(argv[1], do_work, NULL);
  if (ret) {
    std::cerr << "failed to start runtime" << std::endl;
    return ret;
  }*/

  do_work(NULL);

  return 0;
}
