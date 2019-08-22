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


#include "CommunicationStream.hpp"
#include "env/CompilerEnv.hpp" // for TR::Compiler->target.is64Bit()
#include "control/Options.hpp" // TR::Options::useCompressedPointers()
#include "control/CompilationRuntime.hpp"
#include "j9cfg.h" // for JAVA_SPEC_VERSION


namespace JITServer
{
uint32_t CommunicationStream::CONFIGURATION_FLAGS = 0;

void CommunicationStream::initVersion()
   {
   if (TR::Compiler->target.is64Bit() && TR::Options::useCompressedPointers())
      {
      CONFIGURATION_FLAGS |= JITServerCompressedRef;
      }
   CONFIGURATION_FLAGS |= JAVA_SPEC_VERSION & JITServerJavaVersionMask;
   }

#if defined(JITSERVER_ENABLE_SSL)
bool CommunicationStream::useSSL()
   {
   TR::CompilationInfo *compInfo = TR::CompilationInfo::get();
   return (compInfo->getJITServerSslKeys().size() ||
           compInfo->getJITServerSslCerts().size() ||
           compInfo->getJITServerSslRootCerts().size());
   }
#endif
};
