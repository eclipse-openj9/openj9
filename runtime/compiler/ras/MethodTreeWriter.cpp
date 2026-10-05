#include "MethodTreeWriter.hpp"
#include "XMLMethodTreeWriter.hpp"
#include "compiler/control/OMROptions.hpp"
#include "compile/Compilation.hpp"
#include "compile/Compilation_inlines.hpp"
#include <atomic>

std::atomic<int32_t> MethodTreeWriter::nextAvailableCompilationId(0);

MethodTreeWriter * MethodTreeWriter::getMethodTreeWriter(int32_t id, TR_ResolvedMethod * method, TR::Options & options) {
  if (options.getOption(TR_DumpMethodTrees)) {
    return new XMLMethodTreeWriter(id, method, options);
  }
  return new DefaultMethodTreeWriter();
}
