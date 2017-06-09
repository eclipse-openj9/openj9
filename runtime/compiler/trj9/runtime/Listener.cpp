#include "runtime/Listener.hpp"
#include "rpc/Server.h"
#include "env/VMJ9.h"
#include "runtime/CompileService.hpp"

TR_Listener::TR_Listener()
   : _listenerThread(NULL), _listenerMonitor(NULL), _listenerOSThread(NULL), 
   _listenerThreadAttachAttempted(false), _listenerThreadExitFlag(false)
{}



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
      return JNI_ERR; // attaching the JaaS Listener thread failed

   j9thread_set_name(j9thread_self(), "JaaS Server Listener");

   JAAS::SyncServer server;
   CompileService compileService(jitConfig, listenerThread);
   server.runService(&compileService);

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

   _listenerMonitor = TR::Monitor::create("JAAS-ListenerMonitor");
   if (_listenerMonitor)
      {
      // create the thread for listening to JaaS Client compilation request
      const UDATA defaultOSStackSize = javaVM->defaultOSStackSize; //256KB stack size
      if (javaVM->internalVMFunctions->createThreadWithCategory(&_listenerOSThread, 
                                                               defaultOSStackSize,
                                                               priority,
                                                               0,
                                                               &listenerThreadProc,
                                                               javaVM->jitConfig,
                                                               J9THREAD_CATEGORY_SYSTEM_JIT_THREAD))
         { // cannot create the listener thread
         j9tty_printf(PORTLIB, "Error: Unable to create JaaS Listener Thread.\n"); 
         _listenerMonitor = NULL;
         TR::Monitor::destroy(_listenerMonitor);
         }
      else // must wait here until the thread gets created; otherwise an early shutdown
         { // does not know whether or not to destroy the thread
         _listenerMonitor->enter();
         while (!getAttachAttempted())
            _listenerMonitor->wait();
         _listenerMonitor->exit();
         if (!_listenerOSThread)
            {
            j9tty_printf(PORTLIB, "Error: JaaS Listener Thread attach failed.\n");
            }
         }
      }
   else
      {
      j9tty_printf(PORTLIB, "Error: Unable to create Jaas Listener Monitor\n");
      }
   }
