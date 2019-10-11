/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

#include "net/Message.hpp"
#include "env/TRMemory.hpp"
#include "env/CompilerEnv.hpp" // for TR::Compiler->target.is64Bit()
#include "control/Options.hpp" // TR::Options::useCompressedPointers()
#include "net/MessageBuffer.hpp"
#include "net/SSLProtobufStream.hpp"
#include <unistd.h>

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

   static void initVersion();

   static uint64_t getJITServerVersion()
      {
      return ((((uint64_t)CONFIGURATION_FLAGS) << 32) | (MAJOR_NUMBER << 24) | (MINOR_NUMBER << 8));
      }

protected:
   CommunicationStream() :
      _ssl(NULL)
      {
      }

   ~CommunicationStream()
      {
      close(_connfd);
      }

   void initStream(int connfd, BIO *ssl)
      {
      _connfd = connfd;
      _ssl = ssl;
      }



   void readMessage(Message &msg);
   void writeMessage(Message &msg);

   int getConnFD() { return _connfd; }


   
   // TODO: verify that ssl actually works (probably not)
   BIO *_ssl; // SSL connection, null if not using SSL
   int _connfd;
   ServerMessage _sMsg;
   ClientMessage _cMsg;

   static const uint8_t MAJOR_NUMBER = 0;
   static const uint16_t MINOR_NUMBER = 3;
   static const uint8_t PATCH_NUMBER = 0;
   static uint32_t CONFIGURATION_FLAGS;

private:
   // readBlocking and writeBlocking are functions that directly read/write
   // passed object from/to the socket. For the object to be correctly written,
   // it needs to be contiguous.
   template <typename T>
   void readBlocking(T &val)
      {
      readBlocking(&val, sizeof(T));
      }

   template <typename T>
   void readBlocking(T *ptr, size_t size)
      {
      int32_t bytesRead = read(_connfd, ptr, size);
      if (bytesRead == -1)
         {
         throw JITServer::StreamFailure("JITServer I/O error: read error");
         }
      else if (bytesRead < size)
         {
         char *remainingPtr = reinterpret_cast<char *>(ptr) + bytesRead;
         int32_t remainingRead = read(_connfd, remainingPtr, size - bytesRead);
         if (remainingRead != size - bytesRead)
            {
            throw JITServer::StreamFailure("JITServer I/O error: read error");
            }
         }
      }

   template <typename T>
   void writeBlocking(const T &val)
      {
      writeBlocking(&val, sizeof(T));
      }

   template <typename T>
   void writeBlocking(T *ptr, size_t size)
      {
      int32_t bytesWritten = write(_connfd, ptr, size);
      if (bytesWritten == -1)
         {
         throw JITServer::StreamFailure("JITServer I/O error: write error");
         }
      else if (bytesWritten < size)
         {
         char *remainingPtr = reinterpret_cast<char *>(ptr) + bytesWritten;
         int32_t remainingWritten = write(_connfd, remainingPtr, size - bytesWritten);
         if (remainingWritten != size - bytesWritten)
            {
            throw JITServer::StreamFailure("JITServer I/O error: write error");
            }
         }
      }
   };
};

#endif
