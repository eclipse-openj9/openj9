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

#ifndef EXTERNALPROFILER_HPP
#define EXTERNALPROFILER_HPP

#include <stdint.h>

class TR_ValueProfileInfo;
namespace TR { class CFG; }
namespace TR { class Compilation; }
namespace TR { class Node; }
namespace TR { class TreeTop; }
struct TR_ByteCodeInfo;

class TR_ExternalProfiler
   {
public:

   virtual void setBlockAndEdgeFrequencies( TR::CFG *cfg, TR::Compilation *comp) = 0;
   virtual TR_ExternalValueProfileInfo * getValueProfileInfo(TR_ByteCodeInfo & bcInfo, TR::Compilation *comp) = 0;
   virtual bool hasSameBytecodeInfo(TR_ByteCodeInfo & persistentByteCodeInfo, TR_ByteCodeInfo & currentByteCodeInfo, TR::Compilation *comp) = 0;
   virtual void getBranchCounters (TR::Node *node, TR::TreeTop *fallThroughtTree, int32_t *taken, int32_t *notTaken, TR::Compilation *comp) = 0;
   virtual int32_t getSwitchCountForValue(TR::Node *node, int32_t index, TR::Compilation *comp) = 0;
   virtual int32_t getSumSwitchCount (TR::Node *node, TR::Compilation *comp) = 0;
   virtual int32_t getFlatSwitchProfileCounts (TR::Node *node, TR::Compilation *comp) = 0;
   virtual bool isSwitchProfileFlat (TR::Node *node, TR::Compilation *comp) = 0;
   };
#endif

