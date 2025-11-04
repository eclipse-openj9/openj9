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

#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "il/DataTypes.hpp"
#include "il/MethodSymbol.hpp"
#include "infra/Assert.hpp"
#include "runtime/J9Runtime.hpp"

/**
 * Return true if this method is pure ie no side-effects.
 *
 * Currently only runtime helpers and recognized methods can be true.
 */
bool
J9::MethodSymbol::isPureFunction()
   {
   switch(self()->getRecognizedMethod())
      {
      case TR::java_lang_Math_abs_I:
      case TR::java_lang_Math_abs_L:
      case TR::java_lang_Math_abs_F:
      case TR::java_lang_Math_abs_D:
      case TR::java_lang_Math_acos:
      case TR::java_lang_Math_asin:
      case TR::java_lang_Math_atan:
      case TR::java_lang_Math_atan2:
      case TR::java_lang_Math_cbrt:
      case TR::java_lang_Math_ceil:
      case TR::java_lang_Math_copySign_F:
      case TR::java_lang_Math_copySign_D:
      case TR::java_lang_Math_cos:
      case TR::java_lang_Math_cosh:
      case TR::java_lang_Math_exp:
      case TR::java_lang_Math_expm1:
      case TR::java_lang_Math_floor:
      case TR::java_lang_Math_hypot:
      case TR::java_lang_Math_IEEEremainder:
      case TR::java_lang_Math_log:
      case TR::java_lang_Math_log10:
      case TR::java_lang_Math_log1p:
      case TR::java_lang_Math_max_I:
      case TR::java_lang_Math_max_L:
      case TR::java_lang_Math_max_F:
      case TR::java_lang_Math_max_D:
      case TR::java_lang_Math_min_I:
      case TR::java_lang_Math_min_L:
      case TR::java_lang_Math_min_F:
      case TR::java_lang_Math_min_D:
      case TR::java_lang_Math_multiplyHigh:
      case TR::java_lang_Math_nextAfter_F:
      case TR::java_lang_Math_nextAfter_D:
      case TR::java_lang_Math_pow:
      case TR::java_lang_Math_rint:
      case TR::java_lang_Math_round_F:
      case TR::java_lang_Math_round_D:
      case TR::java_lang_Math_scalb_F:
      case TR::java_lang_Math_scalb_D:
      case TR::java_lang_Math_sin:
      case TR::java_lang_Math_sinh:
      case TR::java_lang_Math_sqrt:
      case TR::java_lang_Math_tan:
      case TR::java_lang_Math_tanh:
      case TR::java_lang_ref_Reference_reachabilityFence:
      case TR::java_lang_StrictMath_acos:
      case TR::java_lang_StrictMath_asin:
      case TR::java_lang_StrictMath_atan:
      case TR::java_lang_StrictMath_atan2:
      case TR::java_lang_StrictMath_cbrt:
      case TR::java_lang_StrictMath_ceil:
      case TR::java_lang_StrictMath_copySign_F:
      case TR::java_lang_StrictMath_copySign_D:
      case TR::java_lang_StrictMath_cos:
      case TR::java_lang_StrictMath_cosh:
      case TR::java_lang_StrictMath_exp:
      case TR::java_lang_StrictMath_expm1:
      case TR::java_lang_StrictMath_floor:
      case TR::java_lang_StrictMath_hypot:
      case TR::java_lang_StrictMath_IEEEremainder:
      case TR::java_lang_StrictMath_log:
      case TR::java_lang_StrictMath_log10:
      case TR::java_lang_StrictMath_log1p:
      case TR::java_lang_StrictMath_nextAfter_F:
      case TR::java_lang_StrictMath_nextAfter_D:
      case TR::java_lang_StrictMath_pow:
      case TR::java_lang_StrictMath_random:
      case TR::java_lang_StrictMath_round_F:
      case TR::java_lang_StrictMath_round_D:
      case TR::java_lang_StrictMath_scalb_F:
      case TR::java_lang_StrictMath_scalb_D:
      case TR::java_lang_StrictMath_rint:
      case TR::java_lang_StrictMath_sin:
      case TR::java_lang_StrictMath_sinh:
      case TR::java_lang_StrictMath_sqrt:
      case TR::java_lang_StrictMath_tan:
      case TR::java_lang_StrictMath_tanh:
      case TR::java_nio_Bits_keepAlive:
         /*
      case TR::java_math_BigDecimal_valueOf:
      case TR::java_math_BigDecimal_add:
      case TR::java_math_BigDecimal_subtract:
      case TR::java_math_BigDecimal_multiply:
      case TR::java_math_BigInteger_add:
      case TR::java_math_BigInteger_subtract:
      case TR::java_math_BigInteger_multiply:
         */
         return true;
      default:
         return false;
      }
   return false;
   }

/**
 * Returns true if the function call will not yield to OSR point.
 *
 * An example of kind of function which can go in the list would be recognized calls with
 * NOP calls or the one that are guaranteed to be inlined by codegenerator.
 */
bool
J9::MethodSymbol::functionCallDoesNotYieldOSR()
   {
   switch(self()->getRecognizedMethod())
      {
      case TR::java_lang_ref_Reference_reachabilityFence:
      case TR::java_nio_Bits_keepAlive:
         return true;
      default:
         return OMR::MethodSymbolConnector::functionCallDoesNotYieldOSR();
      }
   return false;
   }

// Which recognized methods are known to have no valid null checks
//
static TR::RecognizedMethod canSkipNullChecks[] =
   {
   // NOTE!! add methods whose checks can be skipped by sov library to the beginning of the list (see stopMethod below)
   TR::java_lang_invoke_FilterArgumentsHandle_invokeExact,
   TR::java_lang_invoke_MethodHandle_undoCustomizationLogic,
   TR::java_lang_invoke_CollectHandle_invokeExact,
   TR::java_util_ArrayList_add,
   TR::java_util_ArrayList_ensureCapacity,
   TR::java_util_ArrayList_get,
#if JAVA_SPEC_VERSION < 17
   TR::java_lang_String_trim,
   TR::java_lang_String_replace,
   TR::java_lang_String_charAt,
   TR::java_lang_String_charAtInternal_I,
   TR::java_lang_String_charAtInternal_IB,
   TR::java_lang_String_hashCodeImplCompressed,
   TR::java_lang_String_hashCodeImplDecompressed,
   TR::java_lang_String_length,
   TR::java_lang_String_lengthInternal,
   //TR::java_lang_String_toLowerCase,
   //TR::java_lang_String_toUpperCase,
   //TR::java_lang_String_toLowerCaseCore,
   //TR::java_lang_String_toUpperCaseCore,
   TR::java_lang_String_equalsIgnoreCase,
   TR::java_lang_String_indexOf_fast,
   TR::java_lang_String_isCompressed,
   TR::java_lang_String_coder,
   TR::java_lang_String_isLatin1,
   TR::java_lang_String_init_int_String_int_String_String,
   TR::java_lang_String_init_int_int_char_boolean,
   TR::java_lang_String_split_str_int,
#endif
   TR::java_lang_StringBuffer_capacityInternal,
   TR::java_lang_StringBuffer_lengthInternalUnsynchronized,
   TR::java_lang_StringBuilder_capacityInternal,
   TR::java_lang_StringBuilder_lengthInternal,
   TR::java_lang_StringUTF16_charAt,
   TR::java_lang_StringUTF16_checkIndex,
   TR::java_lang_StringUTF16_length,
   TR::java_util_Hashtable_clone,
   TR::java_util_Hashtable_contains,
   TR::java_util_HashtableHashEnumerator_hasMoreElements,
   TR::java_util_HashtableHashEnumerator_nextElement,
   TR::java_util_HashMap_resize,
   TR::java_util_HashMapHashIterator_nextNode,
   //TR::java_util_Vector_addElement,
   TR::java_math_BigDecimal_longString1C,
   TR::java_math_BigDecimal_longString2,
   TR::java_math_BigInteger_init_long,
#ifdef OPENJ9_BUILD
   TR::java_math_BigInteger_toByteArray,
#endif // OPENJ9_BUILD
   TR::java_math_BigInteger_stripLeadingZeroBytes1,
   TR::java_math_BigInteger_stripLeadingZeroBytes2,
   TR::java_util_EnumMap__nec_,
   TR::java_nio_Bits_getCharB,
   TR::java_nio_Bits_getCharL,
   TR::java_nio_Bits_getShortB,
   TR::java_nio_Bits_getShortL,
   TR::java_nio_Bits_getIntB,
   TR::java_nio_Bits_getIntL,
   TR::java_nio_Bits_getLongB,
   TR::java_nio_Bits_getLongL,
   TR::java_nio_HeapByteBuffer__get,
   TR::java_nio_HeapByteBuffer_put,
   TR::unknownMethod
   };

static TR::RecognizedMethod stringCanSkipNullChecks[] =
   {
   TR::java_lang_StringLatin1_trim,
   TR::java_lang_StringUTF16_getChars_ByteArray,
   TR::java_lang_StringUTF16_trim,
   TR::java_lang_StringLatin1_replace_char,
   TR::java_lang_StringLatin1_replace_CharSequence,
   TR::java_lang_StringUTF16_replace_char,
   TR::java_lang_StringUTF16_replace_CharSequence,
   TR::java_lang_StringLatin1_toLowerCase,
   TR::java_lang_StringLatin1_toUpperCase,
   TR::java_lang_StringUTF16_toLowerCase,
   TR::java_lang_StringUTF16_toUpperCase,
   TR::unknownMethod
   };

bool
J9::MethodSymbol::safeToSkipNullChecks()
   {
   TR::RecognizedMethod methodId = self()->getRecognizedMethod();
   if (methodId == TR::unknownMethod)
      return false;

   for (int i = 0; canSkipNullChecks[i] != TR::unknownMethod; ++i)
      if (canSkipNullChecks[i] == methodId)
         return true;

   static char *disableStringChkSkips = feGetEnv("TR_DisableStringChkSkips");
   if (!disableStringChkSkips)
      {
      for (int i = 0; stringCanSkipNullChecks[i] != TR::unknownMethod; ++i)
         {
         if (stringCanSkipNullChecks[i] == methodId)
            {
            return true;
            }
         }
      }

   return false;
   }


// Which recognized methods are known to have no valid bound checks
//
static TR::RecognizedMethod canSkipBoundChecks[] =
   {
   // NOTE!! add methods whose checks can be skipped by sov library to the beginning of the list (see stopMethod below)
   //TR::java_util_ArrayList_add,
   //TR::java_util_ArrayList_remove,
   //TR::java_util_ArrayList_ensureCapacity,
   //TR::java_util_ArrayList_get,
   TR::java_lang_Character_toLowerCase,
   TR::java_lang_invoke_FilterArgumentsHandle_invokeExact,
   TR::java_lang_invoke_CollectHandle_invokeExact,
   TR::java_lang_String_trim,
   TR::java_lang_String_charAt,
   TR::java_lang_String_charAtInternal_I,
   TR::java_lang_String_charAtInternal_IB,
   TR::java_lang_String_indexOf_String,
   TR::java_lang_String_indexOf_String_int,
   TR::java_lang_String_indexOf_native,
   TR::java_lang_String_indexOf_fast,
   TR::java_lang_String_replace,
   TR::java_lang_String_compareTo,
   TR::java_lang_String_lastIndexOf,
   TR::java_lang_String_toLowerCase,
   TR::java_lang_String_toUpperCase,
   TR::java_lang_String_toLowerCaseCore,
   TR::java_lang_String_toUpperCaseCore,
   TR::java_lang_String_regionMatches,
   TR::java_lang_String_regionMatches_bool,
   TR::java_lang_String_regionMatchesInternal,
   TR::java_lang_String_equalsIgnoreCase,
   TR::java_lang_String_compareToIgnoreCase,
   TR::java_lang_String_hashCodeImplCompressed,
   TR::java_lang_String_hashCodeImplDecompressed,
   TR::java_lang_String_unsafeCharAt,
   TR::java_lang_String_split_str_int,
   TR::java_lang_String_startsWith,
   TR::java_lang_StringUTF16_compareCodePointCI,
   TR::java_lang_StringUTF16_compareToCIImpl,
   TR::java_lang_StringUTF16_compareValues,
   TR::java_util_Hashtable_get,
   TR::java_util_Hashtable_put,
   TR::java_util_Hashtable_clone,
   TR::java_util_Hashtable_rehash,
   TR::java_util_Hashtable_remove,
   TR::java_util_Hashtable_contains,
   TR::java_util_Hashtable_getEntry,
   TR::java_util_HashtableHashEnumerator_hasMoreElements,
   TR::java_util_HashtableHashEnumerator_nextElement,
   TR::java_util_HashMap_getNode,
   TR::java_util_HashMap_getNode_Object,
   TR::java_util_HashMap_resize,
   //TR::java_util_HashMapHashIterator_nextNode,  /* Unsafe if the Iterator is being incorrectly used (concurrent execution) */
   TR::java_util_HashMapHashIterator_init,        /* Safe because the object is only visible by one thread when init() is executing */
   TR::java_util_EnumMap_put,
   TR::java_util_EnumMap_typeCheck,
   TR::java_util_EnumMap__init_,
   TR::java_util_EnumMap__nec_,
   TR::java_util_TreeMap_all,
   TR::java_math_BigDecimal_longString1C,
   TR::java_math_BigDecimal_longString2,
   TR::java_math_BigInteger_init_long,
#ifdef OPENJ9_BUILD
   TR::java_math_BigInteger_toByteArray,
#endif // OPENJ9_BUILD
   TR::java_math_BigInteger_stripLeadingZeroBytes1,
   TR::java_math_BigInteger_stripLeadingZeroBytes2,
#ifdef OPENJ9_BUILD
   TR::java_math_BigInteger_bitCount,
   TR::java_math_BigInteger_bitLength,
#endif // OPENJ9_BUILD
   TR::java_util_HashMap_get,
   TR::java_util_HashMap_findNonNullKeyEntry,
   TR::java_util_HashMap_putImpl,
   TR::java_lang_String_init_int_String_int_String_String,
   TR::java_lang_String_init_int_int_char_boolean,
   TR::java_nio_Bits_getCharB,
   TR::java_nio_Bits_getCharL,
   TR::java_nio_Bits_getShortB,
   TR::java_nio_Bits_getShortL,
   TR::java_nio_Bits_getIntB,
   TR::java_nio_Bits_getIntL,
   TR::java_nio_Bits_getLongB,
   TR::java_nio_Bits_getLongL,
   TR::java_nio_HeapByteBuffer__get,
   TR::java_nio_HeapByteBuffer_put,
   TR::unknownMethod
   };

static TR::RecognizedMethod stringCanSkipBoundChecks[] =
   {
   TR::java_lang_StringLatin1_trim,
   TR::java_lang_StringUTF16_getChars_ByteArray,
   TR::java_lang_StringLatin1_replace_char,
   TR::java_lang_StringLatin1_replace_CharSequence,
   TR::java_lang_StringUTF16_replace_CharSequence,
   TR::java_lang_StringLatin1_toLowerCase,
   TR::java_lang_StringLatin1_toUpperCase,
   TR::unknownMethod
   };

bool
J9::MethodSymbol::safeToSkipBoundChecks()
   {
   TR::RecognizedMethod methodId = self()->getRecognizedMethod();
   if (methodId == TR::unknownMethod)
      return false;

   for (int i = 0; canSkipBoundChecks[i] != TR::unknownMethod; ++i)
      if (canSkipBoundChecks[i] == methodId)
         return true;

   static char *disableStringChkSkips = feGetEnv("TR_DisableStringChkSkips");
   if (!disableStringChkSkips)
      {
      for (int i = 0; stringCanSkipBoundChecks[i] != TR::unknownMethod; ++i)
         {
         if (stringCanSkipBoundChecks[i] == methodId)
            {
            return true;
            }
         }
      }

   return false;
   }

// Which recognized methods are known to have no valid div checks
//
static TR::RecognizedMethod canSkipDivChecks[] =
   {
   TR::java_util_Hashtable_get,
   TR::java_util_Hashtable_put,
   TR::java_util_Hashtable_rehash,
   TR::java_util_Hashtable_remove,
   TR::java_util_Hashtable_getEntry,
   TR::unknownMethod
   };

bool
J9::MethodSymbol::safeToSkipDivChecks()
   {
   TR::RecognizedMethod methodId = self()->getRecognizedMethod();
   if (methodId == TR::unknownMethod)
      return false;

   for (int i = 0; canSkipDivChecks[i] != TR::unknownMethod; ++i)
      if (canSkipDivChecks[i] == methodId)
         return true;

   return false;
   }


// Which recognized methods are known to have no valid checkcasts
//
static TR::RecognizedMethod canSkipCheckCasts[] =
   {
   TR::java_util_ArrayList_ensureCapacity,
   TR::java_util_HashMap_resize,
   TR::java_util_HashMapHashIterator_nextNode,
   TR::java_util_Hashtable_clone,
   TR::java_util_Hashtable_putAll,
   TR::unknownMethod
   };

bool
J9::MethodSymbol::safeToSkipCheckCasts()
   {
   TR::RecognizedMethod methodId = self()->getRecognizedMethod();
   if (methodId == TR::unknownMethod)
      return false;

   for (int i = 0; canSkipCheckCasts[i] != TR::unknownMethod; ++i)
      if (canSkipCheckCasts[i] == methodId)
         return true;

   return false;
   }


// Which recognized methods are known to have no valid array store checks
//
static TR::RecognizedMethod canSkipArrayStoreChecks[] =
   {
   TR::java_lang_invoke_CollectHandle_invokeExact,
   TR::java_util_ArrayList_add,
   TR::java_util_ArrayList_ensureCapacity,
   TR::java_util_ArrayList_get,
   TR::java_util_ArrayList_remove,
   TR::java_util_ArrayList_set,
   TR::java_util_Hashtable_get,
   TR::java_util_Hashtable_put,
   TR::java_util_Hashtable_clone,
   TR::java_util_Hashtable_putAll,
   TR::java_util_Hashtable_rehash,
   TR::java_util_Hashtable_remove,
   TR::java_util_Hashtable_contains,
   TR::java_util_Hashtable_getEntry,
   TR::java_util_Hashtable_getEnumeration,
   TR::java_util_Hashtable_elements,
   TR::java_util_HashtableHashEnumerator_hasMoreElements,
   TR::java_util_HashtableHashEnumerator_nextElement,
   TR::java_util_HashMap_rehash,
   TR::java_util_HashMap_analyzeMap,
   TR::java_util_HashMap_calculateCapacity,
   TR::java_util_HashMap_findNullKeyEntry,
   TR::java_util_HashMap_get,
   TR::java_util_HashMap_findNonNullKeyEntry,
   TR::java_util_HashMap_putImpl,
   TR::java_util_HashMap_resize,
   TR::java_util_HashMapHashIterator_nextNode,
   TR::java_util_Vector_addElement,
   TR::java_util_Vector_contains,
   TR::java_util_Vector_subList,
   TR::java_util_concurrent_ConcurrentHashMap_addCount,
   TR::java_util_concurrent_ConcurrentHashMap_tryPresize,
   TR::java_util_concurrent_ConcurrentHashMap_transfer,
   TR::java_util_concurrent_ConcurrentHashMap_fullAddCount,
   TR::java_util_concurrent_ConcurrentHashMap_helpTransfer,
   TR::java_util_concurrent_ConcurrentHashMap_initTable,
   TR::java_util_concurrent_ConcurrentHashMap_tabAt,
   TR::java_util_concurrent_ConcurrentHashMap_casTabAt,
   TR::java_util_concurrent_ConcurrentHashMap_setTabAt,
   TR::java_util_concurrent_ConcurrentHashMap_TreeBin_lockRoot,
   TR::java_util_concurrent_ConcurrentHashMap_TreeBin_contendedLock,
   TR::java_util_concurrent_ConcurrentHashMap_TreeBin_find,
   TR::java_util_TreeMap_all,
   TR::java_util_EnumMap_put,
   TR::java_util_EnumMap_typeCheck,
   TR::java_util_EnumMap__init_,
   TR::java_util_EnumMap__nec_,
   TR::java_util_HashMap_all,
   TR::java_util_ArrayList_all,
   TR::java_util_Hashtable_all,
   TR::java_util_concurrent_ConcurrentHashMap_all,
   TR::java_util_Vector_all,
   TR::unknownMethod
   };

bool
J9::MethodSymbol::safeToSkipArrayStoreChecks()
   {
   TR::RecognizedMethod methodId = self()->getRecognizedMethod();
   if (methodId == TR::unknownMethod)
      return false;

   for (int i = 0; canSkipArrayStoreChecks[i] != TR::unknownMethod; ++i)
      if (canSkipArrayStoreChecks[i] == methodId)
         return true;

   return false;
   }

static TR::RecognizedMethod canSkipNonNullableArrayNullStoreCheck[] = {
   TR::java_lang_invoke_CollectHandle_invokeExact,
   TR::java_util_ArrayList_add,
   TR::java_util_ArrayList_ensureCapacity,
   TR::java_util_ArrayList_remove,
   TR::java_util_ArrayList_set,
   TR::java_util_Hashtable_get,
   TR::java_util_Hashtable_put,
   TR::java_util_Hashtable_clone,
   TR::java_util_Hashtable_putAll,
   TR::java_util_Hashtable_rehash,
   TR::java_util_Hashtable_remove,
   TR::java_util_Hashtable_contains,
   TR::java_util_Hashtable_getEntry,
   TR::java_util_Hashtable_getEnumeration,
   TR::java_util_Hashtable_elements,
   TR::java_util_HashtableHashEnumerator_hasMoreElements,
   TR::java_util_HashtableHashEnumerator_nextElement,
   TR::java_util_HashMap_rehash,
   TR::java_util_HashMap_analyzeMap,
   TR::java_util_HashMap_calculateCapacity,
   TR::java_util_HashMap_findNullKeyEntry,
   TR::java_util_HashMap_get,
   TR::java_util_HashMap_getNode,
   TR::java_util_HashMap_putImpl,
   TR::java_util_HashMap_findNonNullKeyEntry,
   TR::java_util_HashMap_resize,
   TR::java_util_HashMap_prepareArray,
   TR::java_util_HashMap_keysToArray,
   TR::java_util_HashMap_valuesToArray,
   TR::java_util_HashMapHashIterator_nextNode,
   TR::java_util_HashMapHashIterator_init,
   TR::java_util_Vector_addElement,
   TR::java_util_Vector_contains,
   TR::java_util_Vector_subList,
   TR::java_util_concurrent_ConcurrentHashMap_addCount,
   TR::java_util_concurrent_ConcurrentHashMap_tryPresize,
   TR::java_util_concurrent_ConcurrentHashMap_transfer,
   TR::java_util_concurrent_ConcurrentHashMap_fullAddCount,
   TR::java_util_concurrent_ConcurrentHashMap_helpTransfer,
   TR::java_util_concurrent_ConcurrentHashMap_initTable,
   TR::java_util_concurrent_ConcurrentHashMap_tabAt,
   TR::java_util_concurrent_ConcurrentHashMap_casTabAt,
   TR::java_util_concurrent_ConcurrentHashMap_setTabAt,
   TR::java_util_concurrent_ConcurrentHashMap_TreeBin_lockRoot,
   TR::java_util_concurrent_ConcurrentHashMap_TreeBin_contendedLock,
   TR::java_util_concurrent_ConcurrentHashMap_TreeBin_find,
   TR::java_util_TreeMap_all,
   TR::java_util_EnumMap_put,
   TR::java_util_EnumMap_typeCheck,
   TR::java_util_EnumMap__init_,
   // TR::java_util_EnumMap__nec_, // Disable it for now because EnumMap.toArray explicitly stores null into array element
   TR::java_util_HashMap_all,
   // TR::java_util_ArrayList_all, // Disable it for now because ArrayList.toArray explicitly stores null into array element
   TR::java_util_Hashtable_all,
   // TR::java_util_concurrent_ConcurrentHashMap_all, // Disable it for now because ConcurrentHashMap.toArray explicitly stores null into array element
   // TR::java_util_Vector_all, // Disable it for now because Vector.toArray explicitly stores null into array element

   // The following list is identified after running sanity.functional tests
   // TR::java_util_AbstractCollection_all, // Disable it for now because AbstractCollection.toArray explicitly stores null into array element
   // TR::java_util_ArrayDeque_all, // Disable it for now because ArrayDeque.toArray explicitly stores null into array element
   TR::java_util_ComparableTimSort_all,
   // TR::java_util_IdentityHashMap_all, // Disable it for now because IdentityHashMap.toArray explicitly stores null into array element
   // TR::java_util_ImmutableCollections_all, // Disable it for now because ImmutableCollections.toArray explicitly stores null into array element
   TR::java_util_LinkedHashMap_all,
   // TR::java_util_LinkedList_all, // Disable it for now because LinkedList.toArray explicitly stores null into array element
   TR::java_util_Map_all,
   TR::java_util_regex_Pattern_all,
   TR::java_util_stream_Nodes_all,
   TR::java_util_TimSort_all,
   TR::java_util_WeakHashMap_all,

   TR::java_lang_Class_all,

   TR::unknownMethod
};

bool
J9::MethodSymbol::safeToSkipNonNullableArrayNullStoreCheck()
   {
   TR::RecognizedMethod methodId = self()->getRecognizedMethod();
   if (methodId == TR::unknownMethod)
      return false;

   for (int i = 0; canSkipNonNullableArrayNullStoreCheck[i] != TR::unknownMethod; ++i)
      if (canSkipNonNullableArrayNullStoreCheck[i] == methodId)
         return true;

   return false;
   }

static TR::RecognizedMethod canSkipFlattenableArrayElementNonHelperCall[] = {
   TR::java_lang_invoke_CollectHandle_invokeExact,
   TR::java_util_ArrayList_add,
   TR::java_util_ArrayList_ensureCapacity,
   TR::java_util_ArrayList_remove,
   TR::java_util_ArrayList_set,
   TR::java_util_Hashtable_get,
   TR::java_util_Hashtable_put,
   TR::java_util_Hashtable_clone,
   TR::java_util_Hashtable_putAll,
   TR::java_util_Hashtable_rehash,
   TR::java_util_Hashtable_remove,
   TR::java_util_Hashtable_contains,
   TR::java_util_Hashtable_getEntry,
   TR::java_util_Hashtable_getEnumeration,
   TR::java_util_Hashtable_elements,
   TR::java_util_HashtableHashEnumerator_hasMoreElements,
   TR::java_util_HashtableHashEnumerator_nextElement,
   TR::java_util_HashMap_rehash,
   TR::java_util_HashMap_analyzeMap,
   TR::java_util_HashMap_calculateCapacity,
   TR::java_util_HashMap_findNullKeyEntry,
   TR::java_util_HashMap_get,
   TR::java_util_HashMap_getNode,
   TR::java_util_HashMap_putImpl,
   TR::java_util_HashMap_findNonNullKeyEntry,
   TR::java_util_HashMap_resize,
   TR::java_util_HashMap_prepareArray,
   TR::java_util_HashMap_keysToArray,
   TR::java_util_HashMap_valuesToArray,
   TR::java_util_HashMapHashIterator_nextNode,
   TR::java_util_HashMapHashIterator_init,
   TR::java_util_Vector_addElement,
   TR::java_util_Vector_contains,
   TR::java_util_Vector_subList,
   TR::java_util_concurrent_ConcurrentHashMap_addCount,
   TR::java_util_concurrent_ConcurrentHashMap_tryPresize,
   TR::java_util_concurrent_ConcurrentHashMap_transfer,
   TR::java_util_concurrent_ConcurrentHashMap_fullAddCount,
   TR::java_util_concurrent_ConcurrentHashMap_helpTransfer,
   TR::java_util_concurrent_ConcurrentHashMap_initTable,
   TR::java_util_concurrent_ConcurrentHashMap_tabAt,
   TR::java_util_concurrent_ConcurrentHashMap_casTabAt,
   TR::java_util_concurrent_ConcurrentHashMap_setTabAt,
   TR::java_util_concurrent_ConcurrentHashMap_TreeBin_lockRoot,
   TR::java_util_concurrent_ConcurrentHashMap_TreeBin_contendedLock,
   TR::java_util_concurrent_ConcurrentHashMap_TreeBin_find,
   TR::java_util_TreeMap_all,
   TR::java_util_EnumMap_put,
   TR::java_util_EnumMap_typeCheck,
   TR::java_util_EnumMap__init_,
   // TR::java_util_EnumMap__nec_, // Disable it for now because EnumMap.toArray might deal with null-restricted arrays which are flattenable
   TR::java_util_HashMap_all,
   // TR::java_util_ArrayList_all, // Disable it for now because ArrayList.toArray might deal with null-restricted arrays which are flattenable
   TR::java_util_Hashtable_all,
   // TR::java_util_concurrent_ConcurrentHashMap_all, // Disable it for now because ConcurrentHashMap.toArray might deal with null-restricted arrays which are flattenable
   // TR::java_util_Vector_all, // Disable it for now because Vector.toArray might deal with null-restricted arrays which are flattenable

   // The following list is identified after running sanity.functional tests
   // TR::java_util_AbstractCollection_all, // Disable it for now because AbstractCollection.toArray might deal with null-restricted arrays which are flattenable
   // TR::java_util_ArrayDeque_all, // Disable it for now because ArrayDeque.toArray might deal with null-restricted arrays which are flattenable
   // TR::java_util_IdentityHashMap_all, // Disable it for now because IdentityHashMap.toArray might deal with null-restricted arrays which are flattenable
   // TR::java_util_ImmutableCollections_all, // Disable it for now because ImmutableCollections.toArray might deal with null-restricted arrays which are flattenable
   // TR::java_util_LinkedList_all, // Disable it for now because LinkedList.toArray might deal with null-restricted arrays which are flattenable
   TR::java_util_Map_all,
   TR::java_util_regex_Pattern_all,
   TR::java_util_stream_Nodes_all,
   TR::java_util_WeakHashMap_all,

   TR::java_lang_Class_all,

   TR::unknownMethod
};

bool
J9::MethodSymbol::safeToSkipFlattenableArrayElementNonHelperCall()
   {
   TR::RecognizedMethod methodId = self()->getRecognizedMethod();
   if (methodId == TR::unknownMethod)
      return false;

   for (int i = 0; canSkipFlattenableArrayElementNonHelperCall[i] != TR::unknownMethod; ++i)
      if (canSkipFlattenableArrayElementNonHelperCall[i] == methodId)
         return true;

   return false;
   }

// Which recognized methods are known to require no checking when lowering to TR::arraycopy
//
static TR::RecognizedMethod canSkipChecksOnArrayCopies[] =
   {
   // NOTE!! add methods whose checks can be skipped by sov library to the beginning of the list (see stopMethod below)
   //TR::java_util_ArrayList_ensureCapacity,
   //TR::java_util_ArrayList_remove,   /* ArrayList is NOT synchronized and therefore it's unsafe to remove checks! */
   TR::java_lang_Character_toLowerCase,
   TR::java_lang_String_concat,
   TR::java_lang_String_replace,
   TR::java_lang_String_toLowerCase,
   TR::java_lang_String_toUpperCase,
   TR::java_lang_String_toLowerCaseCore,
   TR::java_lang_String_toUpperCaseCore,
   TR::java_lang_String_toCharArray,
   TR::java_lang_StringBuffer_append,
   TR::java_lang_StringBuffer_ensureCapacityImpl,
   TR::java_lang_String_init_int_String_int_String_String,
   TR::java_lang_String_init_int_int_char_boolean,
   TR::java_util_HashMap_resize,
   TR::java_util_HashMapHashIterator_nextNode,
   TR::unknownMethod
   };

// These are logically the same as the above (canSkipChecksOnArrayCopies).
// I've broken them out so that we can selectively disable this group
// with the TR_DisableExtraCopyOfOpts env var. If/when these prove to be safe
// they can be migrated into the main group.
static TR::RecognizedMethod extraCanSkipChecksOnArrayCopies[] =
   {
   TR::java_util_Arrays_copyOf_byte,
   TR::java_util_Arrays_copyOf_short,
   TR::java_util_Arrays_copyOf_char,
   TR::java_util_Arrays_copyOf_boolean,
   TR::java_util_Arrays_copyOf_int,
   TR::java_util_Arrays_copyOf_long,
   TR::java_util_Arrays_copyOf_float,
   TR::java_util_Arrays_copyOf_double,
   TR::java_util_Arrays_copyOf_Object1,
   TR::unknownMethod
   };

static TR::RecognizedMethod stringCanSkipChecksOnArrayCopies[] =
   {
   TR::java_lang_String_getBytes,
   TR::java_lang_String_getBytes_subString,
   TR::java_lang_StringLatin1_toLowerCase,
   TR::java_lang_StringLatin1_toUpperCase,
   TR::java_lang_StringUTF16_toLowerCase,
   TR::java_lang_StringUTF16_toUpperCase,
   TR::unknownMethod
   };

bool
J9::MethodSymbol::safeToSkipChecksOnArrayCopies()
   {
   TR::RecognizedMethod methodId = self()->getRecognizedMethod();
   if (methodId == TR::unknownMethod)
      return false;

   for (int i = 0; canSkipChecksOnArrayCopies[i] != TR::unknownMethod; ++i)
      if (canSkipChecksOnArrayCopies[i] == methodId)
         return true;

   static char *disableExtraCopyOfOpts=feGetEnv("TR_DisableExtraCopyOfOpts");
   if (!disableExtraCopyOfOpts)
      for (int i = 0; extraCanSkipChecksOnArrayCopies[i] != TR::unknownMethod; ++i)
         if (extraCanSkipChecksOnArrayCopies[i] == methodId)
            return true;

   static char *disableStringChkSkips = feGetEnv("TR_DisableStringChkSkips");
   if (!disableStringChkSkips)
      {
      for (int i = 0; stringCanSkipChecksOnArrayCopies[i] != TR::unknownMethod; ++i)
         {
         if (stringCanSkipChecksOnArrayCopies[i] == methodId)
            {
            return true;
            }
         }
      }

   return false;
   }

// Which recognized methods are known to not require zero initialization of arrays
//
static TR::RecognizedMethod canSkipZeroInitializationOnNewarrays[] =
   {
   TR::java_lang_Character_toLowerCase,
   TR::java_lang_String_init,
   TR::java_lang_String_init_int_int_char_boolean,
   TR::java_lang_String_concat,
   TR::java_lang_String_replace,
   TR::java_lang_String_toCharArray,
   TR::java_lang_String_toLowerCase,
   //TR::java_lang_String_toUpperCase,
   TR::java_lang_String_toLowerCaseCore,
   //TR::java_lang_String_toUpperCaseCore,
   TR::java_lang_String_split_str_int,
   TR::java_math_BigDecimal_toString,
   TR::java_math_BigInteger_init_long,
#ifdef OPENJ9_BUILD
   TR::java_math_BigInteger_toByteArray,
#endif // OPENJ9_BUILD
   TR::java_math_BigInteger_stripLeadingZeroBytes1,
   TR::java_math_BigInteger_stripLeadingZeroBytes2,
   TR::java_lang_Integer_toString,
   TR::java_lang_Long_toString,
   TR::java_lang_StringCoding_encode,
   TR::java_lang_StringCoding_decode,
   TR::java_lang_StringCoding_StringEncoder_encode,
   TR::java_lang_StringCoding_StringDecoder_decode,
   TR::sun_misc_Unsafe_allocateUninitializedArray0,
   //TR::java_lang_StringBuilder_ensureCapacityImpl,
   //TR::java_lang_StringBuffer_ensureCapacityImpl,
   //TR::java_util_Arrays_copyOf,
   TR::java_io_Writer_write_lStringII,
   TR::java_io_Writer_write_I,
   TR::java_util_regex_Matcher_init,
   TR::java_util_regex_Matcher_usePattern,
   TR::unknownMethod
   };


bool
J9::MethodSymbol::safeToSkipZeroInitializationOnNewarrays()
   {
   TR::RecognizedMethod methodId = self()->getRecognizedMethod();
   if (methodId == TR::unknownMethod)
      return false;

   for (int i = 0; canSkipZeroInitializationOnNewarrays[i] != TR::unknownMethod; ++i)
      if (canSkipZeroInitializationOnNewarrays[i] == methodId)
         return true;

   return false;
   }
