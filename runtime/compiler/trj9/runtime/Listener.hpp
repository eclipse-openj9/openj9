#ifndef LISTENER_HPP
#define LISTENER_HPP

#include "j9.h"
#include "infra/Monitor.hpp"  // TR::Monitor

class TR_Listener
   {
public:
   TR_Listener();
   static TR_Listener* allocate();
   void startListenerThread(J9JavaVM *javaVM);
   void setAttachAttempted(bool b) { _listenerThreadAttachAttempted = b; }
   bool getAttachAttempted() { return _listenerThreadAttachAttempted; }

   J9VMThread* getListenerThread() { return _listenerThread; }
   void setListenerThread(J9VMThread* thread) { _listenerThread = thread; }
   j9thread_t getListenerOSThread() { return _listenerOSThread; }
   TR::Monitor* getListenerMonitor() { return _listenerMonitor; }

   uint32_t getListenerThreadExitFlag() { return _listenerThreadExitFlag; }
   void setListenerThreadExitFlag() { _listenerThreadExitFlag = 1; }

private:
   J9VMThread *_listenerThread;
   TR::Monitor *_listenerMonitor;
   j9thread_t _listenerOSThread;
   volatile bool _listenerThreadAttachAttempted;
   volatile uint32_t _listenerThreadExitFlag;
   };

#endif


