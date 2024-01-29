/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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

#ifndef J9_AHEADOFTIMECOMPILE_HPP
#define J9_AHEADOFTIMECOMPILE_HPP

#ifndef J9_AHEADOFTIMECOMPILE_CONNECTOR
#define J9_AHEADOFTIMECOMPILE_CONNECTOR
namespace J9 { class AheadOfTimeCompile; }
namespace J9 { typedef J9::AheadOfTimeCompile AheadOfTimeCompileConnector; }
#endif // J9_AHEADOFTIMECOMPILE_CONNECTOR

#include "codegen/OMRAheadOfTimeCompile.hpp"
#include "runtime/RelocationRecord.hpp"

#include <stdint.h>
#include "env/jittypes.h"

namespace TR { class Compilation; }
class AOTCacheRecord;
class AOTCacheClassChainRecord;
class AOTCacheWellKnownClassesRecord;


namespace J9
{

class OMR_EXTENSIBLE AheadOfTimeCompile : public OMR::AheadOfTimeCompileConnector
   {
public:
   static const size_t SIZEPOINTER = sizeof(uintptr_t);

   AheadOfTimeCompile(uint32_t *headerSizeMap, TR::Compilation *c) :
      OMR::AheadOfTimeCompileConnector(headerSizeMap, c)
      {
      }

   /**
    * @brief Gets the class chain offset for a given RAMClass. Calls TR_SharedCache::rememberClass()
    *        and fails the compilation if the class chain cannot be created.
    *
    * @param[in] classToRemember the RAMClass to get the class chain offset for
    * @param[out] classChainRecord pointer to the AOT cache class chain record corresponding to the class chain, if this
    *                              is an out-of-process compilation that will be stored in JITServerAOT cache;
    *                              ignored for local compilations and out-of-process compilations not stored in AOT cache
    * @return class chain SCC offset
    */
   uintptr_t getClassChainOffset(TR_OpaqueClassBlock *classToRemember, const AOTCacheClassChainRecord *&classChainRecord);

   uintptr_t findCorrectInlinedSiteIndex(void *constantPool, uintptr_t currentInlinedSiteIndex);

#if defined(J9VM_OPT_JITSERVER)
   /**
    * @brief Adds an AOT cache class record corresponding to a ROMClass SCC offset
    *        if this out-of-process compilation will be stored in AOT cache.
    *
    * The resulting AOT cache class record is the root class record of the class chain record.
    *
    * @param classChainRecord pointer to the AOT cache class chain record for the class
    * @param romClassOffsetAddr pointer to the binary relocation record field that stores the ROMClass SCC offset
    */
   void addClassSerializationRecord(const AOTCacheClassChainRecord *classChainRecord, const uintptr_t *romClassOffsetAddr);

   /**
    * @brief Adds an AOT cache class record corresponding to a ROMClass SCC offset
    *        if this out-of-process compilation will be stored in AOT cache.
    *
    * The AOT cache class record is looked up in the client session or created
    * on demand, possibly requesting missing information from the client.
    *
    * @param ramClass the RAMClass that the ROMClass SCC offset is for
    * @param romClassOffsetAddr pointer to the binary relocation record field that stores the ROMClass SCC offset
    */
   void addClassSerializationRecord(TR_OpaqueClassBlock *ramClass, const uintptr_t *romClassOffsetAddr);

   /**
    * @brief Adds an AOT cache method record corresponding to a ROMMethod SCC offset
    *        if this out-of-process compilation will be stored in AOT cache.
    *
    * The AOT cache method record is looked up in the client session or created
    * on demand, possibly requesting missing information from the client.
    *
    * @param method the RAMMethod that the ROMMethod SCC offset is for
    * @param definingClass RAMClass for the defining class of the method
    * @param romMethodOffsetAddr pointer to the binary relocation record field that stores the ROMMethod SCC offset
    */
   void addMethodSerializationRecord(J9Method *method, TR_OpaqueClassBlock *definingClass, const uintptr_t *romMethodOffsetAddr);

   /**
    * @brief Adds an AOT cache class chain record corresponding to a class chain SCC
    *        offset if this out-of-process compilation will be stored in AOT cache.
    *
    * @param classChainRecord pointer to the AOT cache class chain record
    * @param classChainOffsetAddr pointer to the binary relocation record field that stores the class chain SCC offset
    */
   void addClassChainSerializationRecord(const AOTCacheClassChainRecord *classChainRecord, const uintptr_t *classChainOffsetAddr);

   /**
    * @brief Adds an AOT cache class loader record corresponding to a class chain SCC offset identifying
    *        a class loader if this out-of-process compilation will be stored in AOT cache.
    *
    * The resulting AOT cache class loader record is the class loader record of the root class of the class chain.
    * Note that while the AOT cache class chain record passed to this method is the one for the class being
    * remembered, i.e. the same as the one passed to addClassChainSerializationRecord(), the AOT cache record
    * associated with this SCC offset is the one identifying the class loader of the class being remembered.
    * The JITServer AOT cache identifies class loaders by the name of the first loaded class (not by its class
    * chain), hence the AOT cache class loader records are a distinct type from the class chain records.
    *
    * @param classChainRecord pointer to the AOT cache class chain record for the class
    *                         whose loader is identified by the class chain SCC offset
    * @param loaderChainOffsetAddr pointer to the binary relocation record field that stores
    *                              the class chain SCC offset identifying the class loader
    */
   void addClassLoaderSerializationRecord(const AOTCacheClassChainRecord *classChainRecord, const uintptr_t *loaderChainOffsetAddr);

   /**
    * @brief Adds an AOT cache well-known classes record corresponding to a well-known classes
    *        SCC offset if this out-of-process compilation will be stored in AOT cache.
    *
    * @param wkcRecord pointer to the AOT cache well-known classes record
    * @param wkcOffsetAddr pointer to the binary relocation record field that stores the well-known classes SCC offset
    */
   void addWellKnownClassesSerializationRecord(const AOTCacheWellKnownClassesRecord *wkcRecord, const uintptr_t *wkcOffsetAddr);
#endif /* defined(J9VM_OPT_JITSERVER) */

   void dumpRelocationData();
   uint8_t* dumpRelocationHeaderData(uint8_t *cursor, bool isVerbose);

   /**
    * @brief Initializes the relocation record header.
    *
    * @param relocation pointer to the iterated external relocation
    * @return pointer into the buffer right after the fields of the header (ie the offsets section)
    */
   virtual uint8_t *initializeAOTRelocationHeader(TR::IteratedExternalRelocation *relocation);

   /**
    * @brief Initialization of relocation record headers for whom data for the fields are acquired
    *        in a manner that is specific to a particular platform. This is the default implementation.
    *
    * @param relocation pointer to the iterated external relocation
    * @param reloTarget pointer to the TR_RelocationTarget object
    * @param reloRecord pointer to the TR_RelocationRecord object
    * @param targetKind the TR_ExternalRelocationTargetKind enum value
    *
    * @return true if a platform specific relocation record header was initialized; false otherwise
    */
   bool initializePlatformSpecificAOTRelocationHeader(TR::IteratedExternalRelocation *relocation, TR_RelocationTarget *reloTarget, TR_RelocationRecord *reloRecord, uint8_t targetKind)
      { return false; }

   static void interceptAOTRelocation(TR::ExternalRelocation *relocation);

   uint32_t getSizeOfAOTRelocationHeader(TR_ExternalRelocationTargetKind k) { return TR_RelocationRecord::getSizeOfAOTRelocationHeader(k); }
   uint32_t *setAOTRelocationKindToHeaderSizeMap(uint32_t *p) { TR_ASSERT_FATAL(false, "Should not be called!\n"); return 0; }

   /**
    * Return true if an ExternalRelocation of kind TR_ClassAddress is expected
    * to contain a pointer to TR_RelocationRecordInformation.
    */
   static bool classAddressUsesReloRecordInfo() { return false; }

protected:

#if defined(J9VM_OPT_JITSERVER)
   /**
    * @brief Associates an AOT cache record with the corresponding SCC offset stored in AOT relocation data.
    *
    * If this is an out-of-process compilation that will be stored in JITServer AOT cache, computes the offset into
    * AOT relocation data and calls Compilation::addSerializationRecord(record, offset), otherwise does nothing.
    *
    * @param record pointer to the AOT cache record to be added to this compilation
    * @param sccOffsetAddr pointer to the binary relocation record field that stores the SCC offset
    */
   void addSerializationRecord(const AOTCacheRecord *record, const uintptr_t *sccOffsetAddr);
#endif /* defined(J9VM_OPT_JITSERVER) */

   /**
    * @brief TR_J9SharedCache::offsetInSharedCacheFrom* asserts if the pointer
    * passed in does not exist in the SCC. Under HCR, when an agent redefines
    * a class, it causes the J9Class pointer to stay the same, but the
    * J9ROMClass pointer changes. This means that if the compiler has a
    * reference to a J9Class who J9ROMClass was in the SCC at one point in the
    * compilation, it may no longer be so at another point in the compilation.
    *
    * This means that the compilation is no longer valid and should be aborted.
    * Even if there isn't an abort during the compilation, at the end of the
    * compilation, the compiler will fail the compile if such a redefinition
    * occurred.
    *
    * Calling TR_J9SharedCache::offsetInSharedCacheFromPointer after such a
    * redefinition could result in an assert. Therefore, this method exists as
    * a wrapper around TR_J9SharedCache::isROMClassInSharedCache which doesn't
    * assert and conveniently, updates the location referred to by the cacheOffset
    * pointer passed in as a parameter.
    *
    * If the ptr isn't in the SCC, then the current method will abort the
    * compilation. If the ptr is in the SCC, then the cacheOffset will be updated.
    *
    * @param sharedCache pointer to the TR_SharedCache object
    * @param romClass J9ROMClass * whose offset in the SCC is required
    * @return The offset into the SCC of romClass
    */
   uintptr_t offsetInSharedCacheFromROMClass(TR_SharedCache *sharedCache, J9ROMClass *romClass);

   /**
    * @brief Same circumstance as offsetInSharedCacheFromROMClass above
    *
    * @param sharedCache pointer to the TR_SharedCache object
    * @param romMethod J9ROMMethod * whose offset in the SCC is required
    * @return The offset into the SCC of romMethod
    */
   uintptr_t offsetInSharedCacheFromROMMethod(TR_SharedCache *sharedCache, J9ROMMethod *romMethod);

   /**
    * @brief Wrapper around TR_J9SharedCache::offsetInSharedCacheFromPointer for
    *        consistency with the above APIs
    *
    * @param sharedCache pointer to the TR_SharedCache object
    * @param ptr pointer whose offset in the SCC is required
    * @return The offset into the SCC of ptr
    */
   uintptr_t offsetInSharedCacheFromPointer(TR_SharedCache *sharedCache, void *ptr);

   /**
    * @brief Initialization of relocation record headers for whom data for the fields are acquired
    *        in a manner that is common on all platforms
    *
    * @param relocation pointer to the iterated external relocation
    * @param reloTarget pointer to the TR_RelocationTarget object
    * @param reloRecord pointer to the associated TR_RelocationRecord API object
    * @param kind the TR_ExternalRelocationTargetKind enum value
    */
   void initializeCommonAOTRelocationHeader(TR::IteratedExternalRelocation *relocation, TR_RelocationTarget *reloTarget, TR_RelocationRecord *reloRecord, uint8_t kind);

   /**
    * @brief Common relocation processing for AOT
    */
   void processRelocations();
   };

}

#endif // J9_AHEADOFTIMECOMPILE_HPP
