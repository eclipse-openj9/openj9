/*******************************************************************************
 * Copyright IBM Corp. and others 2019
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "control/JITServerHelpers.hpp"

#include "control/CompilationRuntime.hpp"
#include "control/JITServerCompilationThread.hpp"
#include "control/MethodToBeCompiled.hpp"
#include "env/ClassLoaderTable.hpp"
#include "env/StackMemoryRegion.hpp"
#include "infra/CriticalSection.hpp"
#include "infra/Statistics.hpp"
#include "net/CommunicationStream.hpp"
#include "OMR/Bytes.hpp"// for OMR::alignNoCheck()
#include "runtime/JITServerAOTDeserializer.hpp"
#include "runtime/JITServerSharedROMClassCache.hpp"
#include "romclasswalk.h"
#include "util_api.h"// for allSlotsInROMClassDo()
#include "stackmap_api.h"// for fixReturnBytecodes()

#include <algorithm>


uint64_t     JITServerHelpers::_waitTimeMs = 0;
bool         JITServerHelpers::_serverAvailable = true;
uint64_t     JITServerHelpers::_nextConnectionRetryTime = 0;
TR::Monitor *JITServerHelpers::_clientStreamMonitor = NULL;


// To ensure that the length fields in UTF8 strings appended at the end of the
// packed ROMClass are properly aligned, we must pad the strings accordingly.
// This function returns the total size of a UTF8 string with the padding.
static size_t
getPaddedUTF8Size(size_t length)
   {
   return OMR::alignNoCheck(sizeof(J9UTF8) + length, sizeof(J9UTF8));
   }

static size_t
getPaddedUTF8Size(const J9UTF8 *str)
   {
   return getPaddedUTF8Size(J9UTF8_LENGTH(str));
   }

// Copies a UTF8 string and returns its total size including the padding. src doesn't have to be padded, dst is padded.
// If genratedPrefixLength is non-zero, it is used instead of the full string length.
static size_t
copyUTF8(J9UTF8 *dst, const J9UTF8 *src, size_t generatedPrefixLength = 0)
   {
   size_t length = generatedPrefixLength ? generatedPrefixLength : J9UTF8_LENGTH(src);
   J9UTF8_SET_LENGTH(dst, length);
   memcpy(J9UTF8_DATA(dst), J9UTF8_DATA(src), length);

   // If the length is not aligned, pad the destination string with a zero
   static_assert(sizeof(*src) == 2, "UTF8 header is not 2 bytes large");
   if (!OMR::alignedNoCheck(length, sizeof(*src)))
      J9UTF8_DATA(dst)[length] = '\0';

   return getPaddedUTF8Size(length);
   }

// ROM methods in ROM classes can have debug information. Currently, this debug
// information is stored either inline in full, or in the local SCC (with a
// single J9SRP to the debug information stored inline in its place). See
// ROMClassWriter::writeMethodDebugInfo() in bcutil/ROMClassWriter.cpp for the
// exact format, and allSlotsInMethodDebugInfoDo() in util/romclasswalk.c for
// an example of working with the raw ROMClass data.
//
// Since this information is irrelevant to the operation of the JIT, we don't
// want this debug information to affect the hash of the ROM class. Thus, part
// of ROM class packing involves taking spans of inline debug information
// in the ROM methods of the class and shrinking it to the width of a single
// (zeroed-out) J9SRP.
//
// A ROM class can be divided into alternating segments of debug and non-debug
// information, starting and ending with non-debug segments. This class holds a
// _segments vector that, after an initial traversal of the ROM class, records
// the start of each segment and how much that segment will need to be shifted
// toward the start of the packed ROM class to compensate for the stripped-out
// debug information.
class ROMSegmentMap
   {
public:
   struct Segment
      {
      size_t newBoundaryOffset() const { return _boundaryOffset - _shift; }

      const size_t _boundaryOffset;
      const size_t _shift;
      };

   ROMSegmentMap(TR_Memory *trMemory, const J9ROMClass *romClass) :
      _accumulatedShift(0),
      _segments({{0, 0}}, decltype(_segments)::allocator_type(trMemory->currentStackRegion()))
      {}

   // Note: segments must be added in increasing memory order. This function preserves the debug/non-debug
   // alternation property mentioned in _segments.
   void registerDebugInfoSegment(const J9ROMClass *romClass, const uint8_t *debugInfoSegmentStart, size_t debugInfoSize);

   size_t removedDebugInfoSize() const { return _accumulatedShift; }
   const Vector<Segment>& getSegments() const { return _segments; }

   // Given an offset into the old ROM class before the UTF8 string section, compute the corresponding offset
   // in the packed ROM class
   size_t newOffsetFromOld(size_t oldOffset) const;

private:
   // The total bytes that will have been removed from the ROM class given the segments in _segments.
   // (Note that the space between the segments will never increase during packing).
   size_t _accumulatedShift;
   // The segments in a ROM class. The first segment marks the start of non-debug info, then
   // the segments alternate between starting debug info and non-debug info. Thus the first,
   // last, and even-indexed segments will all be non-debug info, and the rest will be debug info.
   Vector<Segment> _segments;
   };

void
ROMSegmentMap::registerDebugInfoSegment(const J9ROMClass *romClass, const uint8_t *debugInfoSegmentStart, size_t debugInfoSegmentSize)
   {
   // We need to record (the beginnings of) two segments in the original ROM class:
   // 1. [debugInfoSegmentStart, debugInfoSegmentStart + debugInfoSegmentSize)
   // 2. [debugInfoSegmentStart + debugInfoSegmentSize, <start of next segment>)
   //
   // Addresses in segment (1) are debug info and compressed to the single address
   // debugInfoSegmentStart, then shifted toward the start of the ROM class uniformly
   // by _accumulatedShift.
   // Addresses in segment (2) are non-debug info and shifted toward the start of the
   // ROM class uniformly by the updated _accumulatedShift.
   size_t debugOffset = debugInfoSegmentStart - (uint8_t *)romClass;
   TR_ASSERT(_segments.empty() || (_segments.back()._boundaryOffset < debugOffset), "Debug info addresses must be traversed in memory order");
   // Start of segment (1) above - i.e., start of debug info
   _segments.push_back({debugOffset, _accumulatedShift});
   // Segment (1), of size debugInfoSegmentSize, is going to be compressed to the size of a single J9SRP, so
   // addresses after this segment will be shifted toward the start by this amount plus any previous
   // accumulated shift.
   _accumulatedShift += debugInfoSegmentSize - sizeof(J9SRP);
   // Start of segment (2) above - i.e., start of the subsequent non-debug info segment
   _segments.push_back({debugOffset + debugInfoSegmentSize, _accumulatedShift});
   }

size_t
ROMSegmentMap::newOffsetFromOld(size_t oldOffset) const
   {
   TR_ASSERT(!_segments.empty(), "Segment map must be populated");

   Segment find = {oldOffset, 0};
   // Finds the first s for which s._boundaryOffset > oldOffset (the upper segment boundary).
   auto it = std::lower_bound(_segments.begin(), _segments.end(), find, [](Segment x, Segment y) { return x._boundaryOffset <= y._boundaryOffset; });

   // We want the lower segment boundary, the last segment s for which s._boundaryOffset <= oldOffset.
   size_t index = 0;
   if (it == _segments.end())
      {
      index = _segments.size() - 1;
      }
   else
      {
      // Because {0, 0} is the first element of _segments, if we found an upper boundary then it is necessarily not the first element of _segments
      index = it - _segments.begin() - 1;
      }

   Segment startSegment = _segments[index];
   // Even-indexed segments are non-debug info
   if (index % 2 == 0)
      return oldOffset - startSegment._shift;
   else
      return startSegment._boundaryOffset - startSegment._shift;
   }

// State maintained while iterating over UTF8 strings in a ROMClass
struct ROMClassPackContext
   {
   ROMClassPackContext(TR_Memory *trMemory, const J9ROMClass *romClass, size_t origSize,
                       bool noDebugInfoStripping, const J9UTF8 *className, size_t generatedPrefixLength) :
      _segmentMap(trMemory, romClass), _origRomClassStart((const uint8_t *)romClass),
      _noDebugInfoStripping(noDebugInfoStripping), _origSize(origSize), _className(className),
      _generatedPrefixLength(generatedPrefixLength), _callback(NULL), _preStringSize(0), _stringsSize(0),
      _origUtf8SectionStart((const uint8_t *)-1), _origUtf8SectionEnd(NULL), _utf8SectionSize(0),
      _strToOffsetMap(decltype(_strToOffsetMap)::allocator_type(trMemory->currentStackRegion())),
      _srpCallback(NULL), _wsrpCallback(NULL),
      _newUtf8SectionStart((const uint8_t *)-1),
      _packedRomClass(NULL), _cursor(NULL) {}

   bool isInline(const void *address, const J9ROMClass *romClass) const
      {
      return (address >= romClass) && (address < (uint8_t *)romClass + _origSize);
      }

   // Given an address within the original ROM class, returns the corresponding address in the
   // packed ROM class.
   const uint8_t *newAddressFromOld(const uint8_t *oldAddr) const;

   typedef void (*Callback)(const J9ROMClass *, const J9SRP *, const char *, ROMClassPackContext &);
   typedef void (*WSRPCallback)(const J9ROMClass *, const J9WSRP *, const char *, ROMClassPackContext &);

   ROMSegmentMap _segmentMap;
   const uint8_t *_origRomClassStart;
   bool _noDebugInfoStripping;
   const size_t _origSize;
   const J9UTF8 *const _className;
   // Deterministic class name prefix length if the class is runtime-generated and
   // we are packing its ROMClass to compute its deterministic hash, otherwise 0.
   const size_t _generatedPrefixLength;
   Callback _callback;
   // The size of the original ROM class before the UTF8 section, not including any padding that was added during
   // ROM class writing, to be calculated during sectionEndCallback(). This will always be exactly zero or four bytes
   // less than (_origUtf8SectionStart - _origRomClassStart).
   size_t _preStringSize;
   size_t _stringsSize;
   const uint8_t *_origUtf8SectionStart;
   const uint8_t *_origUtf8SectionEnd; // Only needed for assertions
   size_t _utf8SectionSize; // Only needed for assertions
   // Maps original strings to their offsets from UTF8 section start in the packed ROMClass.
   // Offset value -1 signifies that the string is skipped. The truncate flag signifies that the string is the
   // (non-deterministic) runtime-generated class name which must be truncated to the deterministic name prefix.
   UnorderedMap<const J9UTF8 *, std::pair<size_t, bool/*truncate*/>> _strToOffsetMap;
   J9ROMClass *_packedRomClass;
   Callback _srpCallback;
   WSRPCallback _wsrpCallback;
   const uint8_t *_newUtf8SectionStart;
   uint8_t *_cursor;
   };

const uint8_t *
ROMClassPackContext::newAddressFromOld(const uint8_t *oldAddr) const
   {
   TR_ASSERT(isInline(oldAddr, (const J9ROMClass *)_origRomClassStart), "Address must be within the original ROM class");
   // If we aren't stripping debug info, the offset can be transferred directly.
   if (_noDebugInfoStripping)
      return (const uint8_t *)_packedRomClass + (oldAddr - _origRomClassStart);

   // The UTF8 section is shifted as a block to its new start.
   if (oldAddr >= _origUtf8SectionStart)
      return _newUtf8SectionStart + (oldAddr - _origUtf8SectionStart);
   else
      return (const uint8_t *)_packedRomClass + _segmentMap.newOffsetFromOld(oldAddr - _origRomClassStart);
   }

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
   // Truncate all copies of the class name down to the deterministic class name prefix if requested
   bool truncate = ctx._generatedPrefixLength && J9UTF8_EQUALS(str, ctx._className);

   auto result = ctx._strToOffsetMap.insert({ str, { skip ? (size_t)-1 : ctx._stringsSize, truncate } });
   if (!result.second)
      {
      // Duplicate - already visited
      auto &it = result.first;
      if (!skip && (it->second.first == (size_t)-1))
         {
         // Previously visited SRPs to this string were skipped, but this one isn't
         it->second.first = ctx._stringsSize;
         ctx._stringsSize += truncate ? getPaddedUTF8Size(ctx._generatedPrefixLength) : getPaddedUTF8Size(str);
         }
      return;
      }

   size_t strSize = getPaddedUTF8Size(str);
   size_t size = truncate ? getPaddedUTF8Size(ctx._generatedPrefixLength) : strSize;
   ctx._stringsSize += skip ? 0 : size;

   if (ctx.isInline(str, romClass))
      {
      ctx._origUtf8SectionStart = std::min(ctx._origUtf8SectionStart, (const uint8_t *)str);
      ctx._origUtf8SectionEnd = std::max(ctx._origUtf8SectionEnd, (const uint8_t *)str + strSize);
      ctx._utf8SectionSize += strSize;
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
   auto srp = (J9SRP *)ctx.newAddressFromOld((uint8_t *)origSrp);

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
   auto dst = ctx._newUtf8SectionStart + it->second.first;

   NNSRP_PTR_SET(srp, dst);
   if (dst == ctx._cursor)
      ctx._cursor += copyUTF8((J9UTF8 *)dst, str, it->second.second/*truncate*/ ? ctx._generatedPrefixLength : 0);
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

static void
adjustSRPCallback(const J9ROMClass *romClass, const J9SRP *origSrp, const char *slotName, ROMClassPackContext &ctx)
   {
   if (!ctx.isInline(origSrp, romClass))
      return;
   auto srp = (J9SRP *)ctx.newAddressFromOld((uint8_t *)origSrp);
   auto origDest = NNSRP_PTR_GET(origSrp, uint8_t *);
   if (ctx.isInline(origDest, romClass))
      {
      auto dst = ctx.newAddressFromOld(origDest);
      NNSRP_PTR_SET(srp, dst);
      }
   else
      {
      SRP_PTR_SET_TO_NULL(srp);
      }
   }

static void
adjustWSRPCallback(const J9ROMClass *romClass, const J9WSRP *origSrp, const char *slotName, ROMClassPackContext &ctx)
   {
   if (!ctx.isInline(origSrp, romClass))
      return;
   auto srp = (J9WSRP *)ctx.newAddressFromOld((uint8_t *)origSrp);
   auto origDest = NNWSRP_PTR_GET(origSrp, uint8_t *);
   if (ctx.isInline(origDest, romClass))
      {
      auto dst = ctx.newAddressFromOld(origDest);
      NNWSRP_PTR_SET(srp, dst);
      }
   else
      {
      WSRP_PTR_SET_TO_NULL(srp);
      }
   }

static void
srpSlotCallback(const J9ROMClass *romClass, const J9SRP *srp, const char *slotName, void *userData)
   {
   auto &ctx = *(ROMClassPackContext *)userData;
   if (ctx._srpCallback && *srp)
      ctx._srpCallback(romClass, srp, slotName, ctx);
   }

static void
wsrpSlotCallback(const J9ROMClass *romClass, const J9WSRP *srp, const char *slotName, void *userData)
   {
   auto &ctx = *(ROMClassPackContext *)userData;
   if (ctx._wsrpCallback && *srp)
      ctx._wsrpCallback(romClass, srp, slotName, ctx);
   }

// Invoked for each slot in a ROMClass. Calls ctx._callback for all non-null SRPs to UTF8 strings.
static void
slotCallback(J9ROMClass *romClass, uint32_t slotType, void *slotPtr, const char *slotName, void *userData)
   {
   switch (slotType)
      {
      case J9ROM_SRP:
         srpSlotCallback(romClass, (const J9SRP *)slotPtr, slotName, userData);
         break;

      case J9ROM_WSRP:
         wsrpSlotCallback(romClass, (const J9WSRP *)slotPtr, slotName, userData);
         break;

      case J9ROM_UTF8:
         utf8SlotCallback(romClass, (const J9SRP *)slotPtr, slotName, userData);
         break;

      case J9ROM_NAS:
         if (auto nas = SRP_PTR_GET(slotPtr, const J9ROMNameAndSignature *))
            {
            utf8SlotCallback(romClass, &nas->name, slotName, userData);
            utf8SlotCallback(romClass, &nas->signature, slotName, userData);
            }
         srpSlotCallback(romClass, (const J9SRP *)slotPtr, slotName, userData);
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
   totalSize += getPaddedUTF8Size(J9ROMCLASS_CLASSNAME(romClass));
   totalSize += getPaddedUTF8Size(J9ROMCLASS_SUPERCLASSNAME(romClass));

   totalSize += romClass->interfaceCount * sizeof(J9SRP);
   for (size_t i = 0; i < romClass->interfaceCount; ++i)
      {
      auto name = NNSRP_GET(J9ROMCLASS_INTERFACES(romClass)[i], const J9UTF8 *);
      totalSize += getPaddedUTF8Size(name);
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

// Calculate the size of the portion of a ROM class that occurs before its UTF8 string section (the pre-string section),
// without any padding that may have been added during initial ROM class writing.
//
// Why is this necessary? The sections inside a ROMClass before the UTF8 string section are not, taken together,
// necessarily 64-bit aligned. The ROM class format requires the entire pre-string segment of the ROM class to
// be 64-bit aligned, so padding may have been added between the last pre-string section and the UTF8 string section.
// When we strip out the debug info from a ROM class, we might reintroduce a misalignment in this pre-string section.
// Fixing this misalignment might require us to add more padding, or to drop some of the padding that was already added
// during ROM class writing. We can only decide to do one or the other if we know the unpadded size of the pre-string
// section. Once we have this information, we can calculate the aligned and padded size of the pre-string section
// (currently called the packedNonStringSize) properly in packROMClass.
static void
sectionEndCallback(J9ROMClass *romClass, void *sectionPtr, uintptr_t sectionSize, const char *sectionName, void *userData)
   {
   auto ctx = (ROMClassPackContext *)userData;
   // Important ROM class walker properties that allow us to calculate the _preStringSize in parallel with the UTF8 string section
   // start:
   //
   // 1. The ROM class walker only visits sections that are entirely within the pre-string section or entirely outside the ROM class
   // 2. The sections entirely cover the pre-string section, up to but not including any padding that was added during writing
   //
   // That the _preStringSize ends up being where we expect it to be (equal to the starting offset of the UTF8 string section up to
   // alignment padding) is checked in a fatal assert when we calculate the packedNonStringSize in packROMClass.
   if (ctx->isInline(sectionPtr, romClass))
      {
      size_t endOffset = ((uint8_t *)sectionPtr - (uint8_t *)romClass) + sectionSize;
      ctx->_preStringSize = std::max(ctx->_preStringSize, endOffset);
      }
   }

// Packs a ROMClass to be transferred to the server.
//
// The result is allocated from the stack region of trMemory (as well as temporary data
// structures used for packing). This function should be used with TR::StackMemoryRegion.
// If passed non-zero expectedSize, and it doesn't match the resulting packedSize
// (which is returned to the caller by reference), this function returns NULL.
//
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
// A non-zero generatedPrefixLength represents the length of the deterministic class name prefix for a recognized
// runtime-generated class (e.g., a lambda). In such cases all instances of the class name (including CP entries)
// are truncated to the deterministic prefix, and other stings in the UTF8 section are shifted accordingly.
//
// A NULL fej9 means that romClass is already packed (hence we can skip certain parts of
// the packing procedure), and this function is getting called at the server to re-pack a
// runtime-generated ROMClass in order to compute its deterministic hash as described above.
//
J9ROMClass *
JITServerHelpers::packROMClass(const J9ROMClass *romClass, TR_Memory *trMemory, TR_J9VMBase *fej9,
                               size_t &packedSize, size_t expectedSize, size_t generatedPrefixLength)
   {
   const J9UTF8 *name = J9ROMCLASS_CLASSNAME(romClass);
   // Primitive ROMClasses have different layout (see runtime/vm/romclasses.c): the last
   // ROMClass includes all the others' UTF8 name strings in its romSize, which breaks the
   // generic packing implementation. Pretend that its romSize only includes the header.
   size_t origRomSize = J9ROMCLASS_IS_PRIMITIVE_TYPE(romClass) ? sizeof(*romClass) : romClass->romSize;
   packedSize = origRomSize;

   bool alreadyPacked = !fej9;
   bool isSharedROMClass = !alreadyPacked && fej9->sharedCache() &&
                           fej9->sharedCache()->isROMClassInSharedCache((J9ROMClass *)romClass);
   // Shared ROM classes have their return bytecodes fixed when they are created, and of course if
   // there are no ROM methods there is nothing to fix up. N.B. unnecessary return bytecode fixing
   // won't cause any errors; it's just a waste of time.
   bool needsBytecodeFixing = !alreadyPacked && !(isSharedROMClass || (romClass->romMethodCount == 0));

   bool needsDebugInfoStripping = false;
   if (!alreadyPacked)
      {
      // Walk the methods first to see if any of the debug info is inline, and force debug info stripping in that case
      J9ROMMethod *romMethod = J9ROMCLASS_ROMMETHODS(romClass);
      for (size_t i = 0; i < romClass->romMethodCount; ++i)
         {
         if (J9ROMMETHOD_HAS_DEBUG_INFO(romMethod))
            {
            auto debugInfo = methodDebugInfoFromROMMethod(romMethod);
            // Low bit being set indicates inline debug info
            if (1 == (debugInfo->srpToVarInfo & 1))
               {
               needsDebugInfoStripping = true;
               break;
               }
            }
         romMethod = nextROMMethod(romMethod);
         }

      // Remove intermediate class data (not used by the JIT)
      const uint8_t *icData = J9ROMCLASS_INTERMEDIATECLASSDATA(romClass);
      if (JITServerHelpers::isAddressInROMClass(icData, romClass) && (icData != (const uint8_t *)romClass))
         {
         TR_ASSERT_FATAL(icData + romClass->intermediateClassDataLength == (const uint8_t *)romClass + romClass->romSize,
                         "Intermediate class data not stored at the end of ROMClass %.*s", name->length, name->data);
         packedSize -= romClass->intermediateClassDataLength;
         }
      }

   ROMClassPackContext ctx(trMemory, romClass, origRomSize, !needsDebugInfoStripping, name, generatedPrefixLength);

   if (needsDebugInfoStripping)
      {
      // Populate the segment map
      J9ROMMethod *romMethod = J9ROMCLASS_ROMMETHODS(romClass);
      for (size_t i = 0; i < romClass->romMethodCount; ++i)
         {
         if (J9ROMMETHOD_HAS_DEBUG_INFO(romMethod))
            {
            auto debugInfo = methodDebugInfoFromROMMethod(romMethod);
            size_t inlineSize = 0;
            // Low bit set indicates that the debug info is inline
            if (1 == (*((U_32 *)debugInfo) & 1))
               inlineSize = *((U_32 *)debugInfo) & ~1;
            else
               inlineSize = sizeof(J9SRP);

            ctx._segmentMap.registerDebugInfoSegment(romClass, (const uint8_t *)debugInfo, inlineSize);
            }
         romMethod = nextROMMethod(romMethod);
         }
      }

   // The size of the original ROM class before the UTF8 section, including padding.
   size_t copySize = 0;
   // The size of the packed ROM class before the UTF8 section, including padding.
   size_t packedNonStringSize = 0;
   if (isArrayROMClass(romClass))
      {
      copySize = sizeof(*romClass);
      packedNonStringSize = copySize;
      packedSize = getArrayROMClassPackedSize(romClass);
      }
   else
      {
      // 1st pass: iterate all strings in the ROMClass to compute its total size (including
      // interned strings) and map the strings to their locations in the packed ROMClass.
      // Also compute the _preStringSize of the ROM class if stripping method debug info.
      ctx._callback = sizeInfoCallback;
      allSlotsInROMClassDo((J9ROMClass *)romClass, slotCallback,
                           needsDebugInfoStripping ? sectionEndCallback : NULL, NULL, &ctx);
      // Handle the case when all strings are interned
      auto classEnd = (const uint8_t *)romClass + packedSize;
      ctx._origUtf8SectionStart = std::min(ctx._origUtf8SectionStart, classEnd);

      auto end = ctx._origUtf8SectionEnd ? ctx._origUtf8SectionEnd : classEnd;
      TR_ASSERT_FATAL(ctx._utf8SectionSize == end - ctx._origUtf8SectionStart,
                      "Missed strings in ROMClass %.*s UTF8 section: %zu != %zu",
                      name->length, name->data, ctx._utf8SectionSize, end - ctx._origUtf8SectionStart);
      end = (const uint8_t *)OMR::alignNoCheck((uintptr_t)end, sizeof(uint64_t));
      TR_ASSERT_FATAL(end == classEnd, "UTF8 section not stored at the end of ROMClass %.*s: %p != %p",
                      name->length, name->data, end, classEnd);

      copySize = ctx._origUtf8SectionStart - (const uint8_t *)romClass;
      if (needsDebugInfoStripping)
         {
         // A note to future debuggers: if the assert below failed and
         // ctx._preString < ctx._origUtf8SectionStart - ctx._origRomClassStart, then the most
         // likely cause is that someone added a new section to J9ROMClass and neglected to update
         // romclasswalk.c. Either that or the size of a section is being calculated incorrectly.
         TR_ASSERT_FATAL(
            OMR::alignNoCheck(ctx._preStringSize, sizeof(uint64_t)) == ctx._origUtf8SectionStart - ctx._origRomClassStart,
            "Pre-string end offset in ROMClass %.*s must be within padding of the UTF8 section start: %lu %lu",
            name->length, name->data, ctx._preStringSize, ctx._origUtf8SectionStart - ctx._origRomClassStart
         );
         // See sectionEndCallback() for why this can't be calculated as, say,
         // OMR::alignNoCheck(copySize - ctx._segmentMap.removedDebugInfoSize(), sizeof(uint64_t))
         packedNonStringSize = OMR::alignNoCheck(ctx._preStringSize - ctx._segmentMap.removedDebugInfoSize(), sizeof(uint64_t));
         packedSize = OMR::alignNoCheck(packedNonStringSize + ctx._stringsSize, sizeof(uint64_t));
         }
      else
         {
         packedNonStringSize = copySize;
         packedSize = OMR::alignNoCheck(copySize + ctx._stringsSize, sizeof(uint64_t));
         }
      }

   // Check if expected size matches, otherwise fail early. Size mismatch can occur when JIT client
   // packs a ROMClass before computing its hash when deserializing a JITServer-cached AOT method that
   // was compiled for a different version of the class. In such cases, we can skip the rest of the
   // packing procedure and hash computation since we already know that the ROMClass doesn't match.
   if (expectedSize && (expectedSize != packedSize))
      return NULL;

   ctx._packedRomClass = (J9ROMClass *)trMemory->allocateStackMemory(packedSize);
   if (!ctx._packedRomClass)
      throw std::bad_alloc();
   ctx._newUtf8SectionStart = (const uint8_t *)ctx._packedRomClass + packedNonStringSize;

   // Copy the pre-string section of the original ROM class to the packed ROM class.
   if (needsDebugInfoStripping)
      {
      // Non-shared, non-array ROM classes need to have their inline debug info stripped.
      auto segments = ctx._segmentMap.getSegments();
      // We will have recorded k+1 starts of non-debug info segments, interspersed with the starts of k debug info segments. (k can be zero).
      TR_ASSERT(segments.size() % 2 == 1, "ROM class %.*s has an even number of segments", name->length, name->data);
      // First we copy over the initial 2k (non-debug, debug) pairs
      for (size_t i = 0; i < segments.size() - 1; i += 2)
         {
         auto nonDebugSegment = segments[i];
         auto debugSegment = segments[i+1];
         auto nonDebugSrc = (uint8_t *)romClass + nonDebugSegment._boundaryOffset;
         auto nonDebugDest = (uint8_t *)ctx._packedRomClass + nonDebugSegment.newBoundaryOffset();
         auto debugSrc = (uint8_t *)romClass + debugSegment._boundaryOffset;
         auto debugDest = (uint8_t *)ctx._packedRomClass + debugSegment.newBoundaryOffset();
         memcpy(nonDebugDest, nonDebugSrc, debugSrc - nonDebugSrc);
         memset(debugDest, 0, sizeof(J9SRP));
         }

      // Copy the last run of non-debug info
      auto finalSegment = segments.back();
      size_t finalCopySize = ctx._preStringSize - finalSegment._boundaryOffset;
      memcpy((uint8_t *)ctx._packedRomClass + finalSegment.newBoundaryOffset(),
             (uint8_t *)romClass + finalSegment._boundaryOffset,
             finalCopySize);

      // Zero-out the padding before the UTF8 section
      size_t paddingByteSize = packedNonStringSize - (ctx._preStringSize - ctx._segmentMap.removedDebugInfoSize());
      memset((uint8_t *)ctx._packedRomClass + (packedNonStringSize - paddingByteSize), 0, paddingByteSize);
      }
   else
      {
      memcpy(ctx._packedRomClass, romClass, copySize);
      }

   ctx._packedRomClass->romSize = packedSize;
   ctx._cursor = (uint8_t *)ctx._packedRomClass + packedNonStringSize;

   // If we had to strip out any inline debug info, we also (effectively) zeroed out
   // all the SRPs to any out-of-line debug info that may also have been in the ROM class.
   // Otherwise we must zero out the SRPs to out-of-line method debug info.
   if (!alreadyPacked && !needsDebugInfoStripping)
      {
      J9ROMMethod *romMethod = J9ROMCLASS_ROMMETHODS(ctx._packedRomClass);
      for (size_t i = 0; i < ctx._packedRomClass->romMethodCount; ++i)
         {
         if (J9ROMMETHOD_HAS_DEBUG_INFO(romMethod))
            {
            auto debugInfo = methodDebugInfoFromROMMethod(romMethod);
            TR_ASSERT_FATAL(!(debugInfo->srpToVarInfo & 1), "ROM class %.*s has inline debug info that should have been stripped", name->length, name->data);
            debugInfo->srpToVarInfo = 0;
            }
         romMethod = nextROMMethod(romMethod);
         }
      }

   if (isArrayROMClass(romClass))
      {
      packArrayROMClassData(romClass, ctx);
      }
   else
      {
      // 2nd pass: copy all strings to their locations in the packed ROMClass, and, if debug info was stripped,
      // adjust all SRPs that were invalidated by discarding the method debug info.
      ctx._callback = packCallback;
      ctx._srpCallback = needsDebugInfoStripping ? adjustSRPCallback : NULL;
      ctx._wsrpCallback = needsDebugInfoStripping ? adjustWSRPCallback : NULL;
      allSlotsInROMClassDo((J9ROMClass *)romClass, slotCallback, NULL, NULL, &ctx);
      }

   // Zero out SRP to intermediate class data. This needs to be done after adjustWSRPCallback
   // have run - otherwise the intermediateClassData WSRP being NULL while the original isn't will confuse it.
   ctx._packedRomClass->intermediateClassData = 0;
   ctx._packedRomClass->intermediateClassDataLength = 0;

   // Pad to required alignment
   auto end = (uint8_t *)OMR::alignNoCheck((uintptr_t)ctx._cursor, sizeof(uint64_t));
   TR_ASSERT_FATAL(end == (uint8_t *)ctx._packedRomClass + packedSize, "Invalid final cursor position: %p != %p",
                   end, (uint8_t *)ctx._packedRomClass + packedSize);
   memset(ctx._cursor, 0, end - ctx._cursor);

   if (needsBytecodeFixing)
      fixReturnBytecodes(fej9->getJ9JITConfig()->javaVM->portLibrary, ctx._packedRomClass);

   return ctx._packedRomClass;
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

// To print at the server, run ./jitserver -Xdump:jit:events=user
// Then kill -3 <pidof jitserver>
void
JITServerHelpers::printJITServerMsgStats(J9JITConfig *jitConfig, TR::CompilationInfo *compInfo)
   {
   PORT_ACCESS_FROM_JITCONFIG(jitConfig);

   j9tty_printf(PORTLIB, "JITServer Message Type Statistics:\n");
   j9tty_printf(PORTLIB, "Type# #called");
#if defined(MESSAGE_SIZE_STATS)
   j9tty_printf(PORTLIB, "\t\tMax\t\tMin\t\tMean\t\tStdDev\t\tSum");
#endif /* defined(MESSAGE_SIZE_STATS) */
   j9tty_printf(PORTLIB, "\t\tTypeName\n");

   uint64_t totalMsgCount = 0;
   for (int i = 0; i < JITServer::MessageType_MAXTYPE; ++i)
      {
      if (JITServer::CommunicationStream::_msgTypeCount[i])
         {
         j9tty_printf(PORTLIB, "#%04d %7u", i, JITServer::CommunicationStream::_msgTypeCount[i]);
#if defined(MESSAGE_SIZE_STATS)
         auto &stat = JITServer::CommunicationStream::_msgSizeStats[i];
         j9tty_printf(PORTLIB, "\t%f\t%f\t%f\t%f\t%f",
                      stat.maxVal(), stat.minVal(), stat.mean(), stat.stddev(), stat.sum());
#endif /* defined(MESSAGE_SIZE_STATS) */
         j9tty_printf(PORTLIB, "\t\t%s\n", JITServer::messageNames[i]);
         totalMsgCount += JITServer::CommunicationStream::_msgTypeCount[i];
         }
      }

   j9tty_printf(PORTLIB, "Total number of messages: %llu\n", (unsigned long long)totalMsgCount);
   j9tty_printf(PORTLIB, "Total amount of data received: %llu bytes\n",
                (unsigned long long)JITServer::CommunicationStream::_totalMsgSize);

   uint32_t numCompilations = 0;
   uint32_t numDeserializedMethods = 0;
   if (compInfo->getPersistentInfo()->getRemoteCompilationMode() == JITServer::CLIENT)
      {
      numCompilations = JITServer::CommunicationStream::_msgTypeCount[JITServer::MessageType::compilationCode];
      if (auto deserializer = compInfo->getJITServerAOTDeserializer())
         numDeserializedMethods = deserializer->getNumDeserializedMethods();
      }
   else if (compInfo->getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER)
      {
      numCompilations = JITServer::CommunicationStream::_msgTypeCount[JITServer::MessageType::compilationRequest];
      if (auto aotCacheMap = compInfo->getJITServerAOTCacheMap())
         numDeserializedMethods = aotCacheMap->getNumDeserializedMethods();
      }

   if (numCompilations)
      j9tty_printf(PORTLIB, "Average number of messages per compilation: %f\n", totalMsgCount / float(numCompilations));
   if (numDeserializedMethods)
      j9tty_printf(PORTLIB, "Average number of messages per compilation request (including AOT cache hits): %f\n",
                   totalMsgCount / float(numCompilations + numDeserializedMethods));
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

/*
 * Free the persistent memory allocated for the given `romClass`.
 * If the server uses a global cache of ROMClasses shared among the different JVM clients,
 * this routine decrements the reference count for the `romClass` given as parameter,
 * and only if the ref count reaches 0, the persistent memory is freed.
 */
void
JITServerHelpers::freeRemoteROMClass(J9ROMClass *romClass, TR_PersistentMemory *persistentMemory)
   {
   if (auto cache = TR::CompilationInfo::get()->getJITServerSharedROMClassCache())
      cache->release(romClass);
   else
      persistentMemory->freePersistentMemory(romClass);
   }

/*
 * `cacheRemoteROMClassOrFreeIt` takes a romClass data structure allocated with
 * persistent memory and attempts to cache it in the per-client data session.
 * If caching suceeds, the method returns a pointer to the romClass received as parameter.
 * If the caching fails, the memory for romClass received as parameter is freed
 * and the method returns a pointer to the romClass from the cache
 */
J9ROMClass *
JITServerHelpers::cacheRemoteROMClassOrFreeIt(ClientSessionData *clientSessionData, J9Class *clazz,
                                              J9ROMClass *romClass, const ClassInfoTuple &classInfoTuple)
   {
   OMR::CriticalSection cacheRemoteROMClass(clientSessionData->getROMMapMonitor());
   auto it = clientSessionData->getROMClassMap().find(clazz);
   if (it == clientSessionData->getROMClassMap().end())
      {
      JITServerHelpers::cacheRemoteROMClass(clientSessionData, clazz, romClass, classInfoTuple);
      return romClass;
      }
   // romClass is already present in the cache; must free the duplicate
   JITServerHelpers::freeRemoteROMClass(romClass, clientSessionData->persistentMemory());
   // Return the cached romClass
   return it->second._romClass;
   }

ClientSessionData::ClassInfo &
JITServerHelpers::cacheRemoteROMClass(ClientSessionData *clientSessionData, J9Class *clazz,
                                      J9ROMClass *romClass, const ClassInfoTuple &classInfoTuple)
   {
   ClientSessionData::ClassInfo classInfoStruct(clientSessionData->persistentMemory());

   classInfoStruct._romClass = romClass;
   J9Method *methods = std::get<1>(classInfoTuple);
   classInfoStruct._methodsOfClass = methods;
   classInfoStruct._baseComponentClass = std::get<2>(classInfoTuple);
   classInfoStruct._numDimensions = std::get<3>(classInfoTuple);
   classInfoStruct._parentClass = std::get<4>(classInfoTuple);
   auto &tmpInterfaces = std::get<5>(classInfoTuple);
   auto &persistentAllocator = clientSessionData->persistentMemory()->_persistentAllocator.get();
   classInfoStruct._interfaces = new (persistentAllocator) PersistentVector<TR_OpaqueClassBlock *>(
      tmpInterfaces.begin(), tmpInterfaces.end(),
      PersistentVector<TR_OpaqueClassBlock *>::allocator_type(persistentAllocator)
   );
   auto &methodTracingInfo = std::get<6>(classInfoTuple);
   classInfoStruct._classHasFinalFields = std::get<7>(classInfoTuple);
   classInfoStruct._classDepthAndFlags = std::get<8>(classInfoTuple);
   classInfoStruct._classInitialized = std::get<9>(classInfoTuple);
   classInfoStruct._byteOffsetToLockword = std::get<10>(classInfoTuple);
   classInfoStruct._leafComponentClass = std::get<11>(classInfoTuple);
   classInfoStruct._classLoader = std::get<12>(classInfoTuple);
   classInfoStruct._hostClass = std::get<13>(classInfoTuple);
   classInfoStruct._componentClass = std::get<14>(classInfoTuple);
   classInfoStruct._arrayClass = std::get<15>(classInfoTuple);
   classInfoStruct._totalInstanceSize = std::get<16>(classInfoTuple);
   classInfoStruct._remoteRomClass = std::get<17>(classInfoTuple);
   classInfoStruct._constantPool = (J9ConstantPool *)std::get<18>(classInfoTuple);
   classInfoStruct._classFlags = std::get<19>(classInfoTuple);
   classInfoStruct._classChainOffsetIdentifyingLoader = std::get<20>(classInfoTuple);
   auto &origROMMethods = std::get<21>(classInfoTuple);
   classInfoStruct._classNameIdentifyingLoader = std::get<22>(classInfoTuple);
   classInfoStruct._arrayElementSize = std::get<23>(classInfoTuple);
   classInfoStruct._defaultValueSlotAddress = std::get<24>(classInfoTuple);

   auto result = clientSessionData->getROMClassMap().insert({ clazz, classInfoStruct });

   auto &methodMap = clientSessionData->getJ9MethodMap();
   uint32_t numMethods = romClass->romMethodCount;
   J9ROMMethod *romMethod = J9ROMCLASS_ROMMETHODS(romClass);
   for (uint32_t i = 0; i < numMethods; i++)
      {
      ClientSessionData::J9MethodInfo m(romMethod, origROMMethods[i], (TR_OpaqueClassBlock *)clazz,
                                        i, static_cast<bool>(methodTracingInfo[i]));
      methodMap.insert({ &methods[i], m });
      romMethod = nextROMMethod(romMethod);
      }

   // Return a reference to the ClassInfo structure stored in the map in the client session
   return result.first->second;
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
   TR_J9VM *fe = (TR_J9VM *)TR_J9VMBase::get(vmThread->javaVM->jitConfig, vmThread);
   J9Method *methodsOfClass = (J9Method *)fe->getMethods((TR_OpaqueClassBlock *)clazz);
   int32_t numDims = 0;
   TR_OpaqueClassBlock *baseClass = fe->getBaseComponentClass((TR_OpaqueClassBlock *)clazz, numDims);
   TR_OpaqueClassBlock *parentClass = fe->getSuperClass((TR_OpaqueClassBlock *)clazz);

   uint32_t numMethods = clazz->romClass->romMethodCount;
   std::vector<uint8_t> methodTracingInfo;
   methodTracingInfo.reserve(numMethods);

   std::vector<J9ROMMethod *> origROMMethods;
   origROMMethods.reserve(numMethods);
   for (uint32_t i = 0; i < numMethods; ++i)
      {
      methodTracingInfo.push_back(static_cast<uint8_t>(fe->isMethodTracingEnabled((TR_OpaqueMethodBlock *)&methodsOfClass[i])));
      // record client-side pointers to ROM methods
      origROMMethods.push_back(fe->getROMMethodFromRAMMethod(&methodsOfClass[i]));
      }

   bool classHasFinalFields = fe->hasFinalFieldsInClass((TR_OpaqueClassBlock *)clazz);
   uintptr_t classDepthAndFlags = fe->getClassDepthAndFlagsValue((TR_OpaqueClassBlock *)clazz);
   bool classInitialized = fe->isClassInitialized((TR_OpaqueClassBlock *)clazz);
   uint32_t byteOffsetToLockword = fe->getByteOffsetToLockword((TR_OpaqueClassBlock *)clazz);
   TR_OpaqueClassBlock *leafComponentClass = fe->getLeafComponentClassFromArrayClass((TR_OpaqueClassBlock *)clazz);
   void *classLoader = fe->getClassLoader((TR_OpaqueClassBlock *)clazz);
   TR_OpaqueClassBlock *hostClass = fe->convertClassPtrToClassOffset(clazz->hostClass);
   TR_OpaqueClassBlock *componentClass = fe->getComponentClassFromArrayClass((TR_OpaqueClassBlock *)clazz);
   TR_OpaqueClassBlock *arrayClass = fe->getArrayClassFromComponentClass((TR_OpaqueClassBlock *)clazz);
   uintptr_t totalInstanceSize = clazz->totalInstanceSize;
   uintptr_t cp = fe->getConstantPoolFromClass((TR_OpaqueClassBlock *)clazz);
   uintptr_t classFlags = fe->getClassFlagsValue((TR_OpaqueClassBlock *)clazz);
   auto sharedCache = fe->sharedCache();
   uintptr_t *classChainIdentifyingLoader = NULL;
   uintptr_t classChainOffsetIdentifyingLoader = sharedCache ?
      sharedCache->getClassChainOffsetIdentifyingLoaderNoFail((TR_OpaqueClassBlock *)clazz, &classChainIdentifyingLoader) : 0;

   std::string classNameIdentifyingLoader;
   if (fe->getPersistentInfo()->getJITServerUseAOTCache() &&
       (fe->getPersistentInfo()->getJITServerAOTCacheIgnoreLocalSCC() || classChainIdentifyingLoader))
      {
      auto loader = fe->getClassLoader((TR_OpaqueClassBlock *)clazz);
      auto nameInfo = fe->getPersistentInfo()->getPersistentClassLoaderTable()->lookupClassNameAssociatedWithClassLoader(loader);
      if (nameInfo)
         {
         classNameIdentifyingLoader = std::string((const char *)J9UTF8_DATA(nameInfo), J9UTF8_LENGTH(nameInfo));
         }
      }

   std::string packedROMClassStr;
   std::string romClassHash;
   if (serializeClass)
      {
      TR::StackMemoryRegion stackMemoryRegion(*trMemory);
      size_t hashedSize;
      J9ROMClass *packedROMClass = packROMClass(clazz->romClass, trMemory, fe, hashedSize);
      packedROMClassStr = std::string((const char *)packedROMClass, packedROMClass->romSize);

      auto deserializer = fe->_compInfo->getJITServerAOTDeserializer();
      if (deserializer)
         romClassHash = deserializer->findGeneratedClassHash((J9ClassLoader *)classLoader, clazz, fe, vmThread);
      }

   int32_t arrayElementSize = vmThread->javaVM->internalVMFunctions->arrayElementSize((J9ArrayClass*)clazz);

   // getDefaultValueSlotAddress can only be called if the value type class is initialized
   j9object_t* defaultValueSlotAddress = NULL;
   if (TR::Compiler->om.areValueTypesEnabled() &&
       classInitialized &&
       TR::Compiler->cls.isValueTypeClass(TR::Compiler->cls.convertClassPtrToClassOffset(clazz)))
      {
      defaultValueSlotAddress = vmThread->javaVM->internalVMFunctions->getDefaultValueSlotAddress(clazz);
      }

   return std::make_tuple(
      packedROMClassStr, methodsOfClass, baseClass, numDims, parentClass,
      TR::Compiler->cls.getITable((TR_OpaqueClassBlock *)clazz), methodTracingInfo,
      classHasFinalFields, classDepthAndFlags, classInitialized, byteOffsetToLockword, leafComponentClass,
      classLoader, hostClass, componentClass, arrayClass, totalInstanceSize, clazz->romClass,
      cp, classFlags, classChainOffsetIdentifyingLoader, origROMMethods, classNameIdentifyingLoader, arrayElementSize,
      defaultValueSlotAddress, romClassHash
   );
   }

J9ROMClass *
JITServerHelpers::romClassFromString(const std::string &romClassStr, const std::string &romClassHashStr, TR_PersistentMemory *persistentMemory)
   {
   if (auto cache = TR::CompilationInfo::get()->getJITServerSharedROMClassCache())
      {
      auto hash = romClassHashStr.empty() ? NULL : (const JITServerROMClassHash *)romClassHashStr.data();
      return cache->getOrCreate((const J9ROMClass *)romClassStr.data(), hash);
      }

   auto romClass = (J9ROMClass *)persistentMemory->allocatePersistentMemory(romClassStr.size(), TR_Memory::ROMClass);
   if (!romClass)
      throw std::bad_alloc();
   memcpy(romClass, romClassStr.data(), romClassStr.size());
   return romClass;
   }

J9ROMClass *
JITServerHelpers::getRemoteROMClass(J9Class *clazz, JITServer::ServerStream *stream,
                                    TR_PersistentMemory *persistentMemory, ClassInfoTuple &classInfoTuple)
   {
   stream->write(JITServer::MessageType::ResolvedMethod_getRemoteROMClassAndMethods, clazz);
   auto recv = stream->read<ClassInfoTuple>();
   classInfoTuple = std::get<0>(recv);
   return romClassFromString(std::get<0>(classInfoTuple), std::get<25>(classInfoTuple), persistentMemory);
   }

// Return true if able to get data from cache, return false otherwise.
bool
JITServerHelpers::getAndCacheRAMClassInfo(J9Class *clazz, ClientSessionData *clientSessionData,
                                          JITServer::ServerStream *stream, ClassInfoDataType dataType, void *data)
   {
   if (!clazz)
      return false;

      {
      OMR::CriticalSection getRemoteROMClass(clientSessionData->getROMMapMonitor());
      auto it = clientSessionData->getROMClassMap().find(clazz);
      if (it != clientSessionData->getROMClassMap().end())
         {
         JITServerHelpers::getROMClassData(it->second, dataType, data);
         return true;
         }
      }

   stream->write(JITServer::MessageType::ResolvedMethod_getRemoteROMClassAndMethods, clazz);
   auto recv = stream->read<ClassInfoTuple>();
   auto &classInfoTuple = std::get<0>(recv);

   OMR::CriticalSection cacheRemoteROMClass(clientSessionData->getROMMapMonitor());
   auto it = clientSessionData->getROMClassMap().find(clazz);
   if (it == clientSessionData->getROMClassMap().end())
      {
      auto romClass = romClassFromString(std::get<0>(classInfoTuple), std::get<25>(classInfoTuple), clientSessionData->persistentMemory());
      auto &classInfoStruct = JITServerHelpers::cacheRemoteROMClass(clientSessionData, clazz, romClass, classInfoTuple);
      JITServerHelpers::getROMClassData(classInfoStruct, dataType, data);
      }
   else
      {
      JITServerHelpers::getROMClassData(it->second, dataType, data);
      }
   return false;
   }

// Return true if able to get data from cache, return false otherwise.
bool
JITServerHelpers::getAndCacheRAMClassInfo(J9Class *clazz, ClientSessionData *clientSessionData,
                                          JITServer::ServerStream *stream, ClassInfoDataType dataType1, void *data1,
                                          ClassInfoDataType dataType2, void *data2)
   {
   if (!clazz)
      return false;

      {
      OMR::CriticalSection getRemoteROMClass(clientSessionData->getROMMapMonitor());
      auto it = clientSessionData->getROMClassMap().find(clazz);
      if (it != clientSessionData->getROMClassMap().end())
         {
         JITServerHelpers::getROMClassData(it->second, dataType1, data1);
         JITServerHelpers::getROMClassData(it->second, dataType2, data2);
         return true;
         }
      }

   stream->write(JITServer::MessageType::ResolvedMethod_getRemoteROMClassAndMethods, clazz);
   auto recv = stream->read<ClassInfoTuple>();
   auto &classInfoTuple = std::get<0>(recv);

   OMR::CriticalSection cacheRemoteROMClass(clientSessionData->getROMMapMonitor());
   auto it = clientSessionData->getROMClassMap().find(clazz);
   if (it == clientSessionData->getROMClassMap().end())
      {
      auto romClass = romClassFromString(std::get<0>(classInfoTuple), std::get<25>(classInfoTuple), clientSessionData->persistentMemory());
      auto &classInfoStruct = JITServerHelpers::cacheRemoteROMClass(clientSessionData, clazz, romClass, classInfoTuple);
      JITServerHelpers::getROMClassData(classInfoStruct, dataType1, data1);
      JITServerHelpers::getROMClassData(classInfoStruct, dataType2, data2);
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
         *(uint32_t *)data = classInfo._romClass->modifiers;
         break;
      case CLASSINFO_ROMCLASS_EXTRAMODIFIERS :
         *(uint32_t *)data = classInfo._romClass->extraModifiers;
         break;
      case CLASSINFO_BASE_COMPONENT_CLASS :
         *(TR_OpaqueClassBlock **)data = classInfo._baseComponentClass;
         break;
      case CLASSINFO_NUMBER_DIMENSIONS :
         *(int32_t *)data = classInfo._numDimensions;
         break;
      case CLASSINFO_PARENT_CLASS :
         *(TR_OpaqueClassBlock **)data = classInfo._parentClass;
         break;
      case CLASSINFO_CLASS_HAS_FINAL_FIELDS :
         *(bool *)data = classInfo._classHasFinalFields;
         break;
      case CLASSINFO_CLASS_DEPTH_AND_FLAGS :
         *(uintptr_t *)data = classInfo._classDepthAndFlags;
         break;
      case CLASSINFO_CLASS_INITIALIZED :
         *(bool *)data = classInfo._classInitialized;
         break;
      case CLASSINFO_BYTE_OFFSET_TO_LOCKWORD :
         *(uint32_t *)data = classInfo._byteOffsetToLockword;
         break;
      case CLASSINFO_LEAF_COMPONENT_CLASS :
         *(TR_OpaqueClassBlock **)data = classInfo._leafComponentClass;
         break;
      case CLASSINFO_CLASS_LOADER :
         *(void **)data = classInfo._classLoader;
         break;
      case CLASSINFO_HOST_CLASS :
         *(TR_OpaqueClassBlock **)data = classInfo._hostClass;
         break;
      case CLASSINFO_COMPONENT_CLASS :
         *(TR_OpaqueClassBlock **)data = classInfo._componentClass;
         break;
      case CLASSINFO_ARRAY_CLASS :
         *(TR_OpaqueClassBlock **)data = classInfo._arrayClass;
         break;
      case CLASSINFO_TOTAL_INSTANCE_SIZE :
         *(uintptr_t *)data = classInfo._totalInstanceSize;
         break;
      case CLASSINFO_REMOTE_ROM_CLASS :
         *(J9ROMClass **)data = classInfo._remoteRomClass;
         break;
      case CLASSINFO_CLASS_FLAGS :
         *(uintptr_t *)data = classInfo._classFlags;
         break;
      case CLASSINFO_METHODS_OF_CLASS :
         *(J9Method **)data = classInfo._methodsOfClass;
         break;
      case CLASSINFO_CONSTANT_POOL :
         *(J9ConstantPool **)data = classInfo._constantPool;
         break;
      case CLASSINFO_CLASS_CHAIN_OFFSET_IDENTIFYING_LOADER:
         *(uintptr_t *)data = classInfo._classChainOffsetIdentifyingLoader;
         break;
      case CLASSINFO_ARRAY_ELEMENT_SIZE:
         *(int32_t *)data = classInfo._arrayElementSize;
         break;
      case CLASSINFO_DEFAULT_VALUE_SLOT_ADDRESS:
         *(j9object_t **)data = classInfo._defaultValueSlotAddress;
         break;
      default:
         TR_ASSERT(false, "Class Info not supported %u\n", dataType);
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
JITServerHelpers::postStreamFailure(OMRPortLibrary *portLibrary, TR::CompilationInfo *compInfo, bool retryConnectionImmediately, bool connectionFailure)
   {
   OMR::CriticalSection postStreamFailure(getClientStreamMonitor());

   OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
   uint64_t current_time = omrtime_current_time_millis();
   if (retryConnectionImmediately)
      {
      _nextConnectionRetryTime = current_time;
      }
   else
      {
      if (!_waitTimeMs)
         _waitTimeMs = TR::Options::_reconnectWaitTimeMs;
      if (current_time >= _nextConnectionRetryTime)
         _waitTimeMs *= 2; // Exponential backoff
      _nextConnectionRetryTime = current_time + _waitTimeMs;
      }

   // If this is a connection failure, set the global flag _serverAvailable to false
   // so that other threads can see it and avoid trying too often.
   // If there was another networking problem (read/write error), the client stream
   // will be deleted, the socket will be closed and the client will be forced to
   // reconnect again. The client will discover if the server is down during the connection attempt.
   if ((connectionFailure && !retryConnectionImmediately) || !JITServer::CommunicationStream::shouldReadRetry())
      {
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseJITServerConns))
         {
         if (compInfo->getPersistentInfo()->getServerUID() != 0)
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
                                           "t=%6u Lost connection to the server (serverUID=%llu). Next attempt in %llu ms.",
                                           (uint32_t)compInfo->getPersistentInfo()->getElapsedTime(),
                                           (unsigned long long)compInfo->getPersistentInfo()->getServerUID(),
                                           _waitTimeMs);
            }
         else // Was not connected to a server
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
                                           "t=%6u Could not connect to a server. Next attempt in %llu ms.",
                                           (uint32_t)compInfo->getPersistentInfo()->getElapsedTime(),
                                           _waitTimeMs);
            }
         }

      // For read failures we should not set the server availbility to false
      if (connectionFailure)
         {
         compInfo->getPersistentInfo()->setServerUID(0);
         _serverAvailable = false;
	 }

      // Ensure that log files are not supressed if methods are now
      // going to be compiled locally
      if (TR::Options::requiresDebugObject())
         TR::Options::suppressLogFileBecauseDebugObjectNotCreated(false);

      // Reset the activation policy flag in case we never reconnect to the server
      // and client compiles locally or connects to a new server
      compInfo->setCompThreadActivationPolicy(JITServer::CompThreadActivationPolicy::AGGRESSIVE);
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCompilationThreads) ||
          TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseJITServer))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
                                        "t=%6u Resetting activation policy to AGGRESSIVE because client has lost connection to server",
                                        (uint32_t)compInfo->getPersistentInfo()->getElapsedTime());
         }
      }
   }

void
JITServerHelpers::postStreamConnectionSuccess()
   {
   _serverAvailable = true;
   _waitTimeMs = TR::Options::_reconnectWaitTimeMs;
   }

bool
JITServerHelpers::shouldRetryConnection(OMRPortLibrary *portLibrary)
   {
   OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
   return omrtime_current_time_millis() >= _nextConnectionRetryTime;
   }

bool
JITServerHelpers::isAddressInROMClass(const void *address, const J9ROMClass *romClass)
   {
   return ((address >= romClass) && (address < (((uint8_t*) romClass) + romClass->romSize)));
   }


uintptr_t
JITServerHelpers::walkReferenceChainWithOffsets(TR_J9VM *fe, const std::vector<uintptr_t> &listOfOffsets, uintptr_t receiver)
   {
   uintptr_t result = receiver;
   for (size_t i = 0; i < listOfOffsets.size(); i++)
      {
      result = fe->getReferenceFieldAt(result, listOfOffsets[i]);
      }
   return result;
   }

uintptr_t
JITServerHelpers::getRemoteClassDepthAndFlagsWhenROMClassNotCached(J9Class *clazz, ClientSessionData *clientSessionData,
                                                                   JITServer::ServerStream *stream)
{
   stream->write(JITServer::MessageType::ResolvedMethod_getRemoteROMClassAndMethods, clazz);
   auto recv = stream->read<JITServerHelpers::ClassInfoTuple>();
   auto &classInfoTuple = std::get<0>(recv);

   OMR::CriticalSection cacheRemoteROMClass(clientSessionData->getROMMapMonitor());
   auto it = clientSessionData->getROMClassMap().find(clazz);
   if (it == clientSessionData->getROMClassMap().end())
      {
      auto romClass = JITServerHelpers::romClassFromString(std::get<0>(classInfoTuple), std::get<25>(classInfoTuple), clientSessionData->persistentMemory());
      auto &classInfoStruct = JITServerHelpers::cacheRemoteROMClass(clientSessionData, clazz, romClass, classInfoTuple);
      return classInfoStruct._classDepthAndFlags;
      }
   else
      {
      return it->second._classDepthAndFlags;
      }
}

static void
addRAMClassToChain(std::vector<J9Class *> &chain, J9Class *clazz, std::vector<J9Class *> &uncached,
                   PersistentUnorderedSet<J9Class *> &cached)
   {
   chain.push_back(clazz);
   if (cached.insert(clazz).second)
      uncached.push_back(clazz);
   }

std::vector<J9Class *>
JITServerHelpers::getRAMClassChain(J9Class *clazz, size_t numClasses, J9VMThread *vmThread, TR_Memory *trMemory,
                                   TR::CompilationInfo *compInfo, std::vector<J9Class *> &uncachedRAMClasses,
                                   std::vector<ClassInfoTuple> &uncachedClassInfos)
   {
   TR_ASSERT(uncachedRAMClasses.empty(), "Must pass empty vector");
   TR_ASSERT(uncachedClassInfos.empty(), "Must pass empty vector");
   TR_ASSERT(numClasses <= TR_J9SharedCache::maxClassChainLength, "Class chain is too long");

   std::vector<J9Class *> chain;
   chain.reserve(numClasses);
   uncachedRAMClasses.reserve(numClasses);
   auto &cached = compInfo->getclassesCachedAtServer();

      {
      OMR::CriticalSection cs(compInfo->getclassesCachedAtServerMonitor());

      addRAMClassToChain(chain, clazz, uncachedRAMClasses, cached);
      for (size_t i = 0; i < J9CLASS_DEPTH(clazz); ++i)
         addRAMClassToChain(chain, clazz->superclasses[i], uncachedRAMClasses, cached);
      for (auto it = (J9ITable *)clazz->iTable; it; it = it->next)
         addRAMClassToChain(chain, it->interfaceClass, uncachedRAMClasses, cached);
      TR_ASSERT(chain.size() == numClasses, "Invalid RAM class chain length: %zu != %zu", chain.size(), numClasses);
      }

   uncachedClassInfos.reserve(uncachedRAMClasses.size());
   for (J9Class *c : uncachedRAMClasses)
      uncachedClassInfos.push_back(packRemoteROMClassInfo(c, vmThread, trMemory, true));

   return chain;
   }

void
JITServerHelpers::cacheRemoteROMClassBatch(ClientSessionData *clientData, const std::vector<J9Class *> &ramClasses,
                                           const std::vector<ClassInfoTuple> &classInfoTuples)
   {
   TR_ASSERT_FATAL(ramClasses.size() == classInfoTuples.size(), "Must have equal length");

   for (size_t i = 0; i < ramClasses.size(); ++i)
      {
      auto romClass = romClassFromString(std::get<0>(classInfoTuples[i]), std::get<25>(classInfoTuples[i]), clientData->persistentMemory());
      cacheRemoteROMClassOrFreeIt(clientData, ramClasses[i], romClass, classInfoTuples[i]);
      }
   }

uint64_t
JITServerHelpers::generateUID()
   {
   // Collisions are possible, but very unlikely.
   // Using more bits for the UID can reduce the probability of a collision further.
   std::random_device rd;
   std::mt19937_64 rng(rd());
   std::uniform_int_distribution<uint64_t> dist;
   // Generated uid must not be 0
   uint64_t uid = dist(rng);
   while (0 == uid)
      uid = dist(rng);
   return uid;
   }


//TODO: support arrays of generated classes for completeness

uint32_t
JITServerHelpers::getFullClassNameLength(const J9ROMClass *romClass, const J9ROMClass *baseComponent,
                                         uint32_t numDimensions, bool checkGenerated)
   {
   if (!numDimensions)
      {
      size_t prefixLen = checkGenerated ? getGeneratedClassNamePrefixLength(romClass) : 0;
      return prefixLen ? prefixLen : J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(romClass));
      }

   TR_ASSERT(baseComponent, "Invalid arguments");
   return J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(baseComponent)) + numDimensions +
          (J9ROMCLASS_IS_ARRAY(baseComponent) ? 0 : 2);
   }

void
JITServerHelpers::getFullClassName(uint8_t *name, uint32_t length, const J9ROMClass *romClass,
                                   const J9ROMClass *baseComponent, uint32_t numDimensions, bool checkGenerated)
   {
   TR_ASSERT(length == getFullClassNameLength(romClass, baseComponent, numDimensions, checkGenerated), "Invalid length");

   if (!numDimensions)
      {
      memcpy(name, J9UTF8_DATA(J9ROMCLASS_CLASSNAME(romClass)), length);
      return;
      }

   TR_ASSERT(baseComponent, "Invalid arguments");
   // The base component is the class that represents the leaf of the tree that is the (generally multi-dimensional)
   // array. For primitive arrays of any number of dimensions, the base component is always a 1D primitive array
   // (e.g., "[I"), while for object arrays the base component is always a regular (non-primitive) class.
   bool primitive = J9ROMCLASS_IS_ARRAY(baseComponent);
   const J9UTF8 *baseName = J9ROMCLASS_CLASSNAME(baseComponent);
   uint32_t baseNameLength = J9UTF8_LENGTH(baseName);

   // Build the array class signature that will be passed to jitGetClassInClassloaderFromUTF8() on the client.
   uint32_t i;
   for (i = 0; i < numDimensions; ++i)
      name[i] = '[';
   if (!primitive)
      name[i++] = 'L';
   memcpy(name + i, J9UTF8_DATA(baseName), baseNameLength);
   if (!primitive)
      name[i + baseNameLength] = ';';
   }


size_t
JITServerHelpers::getGeneratedClassNamePrefixLength(const J9UTF8 *name)
   {
   if (TR::Options::_aotCacheDisableGeneratedClassSupport)
      return 0;

   auto nameStr = (const char *)J9UTF8_DATA(name);
   size_t nameLen = J9UTF8_LENGTH(name);

   // Check if this is a lambda class
   size_t prefixLen = 0;
   if (isLambdaClassName(nameStr, nameLen, &prefixLen))
      {
      TR_ASSERT(prefixLen, "Invalid lambda class name %.*s", (int)nameLen, nameStr);
      return prefixLen;
      }

   //NOTE: Current implementation does not support lambda forms since the AOT compiler does not support them
   // either. If support is to be implemented in the future, note that there can be multiple identical (except
   // for the non-deterministic part of the class name) lambda form classes loaded in the same JVM instance.

   // Check if this is a generated dynamic proxy class: com/sun/proxy/$Proxy<index>
   static const char proxyPrefix[] = "com/sun/proxy/$Proxy";
   static const size_t proxyPrefixLen = sizeof(proxyPrefix) - 1;
   if ((nameLen > proxyPrefixLen) && (memcmp(nameStr, proxyPrefix, proxyPrefixLen) == 0))
      return proxyPrefixLen;

   // Check if this is a generated reflection accessor class: sun/reflect/Generated<kind>Accessor<index>
   static const char accessorPrefixStart[] = "sun/reflect/Generated";
   static const char accessorPrefixEnd[] = "Accessor";
   static const size_t accessorPrefixStartLen = sizeof(accessorPrefixStart) - 1;
   static const size_t accessorPrefixEndLen = sizeof(accessorPrefixEnd) - 1;
   if ((nameLen > accessorPrefixStartLen + accessorPrefixEndLen) &&
       (memcmp(nameStr, accessorPrefixStart, accessorPrefixStartLen) == 0))
      {
      // Using std::search() as a portable alternative to memmem() which is a GNU extension
      const char *kindEnd = std::search(nameStr + accessorPrefixStartLen + 1, nameStr + nameLen,
                                        accessorPrefixEnd, accessorPrefixEnd + accessorPrefixEndLen);
      if (kindEnd != nameStr + nameLen)
         return kindEnd + accessorPrefixEndLen - nameStr;
      }

   return 0;
   }
