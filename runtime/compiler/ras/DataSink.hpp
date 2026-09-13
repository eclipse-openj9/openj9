
#ifndef DATASINK_INCL
#define DATASINK_INCL

#include <string>
#include <memory>
#include "Debug.hpp"

class DataSink {
  public:
    virtual void write(uint8_t byte) = 0;
    virtual void write(std::string && string) = 0;
    static DataSink * getSink();
};

class FileSink : DataSink {
  public:
    FileSink(std::string && filename);
    ~FileSink();
    virtual void write(uint8_t byte) override;
    virtual void write(std::string && string) override;
  private:
    ::FILE * file;
};

#endif
