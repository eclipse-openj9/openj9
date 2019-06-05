/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#include "control/Recompilation.hpp"

// Called at runtime to get the body info from the start PC
//
TR_PersistentJittedBodyInfo *J9::Recompilation::getJittedBodyInfoFromPC(void *startPC)
   {
   TR_UNIMPLEMENTED();
   return NULL;
   }

bool J9::Recompilation::isAlreadyPreparedForRecompile(void *startPC)
   {
   TR_UNIMPLEMENTED();
   return FALSE;
   }

void J9::Recompilation::fixUpMethodCode(void *startPC)
   {
   TR_UNIMPLEMENTED();
   }

void J9::Recompilation::methodHasBeenRecompiled(void *oldStartPC, void *newStartPC, TR_FrontEnd *fe)
   {
   TR_UNIMPLEMENTED();
   }

void J9::Recompilation::methodCannotBeRecompiled(void *oldStartPC, TR_FrontEnd *fe)
   {
   TR_UNIMPLEMENTED();
   }

void J9::Recompilation::invalidateMethodBody(void *startPC, TR_FrontEnd *fe)
   {
   TR_UNIMPLEMENTED();
   }

#if defined(TR_HOST_ARM64)
void fixupMethodInfoAddressInCodeCache(void *startPC, void *bodyInfo)
   {
   TR_UNIMPLEMENTED();
   }
#endif
