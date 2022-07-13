/*******************************************************************************
 * Copyright (c) 2018, 2022 IBM Corp. and others
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

#ifndef COMMUNICATION_STREAM_H
#define COMMUNICATION_STREAM_H

#include <unistd.h>
#include "net/LoadSSLLibs.hpp"
#include "net/Message.hpp"
#include "infra/Statistics.hpp"
#include "env/VerboseLog.hpp"

namespace JITServer
{
enum JITServerCompatibilityFlags
   {
   JITServerJavaVersionMask    = 0x00000FFF,
   JITServerCompressedRef      = 0x00001000,
   };

class CommunicationStream
   {
public:
   static bool useSSL();
   static void initSSL();

   static uint32_t _msgTypeCount[MessageType::MessageType_MAXTYPE];
   static uint64_t _totalMsgSize;
#if defined(MESSAGE_SIZE_STATS)
   static TR_Stats _msgSizeStats[MessageType::MessageType_MAXTYPE];
#endif /* defined(MESSAGE_SIZE_STATS) */

   static void initConfigurationFlags();

   static uint32_t getJITServerVersion()
      {
      return (MAJOR_NUMBER << 24) | (MINOR_NUMBER << 8); // PATCH_NUMBER is ignored
      }

   static uint64_t getJITServerFullVersion()
      {
      return Message::buildFullVersion(getJITServerVersion(), CONFIGURATION_FLAGS);
      }

   static void printJITServerVersion()
      {
      // print the human-readable version string
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "JITServer version: %u.%u.%u", MAJOR_NUMBER, MINOR_NUMBER, PATCH_NUMBER);
      }

protected:
   CommunicationStream() : _ssl(NULL), _connfd(-1) { }

   virtual ~CommunicationStream()
      {
      if (_ssl)
         (*OBIO_free_all)(_ssl);
      if (_connfd != -1)
         close(_connfd);
      }

   void initStream(int connfd, BIO *ssl)
      {
      _connfd = connfd;
      _ssl = ssl;
      }

   // Build a message sent by a remote party by reading from the socket
   // as much as possible (up to internal buffer capacity)
   void readMessage(Message &msg);
   void writeMessage(Message &msg);

   int getConnFD() const { return _connfd; }

   BIO *_ssl; // SSL connection, null if not using SSL
   int _connfd;
   ServerMessage _sMsg;
   ClientMessage _cMsg;

   static const uint8_t MAJOR_NUMBER = 1;
   static const uint16_t MINOR_NUMBER = 38;
   static const uint8_t PATCH_NUMBER = 0;
   static uint32_t CONFIGURATION_FLAGS;

private:
   void readBlocking(char *data, size_t size)
      {
      size_t totalBytesRead = 0;
      if (_ssl)
         {
         while (totalBytesRead < size)
            {
            int bytesRead = (*OBIO_read)(_ssl, data + totalBytesRead, size - totalBytesRead);
            if (bytesRead <= 0)
               {
               (*OERR_print_errors_fp)(stderr);
               throw JITServer::StreamFailure("JITServer I/O error: read error", (*OBIO_should_retry)(_ssl));
               }
            totalBytesRead += bytesRead;
            }
         }
      else
         {
         while (totalBytesRead < size)
            {
            ssize_t bytesRead = read(_connfd, data + totalBytesRead, size - totalBytesRead);
            if (bytesRead <= 0)
               {
               throw JITServer::StreamFailure("JITServer I/O error: read error: " +
                                              (bytesRead ? std::string(strerror(errno)) : "connection closed by peer"), EAGAIN == errno);
               }
            totalBytesRead += bytesRead;
            }
         }
      }

   int32_t readOnceBlocking(char *data, size_t size)
      {
      int32_t bytesRead = -1;
      if (_ssl)
         {
         bytesRead = (*OBIO_read)(_ssl, data, size);
         if (bytesRead <= 0)
            {
            (*OERR_print_errors_fp)(stderr);
            throw JITServer::StreamFailure("JITServer I/O error: read error", (*OBIO_should_retry)(_ssl));
            }
         }
      else
         {
         bytesRead = read(_connfd, data, size);
         if (bytesRead <= 0)
            {
            throw JITServer::StreamFailure("JITServer I/O error: read error: " +
                                           (bytesRead ? std::string(strerror(errno)) : "connection closed by peer"), EAGAIN == errno);
            }
         }
      return bytesRead;
      }

   void writeBlocking(const char *data, size_t size)
      {
      size_t totalBytesWritten = 0;
      if (_ssl)
         {
         while (totalBytesWritten < size)
            {
            int bytesWritten = (*OBIO_write)(_ssl, data + totalBytesWritten, size - totalBytesWritten);
            if (bytesWritten <= 0)
               {
               (*OERR_print_errors_fp)(stderr);
               throw JITServer::StreamFailure("JITServer I/O error: write error");
               }
            totalBytesWritten += bytesWritten;
            }
         }
      else
         {
         while (totalBytesWritten < size)
            {
            ssize_t bytesWritten = write(_connfd, data + totalBytesWritten, size - totalBytesWritten);
            if (bytesWritten <= 0)
               {
               throw JITServer::StreamFailure("JITServer I/O error: write error: " + std::string(strerror(errno)));
               }
            totalBytesWritten += bytesWritten;
            }
         }
      }
   }; // class CommunicationStream
}; // namespace JITServer

#endif // COMMUNICATION_STREAM_H
