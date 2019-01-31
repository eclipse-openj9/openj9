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

#ifndef PICHELPERS_HPP
#define PICHELPERS_HPP

#include <stdint.h>

namespace OMR { class RuntimeAssumption; }
class TR_UnloadedClassPicSite;
extern "C" {
  TR_UnloadedClassPicSite *createClassUnloadPicSite(void *classPointer, void *addressToBePatched, uint32_t size, OMR::RuntimeAssumption **sentinel);
  void jitAddPicToPatchOnClassUnload(void *classPointer, void *addressToBePatched);
  void jitAdd32BitPicToPatchOnClassUnload(void *classPointer, void *addressToBePatched);
  void createClassRedefinitionPicSite(void *classPointer, void *addressToBePatched, uint32_t size, bool unresolved, OMR::RuntimeAssumption **sentinel);
  void createJNICallSite(void *ramMethod, void *addressToBePatched, OMR::RuntimeAssumption **sentinel);
  void jitAddPicToPatchOnClassRedefinition(void *classPointer, void *addressToBePatched, bool unresolved=false);
  void jitAdd32BitPicToPatchOnClassRedefinition(void *classPointer, void *addressToBePatched, bool unresolved=false);
}

#endif // PICHELPERS_HPP
