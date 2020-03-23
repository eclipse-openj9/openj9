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

#ifndef LISTENER_HPP
#define LISTENER_HPP

#include "j9.h"
#include "infra/Monitor.hpp"  // TR::Monitor
#include "net/ServerStream.hpp"

 /**
    @class TR_Listener
    @brief Implementation of a "listener" thread that waits for network connection requests 

    This thread will be created at the JITServer. 
    Typical sequence executed by a JITServer is:
    (1) Create a TR_Listener object with "allocate()" function
    (2) Start a listener thread with  listener->startListenerThread(javaVM);
 */

#define OPENJ9_LISTENER_POLL_TIMEOUT 100 // in milliseconds

class BaseCompileDispatcher;

class TR_Listener
   {
public:
   TR_Listener();
   static TR_Listener* allocate();
   void startListenerThread(J9JavaVM *javaVM);
   void stop();
   /**
      @brief Function called to deal with incoming connection requests

      This function opens a socket (non-blocking), binds it and then waits for incoming
      connection by polling on it with a timeout (see OPENJ9_LISTENER_POLL_TIMEOUT).
      If it ever comes out of polling (due to timeout or a new connection request),
      it checks the exit flag. If the flag is set, then the thread exits.
      Otherwise, it establishes the connection using accept().
      Once a connection is accepted a ServerStream object is created (receiving the newly
      opened socket descriptor as a parameter) and passed to the compilation handler.
      Typically, the compilation handler places the ServerStream object in a queue and
      returns immediately so that other connection requests can be accepted.
      Note: it must be executed on a separate thread as it needs to keep listening for new connections.

      @param [in] compiler Object that defines the behavior when a new connection is accepted
   */
   void serveRemoteCompilationRequests(BaseCompileDispatcher *compiler);
   int32_t waitForListenerThreadExit(J9JavaVM *javaVM);
   void setAttachAttempted(bool b) { _listenerThreadAttachAttempted = b; }
   bool getAttachAttempted() const { return _listenerThreadAttachAttempted; }

   J9VMThread* getListenerThread() const { return _listenerThread; }
   void setListenerThread(J9VMThread* thread) { _listenerThread = thread; }
   j9thread_t getListenerOSThread() const { return _listenerOSThread; }
   TR::Monitor* getListenerMonitor() const { return _listenerMonitor; }

   bool getListenerThreadExitFlag() const { return _listenerThreadExitFlag; }
   void setListenerThreadExitFlag() { _listenerThreadExitFlag = true; }

private:
   J9VMThread *_listenerThread;
   TR::Monitor *_listenerMonitor;
   j9thread_t _listenerOSThread;
   volatile bool _listenerThreadAttachAttempted;
   volatile bool _listenerThreadExitFlag;
   };

/**
   @class BaseCompileDispatcher
   @brief Abstract class defining the interface for the compilation handler

   Typically, an user would derive this class and provide an implementation for "compile()"
   An instance of the derived class needs to be passed to serveRemoteCompilationRequests
   which internally calls "compile()"
 */
class BaseCompileDispatcher
   {
public:
   virtual void compile(JITServer::ServerStream *stream) = 0;
   };

#endif
