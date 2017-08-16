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

TR_ResolvedMethod *
TR_J9ServerVM::createResolvedMethod(TR_Memory * trMemory, TR_OpaqueMethodBlock * aMethod,
                                  TR_ResolvedMethod * owningMethod, TR_OpaqueClassBlock *classForNewInstance)
   {
   return new (trMemory->trHeapMemory()) TR_ResolvedJ9JAASServerMethod(aMethod, this, trMemory, owningMethod);
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
TR_J9ServerVM::getClassFromSignature(const char *sig, int32_t length, TR_OpaqueMethodBlock *method, bool isVettedForAOT)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   std::string str(sig, length);
   stream->write(JAAS::J9ServerMessageType::VM_getClassFromSignature, str, method, isVettedForAOT);
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

bool
TR_J9ServerVM::compiledAsDLTBefore(TR_ResolvedMethod *method)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_compiledAsDLTBefore, method);
   return std::get<0>(stream->read<bool>());
   }

char *
TR_J9ServerVM::getClassNameChars(TR_OpaqueClassBlock * ramClass, int32_t & length)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_getClassNameChars, ramClass);
   const std::string className = std::get<0>(stream->read<std::string>());
   char * classNameChars = (char*) _compInfoPT->getCompilation()->trMemory()->allocateMemory(className.length(), heapAlloc);
   memcpy(classNameChars, &className[0], className.length());
   return classNameChars;
   }

uintptrj_t
TR_J9ServerVM::getOverflowSafeAllocSize()
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_getOverflowSafeAllocSize, JAAS::Void());
   return static_cast<uintptrj_t>(std::get<0>(stream->read<uint64_t>()));
   }

bool
TR_J9ServerVM::isThunkArchetype(J9Method * method)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_isThunkArchetype, method);
   return std::get<0>(stream->read<bool>());
   }

static J9UTF8 *str2utf8(char *string, int32_t length, TR_Memory *trMemory, TR_AllocationKind allocKind)
   {
   J9UTF8 *utf8 = (J9UTF8 *) trMemory->allocateMemory(length, allocKind);
   J9UTF8_SET_LENGTH(utf8, length);
   memcpy(J9UTF8_DATA(utf8), string, length);
   return utf8;
   }

int32_t
TR_J9ServerVM::printTruncatedSignature(char *sigBuf, int32_t bufLen, TR_OpaqueMethodBlock *method)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_printTruncatedSignature, method);
   auto recv = stream->read<std::string, std::string, std::string>();
   const std::string classNameStr = std::get<0>(recv);
   const std::string nameStr = std::get<1>(recv);
   const std::string signatureStr = std::get<2>(recv);
   TR_Memory *trMemory = _compInfoPT->getCompilation()->trMemory();
   J9UTF8 * className = str2utf8((char*)&classNameStr[0], classNameStr.length(), trMemory, heapAlloc);
   J9UTF8 * name = str2utf8((char*)&nameStr[0], nameStr.length(), trMemory, heapAlloc);
   J9UTF8 * signature = str2utf8((char*)&signatureStr[0], signatureStr.length(), trMemory, heapAlloc);
   return TR_J9VMBase::printTruncatedSignature(sigBuf, bufLen, className, name, signature);
   }

bool
TR_J9ServerVM::isPrimitiveClass(TR_OpaqueClassBlock * clazz)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_isPrimitiveClass, clazz);
   return std::get<0>(stream->read<bool>());
   }

bool
TR_J9ServerVM::isClassInitialized(TR_OpaqueClassBlock * clazz)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_isClassInitialized, clazz);
   return std::get<0>(stream->read<bool>());
   }

UDATA
TR_J9ServerVM::getOSRFrameSizeInBytes(TR_OpaqueMethodBlock * method)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_getOSRFrameSizeInBytes, method);
   return std::get<0>(stream->read<UDATA>());
   }

int32_t
TR_J9ServerVM::getByteOffsetToLockword(TR_OpaqueClassBlock * clazz)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_getByteOffsetToLockword, clazz);
   return std::get<0>(stream->read<int32_t>());
   }

bool
TR_J9ServerVM::isString(TR_OpaqueClassBlock * clazz)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_isString1, clazz);
   return std::get<0>(stream->read<bool>());
   }

void *
TR_J9ServerVM::getMethods(TR_OpaqueClassBlock * clazz)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_getMethods, clazz);
   return std::get<0>(stream->read<void *>());
   }

uint32_t
TR_J9ServerVM::getNumMethods(TR_OpaqueClassBlock * clazz)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_getNumMethods, clazz);
   return std::get<0>(stream->read<uint32_t>());
   }

uint32_t
TR_J9ServerVM::getNumInnerClasses(TR_OpaqueClassBlock * clazz)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_getNumInnerClasses, clazz);
   return std::get<0>(stream->read<uint32_t>());
   }

bool
TR_J9ServerVM::isPrimitiveArray(TR_OpaqueClassBlock *clazz)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_isPrimitiveArray, clazz);
   return std::get<0>(stream->read<bool>());
   }

uint32_t
TR_J9ServerVM::getAllocationSize(TR::StaticSymbol *classSym, TR_OpaqueClassBlock *clazz)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_getAllocationSize, classSym, clazz);
   return std::get<0>(stream->read<uint32_t>());
   }

TR_OpaqueClassBlock *
TR_J9ServerVM::getObjectClass(uintptrj_t objectPointer)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_getObjectClass, objectPointer);
   return std::get<0>(stream->read<TR_OpaqueClassBlock *>());
   }

bool
TR_J9ServerVM::stackWalkerMaySkipFrames(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *clazz)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_stackWalkerMaySkipFrames, method, clazz);
   return std::get<0>(stream->read<bool>());
   }
