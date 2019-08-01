/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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

 /**
    @class TR_Listener
    @brief Implementation of a "listener" thread that waits for network connection requests 

    This thread will be created at the JITServer. 
    Typical sequence executed by a JITServer is:
    (1) Create a TR_Listener object with "allocate()" function
    (2) Start a listener thread with  listener->startListenerThread(javaVM);
 
    The current implementation does not provide code for nicely terminating the listener thread.
 */
class TR_Listener
   {
public:
   TR_Listener();
   static TR_Listener* allocate();
   void startListenerThread(J9JavaVM *javaVM);
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

#endif
