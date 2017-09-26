#include "rpc/J9Server.h"
#include "VMJ9Server.hpp"
#include "control/CompilationRuntime.hpp"
#include "trj9/control/CompilationThread.hpp"
#include "trj9/control/MethodToBeCompiled.hpp"
#include "runtime/CodeCache.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/CodeCacheExceptions.hpp"

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
   return createResolvedMethodWithSignature(trMemory, aMethod, classForNewInstance, nullptr, -1, owningMethod);
   }

TR_ResolvedMethod *
TR_J9ServerVM::createResolvedMethodWithSignature(TR_Memory * trMemory, TR_OpaqueMethodBlock * aMethod, TR_OpaqueClassBlock *classForNewInstance,
                          char *signature, int32_t signatureLength, TR_ResolvedMethod * owningMethod)
   {
   TR_ResolvedJ9Method *result = new (trMemory->trHeapMemory()) TR_ResolvedJ9JAASServerMethod(aMethod, this, trMemory, owningMethod);
   if (signature)
      result->setSignature(signature, signatureLength, trMemory);
   return result;
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
   TR_ASSERT(false, "The server vm should not call getClassLoader.");
   return nullptr;
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
#if defined(J9VM_JIT_DYNAMIC_LOOP_TRANSFER)
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   auto mirror = static_cast<TR_ResolvedJ9JAASServerMethod *>(method)->getRemoteMirror();
   stream->write(JAAS::J9ServerMessageType::VM_compiledAsDLTBefore, static_cast<TR_ResolvedMethod *>(mirror));
   return std::get<0>(stream->read<bool>());
#else
   return 0;
#endif
   }

char *
TR_J9ServerVM::getClassNameChars(TR_OpaqueClassBlock * ramClass, int32_t & length)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_getClassNameChars, ramClass);
   const std::string className = std::get<0>(stream->read<std::string>());
   char * classNameChars = (char*) _compInfoPT->getCompilation()->trMemory()->allocateMemory(className.length(), heapAlloc);
   memcpy(classNameChars, &className[0], className.length());
   length = className.length();
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

bool
TR_J9ServerVM::hasFinalFieldsInClass(TR_OpaqueClassBlock *clazz)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_hasFinalFieldsInClass, clazz);
   return std::get<0>(stream->read<bool>());
   }

const char *
TR_J9ServerVM::sampleSignature(TR_OpaqueMethodBlock * aMethod, char *buf, int32_t bufLen, TR_Memory *trMemory_unused)
   {
   // the passed in TR_Memory is possibly null.
   // in the superclass it would be null if it was not needed, but here we always need it.
   // so we just get it out of the compilation.
   TR_Memory *trMemory = _compInfoPT->getCompilation()->trMemory();
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_getClassNameSignatureFromMethod, (J9Method*) aMethod);
   auto recv = stream->read<std::string, std::string, std::string>();
   const std::string str_className = std::get<0>(recv);
   const std::string str_name = std::get<1>(recv);
   const std::string str_signature = std::get<2>(recv);
   J9UTF8 * className = str2utf8((char*)&str_className[0], str_className.length(), trMemory, heapAlloc);
   J9UTF8 * name = str2utf8((char*)&str_name[0], str_name.length(), trMemory, heapAlloc);
   J9UTF8 * signature = str2utf8((char*)&str_signature[0], str_signature.length(), trMemory, heapAlloc);

   int32_t len = J9UTF8_LENGTH(className)+J9UTF8_LENGTH(name)+J9UTF8_LENGTH(signature)+3;
   char * s = len <= bufLen ? buf : (trMemory ? (char*)trMemory->allocateHeapMemory(len) : NULL);
   if (s)
      sprintf(s, "%.*s.%.*s%.*s", J9UTF8_LENGTH(className), utf8Data(className), J9UTF8_LENGTH(name), utf8Data(name), J9UTF8_LENGTH(signature), utf8Data(signature));
   return s;
   }

TR_OpaqueClassBlock *
TR_J9ServerVM::getHostClass(TR_OpaqueClassBlock *clazzOffset)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_getHostClass, clazzOffset);
   return std::get<0>(stream->read<TR_OpaqueClassBlock *>());
   }

intptrj_t
TR_J9ServerVM::getStringUTF8Length(uintptrj_t objectPointer)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_getStringUTF8Length, objectPointer);
   return std::get<0>(stream->read<intptrj_t>());
   }

uintptrj_t
TR_J9ServerVM::getPersistentClassPointerFromClassPointer(TR_OpaqueClassBlock *clazz)
   {
   J9Class *j9clazz = TR::Compiler->cls.convertClassOffsetToClassPtr(clazz);
   return reinterpret_cast<uintptrj_t>(TR::compInfoPT->getAndCacheRemoteROMClass(j9clazz));
   }

bool
TR_J9ServerVM::classInitIsFinished(TR_OpaqueClassBlock *clazz)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_classInitIsFinished, clazz);
   return std::get<0>(stream->read<bool>());
   }

TR_OpaqueClassBlock *
TR_J9ServerVM::getClassFromNewArrayType(int32_t arrayType)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_getClassFromNewArrayType, arrayType);
   return std::get<0>(stream->read<TR_OpaqueClassBlock *>());
   }

bool
TR_J9ServerVM::isCloneable(TR_OpaqueClassBlock *clazzPointer)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_isCloneable, clazzPointer);
   return std::get<0>(stream->read<bool>());
   }

bool
TR_J9ServerVM::canAllocateInlineClass(TR_OpaqueClassBlock *clazz)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_canAllocateInlineClass, clazz);
   return std::get<0>(stream->read<bool>());
   }

TR_OpaqueClassBlock *
TR_J9ServerVM::getArrayClassFromComponentClass(TR_OpaqueClassBlock *componentClass)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_getArrayClassFromComponentClass, componentClass);
   return std::get<0>(stream->read<TR_OpaqueClassBlock *>());
   }

J9Class *
TR_J9ServerVM::matchRAMclassFromROMclass(J9ROMClass *clazz, TR::Compilation *comp)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_matchRAMclassFromROMclass, clazz);
   return std::get<0>(stream->read<J9Class *>());
   }

int32_t *
TR_J9ServerVM::getCurrentLocalsMapForDLT(TR::Compilation *comp)
   {
   int32_t *currentBundles = NULL;
#if defined(J9VM_JIT_DYNAMIC_LOOP_TRANSFER)
   TR_ResolvedJ9JAASServerMethod *currentMethod = (TR_ResolvedJ9JAASServerMethod *)(comp->getCurrentMethod());
   int32_t numBundles = currentMethod->numberOfTemps() + currentMethod->numberOfParameterSlots();
   numBundles = (numBundles+31)/32;
   currentBundles = (int32_t *)comp->trMemory()->allocateHeapMemory(numBundles * sizeof(int32_t));
   jitConfig->javaVM->localMapFunction(_portLibrary, currentMethod->romClassPtr(), currentMethod->romMethod(), comp->getDltBcIndex(), (U_32 *)currentBundles, NULL, NULL, NULL);
#endif
   return currentBundles;
   }

uintptrj_t
TR_J9ServerVM::getReferenceFieldAtAddress(uintptrj_t fieldAddress)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_getReferenceFieldAtAddress, fieldAddress);
   return std::get<0>(stream->read<uintptrj_t>());
   }

uintptrj_t
TR_J9ServerVM::getVolatileReferenceFieldAt(uintptrj_t objectPointer, uintptrj_t fieldOffset)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_getVolatileReferenceFieldAt, objectPointer, fieldOffset);
   return std::get<0>(stream->read<uintptrj_t>());
   }

int32_t
TR_J9ServerVM::getInt32FieldAt(uintptrj_t objectPointer, uintptrj_t fieldOffset)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_getInt32FieldAt, objectPointer, fieldOffset);
   return std::get<0>(stream->read<int32_t>());
   }

int64_t
TR_J9ServerVM::getInt64FieldAt(uintptrj_t objectPointer, uintptrj_t fieldOffset)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_getInt64FieldAt, objectPointer, fieldOffset);
   return std::get<0>(stream->read<int64_t>());
   }

void
TR_J9ServerVM::setInt64FieldAt(uintptrj_t objectPointer, uintptrj_t fieldOffset, int64_t newValue)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_setInt64FieldAt, objectPointer, fieldOffset, newValue);
   }

bool
TR_J9ServerVM::compareAndSwapInt64FieldAt(uintptrj_t objectPointer, uintptrj_t fieldOffset, int64_t oldValue, int64_t newValue)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_compareAndSwapInt64FieldAt, objectPointer, fieldOffset, oldValue, newValue);
   return std::get<0>(stream->read<bool>());
   }

intptrj_t
TR_J9ServerVM::getArrayLengthInElements(uintptrj_t objectPointer)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_getArrayLengthInElements, objectPointer);
   return std::get<0>(stream->read<intptrj_t>());
   }

TR_OpaqueClassBlock *
TR_J9ServerVM::getClassFromJavaLangClass(uintptrj_t objectPointer)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_getClassFromJavaLangClass, objectPointer);
   return std::get<0>(stream->read<TR_OpaqueClassBlock *>());
   }

UDATA
TR_J9ServerVM::getOffsetOfClassFromJavaLangClassField()
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_getOffsetOfClassFromJavaLangClassField, JAAS::Void());
   return std::get<0>(stream->read<UDATA>());
   }

uintptrj_t
TR_J9ServerVM::getConstantPoolFromMethod(TR_OpaqueMethodBlock *method)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_getConstantPoolFromMethod, method);
   return std::get<0>(stream->read<uintptrj_t>());
   }

uintptrj_t
TR_J9ServerVM::getProcessID()
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_getProcessID, JAAS::Void());
   return std::get<0>(stream->read<uintptrj_t>());
   }

UDATA
TR_J9ServerVM::getIdentityHashSaltPolicy()
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_getIdentityHashSaltPolicy, JAAS::Void());
   return std::get<0>(stream->read<UDATA>());
   }

UDATA
TR_J9ServerVM::getOffsetOfJLThreadJ9Thread()
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_getOffsetOfJLThreadJ9Thread, JAAS::Void());
   return std::get<0>(stream->read<UDATA>());
   }

bool
TR_J9ServerVM::scanReferenceSlotsInClassForOffset(TR::Compilation *comp, TR_OpaqueClassBlock *clazz, int32_t offset)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_scanReferenceSlotsInClassForOffset, clazz, offset);
   return std::get<0>(stream->read<bool>());
   }

int32_t
TR_J9ServerVM::findFirstHotFieldTenuredClassOffset(TR::Compilation *comp, TR_OpaqueClassBlock *clazz)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_findFirstHotFieldTenuredClassOffset, clazz);
   return std::get<0>(stream->read<int32_t>());
   }

TR_OpaqueMethodBlock *
TR_J9ServerVM::getResolvedVirtualMethod(TR_OpaqueClassBlock * classObject, I_32 virtualCallOffset, bool ignoreRtResolve)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_getResolvedVirtualMethod, classObject, virtualCallOffset, ignoreRtResolve);
   return std::get<0>(stream->read<TR_OpaqueMethodBlock *>());
   }

// JAAS temporary HACK
//=======================
TR::CodeCache *
TR_J9ServerVM::getDesignatedCodeCache(TR::Compilation *comp)
   {
   if (comp->getOptimizationPlan()->_mandatoryCodeAddress)
      {
      // find the code cache that contains this address and reserve it
      // No need to  releaseClassUnloadMonitorAndAcquireVMaccessIfNeeded because new code caches cannor be added
      TR::CodeCache *codeCache = TR::CodeCacheManager::instance()->findCodeCacheFromPC((void *)comp->getOptimizationPlan()->_mandatoryCodeAddress);
      if (codeCache)
         {
         if (!codeCache->isReserved())
            {
            int32_t compThreadID = comp ? comp->getCompThreadID() : -1;
            codeCache->reserve(compThreadID);
            codeCache->setWarmCodeAlloc(comp->getOptimizationPlan()->_mandatoryCodeAddress);
            comp->setAotMethodCodeStart((uint32_t*)comp->getOptimizationPlan()->_mandatoryCodeAddress);
            if (TR::Options::getVerboseOption(TR_VerboseCodeCache))
               {
               TR_VerboseLog::writeLineLocked(TR_Vlog_CODECACHE, "Reserved code cache %p that includes mandatory address %p. WarmCodeAlloc=%p", 
                  codeCache, comp->getOptimizationPlan()->_mandatoryCodeAddress, codeCache->getWarmCodeAlloc());
               }
            }
         else // Desired cache is already reserved; throw an error
            {
            comp->failCompilation<TR::CodeCacheError>("Desired code cache is already reserved");
            }
         }
      else // Cannot find any cache that holds the desired address
         {
         // Throw an exception
         comp->failCompilation<TR::CodeCacheError>("Cannot find the code cache that contains desired address");
         // Note that given the current hack, the server must preallocate all the code caches inside the repository
         // Use -Xjit:numCodeCachesOnStartup=128 for default values of total code cache and code cache sizes
         }
      return codeCache;
      }
   else
      {
      // TODO: change this if we inherit directly from TR_J9VM
      return TR_J9VM::getDesignatedCodeCache(comp);
      }
   }

static void switchCodeCache(TR::Compilation *comp, TR::CodeCache *oldCache, TR::CodeCache *newCache)
   {
   comp->switchCodeCache(newCache);
   // If the old CC had pre-loaded code, the current compilation may have initialized it and will therefore depend on it
   // so we should initialize it in the new CC as well
   // XXX: We could avoid this if we knew for sure that this compile wasn't the one who initialized it
   if (newCache && oldCache->isCCPreLoadedCodeInitialized())
      newCache->getCCPreLoadedCodeAddress(TR_numCCPreLoadedCode, comp->cg());
   }

uint8_t *
TR_J9ServerVM::allocateCodeMemory(TR::Compilation * comp, uint32_t warmCodeSize, uint32_t coldCodeSize, uint8_t ** coldCode, bool isMethodHeaderNeeded)
   {
   if (comp->getOptimizationPlan()->_mandatoryCodeAddress)
      {
      // TR_ASSERT(coldCodeSize==0, "We should not request a cold block for JAAS");
      // JAAS TODO: We should always request a non cold block for JAAS, below is a temporary workaround to enforce this, 
      // with the trick that making use of how current OMR::CodeGenerator::allocateCodeMemory method performs
      if (warmCodeSize == 0)
         {
         warmCodeSize = coldCodeSize;
         coldCodeSize = 0;
         }

      uint8_t *warmCode = nullptr;
      // We must have reserved a code cache that contains _mandatoryCodeAddress
      TR::CodeCache * codeCache = comp->getCurrentCodeCache(); // This is the reserved code cache
      TR_ASSERT(codeCache && codeCache->isReserved(), "Code cache should have been reserved.");

      bool hadClassUnloadMonitor;
      bool hadVMAccess = releaseClassUnloadMonitorAndAcquireVMaccessIfNeeded(comp, &hadClassUnloadMonitor);

      warmCode = TR::CodeCacheManager::instance()->allocateCodeMemory(warmCodeSize, coldCodeSize, &codeCache, coldCode, needsContiguousAllocation(), isMethodHeaderNeeded);

      acquireClassUnloadMonitorAndReleaseVMAccessIfNeeded(comp, hadVMAccess, hadClassUnloadMonitor);

      if (codeCache != comp->getCurrentCodeCache())
         {
         TR_ASSERT(!codeCache || codeCache->isReserved(), "Substitute code cache isn't marked as reserved");  // Either we didn't get a code cache, or the one we should get is
         comp->setAotMethodCodeStart(warmCode);
         switchCodeCache(comp, comp->getCurrentCodeCache(), codeCache);
         }

      if (NULL == warmCode)
         {
         if (jitConfig->runtimeFlags & J9JIT_CODE_CACHE_FULL)
            comp->failCompilation<TR::CodeCacheError>("Failed to allocate code memory");
         comp->failCompilation<TR::RecoverableCodeCacheError>("Failed to allocate code memory");
         }
      return warmCode;
      }
   else
      {
      return TR_J9VM::allocateCodeMemory(comp, warmCodeSize, coldCodeSize, coldCode, isMethodHeaderNeeded);
      }
   }


bool
TR_J9ServerVM::sameClassLoaders(TR_OpaqueClassBlock * class1, TR_OpaqueClassBlock * class2)
   {
   // conservative answer
   // JAAS TODO: ask client
   return false;
   }

bool
TR_J9ServerVM::isUnloadAssumptionRequired(TR_OpaqueClassBlock *, TR_ResolvedMethod *)
   {
   // conservative answer
   // JAAS TODO: ask client
   return true;
   }

void *
TR_J9ServerVM::setJ2IThunk(char *signatureChars, uint32_t signatureLength, void *thunkptr, TR::Compilation *comp)
   {
   // The 32-bit size and offset are held before the thunkptr.
   // The stored size does not include these two fields so we add 8.
   char *thunkStart = (char *)thunkptr - 8;
   size_t thunkSize = *((U_32 *)thunkStart) + 8;
   std::string thunk(thunkStart, thunkSize);
   std::string signature(signatureChars, signatureLength);

   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
   stream->write(JAAS::J9ServerMessageType::VM_setJ2IThunk, thunk, signature);
//   stream->read<JAAS::Void>();
   return thunkptr;
   }
