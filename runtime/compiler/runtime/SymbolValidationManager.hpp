/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#ifndef SYMBOL_VALIDATION_MANAGER_INCL
#define SYMBOL_VALIDATION_MANAGER_INCL

#include <algorithm>                       // for std::max, etc
#include <map>
#include <set>
#include <stddef.h>                        // for NULL
#include <stdint.h>                        // for int32_t, uint8_t, etc
#include "env/jittypes.h"                  // for uintptrj_t
#include "j9.h"
#include "j9nonbuilder.h"
#include "infra/TRlist.hpp"
#include "env/TRMemory.hpp"
#include "env/VMJ9.h"
#include "runtime/Runtime.hpp"

#define SVM_ASSERT(manager, condition, format, ...) \
   do { (manager->inHeuristicRegion() && condition) ? (void)0 : TR::fatal_assertion(__FILE__, __LINE__, #condition, format, ##__VA_ARGS__); } while(0)

namespace TR {

struct SymbolValidationRecord
   {
   SymbolValidationRecord(TR_ExternalRelocationTargetKind kind)
      : _kind(kind)
      {}

   virtual bool isEqual(SymbolValidationRecord *other) = 0;
   virtual void printFields() = 0;

   virtual bool isClassValidationRecord() { return false; }

   TR_ExternalRelocationTargetKind _kind;
   };

struct ClassValidationRecord : public SymbolValidationRecord
   {
   ClassValidationRecord(TR_ExternalRelocationTargetKind kind)
      : SymbolValidationRecord(kind),
        _classChain(NULL)
      {}

   virtual bool isEqual(SymbolValidationRecord *other) = 0;
   virtual void printFields();

   virtual bool isClassValidationRecord() { return true; }

   void * _classChain;
   };

struct ClassByNameRecord : public ClassValidationRecord
   {
   ClassByNameRecord(TR_OpaqueClassBlock *clazz,
                     TR_OpaqueClassBlock *beholder,
                     char primitiveType)
      : ClassValidationRecord(TR_ValidateClassByName),
        _class(clazz),
        _beholder(beholder),
        _primitiveType(primitiveType)
      {}

   bool operator ==(const ClassByNameRecord &rhs)
      {
      if (_class == rhs._class &&
          _beholder == rhs._beholder &&
          _primitiveType == rhs._primitiveType)
         return true;
      else
         return false;
      }

   virtual bool isEqual(SymbolValidationRecord *other)
      {
      return (*this == *static_cast<ClassByNameRecord *>(other));
      }

   virtual void printFields();

   TR_OpaqueClassBlock * _class;
   TR_OpaqueClassBlock * _beholder;
   char _primitiveType;
   };

struct ProfiledClassRecord : public ClassValidationRecord
   {
   ProfiledClassRecord(TR_OpaqueClassBlock *clazz,
                       char primitiveType)
      : ClassValidationRecord(TR_ValidateProfiledClass),
        _class(clazz),
        _primitiveType(primitiveType)
      {}

   bool operator ==(const ProfiledClassRecord &rhs)
      {
      if (_class == rhs._class &&
          _primitiveType == rhs._primitiveType)
         return true;
      else
         return false;
      }

   virtual bool isEqual(SymbolValidationRecord *other)
      {
      return (*this == *static_cast<ProfiledClassRecord *>(other));
      }

   virtual void printFields();

   TR_OpaqueClassBlock * _class;
   char _primitiveType;
   };

struct ClassFromCPRecord : public ClassValidationRecord
   {
   ClassFromCPRecord(TR_OpaqueClassBlock *clazz, TR_OpaqueClassBlock *beholder, uint32_t cpIndex)
      : ClassValidationRecord(TR_ValidateClassFromCP),
        _class(clazz),
        _beholder(beholder),
        _cpIndex(cpIndex)
      {}

   bool operator ==(const ClassFromCPRecord &rhs)
      {
      if (_class == rhs._class &&
          _beholder == rhs._beholder &&
          _cpIndex == rhs._cpIndex)
         return true;
      else
         return false;
      }

   virtual bool isEqual(SymbolValidationRecord *other)
      {
      return (*this == *static_cast<ClassFromCPRecord *>(other));
      }

   virtual void printFields();

   TR_OpaqueClassBlock * _class;
   TR_OpaqueClassBlock * _beholder;
   uint32_t  _cpIndex;
   };

struct DefiningClassFromCPRecord : public ClassValidationRecord
   {
   DefiningClassFromCPRecord(TR_OpaqueClassBlock *clazz, TR_OpaqueClassBlock *beholder, uint32_t cpIndex, bool isStatic)
      : ClassValidationRecord(TR_ValidateDefiningClassFromCP),
        _class(clazz),
        _beholder(beholder),
        _cpIndex(cpIndex),
        _isStatic(isStatic)
      {}

   bool operator ==(const DefiningClassFromCPRecord &rhs)
      {
      if (_class == rhs._class &&
          _beholder == rhs._beholder &&
          _cpIndex == rhs._cpIndex &&
          _isStatic == rhs._isStatic)
         return true;
      else
         return false;
      }

   virtual bool isEqual(SymbolValidationRecord *other)
      {
      return (*this == *static_cast<DefiningClassFromCPRecord *>(other));
      }

   virtual void printFields();

   TR_OpaqueClassBlock * _class;
   TR_OpaqueClassBlock * _beholder;
   uint32_t  _cpIndex;
   bool      _isStatic;
   };

struct StaticClassFromCPRecord : public ClassValidationRecord
   {
   StaticClassFromCPRecord(TR_OpaqueClassBlock *clazz, TR_OpaqueClassBlock *beholder, uint32_t cpIndex)
      : ClassValidationRecord(TR_ValidateStaticClassFromCP),
        _class(clazz),
        _beholder(beholder),
        _cpIndex(cpIndex)
      {}

   bool operator ==(const StaticClassFromCPRecord &rhs)
      {
      if (_class == rhs._class &&
          _beholder == rhs._beholder &&
          _cpIndex == rhs._cpIndex)
         return true;
      else
         return false;
      }

   virtual bool isEqual(SymbolValidationRecord *other)
      {
      return (*this == *static_cast<StaticClassFromCPRecord *>(other));
      }

   virtual void printFields();

   TR_OpaqueClassBlock * _class;
   TR_OpaqueClassBlock * _beholder;
   uint32_t  _cpIndex;
   };

struct ClassFromMethodRecord : public ClassValidationRecord
   {
   ClassFromMethodRecord(TR_OpaqueClassBlock *clazz, TR_OpaqueMethodBlock *method)
      : ClassValidationRecord(TR_ValidateClassFromMethod),
        _class(clazz),
        _method(method)
      {}

   bool operator ==(const ClassFromMethodRecord &rhs)
      {
      if (_class == rhs._class &&
          _method == rhs._method)
         return true;
      else
         return false;
      }

   virtual bool isEqual(SymbolValidationRecord *other)
      {
      return (*this == *static_cast<ClassFromMethodRecord *>(other));
      }

   virtual void printFields();

   TR_OpaqueClassBlock * _class;
   TR_OpaqueMethodBlock *_method;
   };

struct ComponentClassFromArrayClassRecord : public ClassValidationRecord
   {
   ComponentClassFromArrayClassRecord(TR_OpaqueClassBlock *componentClass, TR_OpaqueClassBlock *arrayClass)
      : ClassValidationRecord(TR_ValidateComponentClassFromArrayClass),
        _componentClass(componentClass),
        _arrayClass(arrayClass)
      {}

   bool operator ==(const ComponentClassFromArrayClassRecord &rhs)
      {
      if (_componentClass == rhs._componentClass &&
          _arrayClass == rhs._arrayClass)
         return true;
      else
         return false;
      }

   virtual bool isEqual(SymbolValidationRecord *other)
      {
      return (*this == *static_cast<ComponentClassFromArrayClassRecord *>(other));
      }

   virtual void printFields();

   TR_OpaqueClassBlock * _componentClass;
   TR_OpaqueClassBlock * _arrayClass;
   };

struct ArrayClassFromComponentClassRecord : public ClassValidationRecord
   {
   ArrayClassFromComponentClassRecord(TR_OpaqueClassBlock *arrayClass, TR_OpaqueClassBlock *componentClass)
      : ClassValidationRecord(TR_ValidateArrayClassFromComponentClass),
        _arrayClass(arrayClass),
        _componentClass(componentClass)
      {}

   bool operator ==(const ArrayClassFromComponentClassRecord &rhs)
      {
      if (_arrayClass == rhs._arrayClass &&
          _componentClass == rhs._componentClass)
         return true;
      else
         return false;
      }

   virtual bool isEqual(SymbolValidationRecord *other)
      {
      return (*this == *static_cast<ArrayClassFromComponentClassRecord *>(other));
      }

   virtual void printFields();

   TR_OpaqueClassBlock * _arrayClass;
   TR_OpaqueClassBlock * _componentClass;
   };

struct SuperClassFromClassRecord : public ClassValidationRecord
   {
   SuperClassFromClassRecord(TR_OpaqueClassBlock *superClass, TR_OpaqueClassBlock *childClass)
      : ClassValidationRecord(TR_ValidateSuperClassFromClass),
        _superClass(superClass),
        _childClass(childClass)
      {}

   bool operator ==(const SuperClassFromClassRecord &rhs)
      {
      if (_superClass == rhs._superClass &&
          _childClass == rhs._childClass)
         return true;
      else
         return false;
      }

   virtual bool isEqual(SymbolValidationRecord *other)
      {
      return (*this == *static_cast<SuperClassFromClassRecord *>(other));
      }

   virtual void printFields();

   TR_OpaqueClassBlock *_superClass;
   TR_OpaqueClassBlock *_childClass;
   };

struct ClassInstanceOfClassRecord : public SymbolValidationRecord
   {
   ClassInstanceOfClassRecord(TR_OpaqueClassBlock *classOne, TR_OpaqueClassBlock *classTwo, bool objectTypeIsFixed, bool castTypeIsFixed, bool isInstanceOf)
      : SymbolValidationRecord(TR_ValidateClassInstanceOfClass),
        _classOne(classOne),
        _classTwo(classTwo),
        _objectTypeIsFixed(objectTypeIsFixed),
        _castTypeIsFixed(castTypeIsFixed),
        _isInstanceOf(isInstanceOf)
      {}

   bool operator ==(const ClassInstanceOfClassRecord &rhs)
      {
      if (_classOne == rhs._classOne &&
          _classTwo == rhs._classTwo &&
          _objectTypeIsFixed == rhs._objectTypeIsFixed &&
          _castTypeIsFixed == rhs._castTypeIsFixed &&
          _isInstanceOf == rhs._isInstanceOf)
         return true;
      else
         return false;
      }

   virtual bool isEqual(SymbolValidationRecord *other)
      {
      return (*this == *static_cast<ClassInstanceOfClassRecord *>(other));
      }

   virtual void printFields();

   TR_OpaqueClassBlock *_classOne;
   TR_OpaqueClassBlock *_classTwo;
   bool _objectTypeIsFixed;
   bool _castTypeIsFixed;
   bool _isInstanceOf;
   };

struct SystemClassByNameRecord : public ClassValidationRecord
   {
   SystemClassByNameRecord(TR_OpaqueClassBlock *systemClass)
      : ClassValidationRecord(TR_ValidateSystemClassByName),
        _systemClass(systemClass)
      {}

   bool operator ==(const SystemClassByNameRecord &rhs)
      {
      if (_systemClass == rhs._systemClass)
         return true;
      else
         return false;
      }

   virtual bool isEqual(SymbolValidationRecord *other)
      {
      return (*this == *static_cast<SystemClassByNameRecord *>(other));
      }

   virtual void printFields();

   TR_OpaqueClassBlock *_systemClass;
   };

struct ClassFromITableIndexCPRecord : public ClassValidationRecord
   {
   ClassFromITableIndexCPRecord(TR_OpaqueClassBlock *clazz, TR_OpaqueClassBlock *beholder, uint32_t cpIndex)
      : ClassValidationRecord(TR_ValidateClassFromITableIndexCP),
        _class(clazz),
        _beholder(beholder),
        _cpIndex(cpIndex)
      {}

   bool operator ==(const ClassFromITableIndexCPRecord &rhs)
      {
      if (_class == rhs._class &&
          _beholder == rhs._beholder &&
          _cpIndex == rhs._cpIndex)
         return true;
      else
         return false;
      }

   virtual bool isEqual(SymbolValidationRecord *other)
      {
      return (*this == *static_cast<ClassFromITableIndexCPRecord *>(other));
      }

   virtual void printFields();

   TR_OpaqueClassBlock * _class;
   TR_OpaqueClassBlock * _beholder;
   int32_t _cpIndex;

   };

struct DeclaringClassFromFieldOrStaticRecord : public ClassValidationRecord
   {
   DeclaringClassFromFieldOrStaticRecord(TR_OpaqueClassBlock *clazz, TR_OpaqueClassBlock *beholder, uint32_t cpIndex)
      : ClassValidationRecord(TR_ValidateDeclaringClassFromFieldOrStatic),
        _class(clazz),
        _beholder(beholder),
        _cpIndex(cpIndex)
      {}

   bool operator ==(const DeclaringClassFromFieldOrStaticRecord &rhs)
      {
      if (_class == rhs._class &&
          _beholder == rhs._beholder &&
          _cpIndex == rhs._cpIndex)
         return true;
      else
         return false;
      }

   virtual bool isEqual(SymbolValidationRecord *other)
      {
      return (*this == *static_cast<DeclaringClassFromFieldOrStaticRecord *>(other));
      }

   virtual void printFields();

   TR_OpaqueClassBlock * _class;
   TR_OpaqueClassBlock * _beholder;
   uint32_t  _cpIndex;
   };

struct ClassClassRecord : public ClassValidationRecord
   {
   ClassClassRecord(TR_OpaqueClassBlock *classClass, TR_OpaqueClassBlock *objectClass)
      : ClassValidationRecord(TR_ValidateClassClass),
        _classClass(classClass),
        _objectClass(objectClass)
      {}

   bool operator ==(const ClassClassRecord &rhs)
      {
      if (_classClass == rhs._classClass &&
          _objectClass == rhs._objectClass)
         return true;
      else
         return false;
      }

   virtual bool isEqual(SymbolValidationRecord *other)
      {
      return (*this == *static_cast<ClassClassRecord *>(other));
      }

   virtual void printFields();

   TR_OpaqueClassBlock *_classClass;
   TR_OpaqueClassBlock *_objectClass;
   };

struct ConcreteSubClassFromClassRecord : public ClassValidationRecord
   {
   ConcreteSubClassFromClassRecord(TR_OpaqueClassBlock *childClass, TR_OpaqueClassBlock *superClass)
      : ClassValidationRecord(TR_ValidateConcreteSubClassFromClass),
        _childClass(childClass),
        _superClass(superClass)
      {}

   bool operator ==(const ConcreteSubClassFromClassRecord &rhs)
      {
      if (_childClass == rhs._childClass &&
          _superClass == rhs._superClass)
         return true;
      else
         return false;
      }

   virtual bool isEqual(SymbolValidationRecord *other)
      {
      return (*this == *static_cast<ConcreteSubClassFromClassRecord *>(other));
      }

   virtual void printFields();

   TR_OpaqueClassBlock *_childClass;
   TR_OpaqueClassBlock *_superClass;
   };

struct ClassChainRecord : public SymbolValidationRecord
   {
   ClassChainRecord(TR_OpaqueClassBlock *clazz, void *classChain)
      : SymbolValidationRecord(TR_ValidateClassChain),
        _class(clazz),
        _classChain(classChain)
      {}

   bool operator ==(const ClassChainRecord &rhs)
      {
      if (_class == rhs._class &&
          _classChain == rhs._classChain)
         return true;
      else
         return false;
      }

   virtual bool isEqual(SymbolValidationRecord *other)
      {
      return (*this == *static_cast<ClassChainRecord *>(other));
      }

   virtual void printFields();

   TR_OpaqueClassBlock *_class;
   void *_classChain;
   };

struct RomClassRecord : public SymbolValidationRecord
   {
   RomClassRecord(TR_OpaqueClassBlock *clazz)
      : SymbolValidationRecord(TR_ValidateRomClass),
        _class(clazz)
      {}

   bool operator ==(const RomClassRecord &rhs)
      {
      if (_class == rhs._class)
         return true;
      else
         return false;
      }

   virtual bool isEqual(SymbolValidationRecord *other)
      {
      return (*this == *static_cast<RomClassRecord *>(other));
      }

   virtual void printFields();

   TR_OpaqueClassBlock *_class;
   };

struct MethodFromInlinedSiteRecord : public SymbolValidationRecord
   {
   MethodFromInlinedSiteRecord(TR_OpaqueMethodBlock *method,
                               int32_t inlinedSiteIndex)
      : SymbolValidationRecord(TR_ValidateMethodFromInlinedSite),
        _method(method),
        _inlinedSiteIndex(inlinedSiteIndex)
      {}

   bool operator ==( const MethodFromInlinedSiteRecord &rhs)
      {
      if (_method == rhs._method &&
          _inlinedSiteIndex == rhs._inlinedSiteIndex)
         return true;
      else
         return false;
      }

   virtual bool isEqual(SymbolValidationRecord *other)
      {
      return (*this == *static_cast<MethodFromInlinedSiteRecord *>(other));
      }

   virtual void printFields();

   TR_OpaqueMethodBlock *_method;
   int32_t _inlinedSiteIndex;
   };

struct MethodByNameRecord : public SymbolValidationRecord
   {
   MethodByNameRecord(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *beholder)
      : SymbolValidationRecord(TR_ValidateMethodByName),
        _method(method),
        _beholder(beholder)
      {}

   bool operator ==( const MethodByNameRecord &rhs)
      {
      if (_method == rhs._method &&
          _beholder == rhs._beholder)
         return true;
      else
         return false;
      }

   virtual bool isEqual(SymbolValidationRecord *other)
      {
      return (*this == *static_cast<MethodByNameRecord *>(other));
      }

   virtual void printFields();

   TR_OpaqueMethodBlock *_method;
   TR_OpaqueClassBlock *_beholder;
   };

struct MethodFromClassRecord : public SymbolValidationRecord
   {
   MethodFromClassRecord(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *beholder, uint32_t index)
      : SymbolValidationRecord(TR_ValidateMethodFromClass),
        _method(method),
        _beholder(beholder),
        _index(index)
      {}

   bool operator ==( const MethodFromClassRecord &rhs)
      {
      if (_method == rhs._method &&
          _beholder == rhs._beholder &&
          _index == rhs._index)
         return true;
      else
         return false;
      }

   virtual bool isEqual(SymbolValidationRecord *other)
      {
      return (*this == *static_cast<MethodFromClassRecord *>(other));
      }

   virtual void printFields();

   TR_OpaqueMethodBlock *_method;
   TR_OpaqueClassBlock *_beholder;
   uint32_t _index;
   };

struct StaticMethodFromCPRecord : public SymbolValidationRecord
   {
   StaticMethodFromCPRecord(TR_OpaqueMethodBlock *method,
                               TR_OpaqueClassBlock *beholder,
                               int32_t cpIndex)
      : SymbolValidationRecord(TR_ValidateStaticMethodFromCP),
        _method(method),
        _beholder(beholder),
        _cpIndex(cpIndex)
      {}

   bool operator ==( const StaticMethodFromCPRecord &rhs)
      {
      if (_method == rhs._method &&
          _beholder == rhs._beholder &&
          _cpIndex == rhs._cpIndex)
         return true;
      else
         return false;
      }

   virtual bool isEqual(SymbolValidationRecord *other)
      {
      return (*this == *static_cast<StaticMethodFromCPRecord *>(other));
      }

   virtual void printFields();

   TR_OpaqueMethodBlock *_method;
   TR_OpaqueClassBlock *_beholder;
   int32_t _cpIndex;
   };

struct SpecialMethodFromCPRecord : public SymbolValidationRecord
   {
   SpecialMethodFromCPRecord(TR_OpaqueMethodBlock *method,
                             TR_OpaqueClassBlock *beholder,
                             int32_t cpIndex)
      : SymbolValidationRecord(TR_ValidateSpecialMethodFromCP),
        _method(method),
        _beholder(beholder),
        _cpIndex(cpIndex)
      {}

   bool operator ==( const SpecialMethodFromCPRecord &rhs)
      {
      if (_method == rhs._method &&
          _beholder == rhs._beholder &&
          _cpIndex == rhs._cpIndex)
         return true;
      else
         return false;
      }

   virtual bool isEqual(SymbolValidationRecord *other)
      {
      return (*this == *static_cast<SpecialMethodFromCPRecord *>(other));
      }

   virtual void printFields();

   TR_OpaqueMethodBlock *_method;
   TR_OpaqueClassBlock *_beholder;
   int32_t _cpIndex;
   };

struct VirtualMethodFromCPRecord : public SymbolValidationRecord
   {
   VirtualMethodFromCPRecord(TR_OpaqueMethodBlock *method,
                             TR_OpaqueClassBlock *beholder,
                             int32_t cpIndex)
      : SymbolValidationRecord(TR_ValidateVirtualMethodFromCP),
        _method(method),
        _beholder(beholder),
        _cpIndex(cpIndex)
      {}

   bool operator ==( const VirtualMethodFromCPRecord &rhs)
      {
      if (_method == rhs._method &&
          _beholder == rhs._beholder &&
          _cpIndex == rhs._cpIndex)
         return true;
      else
         return false;
      }

   virtual bool isEqual(SymbolValidationRecord *other)
      {
      return (*this == *static_cast<VirtualMethodFromCPRecord *>(other));
      }

   virtual void printFields();

   TR_OpaqueMethodBlock *_method;
   TR_OpaqueClassBlock *_beholder;
   int32_t _cpIndex;
   };

struct VirtualMethodFromOffsetRecord : public SymbolValidationRecord
   {
   VirtualMethodFromOffsetRecord(TR_OpaqueMethodBlock *method,
                                 TR_OpaqueClassBlock *beholder,
                                 int32_t virtualCallOffset,
                                 bool ignoreRtResolve)
      : SymbolValidationRecord(TR_ValidateVirtualMethodFromOffset),
        _method(method),
        _beholder(beholder),
        _virtualCallOffset(virtualCallOffset),
        _ignoreRtResolve(ignoreRtResolve)
      {}

   bool operator ==( const VirtualMethodFromOffsetRecord &rhs)
      {
      if (_method == rhs._method &&
          _beholder == rhs._beholder &&
          _virtualCallOffset == rhs._virtualCallOffset &&
          _ignoreRtResolve == rhs._ignoreRtResolve)
         return true;
      else
         return false;
      }

   virtual bool isEqual(SymbolValidationRecord *other)
      {
      return (*this == *static_cast<VirtualMethodFromOffsetRecord *>(other));
      }

   virtual void printFields();

   TR_OpaqueMethodBlock *_method;
   TR_OpaqueClassBlock *_beholder;
   int32_t _virtualCallOffset;
   bool _ignoreRtResolve;
   };

struct InterfaceMethodFromCPRecord : public SymbolValidationRecord
   {
   InterfaceMethodFromCPRecord(TR_OpaqueMethodBlock *method,
                               TR_OpaqueClassBlock *beholder,
                               TR_OpaqueClassBlock *lookup,
                               int32_t cpIndex)
      : SymbolValidationRecord(TR_ValidateInterfaceMethodFromCP),
        _method(method),
        _beholder(beholder),
        _lookup(lookup),
        _cpIndex(cpIndex)
      {}

   bool operator ==( const InterfaceMethodFromCPRecord &rhs)
      {
      if (_method == rhs._method &&
          _beholder == rhs._beholder &&
          _lookup == rhs._lookup &&
          _cpIndex == rhs._cpIndex)
         return true;
      else
         return false;
      }

   virtual bool isEqual(SymbolValidationRecord *other)
      {
      return (*this == *static_cast<InterfaceMethodFromCPRecord *>(other));
      }

   virtual void printFields();

   TR_OpaqueMethodBlock *_method;
   TR_OpaqueClassBlock *_beholder;
   TR_OpaqueClassBlock *_lookup;
   int32_t _cpIndex;
   };

struct MethodFromClassAndSigRecord : public SymbolValidationRecord
   {
   MethodFromClassAndSigRecord(TR_OpaqueMethodBlock *method,
                               TR_OpaqueClassBlock *beholder,
                               TR_OpaqueClassBlock *methodClass)
      : SymbolValidationRecord(TR_ValidateMethodFromClassAndSig),
        _method(method),
        _methodClass(methodClass),
        _beholder(beholder)
      {}

   bool operator ==( const MethodFromClassAndSigRecord &rhs)
      {
      if (_method == rhs._method &&
          _methodClass == rhs._methodClass &&
          _beholder == rhs._beholder)
         return true;
      else
         return false;
      }

   virtual bool isEqual(SymbolValidationRecord *other)
      {
      return (*this == *static_cast<MethodFromClassAndSigRecord *>(other));
      }

   virtual void printFields();

   TR_OpaqueMethodBlock *_method;
   TR_OpaqueClassBlock *_methodClass;
   TR_OpaqueClassBlock *_beholder;
   };

struct StackWalkerMaySkipFramesRecord : public SymbolValidationRecord
   {
   StackWalkerMaySkipFramesRecord(TR_OpaqueMethodBlock *method,
                                  TR_OpaqueClassBlock *methodClass,
                                  bool skipFrames)
      : SymbolValidationRecord(TR_ValidateStackWalkerMaySkipFramesRecord),
        _method(method),
        _methodClass(methodClass),
        _skipFrames(skipFrames)
      {}

   bool operator ==( const StackWalkerMaySkipFramesRecord &rhs)
      {
      if (_method == rhs._method &&
          _methodClass == rhs._methodClass &&
          _skipFrames == rhs._skipFrames)
         return true;
      else
         return false;
      }

   virtual bool isEqual(SymbolValidationRecord *other)
      {
      return (*this == *static_cast<StackWalkerMaySkipFramesRecord *>(other));
      }

   virtual void printFields();

   TR_OpaqueMethodBlock *_method;
   TR_OpaqueClassBlock *_methodClass;
   bool _skipFrames;
   };

struct ArrayClassFromJavaVM : public ClassValidationRecord
   {
   ArrayClassFromJavaVM(TR_OpaqueClassBlock *arrayClass, int32_t arrayClassIndex)
      : ClassValidationRecord(TR_ValidateArrayClassFromJavaVM),
        _arrayClass(arrayClass),
        _arrayClassIndex(arrayClassIndex)
      {}

   bool operator ==(const ArrayClassFromJavaVM &rhs)
      {
      if (_arrayClass == rhs._arrayClass &&
          _arrayClassIndex == rhs._arrayClassIndex)
         return true;
      else
         return false;
      }

   virtual bool isEqual(SymbolValidationRecord *other)
      {
      return (*this == *static_cast<ArrayClassFromJavaVM *>(other));
      }

   virtual void printFields();

   TR_OpaqueClassBlock *_arrayClass;
   int32_t _arrayClassIndex;
   };

struct ClassInfoIsInitialized : public SymbolValidationRecord
   {
   ClassInfoIsInitialized(TR_OpaqueClassBlock *clazz, bool isInitialized)
      : SymbolValidationRecord(TR_ValidateClassInfoIsInitialized),
        _class(clazz),
        _isInitialized(isInitialized)
      {}

   bool operator ==(const ClassInfoIsInitialized &rhs)
      {
      if (_class == rhs._class &&
          _isInitialized == rhs._isInitialized)
         return true;
      else
         return false;
      }

   virtual bool isEqual(SymbolValidationRecord *other)
      {
      return (*this == *static_cast<ClassInfoIsInitialized *>(other));
      }

   virtual void printFields();

   TR_OpaqueClassBlock *_class;
   bool _isInitialized;
   };

struct MethodFromSingleImplementer : public SymbolValidationRecord
   {
   MethodFromSingleImplementer(TR_OpaqueMethodBlock *method,
                               TR_OpaqueClassBlock *thisClass,
                               int32_t cpIndexOrVftSlot,
                               TR_OpaqueMethodBlock *callerMethod,
                               TR_YesNoMaybe useGetResolvedInterfaceMethod)
      : SymbolValidationRecord(TR_ValidateMethodFromSingleImplementer),
        _method(method),
        _thisClass(thisClass),
        _cpIndexOrVftSlot(cpIndexOrVftSlot),
        _callerMethod(callerMethod),
        _useGetResolvedInterfaceMethod(useGetResolvedInterfaceMethod)
      {}

   bool operator ==( const MethodFromSingleImplementer &rhs)
      {
      if (_method == rhs._method &&
          _thisClass == rhs._thisClass &&
          _cpIndexOrVftSlot == rhs._cpIndexOrVftSlot &&
          _callerMethod == rhs._callerMethod &&
          _useGetResolvedInterfaceMethod == rhs._useGetResolvedInterfaceMethod)
         return true;
      else
         return false;
      }

   virtual bool isEqual(SymbolValidationRecord *other)
      {
      return (*this == *static_cast<MethodFromSingleImplementer *>(other));
      }

   virtual void printFields();

   TR_OpaqueMethodBlock *_method;
   TR_OpaqueClassBlock *_thisClass;
   int32_t _cpIndexOrVftSlot;
   TR_OpaqueMethodBlock *_callerMethod;
   TR_YesNoMaybe _useGetResolvedInterfaceMethod;
   };

struct MethodFromSingleInterfaceImplementer : public SymbolValidationRecord
   {
   MethodFromSingleInterfaceImplementer(TR_OpaqueMethodBlock *method,
                                        TR_OpaqueClassBlock *thisClass,
                                        int32_t cpIndex,
                                        TR_OpaqueMethodBlock *callerMethod)
      : SymbolValidationRecord(TR_ValidateMethodFromSingleInterfaceImplementer),
        _method(method),
        _thisClass(thisClass),
        _cpIndex(cpIndex),
        _callerMethod(callerMethod)
      {}

   bool operator ==( const MethodFromSingleInterfaceImplementer &rhs)
      {
      if (_method == rhs._method &&
          _thisClass == rhs._thisClass &&
          _cpIndex == rhs._cpIndex &&
          _callerMethod == rhs._callerMethod)
         return true;
      else
         return false;
      }

   virtual bool isEqual(SymbolValidationRecord *other)
      {
      return (*this == *static_cast<MethodFromSingleInterfaceImplementer *>(other));
      }

   virtual void printFields();

   TR_OpaqueMethodBlock *_method;
   TR_OpaqueClassBlock *_thisClass;
   int32_t _cpIndex;
   TR_OpaqueMethodBlock *_callerMethod;
   };

struct MethodFromSingleAbstractImplementer : public SymbolValidationRecord
   {
   MethodFromSingleAbstractImplementer(TR_OpaqueMethodBlock *method,
                                       TR_OpaqueClassBlock *thisClass,
                                       int32_t vftSlot,
                                       TR_OpaqueMethodBlock *callerMethod)
      : SymbolValidationRecord(TR_ValidateMethodFromSingleAbstractImplementer),
        _method(method),
        _thisClass(thisClass),
        _vftSlot(vftSlot),
        _callerMethod(callerMethod)
      {}

   bool operator ==( const MethodFromSingleAbstractImplementer &rhs)
      {
      if (_method == rhs._method &&
          _thisClass == rhs._thisClass &&
          _vftSlot == rhs._vftSlot &&
          _callerMethod == rhs._callerMethod)
         return true;
      else
         return false;
      }

   virtual bool isEqual(SymbolValidationRecord *other)
      {
      return (*this == *static_cast<MethodFromSingleAbstractImplementer *>(other));
      }

   virtual void printFields();

   TR_OpaqueMethodBlock *_method;
   TR_OpaqueClassBlock *_thisClass;
   int32_t _vftSlot;
   TR_OpaqueMethodBlock *_callerMethod;
   };

struct ImproperInterfaceMethodFromCPRecord : public SymbolValidationRecord
   {
   ImproperInterfaceMethodFromCPRecord(TR_OpaqueMethodBlock *method,
                               TR_OpaqueClassBlock *beholder,
                               int32_t cpIndex)
      : SymbolValidationRecord(TR_ValidateImproperInterfaceMethodFromCP),
        _method(method),
        _beholder(beholder),
        _cpIndex(cpIndex)
      {}

   bool operator ==( const ImproperInterfaceMethodFromCPRecord &rhs)
      {
      if (_method == rhs._method &&
          _beholder == rhs._beholder &&
          _cpIndex == rhs._cpIndex)
         return true;
      else
         return false;
      }

   virtual bool isEqual(SymbolValidationRecord *other)
      {
      return (*this == *static_cast<ImproperInterfaceMethodFromCPRecord *>(other));
      }

   virtual void printFields();

   TR_OpaqueMethodBlock *_method;
   TR_OpaqueClassBlock *_beholder;
   int32_t _cpIndex;
   };

class SymbolValidationManager
   {
public:
   TR_ALLOC(TR_MemoryBase::SymbolValidationManager);

   SymbolValidationManager(TR::Region &region, TR_ResolvedMethod *compilee);

   void* getSymbolFromID(uint16_t id);
   uint16_t getIDFromSymbol(void *symbol);

   bool isAlreadyValidated(void *symbol) { return (getIDFromSymbol(symbol) != 0); }

   bool addClassByNameRecord(TR_OpaqueClassBlock *clazz, TR_OpaqueClassBlock *beholder);
   bool addProfiledClassRecord(TR_OpaqueClassBlock *clazz);
   bool addClassFromCPRecord(TR_OpaqueClassBlock *clazz, J9ConstantPool *constantPoolOfBeholder, uint32_t cpIndex);
   bool addDefiningClassFromCPRecord(TR_OpaqueClassBlock *clazz, J9ConstantPool *constantPoolOfBeholder, uint32_t cpIndex, bool isStatic = false);
   bool addStaticClassFromCPRecord(TR_OpaqueClassBlock *clazz, J9ConstantPool *constantPoolOfBeholder, uint32_t cpIndex);
   bool addClassFromMethodRecord(TR_OpaqueClassBlock *clazz, TR_OpaqueMethodBlock *method);
   bool addComponentClassFromArrayClassRecord(TR_OpaqueClassBlock *componentClass, TR_OpaqueClassBlock *arrayClass);
   bool addArrayClassFromComponentClassRecord(TR_OpaqueClassBlock *arrayClass, TR_OpaqueClassBlock *componentClass);
   bool addSuperClassFromClassRecord(TR_OpaqueClassBlock *superClass, TR_OpaqueClassBlock *childClass);
   bool addClassInstanceOfClassRecord(TR_OpaqueClassBlock *classOne, TR_OpaqueClassBlock *classTwo, bool objectTypeIsFixed, bool castTypeIsFixed, bool isInstanceOf);
   bool addSystemClassByNameRecord(TR_OpaqueClassBlock *systemClass);
   bool addClassFromITableIndexCPRecord(TR_OpaqueClassBlock *clazz, J9ConstantPool *constantPoolOfBeholder, int32_t cpIndex);
   bool addDeclaringClassFromFieldOrStaticRecord(TR_OpaqueClassBlock *clazz, J9ConstantPool *constantPoolOfBeholder, int32_t cpIndex);
   bool addClassClassRecord(TR_OpaqueClassBlock *classClass, TR_OpaqueClassBlock *objectClass);
   bool addConcreteSubClassFromClassRecord(TR_OpaqueClassBlock *childClass, TR_OpaqueClassBlock *superClass);
   bool addArrayClassFromJavaVM(TR_OpaqueClassBlock *arrayClass, int32_t arrayClassIndex);

   bool addClassChainRecord(TR_OpaqueClassBlock *clazz, void *classChain);
   bool addRomClassRecord(TR_OpaqueClassBlock *clazz);

   bool addMethodFromInlinedSiteRecord(TR_OpaqueMethodBlock *method, int32_t inlinedSiteIndex);
   bool addMethodByNameRecord(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *beholder);
   bool addMethodFromClassRecord(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *beholder, uint32_t index);
   bool addStaticMethodFromCPRecord(TR_OpaqueMethodBlock *method, J9ConstantPool *cp, int32_t cpIndex);
   bool addSpecialMethodFromCPRecord(TR_OpaqueMethodBlock *method, J9ConstantPool *cp, int32_t cpIndex);
   bool addVirtualMethodFromCPRecord(TR_OpaqueMethodBlock *method, J9ConstantPool *cp, int32_t cpIndex);
   bool addVirtualMethodFromOffsetRecord(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *beholder, int32_t virtualCallOffset, bool ignoreRtResolve);
   bool addInterfaceMethodFromCPRecord(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *beholder, TR_OpaqueClassBlock *lookup, int32_t cpIndex);
   bool addImproperInterfaceMethodFromCPRecord(TR_OpaqueMethodBlock *method, J9ConstantPool *cp, int32_t cpIndex);
   bool addMethodFromClassAndSignatureRecord(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *methodClass, TR_OpaqueClassBlock *beholder);
   bool addMethodFromSingleImplementerRecord(TR_OpaqueMethodBlock *method,
                                             TR_OpaqueClassBlock *thisClass,
                                             int32_t cpIndexOrVftSlot,
                                             TR_OpaqueMethodBlock *callerMethod,
                                             TR_YesNoMaybe useGetResolvedInterfaceMethod);
   bool addMethodFromSingleInterfaceImplementerRecord(TR_OpaqueMethodBlock *method,
                                           TR_OpaqueClassBlock *thisClass,
                                           int32_t cpIndex,
                                           TR_OpaqueMethodBlock *callerMethod);
   bool addMethodFromSingleAbstractImplementerRecord(TR_OpaqueMethodBlock *method,
                                          TR_OpaqueClassBlock *thisClass,
                                          int32_t vftSlot,
                                          TR_OpaqueMethodBlock *callerMethod);

   bool addStackWalkerMaySkipFramesRecord(TR_OpaqueMethodBlock *method, TR_OpaqueClassBlock *methodClass, bool skipFrames);
   bool addClassInfoIsInitializedRecord(TR_OpaqueClassBlock *clazz, bool isInitialized);



   bool validateClassByNameRecord(uint16_t classID, uint16_t beholderID, J9ROMClass *romClass, char primitiveType);
   bool validateProfiledClassRecord(uint16_t classID, char primitiveType, void *classChainIdentifyingLoader, void *classChainForClassBeingValidated);
   bool validateClassFromCPRecord(uint16_t classID, uint16_t beholderID, uint32_t cpIndex);
   bool validateDefiningClassFromCPRecord(uint16_t classID, uint16_t beholderID, uint32_t cpIndex, bool isStatic);
   bool validateStaticClassFromCPRecord(uint16_t classID, uint16_t beholderID, uint32_t cpIndex);
   bool validateClassFromMethodRecord(uint16_t classID, uint16_t methodID);
   bool validateComponentClassFromArrayClassRecord(uint16_t componentClassID, uint16_t arrayClassID);
   bool validateArrayClassFromComponentClassRecord(uint16_t arrayClassID, uint16_t componentClassID);
   bool validateSuperClassFromClassRecord(uint16_t superClassID, uint16_t childClassID);
   bool validateClassInstanceOfClassRecord(uint16_t classOneID, uint16_t classTwoID, bool objectTypeIsFixed, bool castTypeIsFixed, bool wasInstanceOf);
   bool validateSystemClassByNameRecord(uint16_t systemClassID, J9ROMClass *romClass);
   bool validateClassFromITableIndexCPRecord(uint16_t classID, uint16_t beholderID, uint32_t cpIndex);
   bool validateDeclaringClassFromFieldOrStaticRecord(uint16_t definingClassID, uint16_t beholderID, int32_t cpIndex);
   bool validateClassClassRecord(uint16_t classClassID, uint16_t objectClassID);
   bool validateConcreteSubClassFromClassRecord(uint16_t childClassID, uint16_t superClassID);
   bool validateArrayClassFromJavaVM(uint16_t arrayClassID, int32_t arrayClassIndex);

   bool validateClassChainRecord(uint16_t classID, void *classChain);
   bool validateRomClassRecord(uint16_t classID, J9ROMClass *romClass);

   bool validateMethodFromInlinedSiteRecord(uint16_t methodID, TR_OpaqueMethodBlock *method);
   bool validateMethodByNameRecord(uint16_t methodID, uint16_t beholderID, J9ROMClass *romClass, J9ROMMethod *romMethod);
   bool validateMethodFromClassRecord(uint16_t methodID, uint16_t beholderID, uint32_t index);
   bool validateStaticMethodFromCPRecord(uint16_t methodID, uint16_t beholderID, int32_t cpIndex);
   bool validateSpecialMethodFromCPRecord(uint16_t methodID, uint16_t beholderID, int32_t cpIndex);
   bool validateVirtualMethodFromCPRecord(uint16_t methodID, uint16_t beholderID, int32_t cpIndex);
   bool validateVirtualMethodFromOffsetRecord(uint16_t methodID, uint16_t beholderID, int32_t virtualCallOffset, bool ignoreRtResolve);
   bool validateInterfaceMethodFromCPRecord(uint16_t methodID, uint16_t beholderID, uint16_t lookupID, int32_t cpIndex);
   bool validateImproperInterfaceMethodFromCPRecord(uint16_t methodID, uint16_t beholderID, int32_t cpIndex);
   bool validateMethodFromClassAndSignatureRecord(uint16_t methodID, uint16_t methodClassID, uint16_t beholderID, J9ROMMethod *romMethod);
   bool validateMethodFromSingleImplementerRecord(uint16_t methodID,
                                                  uint16_t thisClassID,
                                                  int32_t cpIndexOrVftSlot,
                                                  uint16_t callerMethodID,
                                                  TR_YesNoMaybe useGetResolvedInterfaceMethod);
   bool validateMethodFromSingleInterfaceImplementerRecord(uint16_t methodID,
                                                uint16_t thisClassID,
                                                int32_t cpIndex,
                                                uint16_t callerMethodID);
   bool validateMethodFromSingleAbstractImplementerRecord(uint16_t methodID,
                                               uint16_t thisClassID,
                                               int32_t vftSlot,
                                               uint16_t callerMethodID);

   bool validateStackWalkerMaySkipFramesRecord(uint16_t methodID, uint16_t methodClassID, bool couldSkipFrames);
   bool validateClassInfoIsInitializedRecord(uint16_t classID, bool wasInitialized);


   static TR_OpaqueClassBlock *getBaseComponentClass(TR_OpaqueClassBlock *clazz, int32_t & numDims);

   typedef TR::list<SymbolValidationRecord *, TR::Region&> SymbolValidationRecordList;

   SymbolValidationRecordList& getValidationRecordList() { return _symbolValidationRecords; }

   void enterHeuristicRegion() { _heuristicRegion++; }
   void exitHeuristicRegion() { _heuristicRegion--; }
   bool inHeuristicRegion() { return (_heuristicRegion > 0); }

   bool verifySymbolHasBeenValidated(void *symbol)
      {
      if (inHeuristicRegion())
         return true;
      else
         return (getIDFromSymbol(symbol) != 0);
      }

private:

   static const uint16_t NO_ID = 0;
   static const uint16_t FIRST_ID = 1;

   uint16_t getNewSymbolID();

   void storeRecord(void *symbol, SymbolValidationRecord *record);
   bool storeClassRecord(TR_OpaqueClassBlock *clazz,
                         ClassValidationRecord *record,
                         int32_t arrayDimsToValidate,
                         bool storeCurrentRecord);
   bool storeValidationRecordIfNecessary(void *symbol, SymbolValidationRecord *record, int32_t arrayDimsToValidate = 0);
   void *storeClassChain(TR_J9VMBase *fej9, TR_OpaqueClassBlock *clazz);

   bool validateSymbol(uint16_t idToBeValidated, void *validSymbol);


   /* Monotonically increasing IDs */
   uint16_t _symbolID;

   uint32_t _heuristicRegion;

   TR::Region &_region;

   /* List of validation records to be written to the AOT buffer */
   SymbolValidationRecordList _symbolValidationRecords;

   typedef TR::typed_allocator<std::pair<void* const, uint16_t>, TR::Region&> SymbolToIdAllocator;
   typedef std::less<void*> SymbolToIdComparator;
   typedef std::map<void*, uint16_t, SymbolToIdComparator, SymbolToIdAllocator> SymbolToIdMap;

   typedef TR::typed_allocator<std::pair<uint16_t const, void*>, TR::Region&> IdToSymbolAllocator;
   typedef std::less<uint16_t> IdToSymbolComparator;
   typedef std::map<uint16_t, void*, IdToSymbolComparator, IdToSymbolAllocator> IdToSymbolMap;

   typedef TR::typed_allocator<void*, TR::Region&> SeenSymbolsAlloc;
   typedef std::less<void*> SeenSymbolsComparator;
   typedef std::set<void*, SeenSymbolsComparator, SeenSymbolsAlloc> SeenSymbolsSet;

   /* Used for AOT Compile */
   SymbolToIdMap _symbolToIdMap;

   /* Used for AOT Load */
   IdToSymbolMap _idToSymbolsMap;

   SeenSymbolsSet _seenSymbolsSet;
   };

}

#endif //SYMBOL_VALIDATION_MANAGER_INCL
