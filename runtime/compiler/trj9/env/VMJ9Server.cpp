#include "rpc/J9Server.h"
#include "VMJ9Server.hpp"
#include "control/CompilationRuntime.hpp"
#include "trj9/control/CompilationThread.hpp"
#include "trj9/control/MethodToBeCompiled.hpp"

bool
TR_J9ServerVM::isClassLibraryMethod(TR_OpaqueMethodBlock *method, bool vettedForAOT)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_isClassLibraryMethod, method, vettedForAOT);
   return std::get<0>(stream->read<bool>());
   }

bool
TR_J9ServerVM::isClassLibraryClass(TR_OpaqueClassBlock *clazz)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_isClassLibraryClass, clazz);
   return std::get<0>(stream->read<bool>());
   }

TR_OpaqueClassBlock *
TR_J9ServerVM::getSuperClass(TR_OpaqueClassBlock *classPointer)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_getSuperClass, classPointer);
   return std::get<0>(stream->read<TR_OpaqueClassBlock *>());
   }

TR_Method *
TR_J9ServerVM::createMethod(TR_Memory * trMemory, TR_OpaqueClassBlock * clazz, int32_t refOffset)
   {
   return new (trMemory->trHeapMemory()) TR_J9Method(this, trMemory, TR::Compiler->cls.convertClassOffsetToClassPtr(clazz), refOffset, true);
   }

TR_YesNoMaybe
TR_J9ServerVM::isInstanceOf(TR_OpaqueClassBlock *a, TR_OpaqueClassBlock *b, bool objectTypeIsFixed, bool castTypeIsFixed, bool optimizeForAOT)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_isInstanceOf, a, b, objectTypeIsFixed, castTypeIsFixed, optimizeForAOT);
   return std::get<0>(stream->read<TR_YesNoMaybe>());
   }

bool
TR_J9ServerVM::isInterfaceClass(TR_OpaqueClassBlock *clazzPointer)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_isInterfaceClass, clazzPointer);
   return std::get<0>(stream->read<bool>());
   }

bool
TR_J9ServerVM::isClassArray(TR_OpaqueClassBlock *clazzPointer)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_isClassArray, clazzPointer);
   return std::get<0>(stream->read<bool>());
   }

bool
TR_J9ServerVM::isClassFinal(TR_OpaqueClassBlock *clazzPointer)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_isClassFinal, clazzPointer);
   return std::get<0>(stream->read<bool>());
   }

bool
TR_J9ServerVM::isAbstractClass(TR_OpaqueClassBlock *clazzPointer)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_isAbstractClass, clazzPointer);
   return std::get<0>(stream->read<bool>());
   }

TR_OpaqueClassBlock *
TR_J9ServerVM::getSystemClassFromClassName(const char * name, int32_t length, bool isVettedForAOT)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_getSystemClassFromClassName, std::string(name, length), isVettedForAOT);
   return std::get<0>(stream->read<TR_OpaqueClassBlock *>());
   }

bool
TR_J9ServerVM::isMethodEnterTracingEnabled(TR_OpaqueMethodBlock *method)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_isMethodEnterTracingEnabled, method);
   return std::get<0>(stream->read<bool>());
   }

bool
TR_J9ServerVM::isMethodExitTracingEnabled(TR_OpaqueMethodBlock *method)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_isMethodExitTracingEnabled, method);
   return std::get<0>(stream->read<bool>());
   }

TR_OpaqueClassBlock *
TR_J9ServerVM::getClassClassPointer(TR_OpaqueClassBlock *objectClassPointer)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_getClassClassPointer, objectClassPointer);
   return std::get<0>(stream->read<TR_OpaqueClassBlock *>());
   }

void *
TR_J9ServerVM::getClassLoader(TR_OpaqueClassBlock * classPointer)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_getClassLoader, classPointer);
   return std::get<0>(stream->read<void *>());
   }

TR_OpaqueClassBlock *
TR_J9ServerVM::getClassOfMethod(TR_OpaqueMethodBlock *method)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_getClassOfMethod, method);
   return std::get<0>(stream->read<TR_OpaqueClassBlock *>());
   }

TR_OpaqueClassBlock *
TR_J9ServerVM::getBaseComponentClass(TR_OpaqueClassBlock * clazz, int32_t & numDims)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_getBaseComponentClass, clazz, numDims);
   auto recv = stream->read<TR_OpaqueClassBlock *, int32_t>();
   numDims = std::get<1>(recv);
   return std::get<0>(recv);
   }

TR_OpaqueClassBlock *
TR_J9ServerVM::getLeafComponentClassFromArrayClass(TR_OpaqueClassBlock * arrayClass)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_getLeafComponentClassFromArrayClass, arrayClass);
   return std::get<0>(stream->read<TR_OpaqueClassBlock *>());
   }

TR_OpaqueClassBlock *
TR_J9ServerVM::getClassFromSignature(const char *sig, int32_t length, TR_ResolvedMethod *method, bool isVettedForAOT)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   std::string str(sig, length);
   stream->write(JAAS::J9ServerMessageType::VM_getClassFromSignature_0, str, method, isVettedForAOT);
   return std::get<0>(stream->read<TR_OpaqueClassBlock *>());
   }

TR_OpaqueClassBlock *
TR_J9ServerVM::getClassFromSignature(const char *sig, int32_t length, TR_OpaqueMethodBlock *method, bool isVettedForAOT)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   std::string str(sig, length);
   stream->write(JAAS::J9ServerMessageType::VM_getClassFromSignature_1, str, method, isVettedForAOT);
   return std::get<0>(stream->read<TR_OpaqueClassBlock *>());
   }

bool
TR_J9ServerVM::jitFieldsAreSame(TR_ResolvedMethod * method1, I_32 cpIndex1, TR_ResolvedMethod * method2, I_32 cpIndex2, int32_t isStatic)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;

   // Pass pointers to client mirrors of the ResolvedMethod objects instead of local objects
   TR_ResolvedJ9JAASServerMethod *serverMethod1 = static_cast<TR_ResolvedJ9JAASServerMethod*>(method1);
   TR_ResolvedJ9JAASServerMethod *serverMethod2 = static_cast<TR_ResolvedJ9JAASServerMethod*>(method2);
   TR_ResolvedMethod *clientMethod1 = serverMethod1->getRemoteMirror();
   TR_ResolvedMethod *clientMethod2 = serverMethod2->getRemoteMirror();

   stream->write(JAAS::J9ServerMessageType::VM_jitFieldsAreSame, clientMethod1, cpIndex1, clientMethod2, cpIndex2, isStatic);
   return std::get<0>(stream->read<bool>());
   }

bool
TR_J9ServerVM::jitStaticsAreSame(TR_ResolvedMethod *method1, I_32 cpIndex1, TR_ResolvedMethod *method2, I_32 cpIndex2)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;

   // Pass pointers to client mirrors of the ResolvedMethod objects instead of local objects
   TR_ResolvedJ9JAASServerMethod *serverMethod1 = static_cast<TR_ResolvedJ9JAASServerMethod*>(method1);
   TR_ResolvedJ9JAASServerMethod *serverMethod2 = static_cast<TR_ResolvedJ9JAASServerMethod*>(method2);
   TR_ResolvedMethod *clientMethod1 = serverMethod1->getRemoteMirror();
   TR_ResolvedMethod *clientMethod2 = serverMethod2->getRemoteMirror();

   stream->write(JAAS::J9ServerMessageType::VM_jitStaticsAreSame, clientMethod1, cpIndex1, clientMethod2, cpIndex2);
   return std::get<0>(stream->read<bool>());
   }

TR_OpaqueClassBlock *
TR_J9ServerVM::getComponentClassFromArrayClass(TR_OpaqueClassBlock *arrayClass)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_getComponentClassFromArrayClass, arrayClass);
   return std::get<0>(stream->read<TR_OpaqueClassBlock *>());
   }

bool
TR_J9ServerVM::classHasBeenReplaced(TR_OpaqueClassBlock *clazz)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_classHasBeenReplaced, clazz);
   return std::get<0>(stream->read<bool>());
   }

bool
TR_J9ServerVM::classHasBeenExtended(TR_OpaqueClassBlock *clazz)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_classHasBeenExtended, clazz);
   return std::get<0>(stream->read<bool>());
   }
