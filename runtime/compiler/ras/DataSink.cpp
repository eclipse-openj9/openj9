#include "DataSink.hpp"
#include <cstdint>
#include <cstdio>
#include <memory>

FileSink::FileSink(std::string && filename) {
  file = fopen(filename.c_str(), "w");
}

FileSink::~FileSink() {
  write("</group>");
  write("</graphDocument>");
  fflush(file);
  fclose(file);
}

void FileSink::write(uint8_t byte) {
  fwrite(&byte, 1, sizeof(byte), file);
}

void FileSink::write(std::string && s) {
  auto cstr = s.c_str();
  auto len = strlen(cstr);
  fwrite(cstr, sizeof(char), len, file);
}
