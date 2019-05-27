/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#ifndef CHTABLE_INCL
#define CHTABLE_INCL

#include <stddef.h>
#include <stdint.h>
#include "compile/Compilation.hpp"
#include "compile/CompilationTypes.hpp"
#include "compile/VirtualGuard.hpp"
#include "env/RuntimeAssumptionTable.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "infra/Link.hpp"
#include "runtime/RuntimeAssumptions.hpp"

class TR_FrontEnd;
class TR_OpaqueClassBlock;
class TR_OpaqueMethodBlock;
class TR_PersistentArrayFieldInfo;
class TR_PersistentCHTable;
class TR_ResolvedMethod;
class TR_VirtualGuard;
namespace TR { class Node; }
namespace TR { class Symbol; }
namespace TR { class SymbolReference; }
template <class T> class List;
template <class T> class TR_Array;
template <class T> class TR_ScratchList;

class TR_VirtualGuardSite
   {
   public:
   TR_ALLOC(TR_Memory::VirtualGuardSiteInfo);
   TR_VirtualGuardSite() : _location(0), _location2(0), _destination(0) {}

   uint8_t* getLocation() { return _location; }
   uint8_t* getLocation2() { return _location2; }
   uint8_t* &getDestination() { return _destination; }

   void setLocation(uint8_t *location) { _location = location; }
   void setLocation2(uint8_t *location2) { _location2 = location2; }
   void setDestination(uint8_t *dest) { _destination = dest; }

   private:
   uint8_t *_location;
   uint8_t *_location2;      // used when ordered pair relocations are needed
   uint8_t *_destination;
   };

class TR_AOTGuardSite : public TR_VirtualGuardSite
   {
   public:
   TR_ALLOC(TR_Memory::VirtualGuardSiteInfo);
   TR_AOTGuardSite() : TR_VirtualGuardSite(), _guardType(TR_NoGuard), _virtualGuard(NULL), _node(NULL), _aconstNode(NULL) {}
   TR_AOTGuardSite(TR_VirtualGuardKind type, TR_VirtualGuard *virtualGuard, TR::Node *node, TR::Node *aconstNode=NULL) : TR_VirtualGuardSite(), _guardType(type), _virtualGuard(virtualGuard), _node(node), _aconstNode(node) {}

   void setType(TR_VirtualGuardKind type) { _guardType = type; }
   TR_VirtualGuardKind getType() { return _guardType; }

   void setGuard(TR_VirtualGuard *virtualGuard) { _virtualGuard = virtualGuard; }
   TR_VirtualGuard * getGuard() { return _virtualGuard; }

   void setNode(TR::Node *node) { _node = node; }
   TR::Node *getNode() { return _node; }

   void setAconstNode(TR::Node *aconstNode) { _aconstNode = aconstNode; }
   TR::Node *getAconstNode() { return _aconstNode; }

   private:
   TR_VirtualGuardKind _guardType;
   TR_VirtualGuard *_virtualGuard;
   TR::Node *_node;
   TR::Node *_aconstNode;
   };

class TR_PatchNOPedGuardSiteOnClassExtend : public TR::PatchNOPedGuardSite
   {
   protected:
   TR_PatchNOPedGuardSiteOnClassExtend(TR_PersistentMemory *pm, TR_OpaqueClassBlock *clazz,
                                         uint8_t *loc, uint8_t *dest)
      : TR::PatchNOPedGuardSite(pm, (uintptrj_t)clazz, RuntimeAssumptionOnClassExtend,
                                 loc, dest) {}
   public:
   static TR_PatchNOPedGuardSiteOnClassExtend *make(
      TR_FrontEnd *fe, TR_PersistentMemory *pm, TR_OpaqueClassBlock *clazz, uint8_t *loc, uint8_t *dest, OMR::RuntimeAssumption **sentinel);
   virtual TR_RuntimeAssumptionKind getAssumptionKind() { return RuntimeAssumptionOnClassExtend; }
   };

class TR_PatchNOPedGuardSiteOnMethodOverride : public TR::PatchNOPedGuardSite
   {
   protected:
   TR_PatchNOPedGuardSiteOnMethodOverride(TR_PersistentMemory *pm, TR_OpaqueMethodBlock *method,
                                         uint8_t *loc, uint8_t *dest)
      : TR::PatchNOPedGuardSite(pm, (uintptrj_t)method, RuntimeAssumptionOnMethodOverride,
                                 loc, dest) {}
   public:
   static TR_PatchNOPedGuardSiteOnMethodOverride *make(
      TR_FrontEnd *fe, TR_PersistentMemory *pm, TR_OpaqueMethodBlock *method, uint8_t *loc, uint8_t *dest, OMR::RuntimeAssumption **sentinel);
   virtual TR_RuntimeAssumptionKind getAssumptionKind() { return RuntimeAssumptionOnMethodOverride; }
   };

class TR_PatchNOPedGuardSiteOnClassPreInitialize : public TR::PatchNOPedGuardSite
   {
   public:
   static TR_PatchNOPedGuardSiteOnClassPreInitialize *make(
      TR_FrontEnd *fe, TR_PersistentMemory *, char *sig, uint32_t sigLen, uint8_t *loc, uint8_t *dest, OMR::RuntimeAssumption **sentinel);
   static uintptrj_t hashCode(char *sig, uint32_t sigLen);

   virtual void reclaim() { jitPersistentFree((void*)_key); }
   virtual bool matches(uintptrj_t key) { return false; }
   virtual bool matches(char *sig, uint32_t sigLen);
   virtual void compensate(TR_FrontEnd *vm, bool isSMP, void *data);
   virtual uintptrj_t hashCode() { return hashCode((char*)getKey(), _sigLen); }
   virtual TR_RuntimeAssumptionKind getAssumptionKind() { return RuntimeAssumptionOnClassPreInitialize; }

   private:
   TR_PatchNOPedGuardSiteOnClassPreInitialize(TR_PersistentMemory *pm, char *sig, uint32_t sigLen,
                                              uint8_t *loc, uint8_t *dest)
      : TR::PatchNOPedGuardSite(pm, (uintptrj_t)sig, RuntimeAssumptionOnClassPreInitialize,
                               loc, dest), _sigLen(sigLen) {}
   uint32_t _sigLen;
   };

class TR_PatchNOPedGuardSiteOnClassRedefinition: public TR::PatchNOPedGuardSite
   {
   protected:
   TR_PatchNOPedGuardSiteOnClassRedefinition(TR_PersistentMemory *pm, TR_OpaqueClassBlock *clazz,
                                             uint8_t *loc, uint8_t *dest)
      : TR::PatchNOPedGuardSite(pm, (uintptrj_t)clazz, RuntimeAssumptionOnClassRedefinitionNOP, loc, dest) {}
   public:
   static TR_PatchNOPedGuardSiteOnClassRedefinition *make(
      TR_FrontEnd *fe, TR_PersistentMemory *pm, TR_OpaqueClassBlock *clazz, uint8_t *loc, uint8_t *dest, OMR::RuntimeAssumption **sentinel);
   virtual TR_RuntimeAssumptionKind getAssumptionKind() { return RuntimeAssumptionOnClassRedefinitionNOP; }
   void setKey(uintptrj_t newKey) {_key = newKey;}
   };

class TR_PatchMultipleNOPedGuardSitesOnClassRedefinition : public TR::PatchMultipleNOPedGuardSites
   {
   protected:
   TR_PatchMultipleNOPedGuardSitesOnClassRedefinition(TR_PersistentMemory *pm, TR_OpaqueClassBlock *clazz, TR::PatchSites *sites)
      : TR::PatchMultipleNOPedGuardSites(pm, (uintptrj_t)clazz, RuntimeAssumptionOnClassRedefinitionNOP, sites) {}
   public:
   static TR_PatchMultipleNOPedGuardSitesOnClassRedefinition *make(
      TR_FrontEnd *fe, TR_PersistentMemory *pm, TR_OpaqueClassBlock *clazz, TR::PatchSites *sites, OMR::RuntimeAssumption **sentinel);
   virtual TR_RuntimeAssumptionKind getAssumptionKind() { return RuntimeAssumptionOnClassRedefinitionNOP; }
   void setKey(uintptrj_t newKey) {_key = newKey;}
   };

class TR_PatchNOPedGuardSiteOnStaticFinalFieldModification : public TR::PatchNOPedGuardSite
   {
   protected:
   TR_PatchNOPedGuardSiteOnStaticFinalFieldModification(TR_PersistentMemory *pm, TR_OpaqueClassBlock *clazz,
                                             uint8_t *loc, uint8_t *dest)
      : TR::PatchNOPedGuardSite(pm, (uintptrj_t)clazz, RuntimeAssumptionOnStaticFinalFieldModification, loc, dest) {}
   public:
   static TR_PatchNOPedGuardSiteOnStaticFinalFieldModification *make(
      TR_FrontEnd *fe, TR_PersistentMemory *pm, TR_OpaqueClassBlock *clazz, uint8_t *loc, uint8_t *dest, OMR::RuntimeAssumption **sentinel);
   virtual TR_RuntimeAssumptionKind getAssumptionKind() { return RuntimeAssumptionOnStaticFinalFieldModification; }
   };

class TR_PatchMultipleNOPedGuardSitesOnStaticFinalFieldModification : public TR::PatchMultipleNOPedGuardSites
   {
   protected:
   TR_PatchMultipleNOPedGuardSitesOnStaticFinalFieldModification(TR_PersistentMemory *pm, TR_OpaqueClassBlock *clazz, TR::PatchSites *sites)
      : TR::PatchMultipleNOPedGuardSites(pm, (uintptrj_t)clazz, RuntimeAssumptionOnStaticFinalFieldModification, sites) {}
   public:
   static TR_PatchMultipleNOPedGuardSitesOnStaticFinalFieldModification *make(
      TR_FrontEnd *fe, TR_PersistentMemory *pm, TR_OpaqueClassBlock *clazz, TR::PatchSites *sites, OMR::RuntimeAssumption **sentinel);
   virtual TR_RuntimeAssumptionKind getAssumptionKind() { return RuntimeAssumptionOnStaticFinalFieldModification; }
   };

class TR_PatchNOPedGuardSiteOnMutableCallSiteChange : public TR::PatchNOPedGuardSite
   {
   protected:
   TR_PatchNOPedGuardSiteOnMutableCallSiteChange(TR_PersistentMemory *pm, uintptrj_t key,
                                         uint8_t *loc, uint8_t *dest)
      : TR::PatchNOPedGuardSite(pm, key, RuntimeAssumptionOnMutableCallSiteChange, loc, dest) {}
   public:
   static TR_PatchNOPedGuardSiteOnMutableCallSiteChange *make(
      TR_FrontEnd *fe, TR_PersistentMemory *pm, uintptrj_t KEY_WILL_GO_HERE, uint8_t *loc, uint8_t *dest, OMR::RuntimeAssumption **sentinel);
   virtual TR_RuntimeAssumptionKind getAssumptionKind() { return RuntimeAssumptionOnMutableCallSiteChange; }
   };

class TR_PatchNOPedGuardSiteOnMethodBreakPoint : public TR::PatchNOPedGuardSite
   {
   protected:
   TR_PatchNOPedGuardSiteOnMethodBreakPoint(TR_PersistentMemory *pm, TR_OpaqueMethodBlock *j9method,
                       uint8_t *location, uint8_t *destination)
      : TR::PatchNOPedGuardSite(pm, (uintptrj_t)j9method, RuntimeAssumptionOnMethodBreakPoint, location, destination) {}

   public: 
   static TR_PatchNOPedGuardSiteOnMethodBreakPoint *make(
      TR_FrontEnd *fe, TR_PersistentMemory * pm, TR_OpaqueMethodBlock *j9method, uint8_t *location, uint8_t *destination,
      OMR::RuntimeAssumption **sentinel);
 
   virtual TR_RuntimeAssumptionKind getAssumptionKind() { return RuntimeAssumptionOnMethodBreakPoint; }
   };

class TR_PatchJNICallSite : public OMR::ValueModifyRuntimeAssumption
   {
   protected:
   TR_PatchJNICallSite(TR_PersistentMemory *pm, uintptrj_t key, uint8_t *pc)
      : OMR::ValueModifyRuntimeAssumption(pm, key), _pc(pc) {}

   public:
   TR_PERSISTENT_ALLOC_THROW(TR_Memory::CallSiteInfo)
   static TR_PatchJNICallSite *make(
      TR_FrontEnd *fe, TR_PersistentMemory * pm, uintptrj_t key, uint8_t *pc, OMR::RuntimeAssumption **sentinel);

   virtual void dumpInfo();

   virtual void compensate(TR_FrontEnd *vm, bool isSMP, void *newAddress);
   virtual bool equals(OMR::RuntimeAssumption &other)
         {
         TR_PatchJNICallSite *site = other.asPJNICSite();
         return site != 0 && getPc() == site->getPc();
         }
   virtual uint8_t *getFirstAssumingPC() { return getPc(); }
   virtual uint8_t *getLastAssumingPC() { return getPc(); }

   virtual TR_PatchJNICallSite *asPJNICSite() { return this; }
   uint8_t *getPc() { return _pc; }
   virtual TR_RuntimeAssumptionKind getAssumptionKind() { return RuntimeAssumptionOnRegisterNative; }

   private:
   uint8_t *_pc;
   };

class TR_PreXRecompile : public OMR::LocationRedirectRuntimeAssumption
   {
   protected:
   TR_PreXRecompile(TR_PersistentMemory *pm, uintptrj_t key, TR_RuntimeAssumptionKind kind, uint8_t *startPC)
      : OMR::LocationRedirectRuntimeAssumption(pm, key), _startPC(startPC) {}

   public:

   // NOTE: this function should be the first non-inline function in the class
   // to make sure that the compiler puts the vtable in codegen.dev
   virtual void dumpInfo();

   virtual void compensate(TR_FrontEnd *vm, bool isSMP, void *newAddress);
   virtual bool equals(OMR::RuntimeAssumption &o1)
         {
         TR_PreXRecompile *a = o1.asPXRecompile();
         return (a != 0) && _startPC == a->_startPC;
         }

   virtual TR_PreXRecompile *asPXRecompile() { return this; }
   virtual uint8_t *getFirstAssumingPC() { return getStartPC(); }
   virtual uint8_t *getLastAssumingPC() { return getStartPC(); }
   uint8_t *getStartPC()          { return _startPC; }


   private:
   uint8_t    *_startPC;
   };

class TR_PreXRecompileOnClassExtend : public TR_PreXRecompile
   {
   protected:
   TR_PreXRecompileOnClassExtend(TR_PersistentMemory *pm, TR_OpaqueClassBlock *clazz, uint8_t *startPC)
      : TR_PreXRecompile(pm, (uintptrj_t)clazz, RuntimeAssumptionOnClassExtend, startPC) {}
   public:
   virtual TR_RuntimeAssumptionKind getAssumptionKind() { return RuntimeAssumptionOnClassExtend; }
   static TR_PreXRecompileOnClassExtend *make(
      TR_FrontEnd *fe, TR_PersistentMemory *pm, TR_OpaqueClassBlock *clazz, uint8_t *startPC, OMR::RuntimeAssumption **sentinel);
   };

class TR_PreXRecompileOnMethodOverride : public TR_PreXRecompile
   {
   protected:
   TR_PreXRecompileOnMethodOverride(TR_PersistentMemory *pm, TR_OpaqueMethodBlock *method, uint8_t *startPC)
      : TR_PreXRecompile(pm, (uintptrj_t)method, RuntimeAssumptionOnMethodOverride, startPC) {}
   public:
   virtual TR_RuntimeAssumptionKind getAssumptionKind() { return RuntimeAssumptionOnMethodOverride; }
   static TR_PreXRecompileOnMethodOverride *make(
      TR_FrontEnd *fe, TR_PersistentMemory *pm, TR_OpaqueMethodBlock *method, uint8_t *startPC, OMR::RuntimeAssumption **sentinel);
   };

class TR_CHTable
   {
   public:

   /**
     * VisitTracker class keeps a record of visited classes, and reset their
     * visited status flags once visiting finish.
     * Usage:
     * {
     *    CHTableCriticalSection lock(...);
     *    VisitTracker           tracker(...);
     *    Work on CHTable entries and call VisitTracker::visit() with each entry
     * }
     * Note that VisitTracker MUST be used inside CHTableCriticalSection
     *
     * A typical work flow is:
     * 1) Enter CHTable critical section
     * 2) Work on CHTable entries marking some of them visited
     * 3) Loop through the list of entries with visited flags set and clear them
     * 4) Leave CHTable critical section
     *
     * Note that exception may occur during Step 2.
     * Previously (RTC 118937), visited classes were tracked by a list inside
     * J9::Compilation, and the exception handler resets visited status flags when
     * an exception occurred.
     *
     * This worked perfectly when longjmp was used: when longjmp was issued, the
     * code is still inside CHTable critical section, and hence resetting/modifying
     * visited status flags was safe.
     *
     * However, this does not work with C++ exception handling and RAII managing
     * entering/leaving of critical sections. The code looks like following:
     * {
     *    CHTableCriticalSection lock(...);
     *    2) Work on CHTable entries marking some of them visited
     *    3) Loop through the list of entries with visited flags set and clear them
     * }
     * Note that Steps 1&4 are replaced by CHTableCriticalSection, which enters the
     * critical section in its constructor and leaves in its destructor. When an
     * exception occurs in Step 2, C++ executes CHTableCriticalSection's destructor
     * before executing the exception handler; and hence the exception handling code
     * is outside of CHTable critical section. In short, resetting/modifying visited
     * status flags in the exception handler is NOT safe.
     *
     * To solve the above mentioned issue, VisitTracker is introduced. It resets
     * visited status flags in its destructor. The code becomes:
     * {
     *    CHTableCriticalSection lock(...);
     *    VisitTracker           tracker(...);
     *    2) Work on CHTable entries marking some of them visited
     * }
     * When an exception occurred in Step 2, before executing the exception handler,
     * C++ first executes VisitTracker's destructor and then CHTableCriticalSection's
     * destructor. Therefore, visited status flags are reset inside CHTable critical
     * section, which is safe.
     *
     */
   class VisitTracker
      {
      public:

      VisitTracker(TR_Memory* m) : _classes(m) {}
      ~VisitTracker()
         {
         auto it = iterator();
         for (auto info = it.getFirst(); info; info = it.getNext())
            info->resetVisited();
         _classes.deleteAll();
         }
      void visit(TR_PersistentClassInfo* info)
         {
          _classes.add(info); // this could throw std::bad_alloc and take us to the exception handler, before which the destructor is called
         info->setVisited();  // it's important to set the visited flag *after* adding to marked list
         }
      ListIterator<TR_PersistentClassInfo> iterator()
         {
         return ListIterator<TR_PersistentClassInfo>(&_classes);
         }

      private:
      TR_ScratchList<TR_PersistentClassInfo> _classes;
      };

   TR_ALLOC(TR_Memory::CHTable)
   TR_CHTable(TR_Memory * trMemory) :
      _preXMethods(0),
      _classes(0),
      _classesThatShouldNotBeNewlyExtended(0),
      _trMemory(trMemory)
      {
      }

   // register the class' extend event to trigger a recompile
   bool recompileOnClassExtend   (TR::Compilation *c, TR_OpaqueClassBlock * classId);

   // register the class' extend event to trigger a recompile
   bool recompileOnNewClassExtend   (TR::Compilation *c, TR_OpaqueClassBlock * classId);

   // register the method's override event to trigger a recompile
   bool recompileOnMethodOverride(TR::Compilation *c, TR_ResolvedMethod *method);

   void cleanupNewlyExtendedInfo(TR::Compilation *comp);

   // Commit the CHTable into the persistent table.  This method must be called
   // with the class table mutex in hand.
   // If an unrecoverable problem has occurred - this method will return false
   // which will result in the failure of the current compilation
   //
   bool commit(TR::Compilation *comp);

   void commitVirtualGuard(TR_VirtualGuard *info, List<TR_VirtualGuardSite> &sites,
                           TR_PersistentCHTable *table, TR::Compilation *comp);
   void commitOSRVirtualGuards(TR::Compilation *comp, TR::list<TR_VirtualGuard*> &vguards);

   TR_Array<TR_OpaqueClassBlock *> *getClasses() { return _classes;}
   TR_Array<TR_OpaqueClassBlock *> *getClassesThatShouldNotBeNewlyExtended() { return _classesThatShouldNotBeNewlyExtended;}

   TR_Memory *               trMemory()                    { return _trMemory; }
   TR_StackMemory            trStackMemory()               { return _trMemory; }
   TR_HeapMemory             trHeapMemory()                { return _trMemory; }

   private:

   friend class TR_Debug;
   friend class TR_DebugExt;

   TR_Array<TR_ResolvedMethod*> *    _preXMethods;
   TR_Array<TR_OpaqueClassBlock *> * _classes;
   TR_Array<TR_OpaqueClassBlock *> * _classesThatShouldNotBeNewlyExtended;
   TR_Memory *                       _trMemory;
   };



// IMPORTANT NOTE
//
// The following classes contains volatile information about the class hierarchy.
// The class table mutex must be acquired before accessing the following datastructures
//
// During compilation, any class hierarchy assumptions are added into the TR_CHTable.
// As the last step at codegen time, TR_CHTable locks the classtable, commits all the information
// into the Persistent table.
//
// At runtime, whenever a class is extended, or method is overridden, one of the methods:
//   methodGotOverridden() or classGotExtended is invoked by the class loader.  It is not necessary
// to lock the class table, since the class loader already holds the lock.
//

// TR_SubClassVisitor is an abstract class which when extended can be used to walk a class's subclasses.
// It deals with acquiring the class table mutex, visiting classes only once (in the case where you start from
// an interface class), avoiding walking all subclasses when it's not necessary and tracing what's being walked.
//
// To use the walker create a subclass of TR_SubclassVisitor and implement a method to override the
// visitSubclass pure virtual method.
//
//
class TR_SubclassVisitor
   {
public:
   TR_SubclassVisitor(TR::Compilation * comp);

   void visit(TR_OpaqueClassBlock *, bool locked = false);

   virtual bool visitSubclass(TR_PersistentClassInfo *) = 0;

   void setTracing(bool t) { _trace = t; }

   void    stopTheWalk()                    { _stopTheWalk = true; }
   int32_t depth()                          { return _depth; }

protected:
   TR_FrontEnd * fe()                       { return _comp->fe(); }
   TR::Compilation * comp()                { return _comp; }

   TR_Memory *               trMemory()                    { return _comp->trMemory(); }
   TR_StackMemory            trStackMemory()               { return _comp->trStackMemory(); }
   TR_HeapMemory             trHeapMemory()                { return _comp->trHeapMemory(); }

   void visitSubclasses(TR_PersistentClassInfo *, TR_CHTable::VisitTracker& visited);

   TR::Compilation *                       _comp;
   int32_t                                  _depth;
   bool                                     _mightVisitAClassMoreThanOnce;
   bool                                     _stopTheWalk;
   bool                                     _trace;
   };

#define INVALID 0
#define VALID_BUT_NOT_ALWAYS_INITIALIZED 1
#define VALID_AND_ALWAYS_INITIALIZED 2
#define VALID_AND_INITIALIZED_STATICALLY 3
#define VALIDITY_FLAGS 3

#define IMMUTABLE_FLAG 4
#define NOT_READ_FLAG 8
#define BIG_DECIMAL_ASSUMPTION_FLAG 16
#define BIG_INTEGER_ASSUMPTION_FLAG 32
#define BIG_DECIMAL_TYPE_FLAG 64
#define BIG_INTEGER_TYPE_FLAG 128

class TR_PersistentFieldInfo : public TR_Link<TR_PersistentFieldInfo>
   {
   public:
   TR_PersistentFieldInfo(char *sig,
                          int32_t sigLength = -1,
                          TR_PersistentFieldInfo *next = NULL,
                          uint8_t b = VALID_BUT_NOT_ALWAYS_INITIALIZED,
                          char *c = 0,
                          int32_t numChars = -1)

      : TR_Link<TR_PersistentFieldInfo>(next),
        _signature(sig),
        _signatureLength(sigLength),
       _isTypeInfoValid(b),
       _numChars(numChars),
       _classPointer(c),
       _canMorph(true)
      {
      _isTypeInfoValid |= IMMUTABLE_FLAG;
      _isTypeInfoValid |= NOT_READ_FLAG;
      _isTypeInfoValid |= BIG_INTEGER_TYPE_FLAG;
      _isTypeInfoValid |= BIG_DECIMAL_TYPE_FLAG;
      }

   void copyData(TR_PersistentFieldInfo *other)
      {
      _isTypeInfoValid = other->getFlags();
      setFieldSignature(other->getFieldSignature());
      setFieldSignatureLength(other->getFieldSignatureLength());
      setClassPointer(other->getClassPointer());
      setNumChars(other->getNumChars());
      setCanChangeToArray(other->canChangeToArray());
      }


   char *getFieldSignature() {return _signature;}
   void setFieldSignature(char *sig) {_signature = sig;}

   int32_t getFieldSignatureLength() {return _signatureLength;}
   void setFieldSignatureLength(int32_t length) {_signatureLength = length;}

   // Downcast
   //
   virtual TR_PersistentArrayFieldInfo *asPersistentArrayFieldInfo() {return NULL;}

   #ifdef DEBUG
       virtual void dumpInfo(TR_FrontEnd *vm, TR::FILE *);
   #endif

   const char *getClassPointer() {return _classPointer;}
   void setClassPointer(const char *c) {_classPointer = c;}
   int32_t getNumChars() {return _numChars;}
   void setNumChars(int32_t n) {_numChars = n;}

   uint8_t getFlags() {return _isTypeInfoValid; }

   uint8_t isTypeInfoValid() {return (_isTypeInfoValid & (VALIDITY_FLAGS));}
   void setIsTypeInfoValid(uint8_t b)
      {
      uint8_t temp = b;

      if (isImmutable())
         temp = (temp | IMMUTABLE_FLAG);

      if (isNotRead())
         temp = (temp | NOT_READ_FLAG);

      if (isBigDecimalType())
         temp = (temp | BIG_DECIMAL_TYPE_FLAG);

      if (hasBigDecimalAssumption())
         temp = (temp | BIG_DECIMAL_ASSUMPTION_FLAG);

      if (isBigIntegerType())
         temp = (temp | BIG_INTEGER_TYPE_FLAG);

      if (hasBigIntegerAssumption())
         temp = (temp | BIG_INTEGER_ASSUMPTION_FLAG);

      _isTypeInfoValid = temp;
      }

   bool isImmutable()
      {
      if ((_isTypeInfoValid & IMMUTABLE_FLAG))
         return true;
      return false;
      }

   void setImmutable(bool b)
      {
      if (b)
         _isTypeInfoValid |= IMMUTABLE_FLAG;
      else
         _isTypeInfoValid &= ~(IMMUTABLE_FLAG);
      }

   bool isNotRead()
      {
      if ((_isTypeInfoValid & NOT_READ_FLAG))
         return true;
      return false;
      }

   void setNotRead(bool b)
      {
      if (b)
         _isTypeInfoValid |= NOT_READ_FLAG;
      else
         _isTypeInfoValid &= ~(NOT_READ_FLAG);
      }

   bool isBigDecimalType()
      {
      if ((_isTypeInfoValid & BIG_DECIMAL_TYPE_FLAG))
         return true;
      return false;
      }

   void setBigDecimalType(bool b)
      {
      if (b)
         _isTypeInfoValid |= BIG_DECIMAL_TYPE_FLAG;
      else
         _isTypeInfoValid &= ~(BIG_DECIMAL_TYPE_FLAG);
      }

   bool isBigIntegerType()
      {
      if ((_isTypeInfoValid & BIG_INTEGER_TYPE_FLAG))
         return true;
      return false;
      }

   void setBigIntegerType(bool b)
      {
      if (b)
         _isTypeInfoValid |= BIG_INTEGER_TYPE_FLAG;
      else
         _isTypeInfoValid &= ~(BIG_INTEGER_TYPE_FLAG);
      }

   bool hasBigDecimalAssumption()
      {
      if ((_isTypeInfoValid & BIG_DECIMAL_ASSUMPTION_FLAG))
         return true;
      return false;
      }

   void setBigDecimalAssumption(bool b)
      {
      if (b)
         _isTypeInfoValid |= BIG_DECIMAL_ASSUMPTION_FLAG;
      else
         _isTypeInfoValid &= ~(BIG_DECIMAL_ASSUMPTION_FLAG);
      }

   bool hasBigIntegerAssumption()
      {
      if ((_isTypeInfoValid & BIG_INTEGER_ASSUMPTION_FLAG))
         return true;
      return false;
      }

   void setBigIntegerAssumption(bool b)
      {
      if (b)
         _isTypeInfoValid |= BIG_INTEGER_ASSUMPTION_FLAG;
      else
         _isTypeInfoValid &= ~(BIG_INTEGER_ASSUMPTION_FLAG);
      }

   bool canChangeToArray() {return _canMorph;}
   void setCanChangeToArray(bool v) {_canMorph = v;}

   protected:

   char *   _signature;
   const char *   _classPointer;
   int32_t  _signatureLength;
   int32_t  _numChars;
   uint8_t  _isTypeInfoValid;
   bool     _canMorph;
   };


class TR_PersistentArrayFieldInfo : public TR_PersistentFieldInfo
   {
   public:
   TR_PersistentArrayFieldInfo(char *sig,
                               int32_t sigLength = -1,
                               TR_PersistentArrayFieldInfo *next = NULL,
                               int32_t numDimensions = -1,
                               int32_t *dimensionInfo = NULL,
                               uint8_t b1 = VALID_BUT_NOT_ALWAYS_INITIALIZED,
                               uint8_t b2 = VALID_BUT_NOT_ALWAYS_INITIALIZED,
                               char *c = "",
                               int32_t numChars = -1)
      : TR_PersistentFieldInfo(sig, sigLength, next, b2, c, numChars),
        _numDimensions(numDimensions),
        _dimensionInfo(dimensionInfo),
       _isDimensionInfoValid(b1)
      {
      }

   int32_t getNumDimensions() {return _numDimensions;}
   void setNumDimensions(int32_t d) {_numDimensions = d;}

   int32_t *getDimensionInfo() {return _dimensionInfo;}
   void setDimensionInfo(int32_t *info) {_dimensionInfo = info;}
   int32_t getDimensionInfo(int32_t i) {return _dimensionInfo[i];}
   void setDimensionInfo(int32_t i, int32_t value) {_dimensionInfo[i] = value;}

   uint8_t isDimensionInfoValid() {return _isDimensionInfoValid;}
   void setIsDimensionInfoValid(uint8_t b) {_isDimensionInfoValid = b;}

   void prepareArrayFieldInfo(int32_t, TR::Compilation *);

   virtual TR_PersistentArrayFieldInfo *asPersistentArrayFieldInfo() {return this;}

   #ifdef DEBUG
       virtual void dumpInfo(TR_FrontEnd *vm, TR::FILE *);
   #endif

   private:
   int32_t * _dimensionInfo;
   int32_t   _numDimensions;
   uint8_t   _isDimensionInfoValid;
   };

class TR_PersistentClassInfoForFields : public TR_LinkHead<TR_PersistentFieldInfo>
   {
public:
   TR_PersistentFieldInfo * find(TR::Compilation *, TR::Symbol *, TR::SymbolReference *);
   TR_PersistentFieldInfo * findFieldInfo(TR::Compilation *, TR::Node * & node, bool canBeArrayShadow);
   };

class TR_ClassQueries
   {
   public:
   static void    getSubClasses            (TR_PersistentClassInfo *clazz,
                                            TR_ScratchList<TR_PersistentClassInfo> &list,
                                            TR_FrontEnd *vm, bool locked = false);
   static int32_t collectImplementorsCapped(TR_PersistentClassInfo *clazz,
                                            TR_ResolvedMethod **implArray,
                                            int32_t maxCount,
                                            int32_t slotOrIndex,
                                            TR_ResolvedMethod *callerMethod,
                                            TR::Compilation *comp,
                                            bool locked = false,
                                            TR_YesNoMaybe useGetResolvedInterfaceMethod = TR_maybe);
   static int32_t collectCompiledImplementorsCapped(TR_PersistentClassInfo *clazz,
                                            TR_ResolvedMethod **implArray,
                                            int32_t maxCount,
                                            int32_t slotOrIndex,
                                            TR_ResolvedMethod *callerMethod,
                                            TR::Compilation *comp,
                                            TR_Hotness hotness,
                                            bool locked = false,
                                            TR_YesNoMaybe useGetResolvedInterfaceMethod = TR_maybe);

   static void    collectLeafs             (TR_PersistentClassInfo *clazz,
                                            TR_ScratchList<TR_PersistentClassInfo> &list,
                                            TR::Compilation *comp,
                                            bool locked = false);

   static void    collectAllNonIFSubClasses(TR_PersistentClassInfo *clazz,
                                            TR_ScratchList<TR_PersistentClassInfo> &leafs,
                                            TR::Compilation *comp,
                                            bool locked = false);

   static void    collectAllSubClasses     (TR_PersistentClassInfo *clazz,
                                            TR_ScratchList<TR_PersistentClassInfo> *leafs,
                                            TR::Compilation *comp, bool locked = false);

   static int32_t countAllNonIFSubClassesWithDepth(TR_PersistentClassInfo *clazz,
                                                   TR::Compilation *comp,
                                                   int32_t maxCount, bool locked = false);

   static void    addAnAssumptionForEachSubClass  (TR_PersistentCHTable *table,
                                                   TR_PersistentClassInfo *clazz,
                                                   List<TR_VirtualGuardSite> &,
                                                   TR::Compilation *comp);

   private:
   static void    collectLeafsLocked(TR_PersistentClassInfo*                 clazz,
                                     TR_ScratchList<TR_PersistentClassInfo>& leafs,
                                     TR_CHTable::VisitTracker&               marked);
   static void    collectAllSubClassesLocked(TR_PersistentClassInfo*                 clazz,
                                             TR_ScratchList<TR_PersistentClassInfo>* leafs,
                                             TR_CHTable::VisitTracker&               marked);
   };

#endif

