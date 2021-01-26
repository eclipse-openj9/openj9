/*******************************************************************************
 * Copyright (c) 2019, 2021 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "control/JITServerHelpers.hpp"

#include "control/CompilationRuntime.hpp"
#include "control/JITServerCompilationThread.hpp"
#include "control/MethodToBeCompiled.hpp"
#include "env/StackMemoryRegion.hpp"
#include "infra/CriticalSection.hpp"
#include "infra/Statistics.hpp"
#include "net/CommunicationStream.hpp"
#include "OMR/Bytes.hpp"// for OMR::alignNoCheck()
#include "romclasswalk.h"
#include "util_api.h"// for allSlotsInROMClassDo()


uint32_t     JITServerHelpers::serverMsgTypeCount[] = {};
uint64_t     JITServerHelpers::_waitTimeMs = 1000;
bool         JITServerHelpers::_serverAvailable = true;
uint64_t     JITServerHelpers::_nextConnectionRetryTime = 0;
TR::Monitor *JITServerHelpers::_clientStreamMonitor = NULL;


// To ensure that the length fields in UTF8 strings appended at the end of the
// packed ROMClass are properly aligned, we must pad the strings accordingly.
// This function returns the total size of a UTF8 string with the padding.
static size_t
getUTF8Size(const J9UTF8 *str)
   {
   return OMR::alignNoCheck(J9UTF8_TOTAL_SIZE(str), sizeof(*str));
   }

// Copies a UTF8 string and returns its total size including the padding.
// src doesn't have to be padded, dst is padded.
static size_t
copyUTF8(J9UTF8 *dst, const J9UTF8 *src)
   {
   size_t size = J9UTF8_TOTAL_SIZE(src);
   memcpy(dst, src, size);
   static_assert(sizeof(*src) == 2, "UTF8 header is not 2 bytes large");
   // If the length is not aligned, pad the destination string with a zero
   if (!OMR::alignedNoCheck(size, sizeof(*src)))
      dst->data[src->length] = '\0';
   return getUTF8Size(src);
   }

// State maintained while iterating over UTF8 strings in a ROMClass
struct ROMClassPackContext
   {
   ROMClassPackContext(TR_Memory *trMemory, size_t origSize) :
      _origSize(origSize), _callback(NULL), _stringsSize(0),
      _utf8SectionStart((const uint8_t *)-1), _utf8SectionEnd(NULL), _utf8SectionSize(0),
      _strToOffsetMap(decltype(_strToOffsetMap)::allocator_type(trMemory->currentStackRegion())),
      _packedRomClass(NULL), _cursor(NULL) {}

   bool isInline(const void *address, const J9ROMClass *romClass)
      {
      return (address >= romClass) && (address < (uint8_t *)romClass + _origSize);
      }

   typedef void (*Callback)(const J9ROMClass *, const J9SRP *, const char *, ROMClassPackContext &);

   const size_t _origSize;
   Callback _callback;
   size_t _stringsSize;
   const uint8_t *_utf8SectionStart;
   const uint8_t *_utf8SectionEnd;// only needed for assertions
   size_t _utf8SectionSize;// only needed for assertions
   // Maps original strings to their offsets from UTF8 section start in the packed ROMClass
   // Offset value -1 signifies that the string is skipped
   UnorderedMap<const J9UTF8 *, size_t> _strToOffsetMap;
   J9ROMClass *_packedRomClass;
   uint8_t *_cursor;
   };

static bool
shouldSkipSlot(const char *slotName)
   {
   // Skip variable names and signatures in method debug info; only their slot names have prefix "variable"
   static const char prefix[] = "variable";
   return strncmp(slotName, prefix, sizeof(prefix) - 1) == 0;
   }

// Updates size info and maps original string to its future location in the packed ROMClass
static void
sizeInfoCallback(const J9ROMClass *romClass, const J9SRP *origSrp, const char *slotName, ROMClassPackContext &ctx)
   {
   // Skip SRPs stored outside of the ROMClass bounds such as the ones in out-of-line
   // method debug info, and the ones that point to strings not used by the JIT.
   bool skip = !ctx.isInline(origSrp, romClass) || shouldSkipSlot(slotName);
   auto str = NNSRP_PTR_GET(origSrp, const J9UTF8 *);
   auto it = ctx._strToOffsetMap.find(str);
   if (it != ctx._strToOffsetMap.end())// duplicate - already visited
      {
      if (!skip && (it->second == (size_t)-1))
         {
         // Previously visited SRPs to this string were skipped, but this one isn't
         it->second = ctx._stringsSize;
         ctx._stringsSize += getUTF8Size(str);
         }
      return;
      }

   ctx._strToOffsetMap.insert(it, { str, skip ? (size_t)-1 : ctx._stringsSize });
   size_t size = getUTF8Size(str);
   ctx._stringsSize += skip ? 0 : size;

   if (ctx.isInline(str, romClass))
      {
      ctx._utf8SectionStart = std::min(ctx._utf8SectionStart, (const uint8_t *)str);
      ctx._utf8SectionEnd = std::max(ctx._utf8SectionEnd, (const uint8_t *)str + size);
      ctx._utf8SectionSize += size;
      }
   }

// Copies original string into its location in the packed ROMClass and updates the SRP to it
static void
packCallback(const J9ROMClass *romClass, const J9SRP *origSrp, const char *slotName, ROMClassPackContext &ctx)
   {
   // Skip SRPs stored outside of the ROMClass bounds such as the ones in out-of-line method debug info
   if (!ctx.isInline(origSrp, romClass))
      return;

   auto str = NNSRP_PTR_GET(origSrp, const J9UTF8 *);
   auto srp = (J9SRP *)((uint8_t *)ctx._packedRomClass + ((uint8_t *)origSrp - (uint8_t *)romClass));

   // Zero out skipped string SRPs
   if (shouldSkipSlot(slotName))
      {
      TR_ASSERT(ctx._strToOffsetMap.find(str) != ctx._strToOffsetMap.end(),
                "UTF8 slot %s not visited in 1st pass", slotName);
      *srp = 0;
      return;
      }

   auto it = ctx._strToOffsetMap.find(str);
   TR_ASSERT(it != ctx._strToOffsetMap.end(), "UTF8 slot %s not visited in 1st pass", slotName);
   auto dst = (uint8_t *)ctx._packedRomClass + (ctx._utf8SectionStart - (uint8_t *)romClass) + it->second;

   NNSRP_PTR_SET(srp, dst);
   if (dst == ctx._cursor)
      ctx._cursor += copyUTF8((J9UTF8 *)dst, str);
   else
      TR_ASSERT((dst < ctx._cursor) && (memcmp(dst, str, J9UTF8_TOTAL_SIZE(str)) == 0), "Must be already copied");
   }

static void
utf8SlotCallback(const J9ROMClass *romClass, const J9SRP *srp, const char *slotName, void *userData)
   {
   auto &ctx = *(ROMClassPackContext *)userData;
   if (*srp)
      ctx._callback(romClass, srp, slotName, ctx);
   }

// Invoked for each slot in a ROMClass. Calls ctx._callback for all non-null SRPs to UTF8 strings.
static void
slotCallback(J9ROMClass *romClass, uint32_t slotType, void *slotPtr, const char *slotName, void *userData)
   {
   switch (slotType)
      {
      case J9ROM_UTF8:
         utf8SlotCallback(romClass, (const J9SRP *)slotPtr, slotName, userData);
         break;

      case J9ROM_NAS:
         if (auto nas = SRP_PTR_GET(slotPtr, const J9ROMNameAndSignature *))
            {
            utf8SlotCallback(romClass, &nas->name, slotName, userData);
            utf8SlotCallback(romClass, &nas->signature, slotName, userData);
            }
         break;
      }
   }

static bool
isArrayROMClass(const J9ROMClass *romClass)
   {
   if (!J9ROMCLASS_IS_ARRAY(romClass))
      return false;

   auto name = J9ROMCLASS_CLASSNAME(romClass);
   TR_ASSERT((name->length == 2) && (name->data[0] == '['),
             "Unexpected array ROMClass name: %.*s", name->length, name->data);
   return true;
   }

// Array ROMClasses have a different layout (see runtime/vm/romclasses.c):
// they all share the same SRP array of interfaces, which breaks the generic
// packing implementation. Instead, we manually pack all the data stored
// outside of the ROMClass header: class name, superclass name, interfaces.
// This function returns the total size of the packed array ROMClass.
static size_t
getArrayROMClassPackedSize(const J9ROMClass *romClass)
   {
   size_t totalSize = sizeof(*romClass);
   totalSize += getUTF8Size(J9ROMCLASS_CLASSNAME(romClass));
   totalSize += getUTF8Size(J9ROMCLASS_SUPERCLASSNAME(romClass));

   totalSize += romClass->interfaceCount * sizeof(J9SRP);
   for (size_t i = 0; i < romClass->interfaceCount; ++i)
      {
      auto name = NNSRP_GET(J9ROMCLASS_INTERFACES(romClass)[i], const J9UTF8 *);
      totalSize += getUTF8Size(name);
      }

   return OMR::alignNoCheck(totalSize, sizeof(uint64_t));
   }

static void
packUTF8(const J9UTF8 *str, J9SRP &srp, ROMClassPackContext &ctx)
   {
   NNSRP_SET(srp, ctx._cursor);
   ctx._cursor += copyUTF8((J9UTF8 *)ctx._cursor, str);
   }

// Packs the data stored outside of the array ROMClass header
static void
packArrayROMClassData(const J9ROMClass *romClass, ROMClassPackContext &ctx)
   {
   NNSRP_SET(ctx._packedRomClass->interfaces, ctx._cursor);
   ctx._cursor += ctx._packedRomClass->interfaceCount * sizeof(J9SRP);

   packUTF8(J9ROMCLASS_CLASSNAME(romClass), ctx._packedRomClass->className, ctx);
   packUTF8(J9ROMCLASS_SUPERCLASSNAME(romClass), ctx._packedRomClass->superclassName, ctx);

   for (size_t i = 0; i < romClass->interfaceCount; ++i)
      {
      auto name = NNSRP_GET(J9ROMCLASS_INTERFACES(romClass)[i], const J9UTF8 *);
      packUTF8(name, J9ROMCLASS_INTERFACES(ctx._packedRomClass)[i], ctx);
      }
   }

// Packs a ROMClass into a std::string to be transferred to the server.
// UTF8 strings that a ROMClass refers to can be interned and stored outside of
// the ROMClass body. This function puts all the strings (including interned ones)
// at the end of the cloned ROMClass in deterministic order and updates the SRPs
// to them. It also removes some of the data that is not used by the JIT compiler.
//
// Note that the strings that were stored inside of the original ROMClass body
// do not keep their offsets in the serialized ROMClass (in the general case).
// The order in which the strings are serialized is determined by the ROMClass
// walk, not by their original locations.
//
// Packing involves 2 passes over all the strings that a ROMClass refers to:
// 1. Compute total size and map original strings to their locations in the packed ROMClass.
// 2. Copy each original string to its location in the packed ROMClass.
//
// This implementation makes (and checks at runtime) the following assumptions:
// - Intermediate class data is either stored at the end of the ROMClass, or points
//   to the ROMClass itself, or is interned (i.e. points outside of the ROMClass).
// - All non-interned strings are stored in a single contiguous range (the UTF8 section)
//   located at the end of the ROMClass (can only be followed by intermediate class data).
// - ROMClass walk visits all the strings that the ROMClass references.
//
static std::string
packROMClass(J9ROMClass *romClass, TR_Memory *trMemory)
   {
   auto name = J9ROMCLASS_CLASSNAME(romClass);
   // Primitive ROMClasses have different layout (see runtime/vm/romclasses.c): the last
   // ROMClass includes all the others' UTF8 name strings in its romSize, which breaks the
   // generic packing implementation. Pretend that its romSize only includes the header.
   size_t origRomSize = J9ROMCLASS_IS_PRIMITIVE_TYPE(romClass) ? sizeof(*romClass) : romClass->romSize;
   size_t totalSize = origRomSize;

   // Remove intermediate class data (not used by JIT)
   uint8_t *icData = J9ROMCLASS_INTERMEDIATECLASSDATA(romClass);
   if (JITServerHelpers::isAddressInROMClass(icData, romClass) && (icData != (uint8_t *)romClass))
      {
      TR_ASSERT_FATAL(icData + romClass->intermediateClassDataLength == (uint8_t *)romClass + romClass->romSize,
                      "Intermediate class data not stored at the end of ROMClass %.*s", name->length, name->data);
      totalSize -= romClass->intermediateClassDataLength;
      }

   // All allocated memory is only used in this function
   TR::StackMemoryRegion stackMemoryRegion(*trMemory);
   ROMClassPackContext ctx(trMemory, origRomSize);

   size_t copySize = 0;
   if (isArrayROMClass(romClass))
      {
      copySize = sizeof(*romClass);
      totalSize = getArrayROMClassPackedSize(romClass);
      }
   else
      {
      // 1st pass: iterate all strings in the ROMClass to compute its total size (including
      // interned strings) and map the strings to their locations in the packed ROMClass
      ctx._callback = sizeInfoCallback;
      allSlotsInROMClassDo(romClass, slotCallback, NULL, NULL, &ctx);
      // Handle the case when all strings are interned
      auto classEnd = (const uint8_t *)romClass + totalSize;
      ctx._utf8SectionStart = std::min(ctx._utf8SectionStart, classEnd);

      auto end = ctx._utf8SectionEnd ? ctx._utf8SectionEnd : classEnd;
      TR_ASSERT_FATAL(ctx._utf8SectionSize == end - ctx._utf8SectionStart,
                      "Missed strings in ROMClass %.*s UTF8 section: %zu != %zu",
                      name->length, name->data, ctx._utf8SectionSize, end - ctx._utf8SectionStart);
      end = (const uint8_t *)OMR::alignNoCheck((uintptr_t)end, sizeof(uint64_t));
      TR_ASSERT_FATAL(end == classEnd, "UTF8 section not stored at the end of ROMClass %.*s: %p != %p",
                      name->length, name->data, end, classEnd);

      copySize = ctx._utf8SectionStart - (const uint8_t *)romClass;
      totalSize = OMR::alignNoCheck(copySize + ctx._stringsSize, sizeof(uint64_t));
      }

   ctx._packedRomClass = (J9ROMClass *)trMemory->allocateStackMemory(totalSize);
   if (!ctx._packedRomClass)
      throw std::bad_alloc();
   memcpy(ctx._packedRomClass, romClass, copySize);
   ctx._packedRomClass->romSize = totalSize;
   ctx._cursor = (uint8_t *)ctx._packedRomClass + copySize;

   // Zero out SRP to intermediate class data
   ctx._packedRomClass->intermediateClassData = 0;
   ctx._packedRomClass->intermediateClassDataLength = 0;

   // Zero out SRPs to out-of-line method debug info
   J9ROMMethod *romMethod = J9ROMCLASS_ROMMETHODS(ctx._packedRomClass);
   for (size_t i = 0; i < ctx._packedRomClass->romMethodCount; ++i)
      {
      if (J9ROMMETHOD_HAS_DEBUG_INFO(romMethod))
         {
         auto debugInfo = methodDebugInfoFromROMMethod(romMethod);
         if (!(debugInfo->srpToVarInfo & 1))
            debugInfo->srpToVarInfo = 0;
         }
      romMethod = nextROMMethod(romMethod);
      }

   if (isArrayROMClass(romClass))
      {
      packArrayROMClassData(romClass, ctx);
      }
   else
      {
      // 2nd pass: copy all strings to their locations in the packed ROMClass
      ctx._callback = packCallback;
      allSlotsInROMClassDo(romClass, slotCallback, NULL, NULL, &ctx);
      }

   // Pad to required alignment
   auto end = (uint8_t *)OMR::alignNoCheck((uintptr_t)ctx._cursor, sizeof(uint64_t));
   TR_ASSERT(end == (uint8_t *)ctx._packedRomClass + totalSize, "Invalid final cursor position: %p != %p",
             end, (uint8_t *)ctx._packedRomClass + totalSize);
   memset(ctx._cursor, 0, end - ctx._cursor);

   return std::string((char *)ctx._packedRomClass, totalSize);
   }

// insertIntoOOSequenceEntryList needs to be executed with sequencingMonitor in hand.
// This method belongs to ClientSessionData, but is temporarily moved here to be able
// to push the ClientSessionData related code as a standalone piece.
void 
JITServerHelpers::insertIntoOOSequenceEntryList(ClientSessionData *clientData, TR_MethodToBeCompiled *entry)
   {
   uint32_t seqNo = ((TR::CompilationInfoPerThreadRemote*)(entry->_compInfoPT))->getSeqNo();
   TR_MethodToBeCompiled *crtEntry = clientData->getOOSequenceEntryList();
   TR_MethodToBeCompiled *prevEntry = NULL;
   while (crtEntry && (seqNo > ((TR::CompilationInfoPerThreadRemote*)(crtEntry->_compInfoPT))->getSeqNo()))
      {
      prevEntry = crtEntry;
      crtEntry = crtEntry->_next;
      }
   entry->_next = crtEntry;
   if (prevEntry)
      prevEntry->_next = entry;
   else
      clientData->setOOSequenceEntryList(entry);
   }

void
JITServerHelpers::printJITServerMsgStats(J9JITConfig *jitConfig, TR::CompilationInfo *compInfo)
   {
   unsigned totalMsgCount = 0;	   
   PORT_ACCESS_FROM_JITCONFIG(jitConfig);
   j9tty_printf(PORTLIB, "JITServer Message Type Statistics:\n");
   j9tty_printf(PORTLIB, "Type# #called");
#ifdef MESSAGE_SIZE_STATS  
   j9tty_printf(PORTLIB, "\t\tMax\t\tMin\t\tMean\t\tStdDev\t\tSum");
#endif // defined(MESSAGE_SIZE_STATS)
   j9tty_printf(PORTLIB, "\t\tTypeName\n");
   if (compInfo->getPersistentInfo()->getRemoteCompilationMode() == JITServer::CLIENT)
      {
      for (int i = 0; i < JITServer::MessageType_MAXTYPE; ++i)
         {
         if (JITServerHelpers::serverMsgTypeCount[i] > 0)
            {
            j9tty_printf(PORTLIB, "#%04d %7u", i, JITServerHelpers::serverMsgTypeCount[i]);
#ifdef MESSAGE_SIZE_STATS            
            j9tty_printf(PORTLIB, "\t%f\t%f\t%f\t%f\t%f", JITServer::CommunicationStream::collectMsgStat[i].maxVal(), 
                     JITServer::CommunicationStream::collectMsgStat[i].minVal(), JITServer::CommunicationStream::collectMsgStat[i].mean(),
                     JITServer::CommunicationStream::collectMsgStat[i].stddev(), JITServer::CommunicationStream::collectMsgStat[i].sum());
#endif // defined(MESSAGE_SIZE_STATS)
            j9tty_printf(PORTLIB, "\t\t%s\n", JITServer::messageNames[i]);
            totalMsgCount += JITServerHelpers::serverMsgTypeCount[i];
            }
         }
      if (JITServerHelpers::serverMsgTypeCount[0])
         j9tty_printf(PORTLIB, "Total number of messages: %d. Average number of messages per compilation:%f\n", totalMsgCount, totalMsgCount/float(JITServerHelpers::serverMsgTypeCount[0]));
      }
   else if (compInfo->getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER)
      {
      // to print in server, run ./jitserver -Xdump:jit:events=user
      // then kill -3 <pidof jitserver>
#ifdef MESSAGE_SIZE_STATS  
      for (int i = 0; i < JITServer::MessageType_MAXTYPE; ++i)
         {
         if (JITServer::CommunicationStream::collectMsgStat[i].samples() > 0)
            { 
            j9tty_printf(PORTLIB, "#%04d %7u", i, JITServer::CommunicationStream::collectMsgStat[i].samples());            
            j9tty_printf(PORTLIB, "\t%f\t%f\t%f\t%f\t%f", JITServer::CommunicationStream::collectMsgStat[i].maxVal(), 
                     JITServer::CommunicationStream::collectMsgStat[i].minVal(), JITServer::CommunicationStream::collectMsgStat[i].mean(),
                     JITServer::CommunicationStream::collectMsgStat[i].stddev(), JITServer::CommunicationStream::collectMsgStat[i].sum());

            j9tty_printf(PORTLIB, "\t\t%s\n", JITServer::messageNames[i]);
            totalMsgCount += JITServer::CommunicationStream::collectMsgStat[i].samples();
            }
         }
      j9tty_printf(PORTLIB, "Total number of messages: %u\n", totalMsgCount);
#endif // defined(MESSAGE_SIZE_STATS)
      }
   }

void
JITServerHelpers::printJITServerCHTableStats(J9JITConfig *jitConfig, TR::CompilationInfo *compInfo)
   {
#ifdef COLLECT_CHTABLE_STATS
   PORT_ACCESS_FROM_JITCONFIG(jitConfig);
   j9tty_printf(PORTLIB, "JITServer CHTable Statistics:\n");
   if (compInfo->getPersistentInfo()->getRemoteCompilationMode() == JITServer::CLIENT)
      {
      JITClientPersistentCHTable *table = (JITClientPersistentCHTable*)compInfo->getPersistentInfo()->getPersistentCHTable();
      j9tty_printf(PORTLIB, "Num updates sent: %d (1 per compilation)\n", table->_numUpdates);
      if (table->_numUpdates)
         {
         j9tty_printf(PORTLIB, "Num commit failures: %d. Average per compilation: %f\n", table->_numCommitFailures, table->_numCommitFailures / float(table->_numUpdates));
         j9tty_printf(PORTLIB, "Num classes updated: %d. Average per compilation: %f\n", table->_numClassesUpdated, table->_numClassesUpdated / float(table->_numUpdates));
         j9tty_printf(PORTLIB, "Num classes removed: %d. Average per compilation: %f\n", table->_numClassesRemoved, table->_numClassesRemoved / float(table->_numUpdates));
         j9tty_printf(PORTLIB, "Total update bytes: %d. Compilation max: %d. Average per compilation: %f\n", table->_updateBytes, table->_maxUpdateBytes, table->_updateBytes / float(table->_numUpdates));
         }
      }
   else if (compInfo->getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER)
      {
      JITServerPersistentCHTable *table = (JITServerPersistentCHTable*)compInfo->getPersistentInfo()->getPersistentCHTable();
      j9tty_printf(PORTLIB, "Num updates received: %d (1 per compilation)\n", table->_numUpdates);
      if (table->_numUpdates)
         {
         j9tty_printf(PORTLIB, "Num classes updated: %d. Average per compilation: %f\n", table->_numClassesUpdated, table->_numClassesUpdated / float(table->_numUpdates));
         j9tty_printf(PORTLIB, "Num classes removed: %d. Average per compilation: %f\n", table->_numClassesRemoved, table->_numClassesRemoved / float(table->_numUpdates));
         j9tty_printf(PORTLIB, "Num class info queries: %d. Average per compilation: %f\n", table->_numQueries, table->_numQueries / float(table->_numUpdates));
         j9tty_printf(PORTLIB, "Total update bytes: %d. Compilation max: %d. Average per compilation: %f\n", table->_updateBytes, table->_maxUpdateBytes, table->_updateBytes / float(table->_numUpdates));
         }
      }
#endif
   }

void
JITServerHelpers::printJITServerCacheStats(J9JITConfig *jitConfig, TR::CompilationInfo *compInfo)
   {
   PORT_ACCESS_FROM_JITCONFIG(jitConfig);
   if (compInfo->getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER)
      {
      auto clientSessionHT = compInfo->getClientSessionHT();
      clientSessionHT->printStats();
      }
   }

void 
JITServerHelpers::cacheRemoteROMClass(ClientSessionData *clientSessionData, J9Class *clazz, J9ROMClass *romClass, ClassInfoTuple *classInfoTuple)
   {
   ClientSessionData::ClassInfo classInfo;
   OMR::CriticalSection cacheRemoteROMClass(clientSessionData->getROMMapMonitor());
   auto it = clientSessionData->getROMClassMap().find((J9Class*)clazz);
   if (it == clientSessionData->getROMClassMap().end())
      {
      JITServerHelpers::cacheRemoteROMClass(clientSessionData, clazz, romClass, classInfoTuple, classInfo);
      }
   }

void
JITServerHelpers::cacheRemoteROMClass(ClientSessionData *clientSessionData, J9Class *clazz, J9ROMClass *romClass, ClassInfoTuple *classInfoTuple, ClientSessionData::ClassInfo &classInfoStruct)
   {
   ClassInfoTuple &classInfo = *classInfoTuple;

   classInfoStruct._romClass = romClass;
   classInfoStruct._methodsOfClass = std::get<1>(classInfo);
   J9Method *methods = classInfoStruct._methodsOfClass;
   classInfoStruct._baseComponentClass = std::get<2>(classInfo);
   classInfoStruct._numDimensions = std::get<3>(classInfo);
   classInfoStruct._parentClass = std::get<4>(classInfo);
   auto &tmpInterfaces = std::get<5>(classInfo);
   classInfoStruct._interfaces = new (PERSISTENT_NEW) PersistentVector<TR_OpaqueClassBlock *>
      (tmpInterfaces.begin(), tmpInterfaces.end(),
       PersistentVector<TR_OpaqueClassBlock *>::allocator_type(TR::Compiler->persistentAllocator()));
   auto &methodTracingInfo = std::get<6>(classInfo);
   classInfoStruct._classHasFinalFields = std::get<7>(classInfo);
   classInfoStruct._classDepthAndFlags = std::get<8>(classInfo);
   classInfoStruct._classInitialized = std::get<9>(classInfo);
   classInfoStruct._byteOffsetToLockword = std::get<10>(classInfo);
   classInfoStruct._leafComponentClass = std::get<11>(classInfo);
   classInfoStruct._classLoader = std::get<12>(classInfo);
   classInfoStruct._hostClass = std::get<13>(classInfo);
   classInfoStruct._componentClass = std::get<14>(classInfo);
   classInfoStruct._arrayClass = std::get<15>(classInfo);
   classInfoStruct._totalInstanceSize = std::get<16>(classInfo);
   classInfoStruct._remoteRomClass = std::get<17>(classInfo);
   classInfoStruct._constantPool = (J9ConstantPool *)std::get<18>(classInfo);
   classInfoStruct._classFlags = std::get<19>(classInfo);
   classInfoStruct._classChainOffsetOfIdentifyingLoaderForClazz = std::get<20>(classInfo);
   clientSessionData->getROMClassMap().insert({ clazz, classInfoStruct});

   auto &origROMMethods = std::get<21>(classInfo);

   uint32_t numMethods = romClass->romMethodCount;
   J9ROMMethod *romMethod = J9ROMCLASS_ROMMETHODS(romClass);
   for (uint32_t i = 0; i < numMethods; i++)
      {
      clientSessionData->getJ9MethodMap().insert({&methods[i],
            {romMethod, origROMMethods[i], NULL, static_cast<bool>(methodTracingInfo[i]), (TR_OpaqueClassBlock *)clazz, false}});
      romMethod = nextROMMethod(romMethod);
      }
   }

J9ROMClass *
JITServerHelpers::getRemoteROMClassIfCached(ClientSessionData *clientSessionData, J9Class *clazz)
   {
   OMR::CriticalSection getRemoteROMClassIfCached(clientSessionData->getROMMapMonitor());
   auto it = clientSessionData->getROMClassMap().find(clazz);
   return (it == clientSessionData->getROMClassMap().end()) ? NULL : it->second._romClass;
   }

JITServerHelpers::ClassInfoTuple
JITServerHelpers::packRemoteROMClassInfo(J9Class *clazz, J9VMThread *vmThread, TR_Memory *trMemory, bool serializeClass)
   {
   // Always use the base VM here.
   // If this method is called inside AOT compilation, TR_J9SharedCacheVM will
   // attempt validation and return NULL for many methods invoked here.
   // We do not want that, because these values will be cached and later used in non-AOT
   // compilations, where we always need a non-NULL result.
   TR_J9VM *fe = (TR_J9VM *) TR_J9VMBase::get(vmThread->javaVM->jitConfig, vmThread);
   J9Method *methodsOfClass = (J9Method*) fe->getMethods((TR_OpaqueClassBlock*) clazz);
   int32_t numDims = 0;
   TR_OpaqueClassBlock *baseClass = fe->getBaseComponentClass((TR_OpaqueClassBlock *) clazz, numDims);
   TR_OpaqueClassBlock *parentClass = fe->getSuperClass((TR_OpaqueClassBlock *) clazz);

   uint32_t numMethods = clazz->romClass->romMethodCount;

   std::vector<uint8_t> methodTracingInfo;
   methodTracingInfo.reserve(numMethods);

   std::vector<J9ROMMethod *> origROMMethods;
   origROMMethods.reserve(numMethods);
   for(uint32_t i = 0; i < numMethods; ++i)
      {
      methodTracingInfo.push_back(static_cast<uint8_t>(fe->isMethodTracingEnabled((TR_OpaqueMethodBlock *) &methodsOfClass[i])));
      // record client-side pointers to ROM methods
      origROMMethods.push_back(fe->getROMMethodFromRAMMethod(&methodsOfClass[i]));
      }

   bool classHasFinalFields = fe->hasFinalFieldsInClass((TR_OpaqueClassBlock *)clazz);
   uintptr_t classDepthAndFlags = fe->getClassDepthAndFlagsValue((TR_OpaqueClassBlock *)clazz);
   bool classInitialized =  fe->isClassInitialized((TR_OpaqueClassBlock *)clazz);
   uint32_t byteOffsetToLockword = fe->getByteOffsetToLockword((TR_OpaqueClassBlock *)clazz);
   TR_OpaqueClassBlock * leafComponentClass = fe->getLeafComponentClassFromArrayClass((TR_OpaqueClassBlock *)clazz);
   void * classLoader = fe->getClassLoader((TR_OpaqueClassBlock *)clazz);
   TR_OpaqueClassBlock * hostClass = fe->convertClassPtrToClassOffset(clazz->hostClass);
   TR_OpaqueClassBlock * componentClass = fe->getComponentClassFromArrayClass((TR_OpaqueClassBlock *)clazz);
   TR_OpaqueClassBlock * arrayClass = fe->getArrayClassFromComponentClass((TR_OpaqueClassBlock *)clazz);
   uintptr_t totalInstanceSize = clazz->totalInstanceSize;
   uintptr_t cp = fe->getConstantPoolFromClass((TR_OpaqueClassBlock *)clazz);
   uintptr_t classFlags = fe->getClassFlagsValue((TR_OpaqueClassBlock *)clazz);
   uintptr_t classChainOffsetOfIdentifyingLoaderForClazz = fe->sharedCache() ? 
      fe->sharedCache()->getClassChainOffsetOfIdentifyingLoaderForClazzInSharedCacheNoFail((TR_OpaqueClassBlock *)clazz) : 0;

   return std::make_tuple(serializeClass ? packROMClass(clazz->romClass, trMemory) : std::string(), methodsOfClass, baseClass, numDims, parentClass,
                          TR::Compiler->cls.getITable((TR_OpaqueClassBlock *) clazz), methodTracingInfo,
                          classHasFinalFields, classDepthAndFlags, classInitialized, byteOffsetToLockword,
                          leafComponentClass, classLoader, hostClass, componentClass, arrayClass, totalInstanceSize,
                          clazz->romClass, cp, classFlags, classChainOffsetOfIdentifyingLoaderForClazz, origROMMethods);
   }

J9ROMClass *
JITServerHelpers::romClassFromString(const std::string &romClassStr, TR_PersistentMemory *trMemory)
   {
   auto romClass = (J9ROMClass *)(trMemory->allocatePersistentMemory(romClassStr.size(), TR_Memory::ROMClass));
   if (!romClass)
      throw std::bad_alloc();
   memcpy(romClass, &romClassStr[0], romClassStr.size());
   return romClass;
   }

J9ROMClass *
JITServerHelpers::getRemoteROMClass(J9Class *clazz, JITServer::ServerStream *stream, TR_Memory *trMemory, ClassInfoTuple *classInfoTuple)
   {
   stream->write(JITServer::MessageType::ResolvedMethod_getRemoteROMClassAndMethods, clazz);
   const auto &recv = stream->read<ClassInfoTuple>();
   *classInfoTuple = std::get<0>(recv);
   return romClassFromString(std::get<0>(*classInfoTuple), trMemory->trPersistentMemory());
   }

J9ROMClass *
JITServerHelpers::getRemoteROMClass(J9Class *clazz, JITServer::ServerStream *stream, TR_PersistentMemory *trMemory, ClassInfoTuple *classInfoTuple)
   {
   stream->write(JITServer::MessageType::ResolvedMethod_getRemoteROMClassAndMethods, clazz);
   const auto &recv = stream->read<ClassInfoTuple>();
   *classInfoTuple = std::get<0>(recv);
   return romClassFromString(std::get<0>(*classInfoTuple), trMemory);
   }

// Return true if able to get data from cache, return false otherwise.
bool
JITServerHelpers::getAndCacheRAMClassInfo(J9Class *clazz, ClientSessionData *clientSessionData, JITServer::ServerStream *stream, ClassInfoDataType dataType, void *data)
   {
   JITServerHelpers::ClassInfoTuple classInfoTuple;
   ClientSessionData::ClassInfo classInfo;
   if (!clazz)
      {
      return false;
      }
      {
      OMR::CriticalSection getRemoteROMClass(clientSessionData->getROMMapMonitor());
      auto it = clientSessionData->getROMClassMap().find((J9Class*)clazz);
      if (it != clientSessionData->getROMClassMap().end())
         {
         JITServerHelpers::getROMClassData(it->second, dataType, data);
         return true;
         }
      }

   stream->write(JITServer::MessageType::ResolvedMethod_getRemoteROMClassAndMethods, clazz);
   const auto &recv = stream->read<ClassInfoTuple>();
   classInfoTuple = std::get<0>(recv);

   OMR::CriticalSection cacheRemoteROMClass(clientSessionData->getROMMapMonitor());
   auto it = clientSessionData->getROMClassMap().find(clazz);
   if (it == clientSessionData->getROMClassMap().end())
      {
      auto romClass = romClassFromString(std::get<0>(classInfoTuple), TR::comp()->trMemory()->trPersistentMemory());
      JITServerHelpers::cacheRemoteROMClass(clientSessionData, clazz, romClass, &classInfoTuple, classInfo);
      JITServerHelpers::getROMClassData(classInfo, dataType, data);
      }
   else
      {
      JITServerHelpers::getROMClassData(it->second, dataType, data);
      }
   return false;
   }

// Return true if able to get data from cache, return false otherwise.
bool
JITServerHelpers::getAndCacheRAMClassInfo(J9Class *clazz, ClientSessionData *clientSessionData, JITServer::ServerStream *stream, ClassInfoDataType dataType1, void *data1, ClassInfoDataType dataType2, void *data2)
   {
   JITServerHelpers::ClassInfoTuple classInfoTuple;
   ClientSessionData::ClassInfo classInfo;
   if (!clazz)
      {
      return false;
      }
      {
      OMR::CriticalSection getRemoteROMClass(clientSessionData->getROMMapMonitor());
      auto it = clientSessionData->getROMClassMap().find((J9Class*)clazz);
      if (it != clientSessionData->getROMClassMap().end())
         {
         JITServerHelpers::getROMClassData(it->second, dataType1, data1);
         JITServerHelpers::getROMClassData(it->second, dataType2, data2);
         return true;
         }
      }
   stream->write(JITServer::MessageType::ResolvedMethod_getRemoteROMClassAndMethods, clazz);
   const auto &recv = stream->read<ClassInfoTuple>();
   classInfoTuple = std::get<0>(recv);

   OMR::CriticalSection cacheRemoteROMClass(clientSessionData->getROMMapMonitor());
   auto it = clientSessionData->getROMClassMap().find(clazz);
   if (it == clientSessionData->getROMClassMap().end())
      {
      auto romClass = romClassFromString(std::get<0>(classInfoTuple), TR::comp()->trMemory()->trPersistentMemory());
      JITServerHelpers::cacheRemoteROMClass(clientSessionData, clazz, romClass, &classInfoTuple, classInfo);
      JITServerHelpers::getROMClassData(classInfo, dataType1, data1);
      JITServerHelpers::getROMClassData(classInfo, dataType2, data2);
      }
   else
      {
      JITServerHelpers::getROMClassData(it->second, dataType1, data1);
      JITServerHelpers::getROMClassData(it->second, dataType2, data2);
      }
   return false;
   }

void
JITServerHelpers::getROMClassData(const ClientSessionData::ClassInfo &classInfo, ClassInfoDataType dataType, void *data)
   {
   switch (dataType)
      {
      case CLASSINFO_ROMCLASS_MODIFIERS :
         {
         *(uint32_t *)data = classInfo._romClass->modifiers;
         }
         break;
      case CLASSINFO_ROMCLASS_EXTRAMODIFIERS :
         {
         *(uint32_t *)data = classInfo._romClass->extraModifiers;
         }
         break;
      case CLASSINFO_BASE_COMPONENT_CLASS :
         {
         *(TR_OpaqueClassBlock **)data = classInfo._baseComponentClass;
         }
         break;
      case CLASSINFO_NUMBER_DIMENSIONS :
         {
         *(int32_t *)data = classInfo._numDimensions;
         }
         break;
      case CLASSINFO_PARENT_CLASS :
         {
         *(TR_OpaqueClassBlock **)data = classInfo._parentClass;
         }
         break;
      case CLASSINFO_CLASS_HAS_FINAL_FIELDS :
         {
         *(bool *)data = classInfo._classHasFinalFields;
         }
         break;
      case CLASSINFO_CLASS_DEPTH_AND_FLAGS :
         {
         *(uintptr_t *)data = classInfo._classDepthAndFlags;
         }
         break;
      case CLASSINFO_CLASS_INITIALIZED :
         {
         *(bool *)data = classInfo._classInitialized;
         }
         break;
      case CLASSINFO_BYTE_OFFSET_TO_LOCKWORD :
         {
         *(uint32_t *)data = classInfo._byteOffsetToLockword;
         }
         break;
      case CLASSINFO_LEAF_COMPONENT_CLASS :
         {
         *(TR_OpaqueClassBlock **)data = classInfo._leafComponentClass;
         }
         break;
      case CLASSINFO_CLASS_LOADER :
         {
         *(void **)data = classInfo._classLoader;
         }
         break;
      case CLASSINFO_HOST_CLASS :
         {
         *(TR_OpaqueClassBlock **)data = classInfo._hostClass;
         }
         break;
      case CLASSINFO_COMPONENT_CLASS :
         {
         *(TR_OpaqueClassBlock **)data = classInfo._componentClass;
         }
         break;
      case CLASSINFO_ARRAY_CLASS :
         {
         *(TR_OpaqueClassBlock **)data = classInfo._arrayClass;
         }
         break;
      case CLASSINFO_TOTAL_INSTANCE_SIZE :
         {
         *(uintptr_t *)data = classInfo._totalInstanceSize;
         }
         break;
      case CLASSINFO_REMOTE_ROM_CLASS :
         {
         *(J9ROMClass **)data = classInfo._remoteRomClass;
         }
         break;
      case CLASSINFO_CLASS_FLAGS :
         {
         *(uintptr_t *)data = classInfo._classFlags;
         }
         break;
      case CLASSINFO_METHODS_OF_CLASS :
         {
         *(J9Method **)data = classInfo._methodsOfClass;
         }
         break;
      case CLASSINFO_CONSTANT_POOL :
         {
         *(J9ConstantPool **)data = classInfo._constantPool;
         }
         break;
      case CLASSINFO_CLASS_CHAIN_OFFSET:
         {
         *(uintptr_t *)data = classInfo._classChainOffsetOfIdentifyingLoaderForClazz;
         }
         break;
      default :
         {
         TR_ASSERT(0, "Class Info not supported %u \n", dataType);
         }
         break;
      }
   }

J9ROMMethod *
JITServerHelpers::romMethodOfRamMethod(J9Method* method)
   {
   // JITServer
   auto clientData = TR::compInfoPT->getClientData();
   J9ROMMethod *romMethod = NULL;

   // Check if the method is already cached.
      {
      OMR::CriticalSection romCache(clientData->getROMMapMonitor());
      auto &map = clientData->getJ9MethodMap();
      auto it = map.find((J9Method*) method);
      if (it != map.end())
         romMethod = it->second._romMethod;
      }

   // If not, cache the associated ROM class and get the ROM method from it.
   if (!romMethod)
      {
      auto stream = TR::CompilationInfo::getStream();
      stream->write(JITServer::MessageType::VM_getClassOfMethod, (TR_OpaqueMethodBlock*) method);
      J9Class *clazz = (J9Class*) std::get<0>(stream->read<TR_OpaqueClassBlock *>());
      TR::compInfoPT->getAndCacheRemoteROMClass(clazz);
         {
         OMR::CriticalSection romCache(clientData->getROMMapMonitor());
         auto &map = clientData->getJ9MethodMap();
         auto it = map.find((J9Method *) method);
         if (it != map.end())
            romMethod = it->second._romMethod;
         }
      }
   TR_ASSERT(romMethod, "Should have acquired romMethod");
   return romMethod;
   }

void
JITServerHelpers::postStreamFailure(OMRPortLibrary *portLibrary, TR::CompilationInfo *compInfo)
   {
   OMR::CriticalSection postStreamFailure(getClientStreamMonitor());

   OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
   uint64_t current_time = omrtime_current_time_millis();
   if (current_time >= _nextConnectionRetryTime)
      {
      _waitTimeMs *= 2; // Exponential backoff
      }
   _nextConnectionRetryTime = current_time + _waitTimeMs;
   _serverAvailable = false;

   // Reset the activation policy flag in case we never reconnect to the server
   // and client compiles locally or connects to a new server
   compInfo->setCompThreadActivationPolicy(JITServer::CompThreadActivationPolicy::AGGRESSIVE);
   if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCompilationThreads) ||
        TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseJITServer))
      {
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
                                     "t=%6u client has lost connection, resetting activation policy to AGGRESSIVE",
                                     (uint32_t) compInfo->getPersistentInfo()->getElapsedTime());
      }
   }

void
JITServerHelpers::postStreamConnectionSuccess()
   {
   _serverAvailable = true;
   _waitTimeMs = 1000;
   }

bool
JITServerHelpers::shouldRetryConnection(OMRPortLibrary *portLibrary)
   {
   OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
   return omrtime_current_time_millis() > _nextConnectionRetryTime;
   }

bool
JITServerHelpers::isAddressInROMClass(const void *address, const J9ROMClass *romClass)
   {
   return ((address >= romClass) && (address < (((uint8_t*) romClass) + romClass->romSize)));
   }


uintptr_t
JITServerHelpers::walkReferenceChainWithOffsets(TR_J9VM * fe, const std::vector<uintptr_t>& listOfOffsets, uintptr_t receiver)
   {
   uintptr_t result = receiver;
   for (size_t i = 0; i < listOfOffsets.size(); i++)
      {
      result = fe->getReferenceFieldAt(result, listOfOffsets[i]);
      }
   return result;
   }

uintptr_t
JITServerHelpers::getRemoteClassDepthAndFlagsWhenROMClassNotCached(J9Class *clazz, ClientSessionData *clientSessionData, JITServer::ServerStream *stream)
{
   ClientSessionData::ClassInfo classInfo;
   stream->write(JITServer::MessageType::ResolvedMethod_getRemoteROMClassAndMethods, clazz);
   const auto &recv = stream->read<JITServerHelpers::ClassInfoTuple>();
   JITServerHelpers::ClassInfoTuple classInfoTuple = std::get<0>(recv);

   OMR::CriticalSection cacheRemoteROMClass(clientSessionData->getROMMapMonitor());
   auto it = clientSessionData->getROMClassMap().find(clazz);
   if (it == clientSessionData->getROMClassMap().end())
      {
      auto romClass = JITServerHelpers::romClassFromString(std::get<0>(classInfoTuple), TR::comp()->trMemory()->trPersistentMemory());
      JITServerHelpers::cacheRemoteROMClass(clientSessionData, clazz, romClass, &classInfoTuple, classInfo);
      return classInfo._classDepthAndFlags;
      }
   else
      {
      return it->second._classDepthAndFlags;
      }
}
