/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#ifndef J9_CLASSENV_INCL
#define J9_CLASSENV_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_CLASSENV_CONNECTOR
#define J9_CLASSENV_CONNECTOR
namespace J9 { class ClassEnv; }
namespace J9 { typedef J9::ClassEnv ClassEnvConnector; }
#endif

#if defined(J9VM_OPT_JITSERVER)
#include <vector>
#endif /* defined(J9VM_OPT_JITSERVER) */
#include "env/jittypes.h"
#include "env/OMRClassEnv.hpp"
#include "infra/Annotations.hpp"
#include "il/DataTypes.hpp"
#include "j9.h"

namespace TR { class SymbolReference; }
namespace TR { class TypeLayout; }
namespace TR { class Region; }
class TR_PersistentClassInfo;
template <typename ListKind> class List;

namespace J9
{

class OMR_EXTENSIBLE ClassEnv : public OMR::ClassEnvConnector
   {
public:

   TR::ClassEnv *self();

   bool classesOnHeap() { return true; }

   bool classObjectsMayBeCollected() { return false; }

   bool romClassObjectsMayBeCollected() { return false; }

   TR_OpaqueClassBlock *getClassFromJavaLangClass(uintptr_t objectPointer);

   J9Class *convertClassOffsetToClassPtr(TR_OpaqueClassBlock *clazzOffset);
   TR_OpaqueClassBlock *convertClassPtrToClassOffset(J9Class *clazzPtr);

   uintptr_t classFlagsValue(TR_OpaqueClassBlock * classPointer);
   uintptr_t classFlagReservableWordInitValue(TR_OpaqueClassBlock * classPointer);
   uintptr_t classDepthOf(TR_OpaqueClassBlock *clazzPointer);
   uintptr_t classInstanceSize(TR_OpaqueClassBlock * clazzPointer);

   J9ROMClass *romClassOf(TR_OpaqueClassBlock * clazz);
   J9ROMClass *romClassOfSuperClass(TR_OpaqueClassBlock * clazz, size_t index);

   J9ITable *iTableOf(TR_OpaqueClassBlock * clazz);
   J9ITable *iTableNext(J9ITable *current);
   J9ROMClass *iTableRomClass(J9ITable *current);
#if defined(J9VM_OPT_JITSERVER)
   std::vector<TR_OpaqueClassBlock *> getITable(TR_OpaqueClassBlock *clazz);
#endif /* defined(J9VM_OPT_JITSERVER) */

   J9Class **superClassesOf(TR_OpaqueClassBlock * clazz);

   bool isStringClass(TR_OpaqueClassBlock *clazz);

   bool classHasIllegalStaticFinalFieldModification(TR_OpaqueClassBlock * clazzPointer);
   void setClassHasIllegalStaticFinalFieldModification(
      TR_OpaqueClassBlock *clazz, TR::Compilation *comp);
   bool isAbstractClass(TR::Compilation *comp, TR_OpaqueClassBlock *clazzPointer);
   bool isInterfaceClass(TR::Compilation *comp, TR_OpaqueClassBlock *clazzPointer);
   bool isConcreteClass(TR::Compilation *comp, TR_OpaqueClassBlock * clazzPointer);
   bool isValueTypeClass(TR_OpaqueClassBlock *);
   bool isValueTypeClassFlattened(TR_OpaqueClassBlock *clazz);
   bool isValueBasedOrValueTypeClass(TR_OpaqueClassBlock *);
   bool isArrayNullRestricted(TR::Compilation *comp, TR_OpaqueClassBlock *arrayClass);

   /** \brief
    *    Returns the size of the flattened array element
    *
    *  \param comp
    *    The compilation object
    *
    *  \param arrayClass
    *    The array class that is to be checked
    *
    *  \return
    *    Size of the flattened array element
    */
   int32_t flattenedArrayElementSize(TR::Compilation *comp, TR_OpaqueClassBlock *arrayClass);

   /** \brief
    *    Checks whether a class implements `IdentityObject`/`IdentityInterface`
    *
    *  \param clazz
    *    The class that is to be checked
    *
    *  \return
    *    `true` if the class implements `IdentityObject`/`IdentityInterface`;
    *    `false` otherwise (that is, if the class could be a value type,
    *    or some abstract class, interface, or java/lang/Object)
    */
   bool classHasIdentity(TR_OpaqueClassBlock *clazz);

   /** \brief
    *    Checks whether a class supports direct memory comparison if its fields meet
    *    the criteria that NO field is of
    *    - type double (D) or float (F)
    *    - nullable-class/interface type
    *    - null-restricted class type that are not both flattened and recursively
    *      compatible for direct memory comparison
    *
    *  \param clazz
    *    The class that is to be checked
    *
    *  \return
    *    `true` if the class supports direct memory comparison. `false` otherwise.
    */
   bool classSupportsDirectMemoryComparison(TR_OpaqueClassBlock *clazz);

   /**
    * \brief
    *    Checks whether instances of the specified class can be trivially initialized by
    *    "zeroing" their fields.
    *    In the case of OpenJ9, this tests whether any field is of null-restricted type that
    *    has not been "flattened" (that is, had the value type's fields inlined into this class).
    *    Such a value type field must be initialized with the default value of the type.
    *
    * \param clazz
    *    The class that is to be checked
    *
    * \return
    *    `true` if instances of the specified class can be initialized by zeroing their fields;
    *    `false` otherwise (that is, if the class has value type fields whose fields have not
    *    been inlined)
    */
   bool isZeroInitializable(TR_OpaqueClassBlock *clazz);
   bool isPrimitiveClass(TR::Compilation *comp, TR_OpaqueClassBlock *clazz);
   bool isAnonymousClass(TR::Compilation *comp, TR_OpaqueClassBlock *clazz);
   bool isPrimitiveArray(TR::Compilation *comp, TR_OpaqueClassBlock *);
   TR::DataTypes primitiveArrayComponentType(TR::Compilation *comp, TR_OpaqueClassBlock *);
   bool isReferenceArray(TR::Compilation *comp, TR_OpaqueClassBlock *);
   bool isClassArray(TR::Compilation *comp, TR_OpaqueClassBlock *);
   bool isClassFinal(TR::Compilation *comp, TR_OpaqueClassBlock *);
   bool hasFinalizer(TR::Compilation *comp, TR_OpaqueClassBlock *classPointer);
   bool isClassInitialized(TR::Compilation *comp, TR_OpaqueClassBlock *);
   /**
    *  \brief
    *    Checks whether a class is visible to another class
    *
    *  \param sourceClass
    *    The source class that is to be checked
    *
    *  \param destClass
    *    The destination class that is to be checked
    *
    *  \return
    *    `true` if `destClass` is visible to `sourceClass`. `false` otherwise.
    */
   bool isClassVisible(TR::Compilation *comp, TR_OpaqueClassBlock *sourceClass, TR_OpaqueClassBlock *destClass);
   bool hasFinalFieldsInClass(TR::Compilation *comp, TR_OpaqueClassBlock *classPointer);
   bool sameClassLoaders(TR::Compilation *comp, TR_OpaqueClassBlock *, TR_OpaqueClassBlock *);
   bool isString(TR::Compilation *comp, TR_OpaqueClassBlock *clazz);
   bool jitStaticsAreSame(TR::Compilation *comp, TR_ResolvedMethod * method1, int32_t cpIndex1, TR_ResolvedMethod * method2, int32_t cpIndex2);
   bool jitFieldsAreSame(TR::Compilation *comp, TR_ResolvedMethod * method1, int32_t cpIndex1, TR_ResolvedMethod * method2, int32_t cpIndex2, int32_t isStatic);

   /** \brief
    *	    Populates a TypeLayout object.
    *
    *  \param region
    *     The region used to allocate TypeLayout.
    *
    *  \param opaqueClazz
    *     Class of the type whose layout needs to be populated.
    *
    *  \return
    *     Returns a pointer to the TypeLayout object populated.
    */
   const TR::TypeLayout* enumerateFields(TR::Region& region, TR_OpaqueClassBlock * clazz, TR::Compilation *comp);

   uintptr_t getArrayElementWidthInBytes(TR::Compilation *comp, TR_OpaqueClassBlock* arrayClass);

   uintptr_t persistentClassPointerFromClassPointer(TR::Compilation *comp, TR_OpaqueClassBlock *clazz);
   TR_OpaqueClassBlock *objectClass(TR::Compilation *comp, uintptr_t objectPointer);
   TR_OpaqueClassBlock *classFromJavaLangClass(TR::Compilation *comp, uintptr_t objectPointer);

   uint16_t getStringCharacter(TR::Compilation *comp, uintptr_t objectPointer, int32_t index);
   bool getStringFieldByName(TR::Compilation *, TR::SymbolReference *stringRef, TR::SymbolReference *fieldRef, void* &pResult);

   using OMR::ClassEnv::classNameChars;
   char *classNameChars(TR::Compilation *, TR::SymbolReference *symRef, int32_t & length);
   char *classNameChars(TR::Compilation *, TR_OpaqueClassBlock * clazz, int32_t & length);

   char *classSignature_DEPRECATED(TR::Compilation *comp, TR_OpaqueClassBlock * clazz, int32_t & length, TR_Memory *);
   char *classSignature(TR::Compilation *comp, TR_OpaqueClassBlock * clazz, TR_Memory *);

   /**
    * \brief
    *    Constructs a class signature char string based on the class name
    *
    * \param[in] name
    *    The class name
    *
    * \param[in,out] len
    *    The input is the length of the class name. Returns the length of the signature
    *
    * \param[in] comp
    *    The compilation object
    *
    * \param[in] allocKind
    *    The type of the memory allocation
    *
    * \param[in] clazz
    *    The class that the class name belongs to
    *
    * \return
    *    A class signature char string
    */
   char *classNameToSignature(const char *name, int32_t &len, TR::Compilation *comp, TR_AllocationKind allocKind = stackAlloc, TR_OpaqueClassBlock *clazz = NULL);

   int32_t vTableSlot(TR::Compilation *comp, TR_OpaqueMethodBlock *, TR_OpaqueClassBlock *);
   int32_t flagValueForPrimitiveTypeCheck(TR::Compilation *comp);
   int32_t flagValueForArrayCheck(TR::Compilation *comp);
   int32_t flagValueForFinalizerCheck(TR::Compilation *comp);

   bool isClassSpecialForStackAllocation(TR_OpaqueClassBlock * classPointer);
   /**
    * Get the virtual function table entry at a specific offset from the class
    *
    * @param clazz The RAM class pointer to read from
    * @param offset An offset into the virtual function table (VFT) of clazz
    * @return The method at the given offset, or (depending on offset) its
    * entry point, or 0 if offset is out of bounds or the result is otherwise
    * unavailable.
    */
   intptr_t getVFTEntry(TR::Compilation *comp, TR_OpaqueClassBlock* clazz, int32_t offset);
   uint8_t *getROMClassRefName(TR::Compilation *comp, TR_OpaqueClassBlock *clazz, uint32_t cpIndex, int &classRefLen);
   J9ROMConstantPoolItem *getROMConstantPool(TR::Compilation *comp, TR_OpaqueClassBlock *clazz);

   /**
    * @brief Determine if a list of classes contains less than two concrete classes.
    * A class is considered concrete if it is not an interface or an abstract class
    * @param subClasses List of subclasses to be checked.
    * @return Returns 'true' if the given list of classes contains less than
    * 2 concrete classses and false otherwise.
    */
   bool containsZeroOrOneConcreteClass(TR::Compilation *comp, List<TR_PersistentClassInfo>* subClasses);

   /** \brief
    *     Returns the reference to the address of the default value instance for a value class.
    *
    *  \param clazz
    *     The class that the default value instance belongs to. Must be an initialized value class.
    *
    *  \return
    *     The reference to the address of the default value instance.
    */
   j9object_t *getDefaultValueSlotAddress(TR::Compilation *comp, TR_OpaqueClassBlock *clazz);
   };

}

#endif
