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

#ifndef J9_RUNTIME_ASSUMPTIONS_INCL
#define J9_RUNTIME_ASSUMPTIONS_INCL

#include "runtime/RuntimeAssumptions.hpp"

#include <stdint.h>
#include "env/RuntimeAssumptionTable.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "infra/Flags.hpp"
#include "infra/Link.hpp"


class TR_RedefinedClassPicSite : public OMR::ValueModifyRuntimeAssumption
   {
   protected:
   TR_RedefinedClassPicSite(TR_PersistentMemory * pm, uintptr_t key, uint8_t *picLocation, uint32_t size, TR_RuntimeAssumptionKind kind)
      : OMR::ValueModifyRuntimeAssumption(pm, key), _picLocation(picLocation), _size(size)
         {}

   public:

   virtual void compensate(TR_FrontEnd *vm, bool isSMP, void *newKey);
   virtual bool equals(OMR::RuntimeAssumption &other)
      {
      TR_RedefinedClassPicSite *o = other.asRCPSite();
      return (o != 0) && o->_picLocation == _picLocation;
      }
   virtual uint8_t *getFirstAssumingPC() { return getPicLocation(); }
   virtual uint8_t *getLastAssumingPC() { return getPicLocation(); }
   virtual TR_RedefinedClassPicSite *asRCPSite() { return this; }
   uint8_t * getPicLocation()    { return _picLocation; }
   void      setPicLocation   (uint8_t *p) { _picLocation = p; }
   void      setKey(uintptr_t newKey) { _key = newKey; }
   uintptr_t getPicEntry() { return *((uintptr_t*)_picLocation); }
   bool      isForAddressMaterializationSequence(){ return _size == 1; }

   virtual void dumpInfo();

   private:
   uint8_t    *_picLocation;
   uint32_t    _size;
   };


class TR_RedefinedClassRPicSite : public TR_RedefinedClassPicSite
   {
   protected:
   TR_RedefinedClassRPicSite(TR_PersistentMemory * pm, uintptr_t key, uint8_t *picLocation, uint32_t size)
          : TR_RedefinedClassPicSite(pm, key, picLocation, size, RuntimeAssumptionOnClassRedefinitionPIC)
      {}

   public:
   virtual TR_RuntimeAssumptionKind getAssumptionKind() { return RuntimeAssumptionOnClassRedefinitionPIC; }
   static TR_RedefinedClassRPicSite *make(
      TR_FrontEnd *fe, TR_PersistentMemory * pm, uintptr_t key, uint8_t *picLocation, uint32_t size,
      OMR::RuntimeAssumption **sentinel);
   };


class TR_RedefinedClassUPicSite : public TR_RedefinedClassPicSite
   {
   protected:
   TR_RedefinedClassUPicSite(TR_PersistentMemory * pm, uintptr_t key, uint8_t *picLocation, uint32_t size)
            : TR_RedefinedClassPicSite(pm, key, picLocation, size, RuntimeAssumptionOnClassRedefinitionUPIC)
      {}
   public:
   virtual TR_RuntimeAssumptionKind getAssumptionKind() { return RuntimeAssumptionOnClassRedefinitionUPIC; }
   static TR_RedefinedClassUPicSite *make(
      TR_FrontEnd *fe, TR_PersistentMemory * pm, uintptr_t key, uint8_t *picLocation, uint32_t size,
      OMR::RuntimeAssumption **sentinel);

   virtual uintptr_t hashCode() { return TR_RuntimeAssumptionTable::hashCode((uintptr_t)getPicLocation()); }
   };

/**
 * @brief
 *    A runtime assumption for patching the block frequency counter increment
 *    instructions in the compiled body that is collecting JProfiling data.
 *    It provides means to patch the instruction to turn off / turn on profiling
 *    data collection.
 */
class TR_JProfBlockFrequencyCounterSites : public OMR::ValueModifyRuntimeAssumption
   {
   protected:
   /**
    * @brief Construct a new tr TR_JProfBlockFrequencyCounterSites object
    *
    * @param pm TR_PersistentMemory object for allocating the runtime assumption
    * @param key key against which this runtime assumption is stored in table
    * @param sites TR::JProfBFPatchSites object that contains the information to
    *              accommodate patching instructons in the compiled method with JProfiling
    *              to turn-on / turn off data collection.
    */
   TR_JProfBlockFrequencyCounterSites(TR_PersistentMemory *pm,
                                       uintptr_t key,
                                       TR::JProfBFPatchSites *sites)
      : OMR::ValueModifyRuntimeAssumption(pm, key),
         _patchSites(sites),
         _patchFlag(Enabled)
   {}


   enum State
      {
      Enabled,
      Disabled
      };
   public:
   /**
    * @brief Get the first instruction in the _patchSites for patching
    *
    * @return uint8_t* first instruction pointer in the _patchSites
    */
   virtual uint8_t *getFirstAssumingPC() { return _patchSites->getFirstLocation(); }

   /**
    * @brief Get the last instruction in the _patchSites for patching
    *
    * @return uint8_t* last instruction pointer in the _patchSites
    */
   virtual uint8_t *getLastAssumingPC() { return _patchSites->getLastLocation(); }

   /**
    * @brief
    *    Goes through the instruction list for the compiled method which
    *    increments static counters in JProfiling Data for block frequencies,
    *    and patches them to diasble / enable Profiling Data Collection.
    *
    * @param vm TR_FrontEnd vm object
    * @param disableDataCollection boolean flag to disable / enable data collection
    * @param newKey
    */
   virtual void compensate(TR_FrontEnd *vm, bool disableDataCollection, void *newKey);

   virtual void dumpInfo() { return; }

   /**
    * @brief Get the Runtime Assumption Kind
    *
    * @return TR_RuntimeAssumptionKind
    */
   virtual TR_RuntimeAssumptionKind getAssumptionKind() { return RuntimeAssumptionOnPatchJProfBlockFreqCounters; }

   /**
    * @brief Construct a new TR_JProfBlockFrequencyCounterSites runtime assumption for the method
    *
    * @param fe TR_FrontEnd object to facilitate adding constructed runtime assumption to RAT
    * @param pm TR_PersistentMemory Persistent Memory object to allocate the persistent object
    * @param key key using which the runtime assumption will be added in the RAT
    * @param sites TR::JProfBFPatchSites object containing information about
    *                instructions to be patched in method
    * @param sentinel Setinnel node for the RAT
    * @return TR_JProfBlockFrequencyCounterSites* returns constructed TR_JProfBlockFrequencyCounterSites object for this method.
    */
   static TR_JProfBlockFrequencyCounterSites *make(TR_FrontEnd *fe,
                                                      TR_PersistentMemory *pm,
                                                      uintptr_t key,
                                                      TR::JProfBFPatchSites *sites,
                                                      OMR::RuntimeAssumption **sentinel);


   /**
    * @brief Cleans up data structure associated with this runtime assumption.
    *
    */
   void reclaim()
      {
      TR::JProfBFPatchSites::reclaim(_patchSites);
      }

   /**
    * @brief Get TR::JProfBFPatchSites object containing information about instructions to be patched in method.
    *
    * @return TR::JProfBFPatchSites*
    */
   TR::JProfBFPatchSites * getPatchSites() { return _patchSites; }

   /**
    * @brief Compares this runtime assumption with another one.
    *
    * @param other Other Runtime Assumption agains which this one will be compared.alignas
    * @return true if equal
    * @return false if not equal
    */
   virtual bool equals(OMR::RuntimeAssumption &other)
      {
      if (other.getAssumptionKind() != RuntimeAssumptionOnPatchJProfBlockFreqCounters)
         return false;

      TR_JProfBlockFrequencyCounterSites *otherSite = reinterpret_cast<TR_JProfBlockFrequencyCounterSites *>(&other);
      return _patchSites->equals(otherSite->getPatchSites());
      }

   private:

   /**
    * @brief TR::JProfBFPatchSites object containing list of instruction addresses in this compiled code and the instruciton itself that updates static counters to facilitate
    *          patching of the sites to turn on / off profiling data collection.
    */
   TR::JProfBFPatchSites *_patchSites;

   /**
    * @brief A flag to indicate the current status of body associated with this runtime assumption.
    *
    */
   uint8_t _patchFlag;

   };
#endif
