/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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
   TR_RedefinedClassPicSite(TR_PersistentMemory * pm, uintptrj_t key, uint8_t *picLocation, uint32_t size, TR_RuntimeAssumptionKind kind)
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
   void      setKey(uintptrj_t newKey) { _key = newKey; }
   uintptrj_t getPicEntry() { return *((uintptrj_t*)_picLocation); }
   bool      isForAddressMaterializationSequence(){ return _size == 1; }

   virtual void dumpInfo();

   private:
   uint8_t    *_picLocation;
   uint32_t    _size;
   };


class TR_RedefinedClassRPicSite : public TR_RedefinedClassPicSite
   {
   protected:
   TR_RedefinedClassRPicSite(TR_PersistentMemory * pm, uintptrj_t key, uint8_t *picLocation, uint32_t size)
          : TR_RedefinedClassPicSite(pm, key, picLocation, size, RuntimeAssumptionOnClassRedefinitionPIC)
      {}

   public:
   virtual TR_RuntimeAssumptionKind getAssumptionKind() { return RuntimeAssumptionOnClassRedefinitionPIC; }
   static TR_RedefinedClassRPicSite *make(
      TR_FrontEnd *fe, TR_PersistentMemory * pm, uintptrj_t key, uint8_t *picLocation, uint32_t size,
      OMR::RuntimeAssumption **sentinel);
   };


class TR_RedefinedClassUPicSite : public TR_RedefinedClassPicSite
   {
   protected:
   TR_RedefinedClassUPicSite(TR_PersistentMemory * pm, uintptrj_t key, uint8_t *picLocation, uint32_t size)
            : TR_RedefinedClassPicSite(pm, key, picLocation, size, RuntimeAssumptionOnClassRedefinitionUPIC)
      {}
   public:
   virtual TR_RuntimeAssumptionKind getAssumptionKind() { return RuntimeAssumptionOnClassRedefinitionUPIC; }
   static TR_RedefinedClassUPicSite *make(
      TR_FrontEnd *fe, TR_PersistentMemory * pm, uintptrj_t key, uint8_t *picLocation, uint32_t size,
      OMR::RuntimeAssumption **sentinel);

   virtual uintptrj_t hashCode() { return TR_RuntimeAssumptionTable::hashCode((uintptrj_t)getPicLocation()); }
   };

#endif
