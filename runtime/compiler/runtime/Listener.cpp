/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

#include "runtime/Listener.hpp"
#include "rpc/J9Server.hpp"
#include "env/VMJ9.h"
#include "runtime/CompileService.hpp"

TR_Listener::TR_Listener()
   : _listenerThread(NULL), _listenerMonitor(NULL), _listenerOSThread(NULL), 
   _listenerThreadAttachAttempted(false), _listenerThreadExitFlag(false)
   {
   }

TR_Listener * TR_Listener::allocate()
   {
   TR_Listener * listener = new (PERSISTENT_NEW) TR_Listener();
   return listener;
   }

static int32_t J9THREAD_PROC listenerThreadProc(void * entryarg)
   {
   J9JITConfig * jitConfig = (J9JITConfig *) entryarg;
   J9JavaVM * vm = jitConfig->javaVM;
   TR_Listener *listener = ((TR_JitPrivateConfig*)(jitConfig->privateConfig))->listener; 
   J9VMThread *listenerThread = NULL;
   PORT_ACCESS_FROM_JITCONFIG(jitConfig);

   int rc = vm->internalVMFunctions->internalAttachCurrentThread(vm, &listenerThread, NULL,
                                  J9_PRIVATE_FLAGS_DAEMON_THREAD | J9_PRIVATE_FLAGS_NO_OBJECT |
                                  J9_PRIVATE_FLAGS_SYSTEM_THREAD | J9_PRIVATE_FLAGS_ATTACHED_THREAD,
                                  listener->getListenerOSThread()); 

   listener->getListenerMonitor()->enter();
   listener->setAttachAttempted(true);
   if (rc == JNI_OK)
      listener->setListenerThread(listenerThread);
   listener->getListenerMonitor()->notifyAll();
   listener->getListenerMonitor()->exit();
   if (rc != JNI_OK)
      return JNI_ERR; // attaching the JITaaS Listener thread failed

   j9thread_set_name(j9thread_self(), "JITaaS Server Listener");

   JITaaS::J9CompileServer server;
   J9CompileDispatcher handler(jitConfig, listenerThread);
   TR::PersistentInfo *info = getCompilationInfo(jitConfig)->getPersistentInfo();
   server.buildAndServe(&handler, info);

   if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "Detaching JITaaSServer listening thread");

   vm->internalVMFunctions->DetachCurrentThread((JavaVM *) vm);
   listener->setListenerThread(NULL);
   listener->getListenerMonitor()->enter();
   listener->setListenerThreadExitFlag();
   listener->getListenerMonitor()->notifyAll();
   j9thread_exit((J9ThreadMonitor*)listener->getListenerMonitor()->getVMMonitor());

   return 0;
   }

void TR_Listener::startListenerThread(J9JavaVM *javaVM)
   {
   PORT_ACCESS_FROM_JAVAVM(javaVM); 

   UDATA priority;
   priority = J9THREAD_PRIORITY_NORMAL;

   _listenerMonitor = TR::Monitor::create("JITaaS-ListenerMonitor");
   if (_listenerMonitor)
      {
      // create the thread for listening to JITaaS Client compilation request
      const UDATA defaultOSStackSize = javaVM->defaultOSStackSize; //256KB stack size
      if (javaVM->internalVMFunctions->createThreadWithCategory(&_listenerOSThread, 
                                                               defaultOSStackSize,
                                                               priority,
                                                               0,
                                                               &listenerThreadProc,
                                                               javaVM->jitConfig,
                                                               J9THREAD_CATEGORY_SYSTEM_JIT_THREAD))
         { // cannot create the listener thread
         j9tty_printf(PORTLIB, "Error: Unable to create JITaaS Listener Thread.\n"); 
         TR::Monitor::destroy(_listenerMonitor);
         _listenerMonitor = NULL;
         }
      else // must wait here until the thread gets created; otherwise an early shutdown
         { // does not know whether or not to destroy the thread
         _listenerMonitor->enter();
         while (!getAttachAttempted())
            _listenerMonitor->wait();
         _listenerMonitor->exit();
         if (!getListenerThread())
            {
            j9tty_printf(PORTLIB, "Error: JITaaS Listener Thread attach failed.\n");
            }
         }
      }
   else
      {
      j9tty_printf(PORTLIB, "Error: Unable to create JITaaS Listener Monitor\n");
      }
   }
