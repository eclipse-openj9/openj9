#include "rpc/J9Server.h"
#include "j9methodServer.hpp"
#include "control/CompilationRuntime.hpp"
#include "control/CompilationThread.hpp"
#include "control/MethodToBeCompiled.hpp"

J9ROMClass *
TR_ResolvedJ9JAASServerMethod::romClassFromString(const std::string &romClassStr, TR_Memory *trMemory)
   {
   auto romClass = (J9ROMClass *)(trMemory->allocateHeapMemory(romClassStr.size()));
   if (!romClass)
      throw std::bad_alloc();
   memcpy(romClass, &romClassStr[0], romClassStr.size());
   return romClass;
   }

static J9ROMMethod *
romMethodAtClassIndex(J9ROMClass *romClass, uint64_t methodIndex)
   {
   J9ROMMethod * romMethod = J9ROMCLASS_ROMMETHODS(romClass);
   for (size_t i = methodIndex; i; --i)
      romMethod = nextROMMethod(romMethod);
   return romMethod;
   }

J9ROMClass *
TR_ResolvedJ9JAASServerMethod::getRemoteROMClass(J9Class *clazz, JAAS::J9ServerStream *stream, TR_Memory *trMemory)
   {
   stream->write(JAAS::J9ServerMessageType::ResolvedMethod_getRemoteROMClass, clazz);
   auto str = std::get<0>(stream->read<std::string>());
   return romClassFromString(str, trMemory);
   }

TR_ResolvedJ9JAASServerMethod::TR_ResolvedJ9JAASServerMethod(TR_OpaqueMethodBlock * aMethod, TR_FrontEnd * fe, TR_Memory * trMemory, TR_ResolvedMethod * owningMethod, uint32_t vTableSlot)
   : TR_ResolvedJ9Method(fe, owningMethod)
   {

   TR_J9VMBase *j9fe = (TR_J9VMBase *)fe;
   TR::CompilationInfo *compInfo = TR::CompilationInfo::get(j9fe->getJ9JITConfig());
   TR::CompilationInfoPerThread *threadCompInfo = compInfo->getCompInfoForThread(j9fe->vmThread());
   _stream = threadCompInfo->getMethodBeingCompiled()->_stream;

   _ramMethod = (J9Method *)aMethod;

   // Create client side mirror of this object to use for calls involving RAM data
   TR_ResolvedJ9Method* owningMethodMirror = owningMethod ? ((TR_ResolvedJ9JAASServerMethod*) owningMethod)->_remoteMirror : nullptr;
   _stream->write(JAAS::J9ServerMessageType::mirrorResolvedJ9Method, aMethod, owningMethodMirror, vTableSlot);
   auto recv = _stream->read<TR_ResolvedJ9Method*, J9RAMConstantPoolItem*, J9Class*, uint64_t, uintptrj_t, void*, bool, bool, TR::RecognizedMethod, TR::RecognizedMethod>();

   _remoteMirror = std::get<0>(recv);

   // Cache the constantPool and constantPoolHeader
   _literals = std::get<1>(recv);
   _ramClass = std::get<2>(recv);

   _romClass = threadCompInfo->getAndCacheRemoteROMClass(_ramClass, trMemory);
   _romMethod = romMethodAtClassIndex(_romClass, std::get<3>(recv));
   _romLiterals = (J9ROMConstantPoolItem *) ((UDATA)romClassPtr() + sizeof(J9ROMClass));

   _vTableSlot = vTableSlot;
   _j9classForNewInstance = nullptr;

   _jniProperties = std::get<4>(recv);
   _jniTargetAddress = std::get<5>(recv);

   _isInterpreted = std::get<6>(recv);
   _isMethodInValidLibrary = std::get<7>(recv);

   TR::RecognizedMethod mandatoryRm = std::get<8>(recv);
   TR::RecognizedMethod rm = std::get<9>(recv);
 
   // initialization from TR_J9Method constructor
   _className = J9ROMCLASS_CLASSNAME(_romClass);
   _name = J9ROMMETHOD_GET_NAME(_romClass, _romMethod);
   _signature = J9ROMMETHOD_GET_SIGNATURE(_romClass, _romMethod);
   parseSignature(trMemory);
   _fullSignature = NULL;
  
   setMandatoryRecognizedMethod(mandatoryRm);
   setRecognizedMethod(rm);
   }

J9ROMClass *
TR_ResolvedJ9JAASServerMethod::romClassPtr()
   {
   return _romClass;
   }

J9RAMConstantPoolItem *
TR_ResolvedJ9JAASServerMethod::literals()
   {
   return _literals;
   }

J9Class *
TR_ResolvedJ9JAASServerMethod::constantPoolHdr()
   {
   return _ramClass;
   }

bool
TR_ResolvedJ9JAASServerMethod::isJNINative()
   {
   if (supportsFastJNI(_fe))
      return _jniTargetAddress != NULL;
   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_isJNINative, _remoteMirror);
   return std::get<0>(_stream->read<bool>());
   }


void
TR_ResolvedJ9JAASServerMethod::setRecognizedMethodInfo(TR::RecognizedMethod rm)
   {
   TR_ResolvedJ9Method::setRecognizedMethodInfo(rm);
   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_setRecognizedMethodInfo, _remoteMirror, rm);
   _stream->read<JAAS::Void>();
   }

J9ClassLoader *
TR_ResolvedJ9JAASServerMethod::getClassLoader()
   {
   TR_ASSERT(false, "Should not call getClassLoader on the server.");
   return nullptr;
   }

bool
TR_ResolvedJ9JAASServerMethod::staticAttributes(TR::Compilation * comp, I_32 cpIndex, void * * address, TR::DataType * type, bool * volatileP, bool * isFinal, bool * isPrivate, bool isStore, bool * unresolvedInCP, bool needAOTValidation)
   {
   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_staticAttributes, _remoteMirror, cpIndex, isStore, needAOTValidation);
   auto recv = _stream->read<void*, TR::DataTypes, bool, bool, bool, bool, bool>();
   *address = std::get<0>(recv);
   *type = std::get<1>(recv);
   *volatileP = std::get<2>(recv);
   if (isFinal) *isFinal = std::get<3>(recv);
   if (isPrivate) *isPrivate = std::get<4>(recv);
   if (unresolvedInCP) *unresolvedInCP = std::get<5>(recv);
   return std::get<6>(recv);
   }

TR_OpaqueClassBlock *
TR_ResolvedJ9JAASServerMethod::getClassFromConstantPool(TR::Compilation * comp, uint32_t cpIndex, bool)
   {
   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_getClassFromConstantPool, _remoteMirror, cpIndex);
   return std::get<0>(_stream->read<TR_OpaqueClassBlock *>());
   }

TR_OpaqueClassBlock *
TR_ResolvedJ9JAASServerMethod::getDeclaringClassFromFieldOrStatic(TR::Compilation *comp, int32_t cpIndex)
   {
   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_getDeclaringClassFromFieldOrStatic, _remoteMirror, cpIndex);
   return std::get<0>(_stream->read<TR_OpaqueClassBlock *>());
   }

TR_OpaqueClassBlock *
TR_ResolvedJ9JAASServerMethod::classOfStatic(I_32 cpIndex, bool returnClassForAOT)
   {
   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_classOfStatic, _remoteMirror, cpIndex, returnClassForAOT);
   return std::get<0>(_stream->read<TR_OpaqueClassBlock *>());
   }

bool
TR_ResolvedJ9JAASServerMethod::isUnresolvedString(I_32 cpIndex, bool optimizeForAOT)
   {
   return true;
   }

TR_ResolvedMethod *
TR_ResolvedJ9JAASServerMethod::getResolvedVirtualMethod(TR::Compilation * comp, I_32 cpIndex, bool ignoreRtResolve, bool * unresolvedInCP)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
#if TURN_OFF_INLINING
   return 0;
#else
   INCREMENT_COUNTER(_fe, totalVirtualMethodRefs);

   TR_ResolvedMethod *resolvedMethod = NULL;

   // See if the constant pool entry is already resolved or not
   //
   if (unresolvedInCP)
       *unresolvedInCP = true;

   if (!((_fe->_jitConfig->runtimeFlags & J9JIT_RUNTIME_RESOLVE) &&
         !comp->ilGenRequest().details().isMethodHandleThunk() && // cmvc 195373
         performTransformation(comp, "Setting as unresolved virtual call cpIndex=%d\n",cpIndex) ) || ignoreRtResolve)
      {
      _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_getResolvedVirtualMethod, literals(), cpIndex);
      auto recv = _stream->read<J9Method*, UDATA, bool>();
      J9Method *ramMethod = std::get<0>(recv);
      UDATA vTableIndex = std::get<1>(recv);
      if (std::get<2>(recv) && unresolvedInCP)
         *unresolvedInCP = false;

      if (vTableIndex)
         {
         TR_AOTInliningStats *aotStats = NULL;
         if (comp->getOption(TR_EnableAOTStats))
            aotStats = & (((TR_JitPrivateConfig *)_fe->_jitConfig->privateConfig)->aotStats->virtualMethods);
         resolvedMethod = createResolvedMethodFromJ9Method(comp, cpIndex, vTableIndex, ramMethod, unresolvedInCP, aotStats);
         }
      }

   if (resolvedMethod == NULL)
      {
      TR::DebugCounter::incStaticDebugCounter(comp, "resources.resolvedMethods/virtual/null");
      INCREMENT_COUNTER(_fe, unresolvedVirtualMethodRefs);
      if (unresolvedInCP)
         handleUnresolvedVirtualMethodInCP(cpIndex, unresolvedInCP);
      }
   else
      {
      if (((TR_ResolvedJ9Method*)resolvedMethod)->isVarHandleAccessMethod())
         {
         // VarHandle access methods are resolved to *_impl()V, restore their signatures to obtain function correctness in the Walker
         J9ROMConstantPoolItem *cpItem = (J9ROMConstantPoolItem *)romLiterals();
         J9ROMMethodRef *romMethodRef = (J9ROMMethodRef *)(cpItem + cpIndex);
         J9ROMNameAndSignature *nameAndSig = J9ROMFIELDREF_NAMEANDSIGNATURE(romMethodRef);
         int32_t signatureLength;
         char   *signature = utf8Data(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig), signatureLength);
         ((TR_ResolvedJ9Method *)resolvedMethod)->setSignature(signature, signatureLength, comp->trMemory());
         }

      TR::DebugCounter::incStaticDebugCounter(comp, "resources.resolvedMethods/virtual");
      TR::DebugCounter::incStaticDebugCounter(comp, "resources.resolvedMethods/virtual:#bytes", sizeof(TR_ResolvedJ9Method));
      }

   return resolvedMethod;
#endif
   }

bool
TR_ResolvedJ9JAASServerMethod::fieldAttributes(TR::Compilation * comp, I_32 cpIndex, U_32 * fieldOffset, TR::DataType * type, bool * volatileP, bool * isFinal, bool * isPrivate, bool isStore, bool * unresolvedInCP, bool needAOTValidation)
   {
   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_fieldAttributes, _remoteMirror, cpIndex, isStore, needAOTValidation);
   auto recv = _stream->read<U_32, TR::DataTypes, bool, bool, bool, bool, bool>();
   *fieldOffset = std::get<0>(recv);
   *type = std::get<1>(recv);
   *volatileP = std::get<2>(recv);
   if (isFinal) *isFinal = std::get<3>(recv);
   if (isPrivate) *isPrivate = std::get<4>(recv);
   if (unresolvedInCP) *unresolvedInCP = std::get<5>(recv);
   return std::get<6>(recv);
   }

TR_ResolvedMethod *
TR_ResolvedJ9JAASServerMethod::getResolvedStaticMethod(TR::Compilation * comp, I_32 cpIndex, bool * unresolvedInCP)
   {
   // JAAS TODO: Decide whether the counters should be updated on the server or the client
   TR_ResolvedMethod *resolvedMethod = NULL;
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
   INCREMENT_COUNTER(_fe, totalStaticMethodRefs);

   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_getResolvedStaticMethod, _remoteMirror, cpIndex);
   J9Method * ramMethod = std::get<0>(_stream->read<J9Method *>());

   bool skipForDebugging = false;
   if (isArchetypeSpecimen())
      {
      // ILGen macros currently must be resolved for correctness, or else they
      // are not recognized and expanded.  If we have unresolved calls, we can't
      // tell whether they're ilgen macros because the recognized-method system
      // only works on resovled methods.
      //
      if (ramMethod)
         skipForDebugging = false;
      else
         {
         comp->failCompilation<TR::ILGenFailure>("Can't compile an archetype specimen with unresolved calls");
         }
      }

   if (ramMethod && !skipForDebugging)
      {
      TR_AOTInliningStats *aotStats = NULL;
      if (comp->getOption(TR_EnableAOTStats))
         aotStats = & (((TR_JitPrivateConfig *)_fe->_jitConfig->privateConfig)->aotStats->staticMethods);
      resolvedMethod = createResolvedMethodFromJ9Method(comp, cpIndex, 0, ramMethod, unresolvedInCP, aotStats);
      if (unresolvedInCP)
         *unresolvedInCP = false;
      }

   if (resolvedMethod == NULL)
      {
      INCREMENT_COUNTER(_fe, unresolvedStaticMethodRefs);
      if (unresolvedInCP)
         handleUnresolvedStaticMethodInCP(cpIndex, unresolvedInCP);
      }

   return resolvedMethod;
   }

TR_ResolvedMethod *
TR_ResolvedJ9JAASServerMethod::getResolvedSpecialMethod(TR::Compilation * comp, I_32 cpIndex, bool * unresolvedInCP)
   {
   // JAAS TODO: Decide whether the counters should be updated on the server or the client
   TR_ResolvedMethod *resolvedMethod = nullptr;
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");

   INCREMENT_COUNTER(_fe, totalSpecialMethodRefs);

   if (unresolvedInCP)
      *unresolvedInCP = true;

   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_getResolvedSpecialMethod, _remoteMirror, cpIndex);
   auto recv = _stream->read<J9Method *, bool>();
   J9Method * ramMethod = std::get<0>(recv);
   bool resolve = std::get<1>(recv);

   if (resolve && ramMethod)
      {
      TR_AOTInliningStats *aotStats = nullptr;
      if (comp->getOption(TR_EnableAOTStats))
         aotStats = & (((TR_JitPrivateConfig *)_fe->_jitConfig->privateConfig)->aotStats->specialMethods);
      resolvedMethod = createResolvedMethodFromJ9Method(comp, cpIndex, 0, ramMethod, unresolvedInCP, aotStats);
      if (unresolvedInCP)
         *unresolvedInCP = false;
      }

   if (resolvedMethod == NULL)
      {
      INCREMENT_COUNTER(_fe, unresolvedSpecialMethodRefs);
      if (unresolvedInCP)
         handleUnresolvedVirtualMethodInCP(cpIndex, unresolvedInCP);
      }

   return resolvedMethod;
   }


TR_ResolvedMethod *
TR_ResolvedJ9JAASServerMethod::createResolvedMethodFromJ9Method( TR::Compilation *comp, int32_t cpIndex, uint32_t vTableSlot, J9Method *j9Method, bool * unresolvedInCP, TR_AOTInliningStats *aotStats)
   {
   return new (comp->trHeapMemory()) TR_ResolvedJ9JAASServerMethod((TR_OpaqueMethodBlock *) j9Method, _fe, comp->trMemory(), this, vTableSlot);
   }

uint32_t
TR_ResolvedJ9JAASServerMethod::classCPIndexOfMethod(uint32_t methodCPIndex)
   {
   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_classCPIndexOfMethod, _remoteMirror, methodCPIndex);
   return std::get<0>(_stream->read<uint32_t>());
   }

void *
TR_ResolvedJ9JAASServerMethod::startAddressForJittedMethod()
   {
   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_startAddressForJittedMethod, _remoteMirror);
   return std::get<0>(_stream->read<void *>());
   }

char *
TR_ResolvedJ9JAASServerMethod::localName(U_32 slotNumber, U_32 bcIndex, I_32 &len, TR_Memory *trMemory)
   {
   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_localName, _remoteMirror, slotNumber, bcIndex);
   const std::string nameString = std::get<0>(_stream->read<std::string>());
   len = nameString.length();
   char *out = (char*) trMemory->allocateHeapMemory(len);
   memcpy(out, &nameString[0], len);
   return out;
   }

bool
TR_ResolvedJ9JAASServerMethod::virtualMethodIsOverridden()
   {
   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_virtualMethodIsOverridden, _remoteMirror);
   return std::get<0>(_stream->read<bool>());
   }


TR_OpaqueClassBlock *
TR_ResolvedJ9JAASServerMethod::getResolvedInterfaceMethod(I_32 cpIndex, UDATA *pITableIndex)
   {
   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_getResolvedInterfaceMethod_2, _remoteMirror, cpIndex);
   auto recv = _stream->read<TR_OpaqueClassBlock *, UDATA>();
   *pITableIndex = std::get<1>(recv);
   return std::get<0>(recv);
   }

TR_ResolvedMethod *
TR_ResolvedJ9JAASServerMethod::getResolvedInterfaceMethod(TR::Compilation * comp, TR_OpaqueClassBlock * classObject, I_32 cpIndex)
   {
   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_getResolvedInterfaceMethod_3, getPersistentIdentifier(), classObject, cpIndex);
   auto recv = _stream->read<bool, J9Method*>();
   bool resolved = std::get<0>(recv);
   J9Method *ramMethod = std::get<1>(recv);

   // If the method ref is unresolved, the bytecodes of the ramMethod will be NULL.
   // IFF resolved, then we can look at the rest of the ref.
   //
   if (resolved)
      {
      TR_AOTInliningStats *aotStats = NULL;
      if (comp->getOption(TR_EnableAOTStats))
         aotStats = & (((TR_JitPrivateConfig *)_fe->_jitConfig->privateConfig)->aotStats->interfaceMethods);
      TR_ResolvedMethod *m = createResolvedMethodFromJ9Method(comp, cpIndex, 0, ramMethod, NULL, aotStats);

      TR_OpaqueClassBlock *c = NULL;
      if (m)
         {
         c = m->classOfMethod();
         if (c && !_fe->isInterfaceClass(c))
            {
            TR::DebugCounter::incStaticDebugCounter(comp, "resources.resolvedMethods/interface");
            TR::DebugCounter::incStaticDebugCounter(comp, "resources.resolvedMethods/interface:#bytes", sizeof(TR_ResolvedJ9Method));
            return m;
            }
         }
      }

   TR::DebugCounter::incStaticDebugCounter(comp, "resources.resolvedMethods/interface/null");
   return 0;
   }

U_32
TR_ResolvedJ9JAASServerMethod::getResolvedInterfaceMethodOffset(TR_OpaqueClassBlock * classObject, I_32 cpIndex)
   {
   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_getResolvedInterfaceMethodOffset, _remoteMirror, classObject, cpIndex);
   auto recv = _stream->read<U_32>();
   return std::get<0>(recv);
   }

void *
TR_ResolvedJ9JAASServerMethod::startAddressForJNIMethod(TR::Compilation *comp)
   {
   // For fastJNI methods, we have the address cached
   if (_jniProperties)
      return _jniTargetAddress;
   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_startAddressForJNIMethod, _remoteMirror);
   return std::get<0>(_stream->read<void *>());
   }

bool
TR_ResolvedJ9JAASServerMethod::getUnresolvedStaticMethodInCP(int32_t cpIndex)
   {
   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_getUnresolvedStaticMethodInCP, _remoteMirror, cpIndex);
   return std::get<0>(_stream->read<bool>());
   }

void *
TR_ResolvedJ9JAASServerMethod::startAddressForInterpreterOfJittedMethod()
   {
   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_startAddressForInterpreterOfJittedMethod, _remoteMirror);
   return std::get<0>(_stream->read<void *>());
   }

bool
TR_ResolvedJ9JAASServerMethod::getUnresolvedSpecialMethodInCP(I_32 cpIndex)
   {
   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_getUnresolvedSpecialMethodInCP, _remoteMirror, cpIndex);
   return std::get<0>(_stream->read<bool>());
   }

TR_ResolvedMethod *
TR_ResolvedJ9JAASServerMethod::getResolvedVirtualMethod(TR::Compilation * comp, TR_OpaqueClassBlock * classObject, I_32 virtualCallOffset , bool ignoreRtResolve)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)_fe;
   // the frontend method performs a RPC
   void * ramMethod = fej9->getResolvedVirtualMethod(classObject, virtualCallOffset, ignoreRtResolve);
   TR_ResolvedMethod *m = ramMethod ? new (comp->trHeapMemory()) TR_ResolvedJ9JAASServerMethod((TR_OpaqueMethodBlock *) ramMethod, _fe, comp->trMemory(), this) : 0;
   return m;
   }

bool
TR_ResolvedJ9JAASServerMethod::getUnresolvedFieldInCP(I_32 cpIndex)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
#if defined(J9VM_INTERP_AOT_COMPILE_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM))
   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_getUnresolvedFieldInCP, _remoteMirror, cpIndex);
   return std::get<0>(_stream->read<bool>());
#else
      return true;
#endif
   }

bool
TR_ResolvedJ9JAASServerMethod::inROMClass(void * address)
   {
   return address >= romClassPtr() && address < ((uint8_t*)romClassPtr()) + romClassPtr()->romSize;
   }

char *
TR_ResolvedJ9JAASServerMethod::getRemoteROMString(int32_t &len, void *basePtr, std::initializer_list<size_t> offsets)
   {
   size_t offsetFromROMClass = (uint8_t*) basePtr - (uint8_t*) romClassPtr();
   std::string offsetsStr((char*) offsets.begin(), offsets.size() * sizeof(size_t));
   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_getRemoteROMString, _remoteMirror, offsetFromROMClass, offsetsStr);
   std::string str = std::get<0>(_stream->read<std::string>());
   len = str.length();
   char *chars = new (TR::comp()->trHeapMemory()) char[len];
   memcpy(chars, &str[0], len);
   return chars;
   }

// Takes a pointer to some data which is placed statically relative to the rom class,
// as well as a list of offsets to J9SRP fields. The first offset is applied before the first
// SRP is followed.
//
// If at any point while following the chain of SRP pointers we land outside the ROM class,
// then we fall back to getRemoteROMString which follows the same process on the client.
//
// This is a workaround because some data referenced in the ROM constant pool is located outside of
// it, but we cannot easily determine if the reference is outside or not (or even if it is a reference!)
// because the data is untyped.
char *
TR_ResolvedJ9JAASServerMethod::getROMString(int32_t &len, void *basePtr, std::initializer_list<size_t> offsets)
   {
   uint8_t *ptr = (uint8_t*) basePtr;
   for (size_t offset : offsets)
      {
      ptr += offset;
      if (!inROMClass(ptr))
         return getRemoteROMString(len, basePtr, offsets);
      ptr = ptr + *(J9SRP*)ptr;
      }
   if (!inROMClass(ptr))
      return getRemoteROMString(len, basePtr, offsets);
   char *data = utf8Data((J9UTF8*) ptr, len);
   return data;
   }

char *
TR_ResolvedJ9JAASServerMethod::fieldOrStaticSignatureChars(I_32 cpIndex, int32_t & len)
   {
   if (cpIndex < 0)
      return 0;

   char *signature = getROMString(len, &romCPBase()[cpIndex],
                           {
                           offsetof(J9ROMFieldRef, nameAndSignature),
                           offsetof(J9ROMNameAndSignature, signature)
                           });
   return signature;
   }

char *
TR_ResolvedJ9JAASServerMethod::getClassNameFromConstantPool(uint32_t cpIndex, uint32_t &length)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");

   int32_t len;
   char *name = getROMString(len, &romLiterals()[cpIndex],
                           {
                           offsetof(J9ROMClassRef, name),
                           });
   length = len; // this is what happends when you have no type conventions
   return name;
   }

char *
TR_ResolvedJ9JAASServerMethod::classNameOfFieldOrStatic(I_32 cpIndex, int32_t & len)
   {
   if (cpIndex == -1)
      return 0;

   J9ROMFieldRef * ref = (J9ROMFieldRef *) (&romCPBase()[cpIndex]);
   TR_ASSERT(inROMClass(ref), "field ref must be in ROM class"); // for now. if this fails make it a remote call
   char *name = getROMString(len, &romCPBase()[ref->classRefCPIndex],
                           {
                           offsetof(J9ROMClassRef, name),
                           });
   return name;
   }

char *
TR_ResolvedJ9JAASServerMethod::classSignatureOfFieldOrStatic(I_32 cpIndex, int32_t & len)
   {
   if (cpIndex == -1)
      return 0;

   char *signature = getROMString(len, &romCPBase()[cpIndex],
                           {
                           offsetof(J9ROMFieldRef, nameAndSignature),
                           offsetof(J9ROMNameAndSignature, signature),
                           });
   return signature;
   }

char *
TR_ResolvedJ9JAASServerMethod::fieldOrStaticNameChars(I_32 cpIndex, int32_t & len)
   {
   if (cpIndex < 0)
      return 0;

   char *name = getROMString(len, &romCPBase()[cpIndex],
                           {
                           offsetof(J9ROMFieldRef, nameAndSignature),
                           offsetof(J9ROMNameAndSignature, name)
                           });
   return name;
   }

char *
TR_ResolvedJ9JAASServerMethod::fieldOrStaticName(I_32 cpIndex, int32_t & len, TR_Memory * trMemory, TR_AllocationKind kind)
   {
   if (cpIndex == -1)
      return "<internal name>";

   J9ROMFieldRef * ref = (J9ROMFieldRef *) (&romCPBase()[cpIndex]);
   J9ROMNameAndSignature * nameAndSignature = J9ROMFIELDREF_NAMEANDSIGNATURE(ref);

   if (inROMClass(nameAndSignature))
      {
      J9UTF8 * declName = J9ROMCLASSREF_NAME((J9ROMClassRef *) (&romCPBase()[ref->classRefCPIndex]));
      J9UTF8 * name = J9ROMNAMEANDSIGNATURE_NAME(nameAndSignature);
      J9UTF8 * signature = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSignature);

      if (inROMClass(declName) && inROMClass(name) && inROMClass(signature))
         {
         len = J9UTF8_LENGTH(declName) + J9UTF8_LENGTH(name) + J9UTF8_LENGTH(signature) +3;
         char * s = (char *)trMemory->allocateMemory(len, kind);
         sprintf(s, "%.*s.%.*s %.*s",
                 J9UTF8_LENGTH(declName), utf8Data(declName),
                 J9UTF8_LENGTH(name), utf8Data(name),
                 J9UTF8_LENGTH(signature), utf8Data(signature));
         return s;
         }
      }

   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_fieldOrStaticName, _remoteMirror, cpIndex);
   std::string recv = std::get<0>(_stream->read<std::string>());
   len = recv.length();
   char * s = (char *)trMemory->allocateMemory(len, kind);
   memcpy(s, &recv[0], len);
   return s;
   }

void *
TR_ResolvedJ9JAASServerMethod::stringConstant(I_32 cpIndex)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
   return (void *) ((U_8 *)&(((J9RAMStringRef *) romLiterals())[cpIndex].stringObject));
   }


bool
TR_ResolvedJ9JAASServerMethod::isSubjectToPhaseChange(TR::Compilation *comp)
   {
   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_isSubjectToPhaseChange, _remoteMirror);
   return std::get<0>(_stream->read<bool>());
   }

TR_ResolvedMethod *
TR_ResolvedJ9JAASServerMethod::getResolvedHandleMethod(TR::Compilation * comp, I_32 cpIndex, bool * unresolvedInCP)
   {
   TR_ASSERT(cpIndex != -1, "cpIndex shouldn't be -1");
#if TURN_OFF_INLINING
   return 0;
#else

   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_getResolvedHandleMethod, _remoteMirror, cpIndex);
   auto recv = _stream->read<TR_OpaqueMethodBlock *, std::string, bool>();
   auto dummyInvokeExact = std::get<0>(recv);
   auto signature = std::get<1>(recv);
   if (unresolvedInCP)
      *unresolvedInCP = std::get<2>(recv);

   return _fe->createResolvedMethodWithSignature(comp->trMemory(), dummyInvokeExact, NULL, &signature[0], signature.length(), this);
#endif
   }

#if defined(J9VM_OPT_REMOVE_CONSTANT_POOL_SPLITTING)
void *
TR_ResolvedJ9JAASServerMethod::methodTypeTableEntryAddress(int32_t cpIndex)
   {
   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_methodTypeTableEntryAddress, _remoteMirror, cpIndex);
   return std::get<0>(_stream->read<void*>());
   }

bool
TR_ResolvedJ9JAASServerMethod::isUnresolvedMethodTypeTableEntry(int32_t cpIndex)
   {
   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_isUnresolvedMethodTypeTableEntry, _remoteMirror, cpIndex);
   return std::get<0>(_stream->read<bool>());
   }
#endif

bool
TR_ResolvedJ9JAASServerMethod::isUnresolvedCallSiteTableEntry(int32_t callSiteIndex)
   {
   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_isUnresolvedCallSiteTableEntry, _remoteMirror, callSiteIndex);
   return std::get<0>(_stream->read<bool>());
   }

void *
TR_ResolvedJ9JAASServerMethod::callSiteTableEntryAddress(int32_t callSiteIndex)
   {
   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_callSiteTableEntryAddress, _remoteMirror, callSiteIndex);
   return std::get<0>(_stream->read<void*>());
   }

TR_ResolvedMethod *
TR_ResolvedJ9JAASServerMethod::getResolvedDynamicMethod(TR::Compilation * comp, I_32 callSiteIndex, bool * unresolvedInCP)
   {
   TR_ASSERT(callSiteIndex != -1, "callSiteIndex shouldn't be -1");

#if TURN_OFF_INLINING
   return 0;
#else

   J9Class    *ramClass = constantPoolHdr();
   J9ROMClass *romClass = romClassPtr();
   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_getResolvedDynamicMethod, callSiteIndex, ramClass, getNonPersistentIdentifier());
   auto recv = _stream->read<TR_OpaqueMethodBlock*, std::string, bool>();
   TR_OpaqueMethodBlock *dummyInvokeExact = std::get<0>(recv);
   std::string signature = std::get<1>(recv);
   if (unresolvedInCP)
      *unresolvedInCP = std::get<2>(recv);
   TR_ResolvedMethod *result = _fe->createResolvedMethodWithSignature(comp->trMemory(), dummyInvokeExact, NULL, &signature[0], signature.size(), this);

   return result;
#endif
   }

bool
TR_ResolvedJ9JAASServerMethod::shouldFailSetRecognizedMethodInfoBecauseOfHCR()
   {
   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_shouldFailSetRecognizedMethodInfoBecauseOfHCR, _remoteMirror);
   return std::get<0>(_stream->read<bool>());
   }

bool
TR_ResolvedJ9JAASServerMethod::isSameMethod(TR_ResolvedMethod * m2)
   {
   if (isNative())
      return false; // A jitted JNI method doesn't call itself

   auto other = static_cast<TR_ResolvedJ9JAASServerMethod*>(m2);

   bool sameRamMethod = ramMethod() == other->ramMethod();
   if (!sameRamMethod)
      return false;

   if (asJ9Method()->isArchetypeSpecimen())
      {
      if (!other->asJ9Method()->isArchetypeSpecimen())
         return false;

      uintptrj_t *thisHandleLocation  = getMethodHandleLocation();
      uintptrj_t *otherHandleLocation = other->getMethodHandleLocation();

      // If these are not MethodHandle thunk archetypes, then we're not sure
      // how to compare them.  Conservatively return false in that case.
      //
      if (!thisHandleLocation)
         return false;
      if (!otherHandleLocation)
         return false;

      bool sameMethodHandle;

      _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_isSameMethod, thisHandleLocation, otherHandleLocation);
      sameMethodHandle = std::get<0>(_stream->read<bool>());

      if (sameMethodHandle)
         {
         // Same ramMethod, same handle.  This means we're talking about the
         // exact same thunk.
         //
         return true;
         }
      else
         {
         // Different method handle.  Assuming we're talking about a custom thunk,
         // then it will be different thunk.
         //
         return false;
         }
      }

   return true;
   }

bool
TR_ResolvedJ9JAASServerMethod::isInlineable(TR::Compilation *comp)
   {
   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_isInlineable, _remoteMirror);
   return std::get<0>(_stream->read<bool>());
   }

bool
TR_ResolvedJ9JAASServerMethod::isWarmCallGraphTooBig(uint32_t bcIndex, TR::Compilation *comp)
   {
   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_isWarmCallGraphTooBig, _remoteMirror, bcIndex);
   return std::get<0>(_stream->read<bool>());
   }

void
TR_ResolvedJ9JAASServerMethod::setWarmCallGraphTooBig(uint32_t bcIndex, TR::Compilation *comp)
   {
   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_setWarmCallGraphTooBig, _remoteMirror, bcIndex);
   _stream->read<JAAS::Void>();
   }

void
TR_ResolvedJ9JAASServerMethod::setVirtualMethodIsOverridden()
   {
   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_setVirtualMethodIsOverridden, _remoteMirror);
   _stream->read<JAAS::Void>();
   }

void *
TR_ResolvedJ9JAASServerMethod::addressContainingIsOverriddenBit()
   {
   _stream->write(JAAS::J9ServerMessageType::ResolvedMethod_addressContainingIsOverriddenBit, _remoteMirror);
   return std::get<0>(_stream->read<void *>());
   }
