/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#pragma csect(CODE,"TRJ9GCStackAtlas#C")
#pragma csect(STATIC,"TRJ9GCStackAtlas#S")
#pragma csect(TEST,"TRJ9GCStackAtlas#T")


#include "codegen/GCStackAtlas.hpp"
#include "codegen/GCStackMap.hpp"
#include "compile/Compilation.hpp"
#include "codegen/CodeGenerator.hpp"


void
J9::GCStackAtlas::close(TR::CodeGenerator *cg)
   {
   // Dump the atlas before merging. The atlas after merging is given by the
   // dump of the external atlas
   //
   TR::Compilation *comp = cg->comp();

   if (comp->getOption(TR_TraceCG))
      {
      comp->getDebug()->print(comp->getOutFile(), self());
      }

   TR_GCStackMap * parameterMap = 0;
   if (self()->getInternalPointerMap())
      {
      parameterMap = self()->getParameterMap();
      }

   if (comp->getOption(TR_DisableMergeStackMaps))
       return;

   // Merge adjacent similar maps
   //
   uint8_t *start = cg->getCodeStart();
   ListElement<TR_GCStackMap> *mapEntry, *next;
   TR_GCStackMap *map, *nextMap;

   for (mapEntry = self()->getStackMapList().getListHead(); mapEntry; mapEntry = next)
      {
      next = mapEntry->getNextElement();
      map = mapEntry->getData();

      // See if the next map can be merged with this one.
      // If they have the same contents, the ranges are merged and a single map
      // represents both ranges.
      //
      if (!next)
         {
         continue;
         }

      nextMap = next->getData();

      int32_t mapBytes = map->getMapSizeInBytes();

      // TODO: We should be using mapsAreIdentical API here instead. Also it seems there is a similar comment in OMR,
      // i.e. "Maps are the same". Do we need a common API here for map merging which can be extended?
      if (nextMap != parameterMap &&
          mapBytes == nextMap->getMapSizeInBytes() &&
          map->getRegisterMap() == nextMap->getRegisterMap() &&
          !memcmp(map->getMapBits(), nextMap->getMapBits(), mapBytes) &&
          (comp->getOption(TR_DisableLiveMonitorMetadata) ||
           ((map->getLiveMonitorBits() != 0) == (nextMap->getLiveMonitorBits() != 0) &&
            (map->getLiveMonitorBits() == 0 ||
             !memcmp(map->getLiveMonitorBits(), nextMap->getLiveMonitorBits(), mapBytes)))) &&
          ((!nextMap->getInternalPointerMap() && !map->getInternalPointerMap()) ||
           (nextMap->getInternalPointerMap() && map->getInternalPointerMap() &&
            map->isInternalPointerMapIdenticalTo(nextMap))) &&
          map->isByteCodeInfoIdenticalTo(nextMap))
         {
         // Maps are the same - can merge
         //
         if (comp->getOption(TR_TraceCG))
             traceMsg(comp,
                     "Map with code offset range starting at [%08x] is identical to the previous map [%08x], merging and eliminating previous\n",
                     nextMap->getLowestCodeOffset(), map->getLowestCodeOffset());
             
         map->setLowestCodeOffset(nextMap->getLowestCodeOffset());
         self()->getStackMapList().removeNext(mapEntry);
         self()->decNumberOfMaps();
         next = mapEntry;
         }
      }
   }
