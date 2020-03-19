/*******************************************************************************
 * Copyright (c) 2018, 2020 IBM Corp. and others
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

#include "control/CompilationRuntime.hpp"
#include "control/Options.hpp" // TR::Options::useCompressedPointers()
#include "env/CompilerEnv.hpp" // for TR::Compiler->target.is64Bit()
#include "net/CommunicationStream.hpp"


namespace JITServer
{
uint32_t CommunicationStream::CONFIGURATION_FLAGS = 0;

void
CommunicationStream::initConfigurationFlags()
   {
   if (TR::Compiler->target.is64Bit() && TR::Options::useCompressedPointers())
      {
      CONFIGURATION_FLAGS |= JITServerCompressedRef;
      }
   CONFIGURATION_FLAGS |= JAVA_SPEC_VERSION & JITServerJavaVersionMask;
   }

bool CommunicationStream::useSSL()
   {
   TR::CompilationInfo *compInfo = TR::CompilationInfo::get();
   return (compInfo->getJITServerSslKeys().size() ||
           compInfo->getJITServerSslCerts().size() ||
           compInfo->getJITServerSslRootCerts().size());
   }

void CommunicationStream::initSSL()
   {
   (*OSSL_load_error_strings)();
   (*OSSL_library_init)();
   // OpenSSL_add_ssl_algorithms() is a synonym for SSL_library_init() and is implemented as a macro
   // It's redundant, should be able to remove it later
   // OpenSSL_add_ssl_algorithms();
   }

void
CommunicationStream::readMessage(Message &msg)
   {
   msg.clearForRead();

   // read message size
   uint32_t serializedSize;
   readBlocking(serializedSize);
   msg.setSerializedSize(serializedSize);

   // read the rest of the message
   uint32_t messageSize = serializedSize - sizeof(uint32_t);
   readBlocking(msg.getBufferStartForRead() + sizeof(uint32_t), messageSize);

   // rebuild the message
   msg.deserialize();
   }

void
CommunicationStream::writeMessage(Message &msg)
   {
   char *serialMsg = msg.serialize();
   // write serialized message to the socket
   writeBlocking(serialMsg, msg.serializedSize());
   msg.clearForWrite();
   }
}
