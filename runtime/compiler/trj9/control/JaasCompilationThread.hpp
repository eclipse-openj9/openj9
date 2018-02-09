#ifndef JAAS_COMPILATION_THREAD_H
#define JAAS_COMPILATION_THREAD_H

#include "control/CompilationThread.hpp"
#include "rpc/J9Client.h"

size_t methodStringLength(J9ROMMethod *);
std::string packROMClass(J9ROMClass *, TR_Memory *);
bool handleServerMessage(JAAS::J9ClientStream *, TR_J9VM *);
TR_MethodMetaData *remoteCompile(J9VMThread *, TR::Compilation *, TR_ResolvedMethod *,
      J9Method *, TR::IlGeneratorMethodDetails &, TR::CompilationInfoPerThreadBase *);
void remoteCompilationEnd(TR::IlGeneratorMethodDetails &details, J9JITConfig *jitConfig,
      TR_FrontEnd *fe, TR_MethodToBeCompiled *entry, TR::Compilation *comp);
void printJaasMsgStats(J9JITConfig *);
void printJaasCHTableStats(J9JITConfig *, TR::CompilationInfo *);

#endif
