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

#ifndef J9_SYMBOLREFERENCETABLE_INCL
#define J9_SYMBOLREFERENCETABLE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_SYMBOLREFERENCETABLE_CONNECTOR
#define J9_SYMBOLREFERENCETABLE_CONNECTOR
namespace J9 { class SymbolReferenceTable; }
namespace J9 { typedef J9::SymbolReferenceTable SymbolReferenceTableConnector; }
#endif

#include "compile/OMRSymbolReferenceTable.hpp"

#include <stddef.h>
#include <stdint.h>
#include "env/jittypes.h"

class TR_BitVector;
class TR_PersistentClassInfo;
namespace TR { class Compilation; }


class TR_ImmutableInfo
   {
   public:
   TR_ALLOC(TR_Memory::SymbolReferenceTable)

   TR_ImmutableInfo(TR_OpaqueClassBlock *clazz, TR_BitVector *immutableSymRefs, TR_BitVector *immutableConstructorDefAliases)
       : _clazz(clazz), _immutableSymRefNumbers(immutableSymRefs), _immutableConstructorDefAliases(immutableConstructorDefAliases)
      {
      }

   TR_OpaqueClassBlock *_clazz;
   TR_BitVector        *_immutableSymRefNumbers;
   TR_BitVector        *_immutableConstructorDefAliases;
   };



namespace J9
{

class SymbolReferenceTable : public OMR::SymbolReferenceTableConnector
   {
   public:

   SymbolReferenceTable(size_t size, TR::Compilation *comp);

   TR::SymbolReference * findOrCreateCountForRecompileSymbolRef();
   TR::SymbolReference * findDiscontiguousArraySizeSymbolRef() { return element(discontiguousArraySizeSymbol); }
   TR::SymbolReference * findOrCreateOSRBufferSymbolRef();
   TR::SymbolReference * findOrCreateOSRScratchBufferSymbolRef();
   TR::SymbolReference * findOrCreateOSRFrameIndexSymbolRef();

   // CG linkage
   TR::SymbolReference * findOrCreateAcquireVMAccessSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol); // minor rework
   TR::SymbolReference * findOrCreateReleaseVMAccessSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol); // minor rework
   TR::SymbolReference * findOrCreateStackOverflowSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol); // minor rework
   TR::SymbolReference * findOrCreateThrowCurrentExceptionSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol = 0); // minor rework

   TR::SymbolReference * findOrCreateWriteBarrierStoreSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol = 0);
   TR::SymbolReference * findOrCreateWriteBarrierStoreGenerationalSymbolRef(TR::ResolvedMethodSymbol *owningMethodSymbol = 0);
   TR::SymbolReference * findOrCreateConstantPoolAddressSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);

   // FE
   TR::SymbolReference * findOrCreateFloatSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex);
   TR::SymbolReference * findOrCreateDoubleSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex);
   TR::SymbolReference * createSystemRuntimeHelper(TR_RuntimeHelper, bool = false, bool = false, bool preservesAllRegisters = false);
   TR::SymbolReference * findOrCreateStaticMethodSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex);
   TR::SymbolReference * findOrCreateSpecialMethodSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex);
   TR::SymbolReference * findOrCreateVirtualMethodSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex);
   TR::SymbolReference * findOrCreateInterfaceMethodSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex);
   TR::SymbolReference * findOrCreateDynamicMethodSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t callSiteIndex);
   TR::SymbolReference * findOrCreateHandleMethodSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex);
   TR::SymbolReference * findOrCreateHandleMethodSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex, char *signature);
   TR::SymbolReference * findOrCreateCallSiteTableEntrySymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t callSiteIndex);
   TR::SymbolReference * findOrCreateMethodTypeTableEntrySymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex);
   TR::SymbolReference * findOrCreateVarHandleMethodTypeTableEntrySymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex);
   TR::SymbolReference * methodSymRefWithSignature(TR::SymbolReference *original, char *effectiveSignature, int32_t effectiveSignatureLength);
   TR::SymbolReference * findOrCreateTypeCheckArrayStoreSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);
   TR::SymbolReference * findOrCreateArrayClassRomPtrSymbolRef();
   TR::SymbolReference * findOrCreateJavaLangReferenceReferentShadowSymbol(TR::ResolvedMethodSymbol *, bool , TR::DataType, uint32_t, bool);
   TR::SymbolReference * findOrCreateWriteBarrierStoreGenerationalAndConcurrentMarkSymbolRef(TR::ResolvedMethodSymbol *owningMethodSymbol = 0);
   TR::SymbolReference * findOrCreateWriteBarrierStoreRealTimeGCSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol = 0);
   TR::SymbolReference * findOrCreateWriteBarrierClassStoreRealTimeGCSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol = 0);
   TR::SymbolReference * findOrCreateWriteBarrierBatchStoreSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol = 0);

   TR::SymbolReference * findOrCreateShadowSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex, bool isStore);
   TR::SymbolReference * findOrFabricateShadowSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, TR::Symbol::RecognizedField recognizedField, TR::DataType type, uint32_t offset, bool isVolatile, bool isPrivate, bool isFinal, char* name = NULL);

   TR::SymbolReference * findOrCreateObjectNewInstanceImplSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol);
   TR::SymbolReference * findOrCreateDLTBlockSymbolRef();
   TR::SymbolReference * findDLTBlockSymbolRef();
   TR::SymbolReference * findOrCreateMultiANewArraySymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);
   TR::SymbolReference * findOrCreateInstanceShapeSymbolRef();
   TR::SymbolReference * findOrCreateInstanceDescriptionSymbolRef();
   TR::SymbolReference * findOrCreateDescriptionWordFromPtrSymbolRef();
   TR::SymbolReference * findOrCreateClassFlagsSymbolRef();
   TR::SymbolReference * findOrCreateClassAndDepthFlagsSymbolRef();
   TR::SymbolReference * findOrCreateArrayComponentTypeAsPrimitiveSymbolRef();
   TR::SymbolReference * findOrCreateMethodTypeCheckSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);
   TR::SymbolReference * findOrCreateIncompatibleReceiverSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);
   TR::SymbolReference * findOrCreateIncompatibleClassChangeErrorSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);
   TR::SymbolReference * findOrCreateReportStaticMethodEnterSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);
   TR::SymbolReference * findOrCreateReportMethodExitSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);
   TR::SymbolReference * findOrCreateHeaderFlagsSymbolRef();
   TR::SymbolReference * findOrCreateDiscontiguousArraySizeSymbolRef();
   TR::SymbolReference * findOrCreateClassLoaderSymbolRef(TR_ResolvedMethod *);
   TR::SymbolReference * findOrCreateCurrentThreadSymbolRef();
   TR::SymbolReference * findOrCreateJ9MethodExtraFieldSymbolRef(intptrj_t offset);
   TR::SymbolReference * findOrCreateJ9MethodConstantPoolFieldSymbolRef(intptrj_t offset);
   TR::SymbolReference * findOrCreateStartPCLinkageInfoSymbolRef(intptrj_t offset);
   TR::SymbolReference * findOrCreatePerCodeCacheHelperSymbolRef(TR_CCPreLoadedCode helper, uintptrj_t helperAddr);
   TR::SymbolReference * findOrCreateANewArraySymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);
   TR::SymbolReference * findOrCreateStringSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex);
   /** \brief
    *     Finds or creates a constant dynamic static symbol reference.
    *
    *  \param owningMethodSymbol
    *     The owning resolved method symbol.
    *
    *  \param cpIndex
    *     The constant pool index of the constant dynamic.
    *
    *  \param symbolTypeSig
    *     The underlying class type signature string of the constant dynamic. For primitive this is the signature of the corresponding autobox class.
    *
    *  \param symbolTypeSigLength
    *     Length of the underlying class type signature string.
    *
    *  \param isCondyPrimitive
    *     Determines whether the constant dynamic is of primitive type.
    *
    *  \return
    *     The static symbol reference of the constant dynamic.
    */
   TR::SymbolReference * findOrCreateConstantDynamicSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex, char* symbolTypeSig, int32_t symbolTypeSigLength, bool isCondyPrimitive);
   TR::SymbolReference * findContiguousArraySizeSymbolRef() { return element(contiguousArraySizeSymbol); }
   TR::SymbolReference * findOrCreateMethodMonitorEntrySymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);
   TR::SymbolReference * findOrCreateMethodMonitorExitSymbolRef(TR::ResolvedMethodSymbol * owningMethodSymbol);
   TR::SymbolReference * findOrCreateCompiledMethodSymbolRef();
   TR::SymbolReference * findOrCreateMethodTypeSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex);
   TR::SymbolReference * findOrCreateMethodHandleSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex);
   TR::SymbolReference * findOrCreateClassStaticsSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex);
   TR::SymbolReference * findOrCreateStaticSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t cpIndex, bool isStore);

   TR::SymbolReference * findOrCreateClassIsArraySymbolRef();

   TR::SymbolReference * findClassFromJavaLangClassAsPrimitiveSymbolRef();
   TR::SymbolReference * findOrCreateClassFromJavaLangClassAsPrimitiveSymbolRef();

   TR::SymbolReference * findOrCreateJavaLangClassFromClassSymbolRef();
   TR::SymbolReference * findOrCreateClassFromJavaLangClassSymbolRef();
   TR::SymbolReference * findOrCreateInitializeStatusFromClassSymbolRef();
   TR::SymbolReference * findInitializeStatusFromClassSymbolRef();
   TR::SymbolReference * findOrCreateClassRomPtrSymbolRef();
   TR::SymbolReference * findOrCreateArrayComponentTypeSymbolRef();

   TR::SymbolReference * findOrCreateProfilingBufferCursorSymbolRef();
   TR::SymbolReference * findOrCreateProfilingBufferEndSymbolRef();
   TR::SymbolReference * findOrCreateProfilingBufferSymbolRef(intptrj_t offset = 0);

   // optimizer
   TR::SymbolReference * findOrCreateRamStaticsFromClassSymbolRef();
   TR::SymbolReference * findOrCreateGlobalFragmentSymbolRef();
   TR::SymbolReference * findOrCreateThreadDebugEventData(int32_t index);

   // optimizer (loop versioner)
   TR::SymbolReference * findOrCreateThreadLowTenureAddressSymbolRef();
   TR::SymbolReference * findOrCreateThreadHighTenureAddressSymbolRef();

   TR::SymbolReference * findOrCreateFragmentParentSymbolRef();

   /** \brief
    *     Finds an unsafe symbol reference with given constraints if it exists.
    *
    *  \param type
    *     The type of the unsafe symbol.
    *
    *  \param javaObjectReference
    *     Determines whether this symbol reference is referencing a Java object field.
    *
    *  \param javaStaticReference
    *     Determines whether this symbol reference is referencing a Java object static field.
    *
    *  \param isVolatile
    *     Determines whether this symbol should be treated with volatile semantics.
    *
    *  \return
    *     The unsafe symbol reference with given constraints if it exists; <c>NULL</c> otherwise.
    */
   TR::SymbolReference* findUnsafeSymbolRef(TR::DataType type, bool javaObjectReference = false, bool javaStaticReference = false, bool isVolatile = false);

   /** \brief
    *     Finds an unsafe symbol reference with given constraints if it exists, or creates one if no such symbol
    *     reference already exists.
    *
    *  \param type
    *     The type of the unsafe symbol.
    *
    *  \param javaObjectReference
    *     Determines whether this symbol reference is referencing a Java object field.
    *
    *  \param javaStaticReference
    *     Determines whether this symbol reference is referencing a Java object static field.
    *
    *  \param isVolatile
    *     Determines whether this symbol should be treated with volatile semantics.
    *
    *  \return
    *     The unsafe symbol reference with given constraints if it exists.
    */
   TR::SymbolReference* findOrCreateUnsafeSymbolRef(TR::DataType type, bool javaObjectReference = false, bool javaStaticReference = false, bool isVolatile = false);

   TR::SymbolReference * findOrCreateImmutableGenericIntShadowSymbolReference(intptrj_t offset); // "Immutable" means no aliasing issues; ie. reads from these shadows can be freely reordered wrt anything else

   TR::SymbolReference * findOrCreateCheckCastForArrayStoreSymbolRef(TR::ResolvedMethodSymbol *owningMethodSymbol);

   /**
    * \brief
    *    Creates a new symbol for a parameter within the supplied owning method of the
    *    specified type and slot index.
    *
    * \param owningMethodSymbol
    *    Resolved method symbol for which this parameter is created.
    *
    * \param slot
    *    Slot index for this parameter.
    *
    * \param type
    *    TR::DataType of the parameter.
    *
    * \param knownObjectIndex
    *    known object index is needed if the parameter is a known object, like the receiver of a customer thunk
    *
    * \return
    *    The created TR::ParameterSymbol
    */
   TR::ParameterSymbol * createParameterSymbol(TR::ResolvedMethodSymbol * owningMethodSymbol, int32_t slot, TR::DataType type, TR::KnownObjectTable::Index knownObjectIndex = TR::KnownObjectTable::UNKNOWN);

   void initShadowSymbol(TR_ResolvedMethod *, TR::SymbolReference *, bool, TR::DataType, uint32_t, bool);

   List<TR::SymbolReference> *dynamicMethodSymrefsByCallSiteIndex(int32_t index);
   bool isFieldClassObject(TR::SymbolReference *symRef);
   bool isFieldTypeBool(TR::SymbolReference *symRef);
   bool isStaticTypeBool(TR::SymbolReference *symRef);
   bool isReturnTypeBool(TR::SymbolReference *symRef);

   /*
    * Creates symbol references for parameters in @param owningMethodSymbol
    * and add them to the current symbol reference table which is usually different
    * from the global symbol reference table. This API is used for peeking and
    * optimizations where a local symbol reference table is needed.
    */
   void addParameters(TR::ResolvedMethodSymbol * owningMethodSymbol);

   // NO LONGER NEEDED?  Disabled since inception (2009)
   void checkUserField(TR::SymbolReference *);

   // Immutability
   void checkImmutable(TR::SymbolReference *);
   TR_ImmutableInfo *findOrCreateImmutableInfo(TR_OpaqueClassBlock *);
   bool isImmutable(TR::SymbolReference *);
   int immutableConstructorId(TR::MethodSymbol *);
   bool hasImmutable() { return _hasImmutable; }
   void setHasImmutable(bool b) { _hasImmutable = b; }
   List<TR_ImmutableInfo> &immutableInfo() { return _immutableInfo; }
   TR_Array<TR_BitVector *> &immutableSymRefNumbers() { return _immutableSymRefNumbers; }

   int userFieldMethodId(TR::MethodSymbol *);
   bool hasUserField() { return _hasUserField; }
   void setHasUserField(bool b) { _hasUserField = b; }

   void performClassLookahead(TR_PersistentClassInfo *, TR_ResolvedMethod *);


   TR::SymbolReference * findShadowSymbol(TR_ResolvedMethod * owningMethod, int32_t cpIndex, TR::DataType, TR::Symbol::RecognizedField *recognizedField = NULL);

   protected:

   TR::Symbol                           *_currentThreadDebugEventDataSymbol;
   List<TR::SymbolReference>            _currentThreadDebugEventDataSymbolRefs;
   List<TR::SymbolReference>            _constantPoolAddressSymbolRefs;

   private:

   TR::SymbolReference * createShadowSymbolWithoutCpIndex(TR::ResolvedMethodSymbol *, bool , TR::DataType, uint32_t, bool);
   TR::SymbolReference * findJavaLangReferenceReferentShadowSymbol(TR_ResolvedMethod * owningMethod, TR::DataType, uint32_t);

   TR_Array<List<TR::SymbolReference>*> _dynamicMethodSymrefsByCallSiteIndex;

   List<TR_ImmutableInfo>              _immutableInfo;
   TR_Array<TR_BitVector *>            _immutableSymRefNumbers;

   /** \brief
    *     Represents the set of symbol references to static fields of Java objects.
    */
   TR_Array<TR::SymbolReference *> * _unsafeJavaStaticSymRefs;

   /** \brief
    *     Represents the set of symbol references to static volatile fields of Java objects.
    */
   TR_Array<TR::SymbolReference *> * _unsafeJavaStaticVolatileSymRefs;
   };

}
#define _numImmutableClasses (TR::java_lang_String_init - TR::java_lang_Boolean_init + 1)

#endif
