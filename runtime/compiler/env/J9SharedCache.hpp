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

#ifndef J9SHARED_CACHE_HPP
#define J9SHARED_CACHE_HPP

#include "compiler/env/SharedCache.hpp"

#include <stdint.h>
#include <map>
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "runtime/J9Runtime.hpp"
#include "runtime/RuntimeAssumptions.hpp"

class TR_J9VMBase;
class TR_ResolvedMethod;
namespace TR { class CompilationInfo; }
#if defined(J9VM_OPT_JITSERVER)
namespace TR { class CompilationInfoPerThread; }
namespace JITServer { class ServerStream; }
class JITServerNoSCCAOTDeserializer;
#endif

struct J9SharedClassConfig;
struct J9SharedClassCacheDescriptor;
struct J9SharedDataDescriptor;

/**
 * \brief An interface to the VM's shared class cache.
 *
 * This class provides an interface to the VM's shared class cache as represented
 * by the descriptors in J9SharedClassConfig::cacheDescriptorList.The cache
 * descriptor list is a circular linked list. It is doubly linked when
 * J9VM_OPT_MULTI_LAYER_SHARED_CLASS_CACHE is defined or singly linked otherwise.
 *
 * If J9VM_OPT_MULTI_LAYER_SHARED_CLASS_CACHE is not defined, the list consists of
 * a single element who's next pointer refers back to itself. Offsets into the
 * shared cache represent the distance from the start of the rom classes section or
 * or start of the metadata section; the offset is then left shifted 1 bit in order
 * to use the low bit to determine whether the offset is relative to the start of
 * the rom classes section or the start of the metadata section. Converting between
 * pointers and offsets can be done with simple pointer arithmetic.
 *
 * If J9VM_OPT_MULTI_LAYER_SHARED_CLASS_CACHE is defined, the head of the list
 * represents the top-most (and therefore writable) layer of the shared class cache,
 * the next element represents the (N-1)th layer, and so on. The element previous to
 * the head represents the base layer. The list of cache layers can be thought of as
 * a single logical cache starting at layer 0 and ending at layer N. Offsets into the
 * shared cache represent the distance from the start of the cache. Converting
 * between pointers and offsets requires traversing the list starting at the base
 * layer.
 *
 * The layout of section of the SCC relevant for this class is:
 *
 *        Increasing Address ----->
 * *-------------*------------------*----------*
 * | ROM CLASSES |--->   FREE   <---| METADATA |
 * *-------------*------------------*----------*
 * ^             ^                  ^          ^
 * |             |                  |          |
 * |             |                  |          cacheDesc->metadataStartAddress
 * |             |                  |
 * |             |                  UPDATEPTR(cacheDesc->cacheStartAddress)
 * |             |
 * |             SEGUPDATEPTR(cacheDesc->cacheStartAddress)
 * |
 * cacheDesc->romclassStartAddress
 *
 * See CompositeCache.cpp for more details.
 */
class TR_J9SharedCache : public TR_SharedCache
   {
public:
   TR_ALLOC_SPECIALIZED(TR_Memory::SharedCache)

   TR_J9SharedCache(TR_J9VMBase *fe);
   TR_J9VMBase *fe() { return _fe; }
   virtual bool isHint(TR_ResolvedMethod *, TR_SharedCacheHint, uint16_t *dataField = NULL);
   virtual bool isHint(J9Method *, TR_SharedCacheHint, uint16_t *dataField = NULL);
   virtual uint16_t getAllEnabledHints(J9Method *method);
   virtual void addHint(J9Method *, TR_SharedCacheHint);
   virtual void addHint(TR_ResolvedMethod *, TR_SharedCacheHint);
   virtual bool isMostlyFull();

   static void validateAOTHeader(J9JITConfig *jitConfig, J9VMThread *vmThread, TR::CompilationInfo *compInfo);

   /**
    * \brief Converts a shared cache offset, calculated from the end of the SCC, into the
    *        metadata section of the SCC into a pointer.
    *
    * The pointer returned from this function should be considered opaque by the consumers of this API,
    * as some subclasses (like TR_J9DeserializerSharedCache) might not return, say, an actual (uintptr *) class chain
    * given an offset that is ostensibly to such a chain.
    *
    * \param[in] offset The offset to convert.
    * \return A pointer. Raises a fatal assertion before returning NULL if the offset is invalid.
    */
   virtual void *pointerFromOffsetInSharedCache(uintptr_t offset);

   /**
    * \brief Converts a pointer into the metadata section of the SCC into an offset, calculated
    *        from the end of the SCC.
    *
    * \param[in] ptr The pointer to convert.
    * \return An offset. Raises a fatal assertion before returning 0 if the pointer is invalid.
    */
   virtual uintptr_t offsetInSharedCacheFromPointer(void *ptr);

   /**
    * \brief Converts an offset into the ROMClass section into a J9ROMClass *.
    *
    * \param[in] offset The offset to convert.
    * \return A J9ROMClass *. Raises a fatal assertion before returning NULL if the offset is invalid.
    */
   virtual J9ROMClass *romClassFromOffsetInSharedCache(uintptr_t offset);

   /**
    * \brief Converts a J9ROMClass * pointer into the SCC into an offset.
    *
    * \param[in] romClass The J9ROMClass * to convert
    * \return An offset. Raises a fatal assertion before returning 0 if the pointer is invalid.
    */
   virtual uintptr_t offsetInSharedCacheFromROMClass(J9ROMClass *romClass);

   /**
    * \brief Converts an offset into the ROMClass section into a J9ROMMethod *.
    *
    * \param[in] offset The offset to convert
    * \return A J9ROMMethod *. Raises a fatal assertion before returning NULL if the offset is invalid.
    */
   virtual J9ROMMethod *romMethodFromOffsetInSharedCache(uintptr_t offset);

   /**
    * \brief Converts a J9ROMMethod * pointer into the SCC into an offset.
    *
    * \param[in] romMethod The J9ROMMethod * to convert
    * \return An offset. Raises a fatal assertion before returning INVALID_ROM_METHOD_OFFSET if the pointer is invalid.
    */
   virtual uintptr_t offsetInSharedCacheFromROMMethod(J9ROMMethod *romMethod);

   /**
    * \brief Converts an offset into the ROMClass section into a pointer.
    *
    * \param[in] offset The offset to convert
    * \return A pointer. Raises a fatal assertion before returning NULL if the offset is invalid.
    */
   virtual void *ptrToROMClassesSectionFromOffsetInSharedCache(uintptr_t offset);

   /**
    * \brief Converts a pointer into the ROM Classes section of the SCC into an offset.
    *
    * \param[in] ptr The pointer to convert
    * \return  An offset. Raises a fatal assertion before returning 0 if the pointer is invalid.
    */
   virtual uintptr_t offsetInSharedCacheFromPtrToROMClassesSection(void *ptr);


   virtual void persistIprofileInfo(TR::ResolvedMethodSymbol *, TR::Compilation *comp);
   virtual void persistIprofileInfo(TR::ResolvedMethodSymbol *, TR_ResolvedMethod*, TR::Compilation *comp);

   static const uint32_t maxClassChainLength = 32;

   virtual bool canRememberClass(TR_OpaqueClassBlock *classPtr)
      {
      return rememberClass((J9Class *)classPtr, NULL, false) != TR_SharedCache::INVALID_CLASS_CHAIN_OFFSET;
      }

   virtual uintptr_t rememberClass(TR_OpaqueClassBlock *classPtr,
                                    const AOTCacheClassChainRecord **classChainRecord = NULL)
      {
      return rememberClass((J9Class *)classPtr, classChainRecord, true);
      }

   virtual uintptr_t rememberClass(J9Class *clazz, const AOTCacheClassChainRecord **classChainRecord = NULL,
                                    bool create = true);

   virtual UDATA storeStringToSCC(const char *string, uintptr_t length);
   virtual const char *getStringFromSCC(UDATA offset);

   virtual bool classMatchesCachedVersion(J9Class *clazz, UDATA *chainData=NULL);
   virtual bool classMatchesCachedVersion(TR_OpaqueClassBlock *classPtr, UDATA *chainData=NULL)
      {
      return classMatchesCachedVersion((J9Class *) classPtr, chainData);
      }

   /**
    * \brief Converts a pointer to a class chain associated to a class loader in the SCC into a J9ClassLoader,
    *        if it can be found in the running JVM.
    *
    * \param[in] chainData The pointer to convert, which should have come from pointerFromOffsetInSharedCache().
    * \return A pointer. Raises a fatal assertion before returning NULL if the pointer is invalid.
    */
   virtual void *lookupClassLoaderAssociatedWithClassChain(void *chainData);
   virtual TR_OpaqueClassBlock *lookupClassFromChainAndLoader(uintptr_t *chainData, void *classLoader,
                                                              TR::Compilation *comp);

   /**
    * \brief Checks whether the specified pointer points into the metadata section
    *        of the shared cache.
    *
    * \param[in] ptr The pointer to check.
    * \param[out] cacheOffset If ptr points into the shared cache and this parameter
    *             is not NULL the result of converting ptr into an offset will be
    *             returned here. If ptr does not point into the shared cache this
    *             parameter is ignored. The offset is calculated from the end of
    *             the SCC.
    * \return True if the pointer points into the shared cache, false otherwise.
    */
   virtual bool isPointerInSharedCache(void *ptr, uintptr_t *cacheOffset = NULL);

   /**
    * \brief Checks whether the specified offset, calculated from the end of the SCC,
    *        is within the metadata section of the shared cache.
    *
    * \param[in] offset The offset to check.
    * \param[out] ptr If offset is within the shared cache and this parameter is not
    *             NULL the result of converting offset into a pointer will be returned
    *             here. If offset is not within the shared cache this parameter is ignored.
    * \return True if the offset is within the shared cache, false otherwise.
    */
   virtual bool isOffsetInSharedCache(uintptr_t encoded_offset, void *ptr = NULL);

   /**
    * \brief Checks whether the J9ROMClass underlying the given class exists in the SCC
    *
    * \param[in] clazz The J9Class * to check
    * \param[out] cacheOffset If the J9ROMClass associated to the J9Class is in the SCC and this parameter
    *             is not NULL the result of converting romClass into an offset will
    *             be returned here. If it does not point into the SCC, this
    *             parameter is ignored.
    * \return True if romClass points into the SCC, false otherwise.
    */
   virtual bool isClassInSharedCache(TR_OpaqueClassBlock *clazz, uintptr_t *cacheOffset = NULL);
   virtual bool isClassInSharedCache(J9Class *clazz, uintptr_t *cacheOffset = NULL)
      { return isClassInSharedCache(reinterpret_cast<TR_OpaqueClassBlock *>(clazz), cacheOffset); }

   /**
    * \brief Checks whether the specified J9ROMClass exists in the SCC
    *
    * \param[in] romClass The J9ROMClass * to check
    * \param[out] cacheOffset If the J9ROMClass is in the SCC and this parameter
    *             is not NULL the result of converting romClass into an offset will
    *             be returned here. If romClass does not point into the SCC, this
    *             parameter is ignored.
    * \return True if romClass points into the SCC, false otherwise.
    */
   virtual bool isROMClassInSharedCache(J9ROMClass *romClass, uintptr_t *cacheOffset = NULL);

   /**
    * \brief Checks whether the specified offset is within the ROMClass section
    *        of the shared cache.
    *
    * \param[in] offset The offset to check
    * \param[out] romClass If offset is within the shared cache and this parameter is not
    *             NULL the result of converting offset into a J9ROMClass * will be returned
    *             here. If offset is not within the shared cache this parameter is ignored.
    * \return True if the offset is within the shared cache, false otherwise.
    */
   virtual bool isROMClassOffsetInSharedCache(uintptr_t offset, J9ROMClass **romClassPtr = NULL);

   /**
    * \brief Checks whether the persisent representation of a J9Method exists in the SCC
    *
    * \param[in] method The J9Method * to check
    * \param[in] definingClass The defining class of method. Unused in the base J9SharedCache implementation of this method.
    * \param[out] cacheOffset If the J9ROMMethod of the J9Method is in the SCC and this parameter
    *             is not NULL the result of converting that J9ROMMethod into an offset will
    *             be returned here. If it is not in the SCC, this parameter is ignored.
    * \return True if the J9Method's romMethod exists in the SCC, false otherwise.
    */
   virtual bool isMethodInSharedCache(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *definingClass, uintptr_t *cacheOffset = NULL);

   /**
    * \brief Checks whether the specified J9ROMMethod exists in the SCC
    *
    * \param[in] romMethod The J9ROMMethod * to check
    * \param[out] cacheOffset If the J9ROMMethod is in the SCC and this parameter
    *             is not NULL the result of converting romMethod into an offset will
    *             be returned here. If romMethod does not point into the SCC, this
    *             parameter is ignored.
    * \return True if romMethod points into the SCC, false otherwise.
    */
   virtual bool isROMMethodInSharedCache(J9ROMMethod *romMethod, uintptr_t *cacheOffset = NULL);

   /**
    * \brief Checks whether the specified offset is within the ROMClass section
    *        of the shared cache.
    * \param[in] offset The offset to check
    * \param[out] romMethodPtr If offset is within the shared cache and this parameter is not
    *             NULL the result of converting offset into a J9ROMMethod * will be returned
    *             here. If offset is not within the shared cache this parameter is ignored.
    * \return True if the offset is within the shared cache, false otherwise.
    */
   virtual bool isROMMethodOffsetInSharedCache(uintptr_t offset, J9ROMMethod **romMethodPtr = NULL);

   /**
    * \brief Checks whether the specified pointer points into the ROMClass section
    *        of the shared cache.
    *
    * \param[in] ptr The pointer to check
    * \param[out] cacheOffset If ptr points into the shared cache and this parameter
    *             is not NULL the result of converting ptr into an offset will be
    *             returned here. If ptr does not point into the shared cache this
    *             parameter is ignored.
    * \return True if the pointer points into the shared cache, false otherwise.
    */
   virtual bool isPtrToROMClassesSectionInSharedCache(void *ptr, uintptr_t *cacheOffset = NULL);

   /**
    * \brief Checks whether the specified offset is within the ROMClass section
    *        of the shared cache.
    *
    * \param[in] offset The offset to check.
    * \param[out] ptr If offset is within the shared cache and this parameter is not
    *             NULL the result of converting offset into a pointer will be returned
    *             here. If offset is not within the shared cache this parameter is ignored.
    * \return True if the offset is within the shared cache, false otherwise.
    */
   virtual bool isOffsetOfPtrToROMClassesSectionInSharedCache(uintptr_t offset, void **ptr = NULL);

   J9ROMClass *startingROMClassOfClassChain(UDATA *classChain);

   virtual uintptr_t getClassChainOffsetIdentifyingLoader(TR_OpaqueClassBlock *clazz, uintptr_t **classChain = NULL);

#if defined(J9VM_OPT_JITSERVER)
   /**
     * \brief Finds the offset in SCC of the class chain identifying the class loader of the given class.
     *        This is very similar to getClassChainOffsetIdentifyingLoader
     *        except that it will not fail the compilation if the offset is not valid.
     * \return Returns the offset of the class chain that identifies given class or 0 is such offset is not valid.
    */
   uintptr_t getClassChainOffsetIdentifyingLoaderNoFail(TR_OpaqueClassBlock *clazz, uintptr_t **classChain = NULL);
#endif /* defined(J9VM_OPT_JITSERVER) */

   uintptr_t getClassChainOffsetIdentifyingLoaderNoThrow(TR_OpaqueClassBlock *clazz);

   /**
    * \brief Store the given well-known classes object in the SCC
    *
    * A word of caution: there is an important difference in the encoding of a well-known classes object
    * (the classChainOffsets parameter to this function) compared to a class chain. A class chain is a
    * (uintptr_t *classChain) value whose first element is the total size of classChain in bytes, with
    * subsequent elements being offsets to ROM classes. A well-known classes object is a
    * (uintptr_t *classChainOffsets) value whose first element is the number of subsequent elements,
    * with subsequent elements being offsets to class chains.
    *
    * \param[in] vmThread VM thread
    * \param[in] classChainOffsets The well-known classes object
    * \param[in] classChainOffsetsSize The number of elements in classChainOffsets
    * \param[in] includedClasses An encoding of the well-known classes object, where the ith bit is set
    *                            exactly when the well-known class at index i of the names[] array of
    *                            TR::SymbolValidationManager::populateWellKnownClasses() is included in
    *                            the object.
    * \return Returns a pointer to the data stored in the local SCC, or NULL if the data could not be stored.
    */
   virtual const void *storeWellKnownClasses(J9VMThread *vmThread, uintptr_t *classChainOffsets, size_t classChainOffsetsSize, unsigned int includedClasses);
   virtual const void *storeSharedData(J9VMThread *vmThread, const char *key, const J9SharedDataDescriptor *descriptor);

   /**
    * \brief Fill the given buffer with the SCC key for the well-known classes object with the given
    *        includedClasses
    *
    * \param[out] buffer The buffer to fill with the SCC key
    * \param[out] size The size of buffer
    * \param[in] includedClasses An encoding of the well-known classes object, where the ith bit is set
    *                            exactly when the well-known class at index i of the names[] array of
    *                            TR::SymbolValidationManager::populateWellKnownClasses() is included in
    *                            the object.
    */
   static void buildWellKnownClassesSCCKey(char *buffer, size_t size, unsigned int includedClasses);

   enum TR_J9SharedCacheDisabledReason
      {
      UNINITIALIZED,
      NOT_DISABLED,
      AOT_DISABLED,
      AOT_HEADER_INVALID,
      AOT_HEADER_FAILED_TO_ALLOCATE,
      J9_SHARED_CACHE_FAILED_TO_ALLOCATE,
      SHARED_CACHE_STORE_ERROR,
      SHARED_CACHE_FULL,
      // The following are probably equivalent to SHARED_CACHE_FULL -
      // they could have failed because of no space but no error code is returned.
      SHARED_CACHE_CLASS_CHAIN_STORE_FAILED,
      AOT_HEADER_STORE_FAILED,
      AOT_HEADER_VALIDATION_DELAYED
      };

   static void setSharedCacheDisabledReason(TR_J9SharedCacheDisabledReason state) { _sharedCacheState = state; }
   static TR_J9SharedCacheDisabledReason getSharedCacheDisabledReason() { return _sharedCacheState; }
   static TR_YesNoMaybe isSharedCacheDisabledBecauseFull(TR::CompilationInfo *compInfo);
   static void setStoreSharedDataFailedLength(UDATA length) {_storeSharedDataFailedLength = length; }

#if defined(J9VM_OPT_CRIU_SUPPORT)
   static void setAOTHeaderValidationDelayed() { _aotHeaderValidationDelayed = true; }
   static void resetAOTHeaderValidationDelayed() { _aotHeaderValidationDelayed = false; }
   static bool aotHeaderValidationDelayed() { return _aotHeaderValidationDelayed; }
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */

   virtual J9SharedClassCacheDescriptor *getCacheDescriptorList();

protected:
   /**
    * \brief Helper method; used to check if a pointer is within the SCC
    *
    * \param[in] cacheDesc the J9SharedClassCacheDescriptor for the cache
    * \param[in] ptr The pointer to check
    * \return True if ptr is within the SCC, false otherwise
    */
   static bool isPointerInCache(const J9SharedClassCacheDescriptor *cacheDesc, void *ptr);

   /**
    * \brief Helper method; used to check if an offset is within the SCC
    *
    * \param[in] cacheDesc the J9SharedClassCacheDescriptor for the cache
    * \param[in] offset the offset to check
    * \return True if offset is within the SCC, false otherwise
    */
   static bool isOffsetInCache(const J9SharedClassCacheDescriptor *cacheDesc, uintptr_t offset);

   static inline bool isOffsetFromStart(uintptr_t offset) { return ((offset & OFFSET_FROM_END) != OFFSET_FROM_END); }
   static inline bool isOffsetFromEnd(uintptr_t offset) { return ((offset & OFFSET_FROM_END) == OFFSET_FROM_END); }
   static inline uintptr_t encodeOffsetFromStart(uintptr_t offset) { return (offset << 1); }
   static inline uintptr_t decodeOffsetFromStart(uintptr_t offset) { return (offset >> 1); }
   static inline uintptr_t encodeOffsetFromEnd(uintptr_t offset) { return ((offset << 1) | OFFSET_FROM_END); }
   static inline uintptr_t decodeOffsetFromEnd(uintptr_t offset) { return (offset >> 1); }

   /**
    * \brief Checks whether the specified pointer points into the ROMClass section
    *        of the shared cache.
    *
    * \param[in] romStructure The pointer to check
    * \param[out] cacheOffset If romStructure points into the shared cache and this parameter
    *             is not NULL the result of converting romStructure into an offset will be
    *             returned here. If romStructure does not point into the shared cache this
    *             parameter is ignored.
    * \return True if the pointer points into the shared cache, false otherwise.
    */
   bool isROMStructureInSharedCache(void *romStructure, uintptr_t *cacheOffset = NULL);

private:
   // This class is intended to be a POD; keep it simple.
   struct SCCHint
      {
      SCCHint() : flags(0), data(0) {}
      void clear() { flags = 0; data = 0; }
      uint16_t flags;
      uint16_t data;
      };

   static const uintptr_t OFFSET_FROM_END = 0x1;

   J9JITConfig *jitConfig() { return _jitConfig; }
   J9JavaVM *javaVM() { return _javaVM; }
   J9SharedClassConfig *sharedCacheConfig() { return _sharedCacheConfig; }

   TR_AOTStats *aotStats() { return _aotStats; }

   void log(const char *format, ...);

   SCCHint getHint(J9VMThread * vmThread, J9Method *method);

   void convertUnsignedOffsetToASCII(UDATA offset, char *myBuffer);
   void createClassKey(UDATA classOffsetInCache, char *key, uint32_t & keyLength);

   bool writeClassToChain(J9ROMClass *romClass, UDATA * & chainPtr);
   bool writeClassesToChain(J9Class *clazz, int32_t numSuperclasses, UDATA * & chainPtr);
   bool writeInterfacesToChain(J9Class *clazz, UDATA * & chainPtr);
   bool fillInClassChain(J9Class *clazz, UDATA *chainData, uint32_t chainLength,
                         uint32_t numSuperclasses, uint32_t numInterfaces);

   bool romclassMatchesCachedVersion(J9ROMClass *romClass, UDATA * & chainPtr, UDATA *chainEnd);
   UDATA *findChainForClass(J9Class *clazz, const char *key, uint32_t keyLength);

   /**
    * \brief Helper Method; Converts an offset into the ROMClass section into a pointer.
    *
    * \param offset The offset to convert
    * \return A pointer. Raises a fatal assertion before returning NULL if the offset is invalid.
    */
   void *romStructureFromOffsetInSharedCache(uintptr_t offset);

   /**
    * \brief Converts a pointer into the ROMClass section of the SCC into an offset.
    *
    * \param[in] romStructure The pointer to convert.
    * \return An offset. Raises a fatal assertion before returning 0 if the pointer is invalid.
    */
   uintptr_t offsetInSharedcacheFromROMStructure(void *romStructure);

   /**
    * \brief Checks whether the specified offset is within the ROMClass section
    *        of the shared cache.
    *
    * \param[in] offset The offset to check.
    * \param[out] romStructurePtr If offset is within the shared cache and this parameter is not
    *             NULL the result of converting offset into a pointer will be returned
    *             here. If offset is not within the shared cache this parameter is ignored.
    * \return True if the offset is within the shared cache, false otherwise.
    */
   bool isROMStructureOffsetInSharedCache(uintptr_t encoded_offset, void **romStructurePtr = NULL);

   /**
    * \brief Validates the provided class chain. This method modifies the chainPtr arg.
    *
    * \param[in] romClass The J9ROMClass of the class whose chain is to be validated
    * \param[in] clazz The TR_OpaqueClassBlock of the class whose chain is to be validated
    * \param[in,out] chainPtr Pointer to the start of the class chain passed by reference
    * \param[in] chainEnd Pointer to the end of the class chain
    * \return true if validation succeeded, false otherwise.
    */
   bool validateClassChain(J9ROMClass *romClass, TR_OpaqueClassBlock *clazz, UDATA * & chainPtr, UDATA *chainEnd);

   /**
    * \brief Validates the super classes portion of the class chain. This method modifies the chainPtr arg.
    *
    * \param[in] clazz The TR_OpaqueClassBlock of the class whose chain is to be validated
    * \param[in,out] chainPtr Pointer to the start of super classes portion of the class chain passed by reference
    * \param[in] chainEnd Pointer to the end of the class chain
    * \return true if validation succeeded, false otherwise.
    */
   bool validateSuperClassesInClassChain(TR_OpaqueClassBlock *clazz, UDATA * & chainPtr, UDATA *chainEnd);

   /**
    * \brief Validates the interfaces portion of the class chain. This method modifies the chainPtr arg.
    *
    * \param[in] clazz The TR_OpaqueClassBlock of the class whose chain is to be validated
    * \param[in,out] chainPtr Pointer to the start of interfaces portion of the class chain passed by reference
    * \param[in] chainEnd Pointer to the end of the class chain
    * \return true if validation succeeded, false otherwise.
    */
   bool validateInterfacesInClassChain(TR_OpaqueClassBlock *clazz, UDATA * & chainPtr, UDATA *chainEnd);

   /**
    * \brief Helper method; used to check if a pointer is within the metadata section of the SCC
    * \param[in] cacheDesc the J9SharedClassCacheDescriptor for the cache
    * \param[in] ptr The pointer to check
    * \return True if ptr is within the metadata section of the SCC, false otherwise
    */
   virtual bool isPointerInMetadataSectionInCache(const J9SharedClassCacheDescriptor *cacheDesc, void *ptr);

   /**
    * \brief Helper method; used to check if an offset is within the metadata section of the SCC
    *
    * \param[in] cacheDesc the J9SharedClassCacheDescriptor for the cache
    * \param[in] offset the offset to check
    * \return True if offset is within the metadata section of the SCC, false otherwise
    */
   virtual bool isOffsetInMetadataSectionInCache(const J9SharedClassCacheDescriptor *cacheDesc, uintptr_t offset);

   /**
    * \brief Helper method; used to check if a pointer is within the ROMClass section of the SCC
    * \param[in] cacheDesc the J9SharedClassCacheDescriptor for the cache
    * \param[in] ptr The pointer to check
    * \return True if ptr is within the ROMClass section of the SCC, false otherwise
    */
   virtual bool isPointerInROMClassesSectionInCache(const J9SharedClassCacheDescriptor *cacheDesc, void *ptr);

   /**
    * \brief Helper method; used to check if an offset is within the ROMClass section of the SCC
    *
    * \param[in] cacheDesc the J9SharedClassCacheDescriptor for the cache
    * \param[in] offset the offset to check
    * \return True if offset is within the ROMClass section of the SCC, false otherwise
    */
   virtual bool isOffsetinROMClassesSectionInCache(const J9SharedClassCacheDescriptor *cacheDesc, uintptr_t offset);

   /**
    * \brief Gets the cached result of a prior class chain validation
    *
    * \param[in] clazz The class to be validated
    *
    * \return The cached CCVResult; CCVResult::notYetValidated if result does not exist.
    */
   const CCVResult getCachedCCVResult(TR_OpaqueClassBlock *clazz);

   /**
    * \brief Caches the result of a class chain validation
    *
    * \param[in] clazz The class to be validated
    * \param[in] result The result represented as a CCVResult
    *
    * \return The result of the insertion
    */
   bool cacheCCVResult(TR_OpaqueClassBlock *clazz, CCVResult result);

   uint16_t _initialHintSCount;
   uint16_t _hintsEnabledMask;

   J9JavaVM *_javaVM;
   J9JITConfig *_jitConfig;
   TR_J9VMBase *_fe;
   TR::CompilationInfo *_compInfo;

   TR_AOTStats *_aotStats;
   J9SharedClassConfig *_sharedCacheConfig;
   UDATA _numDigitsForCacheOffsets;

   uint32_t _logLevel;
   bool _verboseHints;

   static TR_J9SharedCacheDisabledReason _sharedCacheState;
   static TR_YesNoMaybe                  _sharedCacheDisabledBecauseFull;
   static UDATA                          _storeSharedDataFailedLength;
#if defined(J9VM_OPT_CRIU_SUPPORT)
   static bool                           _aotHeaderValidationDelayed;
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */
   };


#if defined(J9VM_OPT_JITSERVER)
/**
* \class TR_J9JITServerSharedCache
* \brief Class used by JITServer for querying client-side SharedCache information
*
* This class is an extension of the TR_J9SharedCache class which overrides a number
* of TR_J9SharedCache's APIs. TR_J9JITServerSharedCache is used by JITServer and
* the overridden APIs mostly send remote messages to JITClient to query information from
* the TR_J9SharedCache on the client. The information is needed for JITServer to
* compile code that is compatible with the client-side VM. To minimize the
* number of remote messages as a way to optimize JITServer performance, a
* lot of client-side TR_J9SharedCache information are cached on JITServer.
*/

class TR_J9JITServerSharedCache : public TR_J9SharedCache
   {
public:
   TR_ALLOC(TR_Memory::SharedCache)

   TR_J9JITServerSharedCache(TR_J9VMBase *fe);

   virtual bool isHint(TR_ResolvedMethod *, TR_SharedCacheHint, uint16_t *dataField = NULL) override { TR_ASSERT_FATAL(false, "called"); return false;}
   virtual bool isHint(J9Method *, TR_SharedCacheHint, uint16_t *dataField = NULL) override { TR_ASSERT_FATAL(false, "called"); return false;}
   virtual uint16_t getAllEnabledHints(J9Method *method) override { TR_ASSERT_FATAL(false, "called"); return 0;}
   virtual void addHint(J9Method *, TR_SharedCacheHint) override;
   virtual bool isMostlyFull() override { TR_ASSERT_FATAL(false, "called"); return false;}

   virtual uintptr_t rememberClass(J9Class *clazz, const AOTCacheClassChainRecord **classChainRecord = NULL,
                                   bool create = true) override;

   virtual UDATA storeStringToSCC(const char *string, uintptr_t length) override { TR_ASSERT_FATAL(false, "called"); return 0;}
   virtual const char *getStringFromSCC(UDATA offset) override { TR_ASSERT_FATAL(false, "called"); return NULL;}

   virtual bool isClassInSharedCache(TR_OpaqueClassBlock *clazz, uintptr_t *cacheOffset = NULL) override;
   virtual bool isMethodInSharedCache(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *definingClass, uintptr_t *cacheOffset = NULL) override;
   virtual bool isROMMethodInSharedCache(J9ROMMethod *romMethod, uintptr_t *cacheOffset = NULL) override { TR_ASSERT_FATAL(false, "called"); return false; }
   virtual uintptr_t offsetInSharedCacheFromROMMethod(J9ROMMethod *romMethod) override { TR_ASSERT_FATAL(false, "called"); return TR_SharedCache::INVALID_ROM_METHOD_OFFSET; }

   virtual bool classMatchesCachedVersion(J9Class *clazz, UDATA *chainData=NULL) override { TR_ASSERT_FATAL(false, "called"); return false;}

   virtual void *lookupClassLoaderAssociatedWithClassChain(void *chainData) override
      { TR_ASSERT_FATAL(false, "called"); return NULL; }
   virtual TR_OpaqueClassBlock *lookupClassFromChainAndLoader(uintptr_t *chainData, void *classLoader,
                                                              TR::Compilation *comp) override
      { TR_ASSERT_FATAL(false, "called"); return NULL;}

   static void setSharedCacheDisabledReason(TR_J9SharedCacheDisabledReason state) { TR_ASSERT_FATAL(false, "called"); }
   static TR_J9SharedCacheDisabledReason getSharedCacheDisabledReason() { TR_ASSERT_FATAL(false, "called"); return TR_J9SharedCache::TR_J9SharedCacheDisabledReason::UNINITIALIZED;}
   static TR_YesNoMaybe isSharedCacheDisabledBecauseFull(TR::CompilationInfo *compInfo) { TR_ASSERT_FATAL(false, "called"); return TR_no;}
   static void setStoreSharedDataFailedLength(UDATA length) { TR_ASSERT_FATAL(false, "called"); }

   virtual uintptr_t getClassChainOffsetIdentifyingLoader(TR_OpaqueClassBlock *clazz, uintptr_t **classChain = NULL) override;

   virtual J9SharedClassCacheDescriptor *getCacheDescriptorList() override;

   void setStream(JITServer::ServerStream *stream) { _stream = stream; }
   void setCompInfoPT(TR::CompilationInfoPerThread *compInfoPT) { _compInfoPT = compInfoPT; }
   virtual const void *storeSharedData(J9VMThread *vmThread, const char *key, const J9SharedDataDescriptor *descriptor) override;

private:

   virtual bool isPointerInMetadataSectionInCache(const J9SharedClassCacheDescriptor *cacheDesc, void *ptr) override
      {
      return isPointerInCache(cacheDesc, ptr);
      }
   virtual bool isOffsetInMetadataSectionInCache(const J9SharedClassCacheDescriptor *cacheDesc, uintptr_t offset) override
      {
      return (isOffsetFromEnd(offset) && isOffsetInCache(cacheDesc, offset));
      }
   virtual bool isPointerInROMClassesSectionInCache(const J9SharedClassCacheDescriptor *cacheDesc, void *ptr) override
      {
      return isPointerInCache(cacheDesc, ptr);
      }
   virtual bool isOffsetinROMClassesSectionInCache(const J9SharedClassCacheDescriptor *cacheDesc, uintptr_t offset) override
      {
      return (isOffsetFromStart(offset) && isOffsetInCache(cacheDesc, offset));
      }

   JITServer::ServerStream *_stream;
   TR::CompilationInfoPerThread *_compInfoPT;
   };

/**
* \class TR_J9DeserializerSharedCache
* \brief Class used by a JITServer client for querying the deserializer during AOT cache loads
*
* This class is an extension of the TR_J9SharedCache class which overrides a number
* of TR_J9SharedCache's APIs. TR_J9DeserializerSharedCache is used by a client of a JITServer
* when relocating a method received from the JITServer's AOT cache. It ignores any local shared cache
* that may exist, and instead will query the JITServerNoSCCAOTDeserializer during relocation, fetching the
* deserializer's cached information.
*
* The offsets used by this class are different from those of the other TR_J9SharedCache classes and their derivatives;
* they are the idAndType of the AOTSerializationRecord records maintained by a JITServer's AOT cache, and don't
* correspond to any storage location of the entities that the offsets refer to.
*/
class TR_J9DeserializerSharedCache : public TR_J9SharedCache
   {
public:
   TR_ALLOC(TR_Memory::SharedCache)

   TR_J9DeserializerSharedCache(TR_J9VMBase *fe, JITServerNoSCCAOTDeserializer *deserializer, TR::CompilationInfoPerThread *compInfoPT);

   virtual void *pointerFromOffsetInSharedCache(uintptr_t offset) override;
   virtual void *lookupClassLoaderAssociatedWithClassChain(void *chainData) override;
   virtual J9ROMClass *romClassFromOffsetInSharedCache(uintptr_t offset) override;
   virtual J9ROMMethod *romMethodFromOffsetInSharedCache(uintptr_t offset) override;
   virtual bool classMatchesCachedVersion(J9Class *clazz, UDATA *chainData=NULL) override;
   virtual TR_OpaqueClassBlock *lookupClassFromChainAndLoader(uintptr_t *chainData, void *classLoader,
                                                              TR::Compilation *comp) override;

   virtual bool isHint(TR_ResolvedMethod *, TR_SharedCacheHint, uint16_t *dataField = NULL) override { TR_ASSERT_FATAL(false, "called"); return false; }
   virtual bool isHint(J9Method *, TR_SharedCacheHint, uint16_t *dataField = NULL) override { TR_ASSERT_FATAL(false, "called"); return false; }
   virtual uint16_t getAllEnabledHints(J9Method *method) override { TR_ASSERT_FATAL(false, "called"); return 0; }
   virtual void addHint(J9Method *, TR_SharedCacheHint) override { TR_ASSERT_FATAL(false, "called"); }
   virtual void addHint(TR_ResolvedMethod *, TR_SharedCacheHint) override { TR_ASSERT_FATAL(false, "called"); }
   virtual bool isMostlyFull() override { TR_ASSERT_FATAL(false, "called"); return false; }

   virtual uintptr_t offsetInSharedCacheFromPointer(void *ptr) override { TR_ASSERT_FATAL(false, "called"); return 0; }
   virtual uintptr_t offsetInSharedCacheFromROMClass(J9ROMClass *romClass) override { TR_ASSERT_FATAL(false, "called"); return 0; }
   virtual uintptr_t offsetInSharedCacheFromROMMethod(J9ROMMethod *romMethod) override { TR_ASSERT_FATAL(false, "called"); return 0; }
   virtual void *ptrToROMClassesSectionFromOffsetInSharedCache(uintptr_t offset) override { TR_ASSERT_FATAL(false, "called"); return NULL; }
   virtual uintptr_t offsetInSharedCacheFromPtrToROMClassesSection(void *ptr) override { TR_ASSERT_FATAL(false, "called"); return 0; }

   virtual void persistIprofileInfo(TR::ResolvedMethodSymbol *, TR::Compilation *comp) override { TR_ASSERT_FATAL(false, "called"); }
   virtual void persistIprofileInfo(TR::ResolvedMethodSymbol *, TR_ResolvedMethod*, TR::Compilation *comp) override { TR_ASSERT_FATAL(false, "called"); }

   virtual bool canRememberClass(TR_OpaqueClassBlock *classPtr) override { TR_ASSERT_FATAL(false, "called"); return false; }
   virtual uintptr_t rememberClass(TR_OpaqueClassBlock *classPtr,
                                   const AOTCacheClassChainRecord **classChainRecord = NULL) override
      { TR_ASSERT_FATAL(false, "called"); return TR_SharedCache::INVALID_CLASS_CHAIN_OFFSET; }

   virtual uintptr_t rememberClass(J9Class *clazz, const AOTCacheClassChainRecord **classChainRecord = NULL,
                                   bool create = true) override
      { TR_ASSERT_FATAL(false, "called"); return TR_SharedCache::INVALID_CLASS_CHAIN_OFFSET; }

   virtual UDATA storeStringToSCC(const char *string, uintptr_t length) override { TR_ASSERT_FATAL(false, "called"); return 0; }
   virtual const char *getStringFromSCC(UDATA offset) override { TR_ASSERT_FATAL(false, "called"); return NULL; }

   virtual bool isPointerInSharedCache(void *ptr, uintptr_t *cacheOffset = NULL) override { TR_ASSERT_FATAL(false, "called"); return false; }
   virtual bool isOffsetInSharedCache(uintptr_t encoded_offset, void *ptr = NULL) override { TR_ASSERT_FATAL(false, "called"); return false; }
   virtual bool isROMClassInSharedCache(J9ROMClass *romClass, uintptr_t *cacheOffset = NULL) override { TR_ASSERT_FATAL(false, "called"); return false; }
   virtual bool isROMClassOffsetInSharedCache(uintptr_t offset, J9ROMClass **romClassPtr = NULL) override { TR_ASSERT_FATAL(false, "called"); return false; }
   virtual bool isROMMethodInSharedCache(J9ROMMethod *romMethod, uintptr_t *cacheOffset = NULL) override { TR_ASSERT_FATAL(false, "called"); return false; }
   virtual bool isROMMethodOffsetInSharedCache(uintptr_t offset, J9ROMMethod **romMethodPtr = NULL) override { TR_ASSERT_FATAL(false, "called"); return false; }
   virtual bool isPtrToROMClassesSectionInSharedCache(void *ptr, uintptr_t *cacheOffset = NULL) override { TR_ASSERT_FATAL(false, "called"); return false; }
   virtual bool isOffsetOfPtrToROMClassesSectionInSharedCache(uintptr_t offset, void **ptr = NULL) override { TR_ASSERT_FATAL(false, "called"); return false; }
   virtual uintptr_t getClassChainOffsetIdentifyingLoader(TR_OpaqueClassBlock *clazz, uintptr_t **classChain = NULL) override { TR_ASSERT_FATAL(false, "called"); return 0; }
   virtual const void *storeSharedData(J9VMThread *vmThread, const char *key, const J9SharedDataDescriptor *descriptor) override { TR_ASSERT_FATAL(false, "called"); return NULL; }

   virtual J9SharedClassCacheDescriptor *getCacheDescriptorList() override { TR_ASSERT_FATAL(false, "called"); return NULL; }

private:
   JITServerNoSCCAOTDeserializer *_deserializer;
   TR::CompilationInfoPerThread *_compInfoPT;
   };

#endif /* defined(J9VM_OPT_JITSERVER) */

#endif
