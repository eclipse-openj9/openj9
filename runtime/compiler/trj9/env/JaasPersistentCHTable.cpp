#include "JaasPersistentCHTable.hpp"
#include "compile/Compilation.hpp"
#include "control/CompilationRuntime.hpp"
#include "J9Server.h"

TR_JaasPersistentCHTable::TR_JaasPersistentCHTable(TR_PersistentMemory *trMemory)
   : TR_PersistentCHTable(trMemory)
   {
   }

TR_PersistentClassInfo *
TR_JaasPersistentCHTable::findClassInfo(TR_OpaqueClassBlock * classId)
   {
   return findClassInfoAfterLocking(classId, TR::comp());
   }


TR_PersistentClassInfo *
TR_JaasPersistentCHTable::findClassInfoAfterLocking(
      TR_OpaqueClassBlock *classId,
      TR::Compilation *comp,
      bool returnClassInfoForAOT)
   {
   if (comp->fej9()->isAOT_DEPRECATED_DO_NOT_USE() && !returnClassInfoForAOT) // for AOT do not use the class hierarchy
      return NULL;

   if (comp->getOption(TR_DisableCHOpts))
      return NULL;

   auto stream = TR::CompilationInfo::getStream();
   stream->write(JAAS::J9ServerMessageType::CHTable_findClassInfo, classId);
   return std::get<0>(stream->read<TR_PersistentClassInfo*>());
   }
