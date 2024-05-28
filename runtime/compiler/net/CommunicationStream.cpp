/*******************************************************************************
 * Copyright IBM Corp. and others 2018
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

#include "control/CompilationRuntime.hpp"
#include "control/Options.hpp" // TR::Options::useCompressedPointers()
#include "env/CompilerEnv.hpp" // for TR::Compiler->target.is64Bit()
#include "net/CommunicationStream.hpp"


namespace JITServer
{

uint32_t CommunicationStream::CONFIGURATION_FLAGS = 0;

uint32_t CommunicationStream::_msgTypeCount[] = {0};
uint64_t CommunicationStream::_totalMsgSize = 0;
uint32_t CommunicationStream::_lastReadError = 0;
uint32_t CommunicationStream::_numConsecutiveReadErrorsOfSameType = 0;
#if defined(MESSAGE_SIZE_STATS)
TR_Stats CommunicationStream::_msgSizeStats[];
#endif /* defined(MESSAGE_SIZE_STATS) */

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
   // It's redundant and doesn't need to be called
   }

void
CommunicationStream::readMessage(Message &msg)
   {
   msg.clearForRead();

   // The message buffer storage and its capacity could be
   // changed when the serialized size is set.
   char *buffer = msg.getBufferStartForRead();
   uint32_t bufferCapacity = msg.getBufferCapacity();

   int32_t bytesRead = readOnceBlocking(buffer, bufferCapacity);

   // bytesRead should be greater than 0 here, readOnceBlocking() throws
   // an exception already if (bytesRead <= 0).
   if (bytesRead < sizeof(uint32_t))
      {
      throw JITServer::StreamFailure("JITServer I/O error: failed to read the size of the message");
      }

   // bytesRead >= sizeof(uint32_t)
   uint32_t serializedSize = ((uint32_t *)buffer)[0];
   if (bytesRead > serializedSize)
      {
      throw JITServer::StreamFailure("JITServer I/O error: read more than the message size");
      }

   // serializedSize >= bytesRead
   uint32_t bytesLeftToRead = serializedSize - bytesRead;

   if (bytesLeftToRead > 0)
      {
      if (serializedSize > bufferCapacity)
         {
         // bytesRead could be less than the buffer capacity.
         msg.expandBuffer(serializedSize, bytesRead);

         // The buffer storage will change after the buffer is expanded.
         buffer = msg.getBufferStartForRead();
         }

      readBlocking(buffer + bytesRead, bytesLeftToRead);
      }

   msg.setSerializedSize(serializedSize);

   // rebuild the message
   msg.deserialize();

   // Update message count and size statistics
   _msgTypeCount[msg.type()] += 1;
   _totalMsgSize += serializedSize;
#if defined(MESSAGE_SIZE_STATS)
   _msgSizeStats[msg.type()].update(serializedSize);
#endif /* defined(MESSAGE_SIZE_STATS) */
   }

void
CommunicationStream::writeMessage(Message &msg)
   {
   char *serialMsg = msg.serialize();
   // write serialized message to the socket
   writeBlocking(serialMsg, msg.serializedSize());
   msg.clearForWrite();
   }

std::string
CommunicationStream::showFullVersionIncompatibility(uint64_t serverFullVersion, uint64_t clientFullVersion)
   {
   // See JITServer::Message::buildFullVersion() and CommunicationStream::initConfigurationFlags() for the encoding

   uint32_t serverProtocolVersion = serverFullVersion & 0xFFFFFFFF;
   uint32_t clientProtocolVersion = clientFullVersion & 0xFFFFFFFF;
   if (serverProtocolVersion != clientProtocolVersion)
      return "protocol version: server " + CommunicationStream::showJITServerVersion(serverProtocolVersion) +
             ", client " + CommunicationStream::showJITServerVersion(clientProtocolVersion);

   uint32_t serverFlags = serverFullVersion >> 32;
   uint32_t clientFlags = clientFullVersion >> 32;

   uint32_t serverJDKVersion = serverFlags & JITServerCompatibilityFlags::JITServerJavaVersionMask;
   uint32_t clientJDKVersion = clientFlags & JITServerCompatibilityFlags::JITServerJavaVersionMask;
   if (serverJDKVersion != clientJDKVersion)
      return "JDK version: server " + std::to_string(serverJDKVersion) +
             ", client " + std::to_string(clientJDKVersion);

   bool serverCompressesRefs = serverFlags & JITServerCompatibilityFlags::JITServerCompressedRef;
   bool clientCompressesRefs = clientFlags & JITServerCompatibilityFlags::JITServerCompressedRef;
   if (serverCompressesRefs != clientCompressesRefs)
      return "compressed refs: server " + std::to_string(serverCompressesRefs) +
             ", client " + std::to_string(clientCompressesRefs);

   return "full version: server " + std::to_string(serverFullVersion) +
          ", client " + std::to_string(clientFullVersion);
   }

}
