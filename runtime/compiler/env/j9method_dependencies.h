
/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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
   static W java_lang_Math_Dependencies[] =
      {
      {w(TR::java_lang_Math_acos, TR::java_lang_StrictMath_acos, "java/lang/StrictMath")},
      {w(TR::java_lang_Math_asin, TR::java_lang_StrictMath_asin, "java/lang/StrictMath")},
      {w(TR::java_lang_Math_atan, TR::java_lang_StrictMath_atan, "java/lang/StrictMath")},
      {w(TR::java_lang_Math_atan2, TR::java_lang_StrictMath_atan2, "java/lang/StrictMath")},
      {w(TR::java_lang_Math_cbrt, TR::java_lang_StrictMath_cbrt, "java/lang/StrictMath")},
      {w(TR::java_lang_Math_ceil, TR::java_lang_StrictMath_ceil, "java/lang/StrictMath")},
      {w(TR::java_lang_Math_copySign_F, TR::java_lang_Float_floatToRawIntBits, "java/lang/Float")},
      {w(TR::java_lang_Math_copySign_F, TR::java_lang_Float_intBitsToFloat, "java/lang/Float")},
      {w(TR::java_lang_Math_copySign_D, TR::java_lang_Double_doubleToRawLongBits, "java/lang/Double")},
      {w(TR::java_lang_Math_copySign_D, TR::java_lang_Double_longBitsToDouble, "java/lang/Double")},
      {w(TR::java_lang_Math_cos, TR::java_lang_StrictMath_cos, "java/lang/StrictMath")},
      {w(TR::java_lang_Math_cosh, TR::java_lang_StrictMath_cosh, "java/lang/StrictMath")},
      {w(TR::java_lang_Math_exp, TR::java_lang_StrictMath_exp, "java/lang/StrictMath")},
      {w(TR::java_lang_Math_expm1, TR::java_lang_StrictMath_expm1, "java/lang/StrictMath")},
      {w(TR::java_lang_Math_floor, TR::java_lang_StrictMath_floor, "java/lang/StrictMath")},
      {w(TR::java_lang_Math_hypot, TR::java_lang_StrictMath_hypot, "java/lang/StrictMath")},
      {w(TR::java_lang_Math_IEEEremainder, TR::java_lang_StrictMath_IEEEremainder, "java/lang/StrictMath")},
      {w(TR::java_lang_Math_log, TR::java_lang_StrictMath_log, "java/lang/StrictMath")},
      {w(TR::java_lang_Math_log10, TR::java_lang_StrictMath_log10, "java/lang/StrictMath")},
      {w(TR::java_lang_Math_log1p, TR::java_lang_StrictMath_log1p, "java/lang/StrictMath")},
      {w(TR::java_lang_Math_max_F, TR::java_lang_Float_floatToRawIntBits, "java/lang/Float")},
      {w(TR::java_lang_Math_max_D, TR::java_lang_Double_doubleToRawLongBits, "java/lang/Double")},
      {w(TR::java_lang_Math_min_F, TR::java_lang_Float_floatToRawIntBits, "java/lang/Float")},
      {w(TR::java_lang_Math_min_D, TR::java_lang_Double_doubleToRawLongBits, "java/lang/Double")},
      {w(TR::java_lang_Math_nextAfter_F, TR::java_lang_Float_floatToRawIntBits, "java/lang/Float")},
      {w(TR::java_lang_Math_nextAfter_F, TR::java_lang_Float_intBitsToFloat, "java/lang/Float")},
      {w(TR::java_lang_Math_nextAfter_D, TR::java_lang_Double_doubleToRawLongBits, "java/lang/Double")},
      {w(TR::java_lang_Math_nextAfter_D, TR::java_lang_Double_longBitsToDouble, "java/lang/Double")},
      {w(TR::java_lang_Math_pow, TR::java_lang_StrictMath_pow, "java/lang/StrictMath")},
      {w(TR::java_lang_Math_rint, TR::java_lang_StrictMath_rint, "java/lang/StrictMath")},
      {w(TR::java_lang_Math_round_F, TR::java_lang_Float_floatToRawIntBits, "java/lang/Float")},
      {w(TR::java_lang_Math_round_D, TR::java_lang_Double_doubleToRawLongBits, "java/lang/Double")},
      {w(TR::java_lang_Math_scalb_F, TR::java_lang_Math_min_I, "java/lang/Math")},
      {w(TR::java_lang_Math_scalb_F, TR::java_lang_Math_max_I, "java/lang/Math")},
      {w(TR::java_lang_Math_sin, TR::java_lang_StrictMath_sin, "java/lang/StrictMath")},
      {w(TR::java_lang_Math_sinh, TR::java_lang_StrictMath_sinh, "java/lang/StrictMath")},
      {w(TR::java_lang_Math_sqrt, TR::java_lang_StrictMath_sqrt, "java/lang/StrictMath")},
      {w(TR::java_lang_Math_tan, TR::java_lang_StrictMath_tan, "java/lang/StrictMath")},
      {w(TR::java_lang_Math_tanh, TR::java_lang_StrictMath_tanh, "java/lang/StrictMath")},
      {  TR::unknownMethod}
      };

   static W java_lang_Long_Dependencies[] =
      {
      {wVer(TR::java_lang_Long_highestOneBit, TR::java_lang_Long_numberOfLeadingZeros, "java/lang/Long", 11, AllFutureJavaVer)},
      {wVer(TR::java_lang_Long_numberOfLeadingZeros, TR::java_lang_Integer_numberOfLeadingZeros, "java/lang/Integer", 11, AllFutureJavaVer)},
      {wVer(TR::java_lang_Long_numberOfTrailingZeros, TR::java_lang_Integer_numberOfTrailingZeros, "java/lang/Integer", 12, AllFutureJavaVer)},
      {  TR::unknownMethod}
      };

   static W java_io_Writer_Dependencies[] =
      {
      {w(TR::java_io_Writer_write_lStringII, TR::java_lang_String_getChars_charArray, "java/lang/String")},
      {  TR::unknownMethod}
      };

   static W java_lang_Class_Dependencies[] =
      {
      {w(TR::java_lang_Class_newInstance, TR::java_lang_ClassLoader_getStackClassLoader, "java/lang/ClassLoader")},
      {w(TR::java_lang_Class_newInstance, TR::java_lang_Class_newInstanceImpl, "java/lang/J9VMInternals")},
      {w(TR::java_lang_Class_isInterface, TR::java_lang_Class_isArray, "java/lang/Class")},
      {w(TR::java_lang_Class_isInterface, TR::java_lang_Class_getModifiersImpl, "java/lang/Class")},
      {  TR::unknownMethod}
      };

   static W java_lang_Float_Dependencies[] =
      {
      {w(TR::java_lang_Float_floatToIntBits, TR::java_lang_Float_floatToRawIntBits, "java/lang/Float")},
      {  TR::unknownMethod}
      };

   static W sun_misc_Unsafe_Dependencies[] =
      {
      {wVer(TR::sun_misc_Unsafe_putInt_jlObjectII_V, TR::sun_misc_Unsafe_putInt_jlObjectJI_V, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::sun_misc_Unsafe_getAndAddInt, TR::sun_misc_Unsafe_getIntVolatile_jlObjectJ_I, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::sun_misc_Unsafe_getAndAddInt, TR::sun_misc_Unsafe_compareAndSwapInt_jlObjectJII_Z, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::sun_misc_Unsafe_getAndSetInt, TR::sun_misc_Unsafe_getIntVolatile_jlObjectJ_I, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::sun_misc_Unsafe_getAndSetInt, TR::sun_misc_Unsafe_compareAndSwapInt_jlObjectJII_Z, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::sun_misc_Unsafe_getAndAddLong, TR::sun_misc_Unsafe_getLongVolatile_jlObjectJ_J, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::sun_misc_Unsafe_getAndAddLong, TR::sun_misc_Unsafe_compareAndSwapLong_jlObjectJJJ_Z, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::sun_misc_Unsafe_getAndSetLong, TR::sun_misc_Unsafe_getLongVolatile_jlObjectJ_J, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::sun_misc_Unsafe_getAndSetLong, TR::sun_misc_Unsafe_compareAndSwapLong_jlObjectJJJ_Z, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::sun_misc_Unsafe_putBoolean_jlObjectJZ_V, TR::sun_misc_Unsafe_putBoolean_jlObjectJZ_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putByte_jlObjectJB_V, TR::sun_misc_Unsafe_putByte_jlObjectJB_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putChar_jlObjectJC_V, TR::sun_misc_Unsafe_putChar_jlObjectJC_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putShort_jlObjectJS_V, TR::sun_misc_Unsafe_putShort_jlObjectJS_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putInt_jlObjectJI_V, TR::sun_misc_Unsafe_putInt_jlObjectJI_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putLong_jlObjectJJ_V, TR::sun_misc_Unsafe_putLong_jlObjectJJ_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putFloat_jlObjectJF_V, TR::sun_misc_Unsafe_putFloat_jlObjectJF_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putDouble_jlObjectJD_V, TR::sun_misc_Unsafe_putDouble_jlObjectJD_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putObject_jlObjectJjlObject_V, TR::sun_misc_Unsafe_putObject_jlObjectJjlObject_V, "jdk/internal/misc/Unsafe", 9, 11)},
      {wVer(TR::sun_misc_Unsafe_putBooleanVolatile_jlObjectJZ_V, TR::sun_misc_Unsafe_putBooleanVolatile_jlObjectJZ_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putByteVolatile_jlObjectJB_V, TR::sun_misc_Unsafe_putByteVolatile_jlObjectJB_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putCharVolatile_jlObjectJC_V, TR::sun_misc_Unsafe_putCharVolatile_jlObjectJC_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putShortVolatile_jlObjectJS_V, TR::sun_misc_Unsafe_putShortVolatile_jlObjectJS_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putIntVolatile_jlObjectJI_V, TR::sun_misc_Unsafe_putIntVolatile_jlObjectJI_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putLongVolatile_jlObjectJJ_V, TR::sun_misc_Unsafe_putLongVolatile_jlObjectJJ_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putFloatVolatile_jlObjectJF_V, TR::sun_misc_Unsafe_putFloatVolatile_jlObjectJF_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putDoubleVolatile_jlObjectJD_V, TR::sun_misc_Unsafe_putDoubleVolatile_jlObjectJD_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putObjectVolatile_jlObjectJjlObject_V, TR::sun_misc_Unsafe_putObjectVolatile_jlObjectJjlObject_V, "jdk/internal/misc/Unsafe", 9, 11)},
      {wVer(TR::sun_misc_Unsafe_getBoolean_jlObjectJ_Z, TR::sun_misc_Unsafe_getBoolean_jlObjectJ_Z, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getByte_jlObjectJ_B, TR::sun_misc_Unsafe_getByte_jlObjectJ_B, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getChar_jlObjectJ_C, TR::sun_misc_Unsafe_getChar_jlObjectJ_C, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getShort_jlObjectJ_S, TR::sun_misc_Unsafe_getShort_jlObjectJ_S, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getInt_jlObjectJ_I, TR::sun_misc_Unsafe_getInt_jlObjectJ_I, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getLong_jlObjectJ_J, TR::sun_misc_Unsafe_getLong_jlObjectJ_J, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getFloat_jlObjectJ_F, TR::sun_misc_Unsafe_getFloat_jlObjectJ_F, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getDouble_jlObjectJ_D, TR::sun_misc_Unsafe_getDouble_jlObjectJ_D, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getObject_jlObjectJ_jlObject, TR::sun_misc_Unsafe_getObject_jlObjectJ_jlObject, "jdk/internal/misc/Unsafe", 9, 11)},
      {wVer(TR::sun_misc_Unsafe_getBooleanVolatile_jlObjectJ_Z, TR::sun_misc_Unsafe_getBooleanVolatile_jlObjectJ_Z, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getByteVolatile_jlObjectJ_B, TR::sun_misc_Unsafe_getByteVolatile_jlObjectJ_B, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getCharVolatile_jlObjectJ_C, TR::sun_misc_Unsafe_getCharVolatile_jlObjectJ_C, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getShortVolatile_jlObjectJ_S, TR::sun_misc_Unsafe_getShortVolatile_jlObjectJ_S, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getIntVolatile_jlObjectJ_I, TR::sun_misc_Unsafe_getIntVolatile_jlObjectJ_I, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getLongVolatile_jlObjectJ_J, TR::sun_misc_Unsafe_getLongVolatile_jlObjectJ_J, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getFloatVolatile_jlObjectJ_F, TR::sun_misc_Unsafe_getFloatVolatile_jlObjectJ_F, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getDoubleVolatile_jlObjectJ_D, TR::sun_misc_Unsafe_getDoubleVolatile_jlObjectJ_D, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getObjectVolatile_jlObjectJ_jlObject, TR::sun_misc_Unsafe_getObjectVolatile_jlObjectJ_jlObject, "jdk/internal/misc/Unsafe", 9, 11)},
      {wVer(TR::sun_misc_Unsafe_putByte_JB_V, TR::sun_misc_Unsafe_putByte_JB_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putShort_JS_V, TR::sun_misc_Unsafe_putShort_JS_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putChar_JC_V, TR::sun_misc_Unsafe_putChar_JC_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putInt_JI_V, TR::sun_misc_Unsafe_putInt_JI_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putLong_JJ_V, TR::sun_misc_Unsafe_putLong_JJ_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putFloat_JF_V, TR::sun_misc_Unsafe_putFloat_JF_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putDouble_JD_V, TR::sun_misc_Unsafe_putDouble_JD_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putAddress_JJ_V, TR::sun_misc_Unsafe_putAddress_JJ_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getByte_J_B, TR::sun_misc_Unsafe_getByte_J_B, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getShort_J_S, TR::sun_misc_Unsafe_getShort_J_S, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getChar_J_C, TR::sun_misc_Unsafe_getChar_J_C, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getInt_J_I, TR::sun_misc_Unsafe_getInt_J_I, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getLong_J_J, TR::sun_misc_Unsafe_getLong_J_J, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getFloat_J_F, TR::sun_misc_Unsafe_getFloat_J_F, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getDouble_J_D, TR::sun_misc_Unsafe_getDouble_J_D, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getAddress_J_J, TR::sun_misc_Unsafe_getAddress_J_J, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_compareAndSwapInt_jlObjectJII_Z, TR::sun_misc_Unsafe_compareAndSwapInt_jlObjectJII_Z, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_compareAndSwapLong_jlObjectJJJ_Z, TR::sun_misc_Unsafe_compareAndSwapLong_jlObjectJJJ_Z, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_compareAndSwapObject_jlObjectJjlObjectjlObject_Z, TR::sun_misc_Unsafe_compareAndSwapObject_jlObjectJjlObjectjlObject_Z, "jdk/internal/misc/Unsafe", 9, 11)},
      {wVer(TR::sun_misc_Unsafe_staticFieldOffset, TR::sun_misc_Unsafe_staticFieldOffset, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_objectFieldOffset, TR::sun_misc_Unsafe_objectFieldOffset, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getAndAddInt, TR::sun_misc_Unsafe_getAndAddInt, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getAndSetInt, TR::sun_misc_Unsafe_getAndSetInt, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getAndAddLong, TR::sun_misc_Unsafe_getAndAddLong, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getAndSetLong, TR::sun_misc_Unsafe_getAndSetLong, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putOrderedInt_jlObjectJI_V, TR::sun_misc_Unsafe_putIntVolatile_jlObjectJI_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putOrderedLong_jlObjectJJ_V, TR::sun_misc_Unsafe_putLongVolatile_jlObjectJJ_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putOrderedObject_jlObjectJjlObject_V, TR::sun_misc_Unsafe_putObjectVolatile_jlObjectJjlObject_V, "jdk/internal/misc/Unsafe", 9, 11)},
      {wVer(TR::sun_misc_Unsafe_copyMemory, TR::sun_misc_Unsafe_copyMemory, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_setMemory, TR::sun_misc_Unsafe_setMemory, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_loadFence, TR::sun_misc_Unsafe_loadFence, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_storeFence, TR::sun_misc_Unsafe_storeFence, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_fullFence, TR::sun_misc_Unsafe_fullFence, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_ensureClassInitialized, TR::sun_misc_Unsafe_ensureClassInitialized, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putObject_jlObjectJjlObject_V, TR::sun_misc_Unsafe_putObject_jlObjectJjlObject_V, "jdk/internal/misc/Unsafe", 12, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putObjectVolatile_jlObjectJjlObject_V, TR::sun_misc_Unsafe_putObjectVolatile_jlObjectJjlObject_V, "jdk/internal/misc/Unsafe", 12, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getObject_jlObjectJ_jlObject, TR::sun_misc_Unsafe_getObject_jlObjectJ_jlObject, "jdk/internal/misc/Unsafe", 12, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getObjectVolatile_jlObjectJ_jlObject, TR::sun_misc_Unsafe_getObjectVolatile_jlObjectJ_jlObject, "jdk/internal/misc/Unsafe", 12, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_compareAndSwapObject_jlObjectJjlObjectjlObject_Z, TR::sun_misc_Unsafe_compareAndSwapObject_jlObjectJjlObjectjlObject_Z, "jdk/internal/misc/Unsafe", 12, AllFutureJavaVer)},
      {  TR::unknownMethod}
      };

   static W java_lang_Double_Dependencies[] =
      {
      {w(TR::java_lang_Double_doubleToLongBits, TR::java_lang_Double_doubleToRawLongBits, "java/lang/Double")},
      {  TR::unknownMethod}
      };

   static W java_lang_Object_Dependencies[] =
      {
      {w(TR::java_lang_Object_clone, TR::java_lang_J9VMInternals_primitiveClone, "java/lang/J9VMInternals")},
      {  TR::unknownMethod}
      };

   static W java_lang_String_Dependencies[] =
      {
      {w(TR::java_lang_String_trim, TR::java_lang_String_lengthInternal, "java/lang/String")},
      {w(TR::java_lang_String_trim, TR::com_ibm_jit_JITHelpers_getByteFromArrayByIndex, "com/ibm/jit/JITHelpers")},
      {w(TR::java_lang_String_trim, TR::com_ibm_jit_JITHelpers_byteToCharUnsigned, "com/ibm/jit/JITHelpers")},
      {w(TR::java_lang_String_trim, TR::java_lang_String_charAtInternal_I, "java/lang/String")},
      {w(TR::java_lang_String_charAt, TR::java_lang_String_lengthInternal, "java/lang/String")},
      {w(TR::java_lang_String_charAt, TR::com_ibm_jit_JITHelpers_getByteFromArrayByIndex, "com/ibm/jit/JITHelpers")},
      {w(TR::java_lang_String_charAt, TR::com_ibm_jit_JITHelpers_byteToCharUnsigned, "com/ibm/jit/JITHelpers")},
      {w(TR::java_lang_String_charAtInternal_I, TR::com_ibm_jit_JITHelpers_getByteFromArrayByIndex, "com/ibm/jit/JITHelpers")},
      {w(TR::java_lang_String_charAtInternal_I, TR::com_ibm_jit_JITHelpers_byteToCharUnsigned, "com/ibm/jit/JITHelpers")},
      {wVer(TR::java_lang_String_charAtInternal_IB, TR::com_ibm_jit_JITHelpers_getByteFromArrayByIndex, "com/ibm/jit/JITHelpers", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_String_charAtInternal_IB, TR::com_ibm_jit_JITHelpers_byteToCharUnsigned, "com/ibm/jit/JITHelpers", AllPastJavaVer, 8)},
      {w(TR::java_lang_String_concat, TR::java_lang_String_lengthInternal, "java/lang/String")},
      {wVer(TR::java_lang_String_concat, TR::java_lang_String_compressedArrayCopy_CICII, "java/lang/String", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_String_concat, TR::java_lang_System_arraycopy, "java/lang/System", AllPastJavaVer, 8)},
      {w(TR::java_lang_String_compressedArrayCopy_BIBII, TR::com_ibm_jit_JITHelpers_getByteFromArrayByIndex, "com/ibm/jit/JITHelpers")},
      {w(TR::java_lang_String_compressedArrayCopy_BIBII, TR::com_ibm_jit_JITHelpers_putByteInArrayByIndex, "com/ibm/jit/JITHelpers")},
      {w(TR::java_lang_String_compressedArrayCopy_BICII, TR::com_ibm_jit_JITHelpers_getByteFromArrayByIndex, "com/ibm/jit/JITHelpers")},
      {w(TR::java_lang_String_compressedArrayCopy_BICII, TR::com_ibm_jit_JITHelpers_putByteInArrayByIndex, "com/ibm/jit/JITHelpers")},
      {w(TR::java_lang_String_compressedArrayCopy_CIBII, TR::com_ibm_jit_JITHelpers_getByteFromArrayByIndex, "com/ibm/jit/JITHelpers")},
      {w(TR::java_lang_String_compressedArrayCopy_CIBII, TR::com_ibm_jit_JITHelpers_putByteInArrayByIndex, "com/ibm/jit/JITHelpers")},
      {w(TR::java_lang_String_compressedArrayCopy_CICII, TR::com_ibm_jit_JITHelpers_getByteFromArrayByIndex, "com/ibm/jit/JITHelpers")},
      {w(TR::java_lang_String_compressedArrayCopy_CICII, TR::com_ibm_jit_JITHelpers_putByteInArrayByIndex, "com/ibm/jit/JITHelpers")},
      {w(TR::java_lang_String_decompressedArrayCopy_CICII, TR::java_lang_System_arraycopy, "java/lang/System")},
      {w(TR::java_lang_String_equals, TR::java_lang_String_lengthInternal, "java/lang/String")},
      {w(TR::java_lang_String_indexOf_String, TR::java_lang_String_indexOf_String_int, "java/lang/String")},
      {w(TR::java_lang_String_indexOf_String_int, TR::java_lang_String_length, "java/lang/String")},
      {w(TR::java_lang_String_indexOf_String_int, TR::java_lang_String_charAtInternal_I, "java/lang/String")},
      {w(TR::java_lang_String_indexOf_String_int, TR::java_lang_String_indexOf_native, "java/lang/String")},
      {w(TR::java_lang_String_indexOf_String_int, TR::java_lang_String_lengthInternal, "java/lang/String")},
      {wVer(TR::java_lang_String_indexOf_String_int, TR::java_lang_String_charAtInternal_IB, "java/lang/String", AllPastJavaVer, 8)},
      {w(TR::java_lang_String_indexOf_native, TR::java_lang_String_lengthInternal, "java/lang/String")},
      {w(TR::java_lang_String_indexOf_native, TR::com_ibm_jit_JITHelpers_intrinsicIndexOfLatin1, "com/ibm/jit/JITHelpers")},
      {w(TR::java_lang_String_indexOf_native, TR::com_ibm_jit_JITHelpers_intrinsicIndexOfUTF16, "com/ibm/jit/JITHelpers")},
      {w(TR::java_lang_String_length, TR::java_lang_String_lengthInternal, "java/lang/String")},
      {w(TR::java_lang_String_replace, TR::java_lang_String_indexOf_native, "java/lang/String")},
      {w(TR::java_lang_String_replace, TR::java_lang_String_lengthInternal, "java/lang/String")},
      {wVer(TR::java_lang_String_replace, TR::java_lang_String_compressedArrayCopy_CICII, "java/lang/String", AllPastJavaVer, 8)},
      {w(TR::java_lang_String_replace, TR::com_ibm_jit_JITHelpers_putByteInArrayByIndex, "com/ibm/jit/JITHelpers")},
      {wVer(TR::java_lang_String_replace, TR::java_lang_System_arraycopy, "java/lang/System", AllPastJavaVer, 8)},
      {w(TR::java_lang_String_hashCode, TR::java_lang_String_lengthInternal, "java/lang/String")},
      {wVer(TR::java_lang_String_hashCode, TR::java_lang_String_hashCodeImplCompressed, "java/lang/String", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_String_hashCode, TR::java_lang_String_hashCodeImplDecompressed, "java/lang/String", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_String_hashCodeImplCompressed, TR::com_ibm_jit_JITHelpers_getByteFromArrayByIndex, "com/ibm/jit/JITHelpers", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_String_hashCodeImplCompressed, TR::com_ibm_jit_JITHelpers_byteToCharUnsigned, "com/ibm/jit/JITHelpers", AllPastJavaVer, 8)},
      {w(TR::java_lang_String_compareTo, TR::java_lang_String_lengthInternal, "java/lang/String")},
      {w(TR::java_lang_String_compareTo, TR::com_ibm_jit_JITHelpers_getByteFromArrayByIndex, "com/ibm/jit/JITHelpers")},
      {w(TR::java_lang_String_compareTo, TR::com_ibm_jit_JITHelpers_byteToCharUnsigned, "com/ibm/jit/JITHelpers")},
      {wVer(TR::java_lang_String_compareTo, TR::java_lang_String_charAtInternal_IB, "java/lang/String", AllPastJavaVer, 8)},
      {w(TR::java_lang_String_lastIndexOf, TR::java_lang_String_lengthInternal, "java/lang/String")},
      {wVer(TR::java_lang_String_lastIndexOf, TR::com_ibm_jit_JITHelpers_getByteFromArrayByIndex, "com/ibm/jit/JITHelpers", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_String_lastIndexOf, TR::com_ibm_jit_JITHelpers_byteToCharUnsigned, "com/ibm/jit/JITHelpers", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_String_lastIndexOf, TR::java_lang_String_charAtInternal_IB, "java/lang/String", AllPastJavaVer, 8)},
      {w(TR::java_lang_String_toLowerCase, TR::com_ibm_jit_JITHelpers_supportsIntrinsicCaseConversion, "com/ibm/jit/JITHelpers")},
      {w(TR::java_lang_String_toLowerCase, TR::java_lang_String_lengthInternal, "java/lang/String")},
      {wVer(TR::java_lang_String_toLowerCase, TR::com_ibm_jit_JITHelpers_toLowerIntrinsicLatin1, "com/ibm/jit/JITHelpers", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_String_toLowerCase, TR::com_ibm_jit_JITHelpers_toLowerIntrinsicUTF16, "com/ibm/jit/JITHelpers", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_String_toLowerCase, TR::java_lang_String_toLowerCaseCore, "java/lang/String", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_String_toLowerCaseCore, TR::java_lang_String_lengthInternal, "java/lang/String", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_String_toLowerCaseCore, TR::java_lang_String_charAtInternal_I, "java/lang/String", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_String_toLowerCaseCore, TR::java_lang_StringBuilder_init_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_String_toLowerCaseCore, TR::java_lang_StringBuilder_append_char, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_String_toLowerCaseCore, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_String_toLowerCaseCore, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::java_lang_String_toUpperCase, TR::com_ibm_jit_JITHelpers_supportsIntrinsicCaseConversion, "com/ibm/jit/JITHelpers")},
      {w(TR::java_lang_String_toUpperCase, TR::java_lang_String_lengthInternal, "java/lang/String")},
      {wVer(TR::java_lang_String_toUpperCase, TR::com_ibm_jit_JITHelpers_toUpperIntrinsicLatin1, "com/ibm/jit/JITHelpers", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_String_toUpperCase, TR::com_ibm_jit_JITHelpers_toUpperIntrinsicUTF16, "com/ibm/jit/JITHelpers", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_String_toUpperCase, TR::java_lang_String_toUpperCaseCore, "java/lang/String", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_String_toUpperCaseCore, TR::java_lang_String_lengthInternal, "java/lang/String", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_String_toUpperCaseCore, TR::java_lang_String_charAtInternal_I, "java/lang/String", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_String_toUpperCaseCore, TR::java_lang_StringBuilder_init_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_String_toUpperCaseCore, TR::java_lang_String_charAt, "java/lang/String", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_String_toUpperCaseCore, TR::java_lang_String_indexOf_native, "java/lang/String", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_String_toUpperCaseCore, TR::java_lang_StringBuilder_append_char, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_String_toUpperCaseCore, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::java_lang_String_regionMatches, TR::java_lang_Object_getClass, "java/lang/Object")},
      {w(TR::java_lang_String_regionMatches, TR::java_lang_String_lengthInternal, "java/lang/String")},
      {w(TR::java_lang_String_regionMatches_bool, TR::java_lang_String_regionMatches, "java/lang/String")},
      {w(TR::java_lang_String_regionMatches_bool, TR::java_lang_Object_getClass, "java/lang/Object")},
      {w(TR::java_lang_String_regionMatches_bool, TR::java_lang_String_lengthInternal, "java/lang/String")},
      {w(TR::java_lang_String_regionMatches_bool, TR::com_ibm_jit_JITHelpers_getByteFromArrayByIndex, "com/ibm/jit/JITHelpers")},
      {w(TR::java_lang_String_regionMatches_bool, TR::com_ibm_jit_JITHelpers_byteToCharUnsigned, "com/ibm/jit/JITHelpers")},
      {wVer(TR::java_lang_String_regionMatches_bool, TR::java_lang_String_charAtInternal_IB, "java/lang/String", AllPastJavaVer, 8)},
      {w(TR::java_lang_String_equalsIgnoreCase, TR::java_lang_String_lengthInternal, "java/lang/String")},
      {w(TR::java_lang_String_equalsIgnoreCase, TR::com_ibm_jit_JITHelpers_getByteFromArrayByIndex, "com/ibm/jit/JITHelpers")},
      {w(TR::java_lang_String_equalsIgnoreCase, TR::com_ibm_jit_JITHelpers_byteToCharUnsigned, "com/ibm/jit/JITHelpers")},
      {wVer(TR::java_lang_String_equalsIgnoreCase, TR::java_lang_String_charAtInternal_IB, "java/lang/String", AllPastJavaVer, 8)},
      {w(TR::java_lang_String_compareToIgnoreCase, TR::java_lang_String_lengthInternal, "java/lang/String")},
      {w(TR::java_lang_String_compareToIgnoreCase, TR::com_ibm_jit_JITHelpers_getByteFromArrayByIndex, "com/ibm/jit/JITHelpers")},
      {wVer(TR::java_lang_String_compareToIgnoreCase, TR::java_lang_String_charAtInternal_IB, "java/lang/String", AllPastJavaVer, 8)},
      {w(TR::java_lang_String_getChars_charArray, TR::java_lang_String_lengthInternal, "java/lang/String")},
      {wVer(TR::java_lang_String_getChars_charArray, TR::java_lang_System_arraycopy, "java/lang/System", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_String_indexOf_String_int, TR::com_ibm_jit_JITHelpers_intrinsicIndexOfStringLatin1, "com/ibm/jit/JITHelpers", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_String_indexOf_String_int, TR::com_ibm_jit_JITHelpers_intrinsicIndexOfStringUTF16, "com/ibm/jit/JITHelpers", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_String_trim, TR::java_lang_Object_getClass, "java/lang/Object", 9, 10)},
      {wVer(TR::java_lang_String_charAt, TR::com_ibm_jit_JITHelpers_getCharFromArrayByIndex, "com/ibm/jit/JITHelpers", 9, AllFutureJavaVer)},
      {wVer(TR::java_lang_String_charAtInternal_I, TR::com_ibm_jit_JITHelpers_getCharFromArrayByIndex, "com/ibm/jit/JITHelpers", 9, AllFutureJavaVer)},
      {wVer(TR::java_lang_String_charAtInternal_IB, TR::com_ibm_jit_JITHelpers_getByteFromArrayByIndex, "com/ibm/jit/JITHelpers", 9, AllFutureJavaVer)},
      {wVer(TR::java_lang_String_charAtInternal_IB, TR::com_ibm_jit_JITHelpers_byteToCharUnsigned, "com/ibm/jit/JITHelpers", 9, AllFutureJavaVer)},
      {wVer(TR::java_lang_String_charAtInternal_IB, TR::com_ibm_jit_JITHelpers_getCharFromArrayByIndex, "com/ibm/jit/JITHelpers", 9, AllFutureJavaVer)},
      {wVer(TR::java_lang_String_concat, TR::java_lang_String_compressedArrayCopy_BIBII, "java/lang/String", 9, AllFutureJavaVer)},
      {wVer(TR::java_lang_String_concat, TR::java_lang_String_decompressedArrayCopy_BIBII, "java/lang/String", 9, AllFutureJavaVer)},
      {wVer(TR::java_lang_String_decompressedArrayCopy_BIBII, TR::com_ibm_jit_JITHelpers_getCharFromArrayByIndex, "com/ibm/jit/JITHelpers", 9, AllFutureJavaVer)},
      {wVer(TR::java_lang_String_decompressedArrayCopy_BIBII, TR::com_ibm_jit_JITHelpers_putCharInArrayByIndex, "com/ibm/jit/JITHelpers", 9, AllFutureJavaVer)},
      {wVer(TR::java_lang_String_decompressedArrayCopy_BICII, TR::com_ibm_jit_JITHelpers_getCharFromArrayByIndex, "com/ibm/jit/JITHelpers", 9, AllFutureJavaVer)},
      {wVer(TR::java_lang_String_decompressedArrayCopy_BICII, TR::com_ibm_jit_JITHelpers_putCharInArrayByIndex, "com/ibm/jit/JITHelpers", 9, AllFutureJavaVer)},
      {wVer(TR::java_lang_String_decompressedArrayCopy_CIBII, TR::com_ibm_jit_JITHelpers_getCharFromArrayByIndex, "com/ibm/jit/JITHelpers", 9, AllFutureJavaVer)},
      {wVer(TR::java_lang_String_decompressedArrayCopy_CIBII, TR::com_ibm_jit_JITHelpers_putCharInArrayByIndex, "com/ibm/jit/JITHelpers", 9, AllFutureJavaVer)},
      {wVer(TR::java_lang_String_replace, TR::java_lang_String_compressedArrayCopy_BIBII, "java/lang/String", 9, AllFutureJavaVer)},
      {wVer(TR::java_lang_String_replace, TR::com_ibm_jit_JITHelpers_putCharInArrayByIndex, "com/ibm/jit/JITHelpers", 9, AllFutureJavaVer)},
      {wVer(TR::java_lang_String_replace, TR::java_lang_String_decompressedArrayCopy_BIBII, "java/lang/String", 9, AllFutureJavaVer)},
      {wVer(TR::java_lang_String_hashCode, TR::java_lang_String_hashCodeImplCompressed, "java/lang/String", 9, AllFutureJavaVer)},
      {wVer(TR::java_lang_String_hashCode, TR::java_lang_String_hashCodeImplDecompressed, "java/lang/String", 9, AllFutureJavaVer)},
      {wVer(TR::java_lang_String_hashCodeImplCompressed, TR::com_ibm_jit_JITHelpers_getByteFromArrayByIndex, "com/ibm/jit/JITHelpers", 9, AllFutureJavaVer)},
      {wVer(TR::java_lang_String_hashCodeImplCompressed, TR::com_ibm_jit_JITHelpers_byteToCharUnsigned, "com/ibm/jit/JITHelpers", 9, AllFutureJavaVer)},
      {wVer(TR::java_lang_String_hashCodeImplDecompressed, TR::com_ibm_jit_JITHelpers_getCharFromArrayByIndex, "com/ibm/jit/JITHelpers", 9, AllFutureJavaVer)},
      {wVer(TR::java_lang_String_compareTo, TR::java_lang_Object_getClass, "java/lang/Object", 9, 10)},
      {wVer(TR::java_lang_String_compareTo, TR::java_lang_String_charAtInternal_IB, "java/lang/String", 9, AllFutureJavaVer)},
      {wVer(TR::java_lang_String_toLowerCase, TR::com_ibm_jit_JITHelpers_toLowerIntrinsicLatin1, "com/ibm/jit/JITHelpers", 9, AllFutureJavaVer)},
      {wVer(TR::java_lang_String_toLowerCase, TR::com_ibm_jit_JITHelpers_toLowerIntrinsicUTF16, "com/ibm/jit/JITHelpers", 9, AllFutureJavaVer)},
      {wVer(TR::java_lang_String_toUpperCase, TR::com_ibm_jit_JITHelpers_toUpperIntrinsicLatin1, "com/ibm/jit/JITHelpers", 9, AllFutureJavaVer)},
      {wVer(TR::java_lang_String_toUpperCase, TR::com_ibm_jit_JITHelpers_toUpperIntrinsicUTF16, "com/ibm/jit/JITHelpers", 9, AllFutureJavaVer)},
      {wVer(TR::java_lang_String_regionMatches_bool, TR::java_lang_String_charAtInternal_IB, "java/lang/String", 9, AllFutureJavaVer)},
      {wVer(TR::java_lang_String_equalsIgnoreCase, TR::java_lang_String_charAtInternal_IB, "java/lang/String", 9, AllFutureJavaVer)},
      {wVer(TR::java_lang_String_compareToIgnoreCase, TR::java_lang_Object_getClass, "java/lang/Object", 9, 10)},
      {wVer(TR::java_lang_String_compareToIgnoreCase, TR::java_lang_String_charAtInternal_IB, "java/lang/String", 9, AllFutureJavaVer)},
      {wVer(TR::java_lang_String_getChars_byteArray, TR::java_lang_String_lengthInternal, "java/lang/String", 9, 10)},
      {  TR::unknownMethod}
      };

   static W java_lang_System_Dependencies[] =
      {
      {w(TR::java_lang_System_identityHashCode, TR::java_lang_J9VMInternals_fastIdentityHashCode, "java/lang/J9VMInternals")},
      {  TR::unknownMethod}
      };

   static W java_lang_Integer_Dependencies[] =
      {
      {wVer(TR::java_lang_Integer_highestOneBit, TR::java_lang_Integer_numberOfLeadingZeros, "java/lang/Integer", 11, AllFutureJavaVer)},
      {  TR::unknownMethod}
      };

   static W java_util_HashMap_Dependencies[] =
      {
      {w(TR::java_util_HashMap_get, TR::java_util_HashMap_getNode, "java/util/HashMap")},
      {  TR::unknownMethod}
      };

   static W java_util_Hashtable_Dependencies[] =
      {
      {wVer(TR::java_util_Hashtable_clone, TR::java_lang_Object_clone, "java/lang/Object", AllPastJavaVer, 8)},
      {w(TR::java_util_Hashtable_putAll, TR::java_util_Hashtable_put, "java/util/Hashtable")},
      {w(TR::java_util_Hashtable_rehash, TR::java_lang_Math_min_F, "java/lang/Math")},
      {  TR::unknownMethod}
      };

   static W java_util_ArrayList_Dependencies[] =
      {
      {wVer(TR::java_util_ArrayList_remove, TR::java_lang_System_arraycopy, "java/lang/System", AllPastJavaVer, 9)},
      {  TR::unknownMethod}
      };

   static W java_lang_StrictMath_Dependencies[] =
      {
      {w(TR::java_lang_StrictMath_copySign_F, TR::java_lang_Math_copySign_F, "java/lang/Math")},
      {w(TR::java_lang_StrictMath_copySign_D, TR::java_lang_Math_copySign_D, "java/lang/Math")},
      {w(TR::java_lang_StrictMath_max_F, TR::java_lang_Math_max_F, "java/lang/Math")},
      {w(TR::java_lang_StrictMath_max_D, TR::java_lang_Math_max_D, "java/lang/Math")},
      {w(TR::java_lang_StrictMath_min_F, TR::java_lang_Math_min_F, "java/lang/Math")},
      {w(TR::java_lang_StrictMath_min_D, TR::java_lang_Math_min_D, "java/lang/Math")},
      {w(TR::java_lang_StrictMath_nextAfter_F, TR::java_lang_Math_nextAfter_F, "java/lang/Math")},
      {w(TR::java_lang_StrictMath_nextAfter_D, TR::java_lang_Math_nextAfter_D, "java/lang/Math")},
      {w(TR::java_lang_StrictMath_rint, TR::java_lang_Math_copySign_D, "java/lang/Math")},
      {w(TR::java_lang_StrictMath_rint, TR::java_lang_Math_abs_D, "java/lang/Math")},
      {w(TR::java_lang_StrictMath_round_F, TR::java_lang_Math_round_F, "java/lang/Math")},
      {w(TR::java_lang_StrictMath_round_D, TR::java_lang_Math_round_D, "java/lang/Math")},
      {w(TR::java_lang_StrictMath_scalb_F, TR::java_lang_Math_scalb_F, "java/lang/Math")},
      {wVer(TR::java_lang_StrictMath_cbrt, TR::java_lang_StrictMath_cbrt, "java/lang/FdLibm$Cbrt", 9, AllFutureJavaVer)},
      {wVer(TR::java_lang_StrictMath_exp, TR::java_lang_StrictMath_exp, "java/lang/FdLibm$Exp", 9, AllFutureJavaVer)},
      {wVer(TR::java_lang_StrictMath_hypot, TR::java_lang_StrictMath_hypot, "java/lang/FdLibm$Hypot", 9, AllFutureJavaVer)},
      {wVer(TR::java_lang_StrictMath_pow, TR::java_lang_StrictMath_pow, "java/lang/FdLibm$Pow", 9, AllFutureJavaVer)},
      {  TR::unknownMethod}
      };

   static W java_math_BigDecimal_Dependencies[] =
      {
      {wVer(TR::java_math_BigDecimal_add, TR::java_math_BigDecimal_DFPHWAvailable, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_add, TR::java_math_BigDecimal_possibleClone, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_add, TR::java_math_BigDecimal_longAdd, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_subtract, TR::java_math_BigDecimal_DFPHWAvailable, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_subtract, TR::java_math_BigDecimal_possibleClone, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_multiply, TR::java_lang_Long_numberOfLeadingZeros, "java/lang/Long", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_possibleClone, TR::java_math_BigDecimal_DFPHWAvailable, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_valueOf_J, TR::java_math_BigDecimal_valueOf, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_setScale, TR::java_math_BigDecimal_DFPHWAvailable, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_setScale, TR::java_math_BigInteger_multiply, "java/math/BigInteger", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_setScale, TR::java_lang_Long_numberOfLeadingZeros, "java/lang/Long", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_slowSubMulAddAddMulSetScale, TR::java_math_BigDecimal_subtract, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_slowSubMulAddAddMulSetScale, TR::java_math_BigDecimal_multiply, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_slowSubMulAddAddMulSetScale, TR::java_math_BigDecimal_setScale, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_slowSubMulAddAddMulSetScale, TR::java_math_BigDecimal_add, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_slowSubMulSetScale, TR::java_math_BigDecimal_subtract, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_slowSubMulSetScale, TR::java_math_BigDecimal_multiply, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_slowSubMulSetScale, TR::java_math_BigDecimal_setScale, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_slowAddAddMulSetScale, TR::java_math_BigDecimal_add, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_slowAddAddMulSetScale, TR::java_math_BigDecimal_multiply, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_slowAddAddMulSetScale, TR::java_math_BigDecimal_setScale, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_slowMulSetScale, TR::java_math_BigDecimal_multiply, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_slowMulSetScale, TR::java_math_BigDecimal_setScale, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_subMulAddAddMulSetScale, TR::java_lang_Object_getClass, "java/lang/Object", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_subMulAddAddMulSetScale, TR::java_lang_Math_pow, "java/lang/Math", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_subMulAddAddMulSetScale, TR::java_math_BigDecimal_noLLOverflowAdd, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_subMulAddAddMulSetScale, TR::java_math_BigDecimal_noLLOverflowMul, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_subMulAddAddMulSetScale, TR::java_math_BigDecimal_valueOf, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_subMulAddAddMulSetScale, TR::java_math_BigDecimal_slowSubMulAddAddMulSetScale, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_subMulSetScale, TR::java_lang_Object_getClass, "java/lang/Object", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_subMulSetScale, TR::java_lang_Math_pow, "java/lang/Math", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_subMulSetScale, TR::java_math_BigDecimal_noLLOverflowAdd, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_subMulSetScale, TR::java_math_BigDecimal_noLLOverflowMul, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_subMulSetScale, TR::java_math_BigDecimal_valueOf, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_subMulSetScale, TR::java_math_BigDecimal_slowSubMulSetScale, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_addAddMulSetScale, TR::java_lang_Object_getClass, "java/lang/Object", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_addAddMulSetScale, TR::java_lang_Math_pow, "java/lang/Math", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_addAddMulSetScale, TR::java_math_BigDecimal_noLLOverflowAdd, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_addAddMulSetScale, TR::java_math_BigDecimal_noLLOverflowMul, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_addAddMulSetScale, TR::java_math_BigDecimal_valueOf, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_addAddMulSetScale, TR::java_math_BigDecimal_slowAddAddMulSetScale, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_mulSetScale, TR::java_math_BigDecimal_noLLOverflowMul, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_mulSetScale, TR::java_math_BigDecimal_valueOf, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_mulSetScale, TR::java_math_BigDecimal_slowMulSetScale, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_longString1C, TR::java_lang_Math_abs_L, "java/lang/Math", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_longString2, TR::java_lang_Math_abs_L, "java/lang/Math", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_toString, TR::java_math_BigDecimal_longString1C, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_toString, TR::java_math_BigDecimal_longString2, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_longAdd, TR::java_math_BigInteger_add, "java/math/BigInteger", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_longAdd, TR::java_lang_Math_max_L, "java/lang/Math", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_longAdd, TR::java_lang_Math_max_I, "java/lang/Math", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_longAdd, TR::java_lang_Long_numberOfLeadingZeros, "java/lang/Long", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_longAdd, TR::java_math_BigInteger_multiply, "java/math/BigInteger", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_longAdd, TR::java_math_BigDecimal_slAdd, "java/math/BigDecimal", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_slAdd, TR::java_lang_Math_max_L, "java/lang/Math", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_slAdd, TR::java_lang_Math_max_I, "java/lang/Math", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_slAdd, TR::java_math_BigInteger_add, "java/math/BigInteger", AllPastJavaVer, 8)},
      {wVer(TR::java_math_BigDecimal_slAdd, TR::java_math_BigInteger_multiply, "java/math/BigInteger", AllPastJavaVer, 8)},
      {w(TR::java_math_BigDecimal_doubleValue, TR::java_math_BigDecimal_toString, "java/math/BigDecimal")},
      {w(TR::java_math_BigDecimal_floatValue, TR::java_math_BigDecimal_toString, "java/math/BigDecimal")},
      {w(TR::java_math_BigDecimal_valueOf, TR::java_math_BigDecimal_valueOf_J, "java/math/BigDecimal")},
      {w(TR::java_math_BigDecimal_setScale, TR::java_math_BigDecimal_valueOf, "java/math/BigDecimal")},
      {w(TR::java_math_BigDecimal_doubleValue, TR::java_lang_Math_abs_L, "java/lang/Math")},
      {w(TR::java_math_BigDecimal_floatValue, TR::java_lang_Math_abs_L, "java/lang/Math")},
      {  TR::unknownMethod}
      };

   static W java_lang_FdLibm_Pow_Dependencies[] =
      {
      {wVer(TR::java_lang_StrictMath_pow, TR::java_lang_Math_abs_D, "java/lang/Math", 9, AllFutureJavaVer)},
      {wVer(TR::java_lang_StrictMath_pow, TR::java_lang_Math_sqrt, "java/lang/Math", 9, AllFutureJavaVer)},
      {  TR::unknownMethod}
      };

   static W java_lang_ClassLoader_Dependencies[] =
      {
      {w(TR::java_lang_ClassLoader_callerClassLoader, TR::java_lang_ClassLoader_getStackClassLoader, "java/lang/ClassLoader")},
      {w(TR::java_lang_ClassLoader_getCallerClassLoader, TR::java_lang_ClassLoader_getStackClassLoader, "java/lang/ClassLoader")},
      {  TR::unknownMethod}
      };

   static W java_lang_FdLibm_Cbrt_Dependencies[] =
      {
      {wVer(TR::java_lang_StrictMath_cbrt, TR::java_lang_Math_abs_D, "java/lang/Math", 9, AllFutureJavaVer)},
      {  TR::unknownMethod}
      };

   static W java_lang_StringBuffer_Dependencies[] =
      {
      {wVer(TR::java_lang_StringBuffer_append, TR::java_lang_StringBuffer_lengthInternalUnsynchronized, "java/lang/StringBuffer", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_StringBuffer_append, TR::java_lang_StringBuffer_capacityInternal, "java/lang/StringBuffer", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_StringBuffer_append, TR::java_lang_StringBuffer_ensureCapacityImpl, "java/lang/StringBuffer", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_StringBuffer_append, TR::java_lang_String_decompressedArrayCopy_CICII, "java/lang/String", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_StringBuffer_append, TR::java_lang_StringBuffer_lengthInternalUnsynchronized, "java/lang/StringBuffer", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_StringBuffer_append, TR::java_lang_StringBuffer_capacityInternal, "java/lang/StringBuffer", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_StringBuffer_append, TR::java_lang_StringBuffer_ensureCapacityImpl, "java/lang/StringBuffer", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_StringBuffer_append, TR::java_lang_String_decompressedArrayCopy_CICII, "java/lang/String", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_StringBuffer_ensureCapacityImpl, TR::java_lang_StringBuffer_lengthInternalUnsynchronized, "java/lang/StringBuffer", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_StringBuffer_ensureCapacityImpl, TR::java_lang_StringBuffer_capacityInternal, "java/lang/StringBuffer", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_StringBuffer_ensureCapacityImpl, TR::java_lang_String_compressedArrayCopy_CICII, "java/lang/String", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_StringBuffer_ensureCapacityImpl, TR::java_lang_String_decompressedArrayCopy_CICII, "java/lang/String", AllPastJavaVer, 8)},
      {  TR::unknownMethod}
      };

   static W com_ibm_jit_JITHelpers_Dependencies[] =
      {
      {w(TR::com_ibm_jit_JITHelpers_isArray, TR::com_ibm_jit_JITHelpers_is32Bit, "com/ibm/jit/JITHelpers")},
      {w(TR::com_ibm_jit_JITHelpers_isArray, TR::com_ibm_jit_JITHelpers_getJ9ClassFromObject64, "com/ibm/jit/JITHelpers")},
      {w(TR::com_ibm_jit_JITHelpers_isArray, TR::com_ibm_jit_JITHelpers_getClassDepthAndFlagsFromJ9Class64, "com/ibm/jit/JITHelpers")},
      {w(TR::com_ibm_jit_JITHelpers_intrinsicIndexOfLatin1, TR::com_ibm_jit_JITHelpers_getByteFromArrayByIndex, "com/ibm/jit/JITHelpers")},
      {w(TR::com_ibm_jit_JITHelpers_intrinsicIndexOfUTF16, TR::com_ibm_jit_JITHelpers_getCharFromArrayByIndex, "com/ibm/jit/JITHelpers")},
      {w(TR::com_ibm_jit_JITHelpers_getJ9ClassFromObject64, TR::java_lang_Object_getClass, "java/lang/Object")},
      {w(TR::com_ibm_jit_JITHelpers_getJ9ClassFromObject64, TR::com_ibm_jit_JITHelpers_getJ9ClassFromClass64, "com/ibm/jit/JITHelpers")},
      {w(TR::com_ibm_jit_JITHelpers_optimizedClone, TR::java_lang_Object_getClass, "java/lang/Object")},
      {w(TR::com_ibm_jit_JITHelpers_optimizedClone, TR::java_lang_Class_isArray, "java/lang/Class")},
      {w(TR::com_ibm_jit_JITHelpers_optimizedClone, TR::java_lang_Class_getComponentType, "java/lang/Class")},
      {w(TR::com_ibm_jit_JITHelpers_optimizedClone, TR::java_lang_reflect_Array_getLength, "java/lang/reflect/Array")},
      {w(TR::com_ibm_jit_JITHelpers_optimizedClone, TR::java_lang_System_arraycopy, "java/lang/System")},
      {wVer(TR::com_ibm_jit_JITHelpers_optimizedClone, TR::sun_misc_Unsafe_getInt_jlObjectJ_I, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_optimizedClone, TR::sun_misc_Unsafe_getInt_J_I, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_optimizedClone, TR::sun_misc_Unsafe_putInt_jlObjectII_V, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_optimizedClone, TR::sun_misc_Unsafe_putInt_jlObjectJI_V, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_optimizedClone, TR::sun_misc_Unsafe_getLong_jlObjectJ_J, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_optimizedClone, TR::sun_misc_Unsafe_getLong_J_J, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {w(TR::com_ibm_jit_JITHelpers_optimizedClone, TR::com_ibm_jit_JITHelpers_getClassFlagsFromJ9Class64, "com/ibm/jit/JITHelpers")},
      {wVer(TR::com_ibm_jit_JITHelpers_optimizedClone, TR::sun_misc_Unsafe_putLong_jlObjectJJ_V, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_optimizedClone, TR::sun_misc_Unsafe_storeFence, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_getIntFromObject, TR::sun_misc_Unsafe_getInt_jlObjectJ_I, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_getIntFromObjectVolatile, TR::sun_misc_Unsafe_getIntVolatile_jlObjectJ_I, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_getLongFromObject, TR::sun_misc_Unsafe_getLong_jlObjectJ_J, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_getLongFromObjectVolatile, TR::sun_misc_Unsafe_getLongVolatile_jlObjectJ_J, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_getObjectFromObject, TR::sun_misc_Unsafe_getObject_jlObjectJ_jlObject, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_getObjectFromObjectVolatile, TR::sun_misc_Unsafe_getObjectVolatile_jlObjectJ_jlObject, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_putIntInObject, TR::sun_misc_Unsafe_putInt_jlObjectJI_V, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_putIntInObjectVolatile, TR::sun_misc_Unsafe_putIntVolatile_jlObjectJI_V, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_putLongInObject, TR::sun_misc_Unsafe_putLong_jlObjectJJ_V, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_putLongInObjectVolatile, TR::sun_misc_Unsafe_putLongVolatile_jlObjectJJ_V, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_putObjectInObject, TR::sun_misc_Unsafe_putObject_jlObjectJjlObject_V, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_putObjectInObjectVolatile, TR::sun_misc_Unsafe_putObjectVolatile_jlObjectJjlObject_V, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_compareAndSwapIntInObject, TR::sun_misc_Unsafe_compareAndSwapInt_jlObjectJII_Z, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_compareAndSwapLongInObject, TR::sun_misc_Unsafe_compareAndSwapLong_jlObjectJJJ_Z, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_compareAndSwapObjectInObject, TR::sun_misc_Unsafe_compareAndSwapObject_jlObjectJjlObjectjlObject_Z, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_getByteFromArray, TR::sun_misc_Unsafe_getByte_jlObjectJ_B, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {w(TR::com_ibm_jit_JITHelpers_getByteFromArrayByIndex, TR::java_lang_Object_getClass, "java/lang/Object")},
      {wVer(TR::com_ibm_jit_JITHelpers_getByteFromArrayVolatile, TR::sun_misc_Unsafe_getByteVolatile_jlObjectJ_B, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_getCharFromArray, TR::sun_misc_Unsafe_getChar_jlObjectJ_C, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {w(TR::com_ibm_jit_JITHelpers_getCharFromArrayByIndex, TR::java_lang_Object_getClass, "java/lang/Object")},
      {w(TR::com_ibm_jit_JITHelpers_getCharFromArrayByIndex, TR::com_ibm_jit_JITHelpers_byteToCharUnsigned, "com/ibm/jit/JITHelpers")},
      {wVer(TR::com_ibm_jit_JITHelpers_getCharFromArrayVolatile, TR::sun_misc_Unsafe_getCharVolatile_jlObjectJ_C, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_getIntFromArray, TR::sun_misc_Unsafe_getInt_jlObjectJ_I, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_getIntFromArrayVolatile, TR::sun_misc_Unsafe_getIntVolatile_jlObjectJ_I, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_getLongFromArray, TR::sun_misc_Unsafe_getLong_jlObjectJ_J, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_getLongFromArrayVolatile, TR::sun_misc_Unsafe_getLongVolatile_jlObjectJ_J, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_getObjectFromArray, TR::sun_misc_Unsafe_getObject_jlObjectJ_jlObject, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_getObjectFromArrayVolatile, TR::sun_misc_Unsafe_getObjectVolatile_jlObjectJ_jlObject, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_putByteInArray, TR::sun_misc_Unsafe_putByte_jlObjectJB_V, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {w(TR::com_ibm_jit_JITHelpers_putByteInArrayByIndex, TR::java_lang_Object_getClass, "java/lang/Object")},
      {wVer(TR::com_ibm_jit_JITHelpers_putByteInArrayVolatile, TR::sun_misc_Unsafe_putByteVolatile_jlObjectJB_V, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_putCharInArray, TR::sun_misc_Unsafe_putChar_jlObjectJC_V, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {w(TR::com_ibm_jit_JITHelpers_putCharInArrayByIndex, TR::java_lang_Object_getClass, "java/lang/Object")},
      {wVer(TR::com_ibm_jit_JITHelpers_putCharInArrayVolatile, TR::sun_misc_Unsafe_putCharVolatile_jlObjectJC_V, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_putIntInArray, TR::sun_misc_Unsafe_putInt_jlObjectJI_V, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_putIntInArrayVolatile, TR::sun_misc_Unsafe_putIntVolatile_jlObjectJI_V, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_putLongInArray, TR::sun_misc_Unsafe_putLong_jlObjectJJ_V, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_putLongInArrayVolatile, TR::sun_misc_Unsafe_putLongVolatile_jlObjectJJ_V, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_putObjectInArray, TR::sun_misc_Unsafe_putObject_jlObjectJjlObject_V, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_putObjectInArrayVolatile, TR::sun_misc_Unsafe_putObjectVolatile_jlObjectJjlObject_V, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_compareAndSwapIntInArray, TR::sun_misc_Unsafe_compareAndSwapInt_jlObjectJII_Z, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_compareAndSwapLongInArray, TR::sun_misc_Unsafe_compareAndSwapLong_jlObjectJJJ_Z, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_compareAndSwapObjectInArray, TR::sun_misc_Unsafe_compareAndSwapObject_jlObjectJjlObjectjlObject_Z, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {w(TR::com_ibm_jit_JITHelpers_getClassInitializeStatus, TR::com_ibm_jit_JITHelpers_is32Bit, "com/ibm/jit/JITHelpers")},
      {w(TR::com_ibm_jit_JITHelpers_getClassInitializeStatus, TR::com_ibm_jit_JITHelpers_getJ9ClassFromClass64, "com/ibm/jit/JITHelpers")},
      {wVer(TR::com_ibm_jit_JITHelpers_getClassInitializeStatus, TR::sun_misc_Unsafe_getInt_J_I, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_jit_JITHelpers_getClassInitializeStatus, TR::sun_misc_Unsafe_getLong_J_J, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {w(TR::com_ibm_jit_JITHelpers_intrinsicIndexOfStringLatin1, TR::com_ibm_jit_JITHelpers_getByteFromArrayByIndex, "com/ibm/jit/JITHelpers")},
      {w(TR::com_ibm_jit_JITHelpers_intrinsicIndexOfStringLatin1, TR::com_ibm_jit_JITHelpers_byteToCharUnsigned, "com/ibm/jit/JITHelpers")},
      {w(TR::com_ibm_jit_JITHelpers_intrinsicIndexOfStringLatin1, TR::com_ibm_jit_JITHelpers_intrinsicIndexOfLatin1, "com/ibm/jit/JITHelpers")},
      {w(TR::com_ibm_jit_JITHelpers_intrinsicIndexOfStringUTF16, TR::com_ibm_jit_JITHelpers_getCharFromArrayByIndex, "com/ibm/jit/JITHelpers")},
      {w(TR::com_ibm_jit_JITHelpers_intrinsicIndexOfStringUTF16, TR::com_ibm_jit_JITHelpers_intrinsicIndexOfUTF16, "com/ibm/jit/JITHelpers")},
      {wVer(TR::com_ibm_jit_JITHelpers_optimizedClone, TR::sun_misc_Unsafe_getInt_jlObjectJ_I, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_optimizedClone, TR::sun_misc_Unsafe_getInt_J_I, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_optimizedClone, TR::sun_misc_Unsafe_getObject_jlObjectJ_jlObject, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_optimizedClone, TR::sun_misc_Unsafe_putObject_jlObjectJjlObject_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_optimizedClone, TR::sun_misc_Unsafe_putInt_jlObjectJI_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_optimizedClone, TR::sun_misc_Unsafe_getLong_jlObjectJ_J, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_optimizedClone, TR::sun_misc_Unsafe_getLong_J_J, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_optimizedClone, TR::sun_misc_Unsafe_putLong_jlObjectJJ_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_optimizedClone, TR::sun_misc_Unsafe_storeFence, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_getIntFromObject, TR::sun_misc_Unsafe_getInt_jlObjectJ_I, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_getIntFromObjectVolatile, TR::sun_misc_Unsafe_getIntVolatile_jlObjectJ_I, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_getLongFromObject, TR::sun_misc_Unsafe_getLong_jlObjectJ_J, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_getLongFromObjectVolatile, TR::sun_misc_Unsafe_getLongVolatile_jlObjectJ_J, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_getObjectFromObject, TR::sun_misc_Unsafe_getObject_jlObjectJ_jlObject, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_getObjectFromObjectVolatile, TR::sun_misc_Unsafe_getObjectVolatile_jlObjectJ_jlObject, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_putIntInObject, TR::sun_misc_Unsafe_putInt_jlObjectJI_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_putIntInObjectVolatile, TR::sun_misc_Unsafe_putIntVolatile_jlObjectJI_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_putLongInObject, TR::sun_misc_Unsafe_putLong_jlObjectJJ_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_putLongInObjectVolatile, TR::sun_misc_Unsafe_putLongVolatile_jlObjectJJ_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_putObjectInObject, TR::sun_misc_Unsafe_putObject_jlObjectJjlObject_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_putObjectInObjectVolatile, TR::sun_misc_Unsafe_putObjectVolatile_jlObjectJjlObject_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_compareAndSwapIntInObject, TR::sun_misc_Unsafe_compareAndSwapInt_jlObjectJII_Z, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_compareAndSwapLongInObject, TR::sun_misc_Unsafe_compareAndSwapLong_jlObjectJJJ_Z, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_compareAndSwapObjectInObject, TR::sun_misc_Unsafe_compareAndSwapObject_jlObjectJjlObjectjlObject_Z, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_getByteFromArray, TR::sun_misc_Unsafe_getByte_jlObjectJ_B, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_getByteFromArrayVolatile, TR::sun_misc_Unsafe_getByteVolatile_jlObjectJ_B, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_getCharFromArray, TR::sun_misc_Unsafe_getChar_jlObjectJ_C, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_getCharFromArrayVolatile, TR::sun_misc_Unsafe_getCharVolatile_jlObjectJ_C, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_getIntFromArray, TR::sun_misc_Unsafe_getInt_jlObjectJ_I, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_getIntFromArrayVolatile, TR::sun_misc_Unsafe_getIntVolatile_jlObjectJ_I, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_getLongFromArray, TR::sun_misc_Unsafe_getLong_jlObjectJ_J, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_getLongFromArrayVolatile, TR::sun_misc_Unsafe_getLongVolatile_jlObjectJ_J, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_getObjectFromArray, TR::sun_misc_Unsafe_getObject_jlObjectJ_jlObject, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_getObjectFromArrayVolatile, TR::sun_misc_Unsafe_getObjectVolatile_jlObjectJ_jlObject, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_putByteInArray, TR::sun_misc_Unsafe_putByte_jlObjectJB_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_putByteInArrayVolatile, TR::sun_misc_Unsafe_putByteVolatile_jlObjectJB_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_putCharInArray, TR::sun_misc_Unsafe_putChar_jlObjectJC_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_putCharInArrayVolatile, TR::sun_misc_Unsafe_putCharVolatile_jlObjectJC_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_putIntInArray, TR::sun_misc_Unsafe_putInt_jlObjectJI_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_putIntInArrayVolatile, TR::sun_misc_Unsafe_putIntVolatile_jlObjectJI_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_putLongInArray, TR::sun_misc_Unsafe_putLong_jlObjectJJ_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_putLongInArrayVolatile, TR::sun_misc_Unsafe_putLongVolatile_jlObjectJJ_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_putObjectInArray, TR::sun_misc_Unsafe_putObject_jlObjectJjlObject_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_putObjectInArrayVolatile, TR::sun_misc_Unsafe_putObjectVolatile_jlObjectJjlObject_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_compareAndSwapIntInArray, TR::sun_misc_Unsafe_compareAndSwapInt_jlObjectJII_Z, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_compareAndSwapLongInArray, TR::sun_misc_Unsafe_compareAndSwapLong_jlObjectJJJ_Z, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_compareAndSwapObjectInArray, TR::sun_misc_Unsafe_compareAndSwapObject_jlObjectJjlObjectjlObject_Z, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_getClassInitializeStatus, TR::sun_misc_Unsafe_getInt_J_I, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::com_ibm_jit_JITHelpers_getClassInitializeStatus, TR::sun_misc_Unsafe_getLong_J_J, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {  TR::unknownMethod}
      };

   static W java_lang_StringCoding_Dependencies[] =
      {
      {wVer(TR::java_lang_StringCoding_implEncodeISOArray, TR::java_lang_StringUTF16_getChar, "java/lang/StringUTF16", 9, AllFutureJavaVer)},
      {  TR::unknownMethod}
      };

   static W java_lang_FdLibm_Hypot_Dependencies[] =
      {
      {wVer(TR::java_lang_StrictMath_hypot, TR::java_lang_Math_abs_D, "java/lang/Math", 9, AllFutureJavaVer)},
      {wVer(TR::java_lang_StrictMath_hypot, TR::java_lang_Math_sqrt, "java/lang/Math", 9, AllFutureJavaVer)},
      {  TR::unknownMethod}
      };

   static W java_lang_J9VMInternals_Dependencies[] =
      {
      {w(TR::java_lang_J9VMInternals_fastIdentityHashCode, TR::java_lang_J9VMInternals_identityHashCode, "java/lang/J9VMInternals")},
      {w(TR::java_lang_J9VMInternals_fastIdentityHashCode, TR::com_ibm_jit_JITHelpers_is32Bit, "com/ibm/jit/JITHelpers")},
      {w(TR::java_lang_J9VMInternals_fastIdentityHashCode, TR::com_ibm_jit_JITHelpers_getIntFromObject, "com/ibm/jit/JITHelpers")},
      {w(TR::java_lang_J9VMInternals_fastIdentityHashCode, TR::com_ibm_jit_JITHelpers_isArray, "com/ibm/jit/JITHelpers")},
      {w(TR::java_lang_J9VMInternals_fastIdentityHashCode, TR::java_lang_Integer_toUnsignedLong, "java/lang/Integer")},
      {w(TR::java_lang_J9VMInternals_fastIdentityHashCode, TR::com_ibm_jit_JITHelpers_getLongFromObject, "com/ibm/jit/JITHelpers")},
      {w(TR::java_lang_J9VMInternals_fastIdentityHashCode, TR::com_ibm_jit_JITHelpers_getBackfillOffsetFromJ9Class64, "com/ibm/jit/JITHelpers")},
      {  TR::unknownMethod}
      };

   static W java_lang_StringBuilder_Dependencies[] =
      {
      {wVer(TR::java_lang_StringBuilder_append_bool, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_StringBuilder_append_char, TR::java_lang_StringBuilder_lengthInternal, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_StringBuilder_append_char, TR::java_lang_StringBuilder_capacityInternal, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_StringBuilder_append_char, TR::java_lang_StringBuilder_ensureCapacityImpl, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_StringBuilder_append_char, TR::com_ibm_jit_JITHelpers_putByteInArrayByIndex, "com/ibm/jit/JITHelpers", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_StringBuilder_append_double, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_StringBuilder_append_float, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_StringBuilder_append_int, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_StringBuilder_append_int, TR::java_lang_StringBuilder_lengthInternal, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_StringBuilder_append_int, TR::java_lang_StringBuilder_capacityInternal, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_StringBuilder_append_int, TR::java_lang_StringBuilder_ensureCapacityImpl, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_StringBuilder_append_long, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_StringBuilder_append_long, TR::java_lang_StringBuilder_lengthInternal, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_StringBuilder_append_long, TR::java_lang_StringBuilder_capacityInternal, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_StringBuilder_append_long, TR::java_lang_StringBuilder_ensureCapacityImpl, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_StringBuilder_append_String, TR::java_lang_StringBuilder_lengthInternal, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_StringBuilder_append_String, TR::java_lang_StringBuilder_capacityInternal, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_StringBuilder_append_String, TR::java_lang_String_lengthInternal, "java/lang/String", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_StringBuilder_append_String, TR::java_lang_String_isCompressed, "java/lang/String", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_StringBuilder_append_String, TR::java_lang_StringBuilder_ensureCapacityImpl, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::java_lang_StringBuilder_append_Object, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder")},
      {wVer(TR::java_lang_StringBuilder_ensureCapacityImpl, TR::java_lang_StringBuilder_lengthInternal, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_StringBuilder_ensureCapacityImpl, TR::java_lang_StringBuilder_capacityInternal, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_StringBuilder_ensureCapacityImpl, TR::java_lang_String_compressedArrayCopy_CICII, "java/lang/String", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_StringBuilder_ensureCapacityImpl, TR::java_lang_String_decompressedArrayCopy_CICII, "java/lang/String", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_StringBuilder_toString, TR::java_lang_StringBuilder_lengthInternal, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_StringBuilder_toString, TR::java_lang_StringBuilder_capacityInternal, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {  TR::unknownMethod}
      };

   static W java_lang_reflect_Array_Dependencies[] =
      {
      {w(TR::java_lang_reflect_Array_getLength, TR::java_lang_Object_getClass, "java/lang/Object")},
      {  TR::unknownMethod}
      };

   static W sun_nio_cs_UTF_8_Encoder_Dependencies[] =
      {
      {w(TR::sun_nio_cs_ISO_8859_1_Encoder_encodeArrayLoop, TR::java_lang_Math_min_I, "java/lang/Math")},
      {wVer(TR::sun_nio_cs_ISO_8859_1_Encoder_encodeArrayLoop, TR::sun_nio_cs_UTF_8_Encoder_encodeUTF_8, "sun/nio/cs/UTF_8$Encoder", AllPastJavaVer, 8)},
      {  TR::unknownMethod}
      };

   static W jdk_internal_misc_Unsafe_Dependencies[] =
      {
      {wVer(TR::sun_misc_Unsafe_putBooleanVolatile_jlObjectJZ_V, TR::sun_misc_Unsafe_putBooleanVolatile_jlObjectJZ_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putByteVolatile_jlObjectJB_V, TR::sun_misc_Unsafe_putByteVolatile_jlObjectJB_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putCharVolatile_jlObjectJC_V, TR::sun_misc_Unsafe_putCharVolatile_jlObjectJC_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putShortVolatile_jlObjectJS_V, TR::sun_misc_Unsafe_putShortVolatile_jlObjectJS_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putIntVolatile_jlObjectJI_V, TR::sun_misc_Unsafe_putIntVolatile_jlObjectJI_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putLongVolatile_jlObjectJJ_V, TR::sun_misc_Unsafe_putLongVolatile_jlObjectJJ_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putFloatVolatile_jlObjectJF_V, TR::sun_misc_Unsafe_putFloatVolatile_jlObjectJF_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putDoubleVolatile_jlObjectJD_V, TR::sun_misc_Unsafe_putDoubleVolatile_jlObjectJD_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putObjectVolatile_jlObjectJjlObject_V, TR::sun_misc_Unsafe_putObjectVolatile_jlObjectJjlObject_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getBooleanVolatile_jlObjectJ_Z, TR::sun_misc_Unsafe_getBooleanVolatile_jlObjectJ_Z, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getByteVolatile_jlObjectJ_B, TR::sun_misc_Unsafe_getByteVolatile_jlObjectJ_B, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getCharVolatile_jlObjectJ_C, TR::sun_misc_Unsafe_getCharVolatile_jlObjectJ_C, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getShortVolatile_jlObjectJ_S, TR::sun_misc_Unsafe_getShortVolatile_jlObjectJ_S, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getIntVolatile_jlObjectJ_I, TR::sun_misc_Unsafe_getIntVolatile_jlObjectJ_I, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getLongVolatile_jlObjectJ_J, TR::sun_misc_Unsafe_getLongVolatile_jlObjectJ_J, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getFloatVolatile_jlObjectJ_F, TR::sun_misc_Unsafe_getFloatVolatile_jlObjectJ_F, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getDoubleVolatile_jlObjectJ_D, TR::sun_misc_Unsafe_getDoubleVolatile_jlObjectJ_D, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getObjectVolatile_jlObjectJ_jlObject, TR::sun_misc_Unsafe_getObjectVolatile_jlObjectJ_jlObject, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putByte_JB_V, TR::sun_misc_Unsafe_putByte_jlObjectJB_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putShort_JS_V, TR::sun_misc_Unsafe_putShort_jlObjectJS_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putChar_JC_V, TR::sun_misc_Unsafe_putChar_jlObjectJC_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putInt_JI_V, TR::sun_misc_Unsafe_putInt_jlObjectJI_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putLong_JJ_V, TR::sun_misc_Unsafe_putLong_jlObjectJJ_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putFloat_JF_V, TR::sun_misc_Unsafe_putFloat_jlObjectJF_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_putDouble_JD_V, TR::sun_misc_Unsafe_putDouble_jlObjectJD_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getByte_J_B, TR::sun_misc_Unsafe_getByte_jlObjectJ_B, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getShort_J_S, TR::sun_misc_Unsafe_getShort_jlObjectJ_S, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getChar_J_C, TR::sun_misc_Unsafe_getChar_jlObjectJ_C, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getInt_J_I, TR::sun_misc_Unsafe_getInt_jlObjectJ_I, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getLong_J_J, TR::sun_misc_Unsafe_getLong_jlObjectJ_J, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getFloat_J_F, TR::sun_misc_Unsafe_getFloat_jlObjectJ_F, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getDouble_J_D, TR::sun_misc_Unsafe_getDouble_jlObjectJ_D, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getAndAddInt, TR::sun_misc_Unsafe_getIntVolatile_jlObjectJ_I, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getAndSetInt, TR::sun_misc_Unsafe_getIntVolatile_jlObjectJ_I, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getAndAddLong, TR::sun_misc_Unsafe_getLongVolatile_jlObjectJ_J, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getAndSetLong, TR::sun_misc_Unsafe_getLongVolatile_jlObjectJ_J, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getAndSetInt, TR::sun_misc_Unsafe_compareAndSwapInt_jlObjectJII_Z, "jdk/internal/misc/Unsafe", 11, AllFutureJavaVer)},
      {wVer(TR::sun_misc_Unsafe_getAndSetLong, TR::sun_misc_Unsafe_compareAndSwapLong_jlObjectJJJ_Z, "jdk/internal/misc/Unsafe", 11, AllFutureJavaVer)},
      {  TR::unknownMethod}
      };

   static W java_util_GregorianCalendar_Dependencies[] =
      {
      {w(TR::java_util_GregorianCalendar_computeFields, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder")},
      {w(TR::java_util_GregorianCalendar_computeFields, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder")},
      {w(TR::java_util_GregorianCalendar_computeFields, TR::java_lang_StringBuilder_append_long, "java/lang/StringBuilder")},
      {w(TR::java_util_GregorianCalendar_computeFields, TR::java_lang_StringBuilder_append_Object, "java/lang/StringBuilder")},
      {w(TR::java_util_GregorianCalendar_computeFields, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder")},
      {w(TR::java_util_GregorianCalendar_computeFields, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder")},
      {  TR::unknownMethod}
      };

   static W sun_nio_cs_US_ASCII_Encoder_Dependencies[] =
      {
      {w(TR::sun_nio_cs_ISO_8859_1_Encoder_encodeArrayLoop, TR::sun_nio_cs_US_ASCII_Encoder_encodeASCII, "sun/nio/cs/US_ASCII$Encoder")},
      {  TR::unknownMethod}
      };

   static W sun_nio_cs_ext_SBCS_Encoder_Dependencies[] =
      {
      {wVer(TR::sun_nio_cs_ISO_8859_1_Encoder_encodeArrayLoop, TR::sun_nio_cs_ext_SBCS_Encoder_encodeSBCS, "sun/nio/cs/ext/SBCS_Encoder", AllPastJavaVer, 8)},
      {  TR::unknownMethod}
      };

   static W java_lang_invoke_MethodHandle_Dependencies[] =
      {
      {w(TR::java_lang_invoke_MethodHandle_asType, TR::java_lang_invoke_MethodHandle_asType_instance, "java/lang/invoke/MethodHandle")},
      {w(TR::java_lang_invoke_MethodHandle_asType_instance, TR::java_lang_Class_isAssignableFrom, "java/lang/Class")},
      {  TR::unknownMethod}
      };

   static W java_lang_invoke_DirectHandle_Dependencies[] =
      {
      {w(TR::java_lang_invoke_DirectHandle_nullCheckIfRequired, TR::java_lang_Object_getClass, "java/lang/Object")},
      {  TR::unknownMethod}
      };

   static W sun_nio_cs_ISO_8859_1_Encoder_Dependencies[] =
      {
      {w(TR::sun_nio_cs_ISO_8859_1_Encoder_encodeArrayLoop, TR::sun_nio_cs_ISO_8859_1_Encoder_encodeISOArray, "sun/nio/cs/ISO_8859_1$Encoder")},
      {  TR::unknownMethod}
      };

   static W java_io_ByteArrayOutputStream_Dependencies[] =
      {
      {w(TR::java_io_ByteArrayOutputStream_write, TR::java_lang_System_arraycopy, "java/lang/System")},
      {  TR::unknownMethod}
      };

   static W java_lang_invoke_CollectHandle_Dependencies[] =
      {
      {w(TR::java_lang_invoke_CollectHandle_allocateArray, TR::java_lang_Class_getComponentType, "java/lang/Class")},
      {  TR::unknownMethod}
      };

   static W com_ibm_dataaccess_DecimalData_Dependencies[] =
      {
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToInteger, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToInteger, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToInteger, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToInteger, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToInteger, TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToInteger_, "com/ibm/dataaccess/DecimalData")},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertIntegerToPackedDecimal, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertIntegerToPackedDecimal, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertIntegerToPackedDecimal, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertIntegerToPackedDecimal, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_DecimalData_convertIntegerToPackedDecimal, TR::com_ibm_dataaccess_DecimalData_convertIntegerToPackedDecimal_, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertIntegerToPackedDecimal_, TR::java_lang_Math_abs_I, "java/lang/Math")},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToLong, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToLong, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToLong, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToLong, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToLong, TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToLong_, "com/ibm/dataaccess/DecimalData")},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertLongToPackedDecimal, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertLongToPackedDecimal, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertLongToPackedDecimal, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertLongToPackedDecimal, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_DecimalData_convertLongToPackedDecimal, TR::com_ibm_dataaccess_DecimalData_convertLongToPackedDecimal_, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertLongToPackedDecimal_, TR::java_lang_Math_abs_L, "java/lang/Math")},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToInteger, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToInteger, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToInteger, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToInteger, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToInteger, TR::com_ibm_dataaccess_DecimalData_JITIntrinsicsEnabled, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToInteger, TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToPackedDecimal_, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToInteger, TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToInteger_, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToInteger, TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToInteger_, "com/ibm/dataaccess/DecimalData")},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertIntegerToExternalDecimal, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertIntegerToExternalDecimal, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertIntegerToExternalDecimal, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertIntegerToExternalDecimal, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_DecimalData_convertIntegerToExternalDecimal, TR::com_ibm_dataaccess_DecimalData_JITIntrinsicsEnabled, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertIntegerToExternalDecimal, TR::com_ibm_dataaccess_DecimalData_convertIntegerToPackedDecimal_, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertIntegerToExternalDecimal, TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToExternalDecimal_, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertIntegerToExternalDecimal, TR::com_ibm_dataaccess_DecimalData_convertIntegerToExternalDecimal_, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertIntegerToExternalDecimal_, TR::java_lang_Math_abs_I, "java/lang/Math")},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToLong, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToLong, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToLong, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToLong, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToLong, TR::com_ibm_dataaccess_DecimalData_JITIntrinsicsEnabled, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToLong, TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToPackedDecimal_, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToLong, TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToLong_, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToLong, TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToLong_, "com/ibm/dataaccess/DecimalData")},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertLongToExternalDecimal, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertLongToExternalDecimal, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertLongToExternalDecimal, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertLongToExternalDecimal, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_DecimalData_convertLongToExternalDecimal, TR::com_ibm_dataaccess_DecimalData_JITIntrinsicsEnabled, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertLongToExternalDecimal, TR::com_ibm_dataaccess_DecimalData_convertLongToPackedDecimal_, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertLongToExternalDecimal, TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToExternalDecimal_, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertLongToExternalDecimal, TR::com_ibm_dataaccess_DecimalData_convertLongToExternalDecimal_, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertLongToExternalDecimal_, TR::java_lang_Math_abs_L, "java/lang/Math")},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToExternalDecimal, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToExternalDecimal, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToExternalDecimal, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToExternalDecimal, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToExternalDecimal, TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToExternalDecimal_, "com/ibm/dataaccess/DecimalData")},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToPackedDecimal, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToPackedDecimal, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToPackedDecimal, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToPackedDecimal, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToPackedDecimal, TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToPackedDecimal_, "com/ibm/dataaccess/DecimalData")},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToUnicodeDecimal, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToUnicodeDecimal, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToUnicodeDecimal, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToUnicodeDecimal, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToUnicodeDecimal, TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToUnicodeDecimal_, "com/ibm/dataaccess/DecimalData")},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToPackedDecimal, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToPackedDecimal, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToPackedDecimal, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToPackedDecimal, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToPackedDecimal, TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToPackedDecimal_, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToBigInteger, TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToBigDecimal, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertBigIntegerToPackedDecimal, TR::com_ibm_dataaccess_DecimalData_convertBigDecimalToPackedDecimal, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToBigDecimal, TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToInteger, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToBigDecimal, TR::java_math_BigDecimal_valueOf, "java/math/BigDecimal")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToBigDecimal, TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToLong, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToBigDecimal, TR::com_ibm_dataaccess_DecimalData_slowSignedPackedToBigDecimal, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertBigDecimalToPackedDecimal, TR::com_ibm_dataaccess_DecimalData_convertIntegerToPackedDecimal, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertBigDecimalToPackedDecimal, TR::com_ibm_dataaccess_DecimalData_convertLongToPackedDecimal, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertBigDecimalToPackedDecimal, TR::com_ibm_dataaccess_DecimalData_slowBigDecimalToSignedPacked, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToBigInteger, TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToPackedDecimal, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToBigInteger, TR::com_ibm_dataaccess_PackedDecimal_checkPackedDecimal, "com/ibm/dataaccess/PackedDecimal")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToBigInteger, TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToBigDecimal, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertBigIntegerToExternalDecimal, TR::com_ibm_dataaccess_DecimalData_convertBigDecimalToPackedDecimal, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertBigIntegerToExternalDecimal, TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToExternalDecimal, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToBigDecimal, TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToInteger, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToBigDecimal, TR::java_math_BigDecimal_valueOf, "java/math/BigDecimal")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToBigDecimal, TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToLong, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToBigDecimal, TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToPackedDecimal, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToBigDecimal, TR::com_ibm_dataaccess_PackedDecimal_checkPackedDecimal, "com/ibm/dataaccess/PackedDecimal")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertExternalDecimalToBigDecimal, TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToBigDecimal, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertBigDecimalToExternalDecimal, TR::com_ibm_dataaccess_DecimalData_convertIntegerToExternalDecimal, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertBigDecimalToExternalDecimal, TR::com_ibm_dataaccess_DecimalData_convertLongToExternalDecimal, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertBigDecimalToExternalDecimal, TR::com_ibm_dataaccess_DecimalData_convertBigDecimalToPackedDecimal, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertBigDecimalToExternalDecimal, TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToExternalDecimal, "com/ibm/dataaccess/DecimalData")},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToInteger, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToInteger, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToInteger, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToInteger, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToInteger, TR::com_ibm_dataaccess_DecimalData_JITIntrinsicsEnabled, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToInteger, TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToPackedDecimal_, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToInteger, TR::com_ibm_dataaccess_PackedDecimal_checkPackedDecimal, "com/ibm/dataaccess/PackedDecimal")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToInteger, TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToInteger_, "com/ibm/dataaccess/DecimalData")},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertIntegerToUnicodeDecimal, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertIntegerToUnicodeDecimal, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertIntegerToUnicodeDecimal, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertIntegerToUnicodeDecimal, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_DecimalData_convertIntegerToUnicodeDecimal, TR::com_ibm_dataaccess_DecimalData_JITIntrinsicsEnabled, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertIntegerToUnicodeDecimal, TR::com_ibm_dataaccess_DecimalData_convertIntegerToPackedDecimal_, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertIntegerToUnicodeDecimal, TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToUnicodeDecimal_, "com/ibm/dataaccess/DecimalData")},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToLong, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToLong, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToLong, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToLong, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToLong, TR::com_ibm_dataaccess_DecimalData_JITIntrinsicsEnabled, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToLong, TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToPackedDecimal_, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToLong, TR::com_ibm_dataaccess_PackedDecimal_checkPackedDecimal, "com/ibm/dataaccess/PackedDecimal")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToLong, TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToLong_, "com/ibm/dataaccess/DecimalData")},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertLongToUnicodeDecimal, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertLongToUnicodeDecimal, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertLongToUnicodeDecimal, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_DecimalData_convertLongToUnicodeDecimal, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_DecimalData_convertLongToUnicodeDecimal, TR::com_ibm_dataaccess_DecimalData_JITIntrinsicsEnabled, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertLongToUnicodeDecimal, TR::com_ibm_dataaccess_DecimalData_convertLongToPackedDecimal_, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertLongToUnicodeDecimal, TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToUnicodeDecimal_, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToBigInteger, TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToPackedDecimal, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToBigInteger, TR::com_ibm_dataaccess_PackedDecimal_checkPackedDecimal, "com/ibm/dataaccess/PackedDecimal")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToBigInteger, TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToBigDecimal, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertBigIntegerToUnicodeDecimal, TR::com_ibm_dataaccess_DecimalData_convertBigDecimalToPackedDecimal, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertBigIntegerToUnicodeDecimal, TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToUnicodeDecimal, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToBigDecimal, TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToPackedDecimal, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToBigDecimal, TR::com_ibm_dataaccess_PackedDecimal_checkPackedDecimal, "com/ibm/dataaccess/PackedDecimal")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertUnicodeDecimalToBigDecimal, TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToBigDecimal, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertBigDecimalToUnicodeDecimal, TR::com_ibm_dataaccess_DecimalData_convertBigDecimalToPackedDecimal, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_convertBigDecimalToUnicodeDecimal, TR::com_ibm_dataaccess_DecimalData_convertPackedDecimalToUnicodeDecimal, "com/ibm/dataaccess/DecimalData")},
      {w(TR::com_ibm_dataaccess_DecimalData_slowBigDecimalToSignedPacked, TR::java_lang_String_toCharArray, "java/lang/String")},
      {  TR::unknownMethod}
      };

   static W com_ibm_jit_DecimalFormatHelper_Dependencies[] =
      {
      {wVer(TR::com_ibm_jit_DecimalFormatHelper_formatAsDouble, TR::java_math_BigDecimal_multiply, "java/math/BigDecimal", 9, 10)},
      {wVer(TR::com_ibm_jit_DecimalFormatHelper_formatAsDouble, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", 9, 10)},
      {wVer(TR::com_ibm_jit_DecimalFormatHelper_formatAsDouble, TR::java_lang_String_length, "java/lang/String", 9, 10)},
      {wVer(TR::com_ibm_jit_DecimalFormatHelper_formatAsDouble, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", 9, 10)},
      {wVer(TR::com_ibm_jit_DecimalFormatHelper_formatAsDouble, TR::java_lang_StringBuilder_append_char, "java/lang/StringBuilder", 9, 10)},
      {wVer(TR::com_ibm_jit_DecimalFormatHelper_formatAsDouble, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", 9, 10)},
      {wVer(TR::com_ibm_jit_DecimalFormatHelper_formatAsDouble, TR::java_math_BigDecimal_doubleValue, "java/math/BigDecimal", 9, 10)},
      {wVer(TR::com_ibm_jit_DecimalFormatHelper_formatAsFloat, TR::java_math_BigDecimal_multiply, "java/math/BigDecimal", 9, 10)},
      {wVer(TR::com_ibm_jit_DecimalFormatHelper_formatAsFloat, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", 9, 10)},
      {wVer(TR::com_ibm_jit_DecimalFormatHelper_formatAsFloat, TR::java_lang_String_length, "java/lang/String", 9, 10)},
      {wVer(TR::com_ibm_jit_DecimalFormatHelper_formatAsFloat, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", 9, 10)},
      {wVer(TR::com_ibm_jit_DecimalFormatHelper_formatAsFloat, TR::java_lang_StringBuilder_append_char, "java/lang/StringBuilder", 9, 10)},
      {wVer(TR::com_ibm_jit_DecimalFormatHelper_formatAsFloat, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", 9, 10)},
      {wVer(TR::com_ibm_jit_DecimalFormatHelper_formatAsFloat, TR::java_math_BigDecimal_floatValue, "java/math/BigDecimal", 9, 10)},
      {  TR::unknownMethod}
      };

   static W java_lang_invoke_PrimitiveHandle_Dependencies[] =
      {
      {w(TR::java_lang_invoke_PrimitiveHandle_initializeClassIfRequired, TR::com_ibm_jit_JITHelpers_getClassInitializeStatus, "com/ibm/jit/JITHelpers")},
      {wVer(TR::java_lang_invoke_PrimitiveHandle_initializeClassIfRequired, TR::sun_misc_Unsafe_ensureClassInitialized, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::java_lang_invoke_PrimitiveHandle_initializeClassIfRequired, TR::sun_misc_Unsafe_ensureClassInitialized, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {  TR::unknownMethod}
      };

   static W com_ibm_dataaccess_PackedDecimal_Dependencies[] =
      {
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_checkPackedDecimal, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_checkPackedDecimal, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_checkPackedDecimal, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_checkPackedDecimal, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_PackedDecimal_checkPackedDecimal, TR::com_ibm_dataaccess_PackedDecimal_checkPackedDecimal_, "com/ibm/dataaccess/PackedDecimal")},
      {w(TR::com_ibm_dataaccess_PackedDecimal_checkPackedDecimal_2bInlined1, TR::com_ibm_dataaccess_PackedDecimal_checkPackedDecimal, "com/ibm/dataaccess/PackedDecimal")},
      {w(TR::com_ibm_dataaccess_PackedDecimal_checkPackedDecimal_2bInlined2, TR::com_ibm_dataaccess_PackedDecimal_checkPackedDecimal, "com/ibm/dataaccess/PackedDecimal")},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_addPackedDecimal, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_addPackedDecimal, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_addPackedDecimal, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_addPackedDecimal, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_PackedDecimal_addPackedDecimal, TR::com_ibm_dataaccess_PackedDecimal_addPackedDecimal_, "com/ibm/dataaccess/PackedDecimal")},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_subtractPackedDecimal, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_subtractPackedDecimal, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_subtractPackedDecimal, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_subtractPackedDecimal, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_PackedDecimal_subtractPackedDecimal, TR::com_ibm_dataaccess_PackedDecimal_subtractPackedDecimal_, "com/ibm/dataaccess/PackedDecimal")},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_multiplyPackedDecimal, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_multiplyPackedDecimal, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_multiplyPackedDecimal, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_multiplyPackedDecimal, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_PackedDecimal_multiplyPackedDecimal, TR::com_ibm_dataaccess_PackedDecimal_multiplyPackedDecimal_, "com/ibm/dataaccess/PackedDecimal")},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_dividePackedDecimal, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_dividePackedDecimal, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_dividePackedDecimal, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_dividePackedDecimal, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_PackedDecimal_dividePackedDecimal, TR::com_ibm_dataaccess_PackedDecimal_dividePackedDecimal_, "com/ibm/dataaccess/PackedDecimal")},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_remainderPackedDecimal, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_remainderPackedDecimal, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_remainderPackedDecimal, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_remainderPackedDecimal, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_PackedDecimal_remainderPackedDecimal, TR::com_ibm_dataaccess_PackedDecimal_remainderPackedDecimal_, "com/ibm/dataaccess/PackedDecimal")},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_lessThanPackedDecimal, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_lessThanPackedDecimal, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_lessThanPackedDecimal, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_lessThanPackedDecimal, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_PackedDecimal_lessThanPackedDecimal, TR::com_ibm_dataaccess_PackedDecimal_lessThanPackedDecimal_, "com/ibm/dataaccess/PackedDecimal")},
      {w(TR::com_ibm_dataaccess_PackedDecimal_lessThanPackedDecimal_, TR::com_ibm_dataaccess_PackedDecimal_greaterThanPackedDecimal_, "com/ibm/dataaccess/PackedDecimal")},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_lessThanOrEqualsPackedDecimal, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_lessThanOrEqualsPackedDecimal, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_lessThanOrEqualsPackedDecimal, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_lessThanOrEqualsPackedDecimal, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_PackedDecimal_lessThanOrEqualsPackedDecimal, TR::com_ibm_dataaccess_PackedDecimal_lessThanOrEqualsPackedDecimal_, "com/ibm/dataaccess/PackedDecimal")},
      {w(TR::com_ibm_dataaccess_PackedDecimal_lessThanOrEqualsPackedDecimal_, TR::com_ibm_dataaccess_PackedDecimal_greaterThanPackedDecimal_, "com/ibm/dataaccess/PackedDecimal")},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_greaterThanPackedDecimal, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_greaterThanPackedDecimal, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_greaterThanPackedDecimal, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_greaterThanPackedDecimal, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_PackedDecimal_greaterThanPackedDecimal, TR::com_ibm_dataaccess_PackedDecimal_greaterThanPackedDecimal_, "com/ibm/dataaccess/PackedDecimal")},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_greaterThanOrEqualsPackedDecimal, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_greaterThanOrEqualsPackedDecimal, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_greaterThanOrEqualsPackedDecimal, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_greaterThanOrEqualsPackedDecimal, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_PackedDecimal_greaterThanOrEqualsPackedDecimal, TR::com_ibm_dataaccess_PackedDecimal_greaterThanOrEqualsPackedDecimal_, "com/ibm/dataaccess/PackedDecimal")},
      {w(TR::com_ibm_dataaccess_PackedDecimal_greaterThanOrEqualsPackedDecimal_, TR::com_ibm_dataaccess_PackedDecimal_lessThanPackedDecimal_, "com/ibm/dataaccess/PackedDecimal")},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_equalsPackedDecimal, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_equalsPackedDecimal, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_equalsPackedDecimal, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_equalsPackedDecimal, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_PackedDecimal_equalsPackedDecimal, TR::com_ibm_dataaccess_PackedDecimal_equalsPackedDecimal_, "com/ibm/dataaccess/PackedDecimal")},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_notEqualsPackedDecimal, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_notEqualsPackedDecimal, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_notEqualsPackedDecimal, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_notEqualsPackedDecimal, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_PackedDecimal_notEqualsPackedDecimal, TR::com_ibm_dataaccess_PackedDecimal_notEqualsPackedDecimal_, "com/ibm/dataaccess/PackedDecimal")},
      {w(TR::com_ibm_dataaccess_PackedDecimal_notEqualsPackedDecimal_, TR::com_ibm_dataaccess_PackedDecimal_equalsPackedDecimal, "com/ibm/dataaccess/PackedDecimal")},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_shiftLeftPackedDecimal, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_shiftLeftPackedDecimal, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_shiftLeftPackedDecimal, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_shiftLeftPackedDecimal, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_PackedDecimal_shiftLeftPackedDecimal, TR::com_ibm_dataaccess_PackedDecimal_shiftLeftPackedDecimal_, "com/ibm/dataaccess/PackedDecimal")},
      {w(TR::com_ibm_dataaccess_PackedDecimal_shiftLeftPackedDecimal_, TR::com_ibm_dataaccess_PackedDecimal_checkPackedDecimal_, "com/ibm/dataaccess/PackedDecimal")},
      {w(TR::com_ibm_dataaccess_PackedDecimal_shiftLeftPackedDecimal_, TR::java_lang_System_arraycopy, "java/lang/System")},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_shiftRightPackedDecimal, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_shiftRightPackedDecimal, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_shiftRightPackedDecimal, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_PackedDecimal_shiftRightPackedDecimal, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_PackedDecimal_shiftRightPackedDecimal, TR::com_ibm_dataaccess_PackedDecimal_shiftRightPackedDecimal_, "com/ibm/dataaccess/PackedDecimal")},
      {w(TR::com_ibm_dataaccess_PackedDecimal_shiftRightPackedDecimal_, TR::com_ibm_dataaccess_PackedDecimal_checkPackedDecimal_, "com/ibm/dataaccess/PackedDecimal")},
      {w(TR::com_ibm_dataaccess_PackedDecimal_shiftRightPackedDecimal_, TR::java_lang_System_arraycopy, "java/lang/System")},
      {w(TR::com_ibm_dataaccess_PackedDecimal_movePackedDecimal, TR::com_ibm_dataaccess_PackedDecimal_shiftLeftPackedDecimal, "com/ibm/dataaccess/PackedDecimal")},
      {  TR::unknownMethod}
      };

   static W java_util_stream_IntPipeline_Head_Dependencies[] =
      {
      {w(TR::java_util_stream_IntPipelineHead_forEach, TR::java_util_stream_IntPipeline_forEach, "java/util/stream/IntPipeline")},
      {  TR::unknownMethod}
      };

   static W java_util_concurrent_atomic_AtomicLong_Dependencies[] =
      {
      {wVer(TR::java_util_concurrent_atomic_AtomicLong_addAndGet, TR::sun_misc_Unsafe_getAndAddLong, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::java_util_concurrent_atomic_AtomicLong_decrementAndGet, TR::sun_misc_Unsafe_getAndAddLong, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::java_util_concurrent_atomic_AtomicLong_getAndAdd, TR::sun_misc_Unsafe_getAndAddLong, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::java_util_concurrent_atomic_AtomicLong_getAndDecrement, TR::sun_misc_Unsafe_getAndAddLong, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::java_util_concurrent_atomic_AtomicLong_getAndIncrement, TR::sun_misc_Unsafe_getAndAddLong, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::java_util_concurrent_atomic_AtomicLong_getAndSet, TR::sun_misc_Unsafe_getAndSetLong, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::java_util_concurrent_atomic_AtomicLong_incrementAndGet, TR::sun_misc_Unsafe_getAndAddLong, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::java_util_concurrent_atomic_AtomicLong_weakCompareAndSet, TR::sun_misc_Unsafe_compareAndSwapLong_jlObjectJJJ_Z, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::java_util_concurrent_atomic_AtomicLong_lazySet, TR::sun_misc_Unsafe_putOrderedLong_jlObjectJJ_V, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::java_util_concurrent_atomic_AtomicLong_addAndGet, TR::sun_misc_Unsafe_getAndAddLong, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::java_util_concurrent_atomic_AtomicLong_decrementAndGet, TR::sun_misc_Unsafe_getAndAddLong, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::java_util_concurrent_atomic_AtomicLong_getAndAdd, TR::sun_misc_Unsafe_getAndAddLong, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::java_util_concurrent_atomic_AtomicLong_getAndDecrement, TR::sun_misc_Unsafe_getAndAddLong, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::java_util_concurrent_atomic_AtomicLong_getAndIncrement, TR::sun_misc_Unsafe_getAndAddLong, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::java_util_concurrent_atomic_AtomicLong_getAndSet, TR::sun_misc_Unsafe_getAndSetLong, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::java_util_concurrent_atomic_AtomicLong_incrementAndGet, TR::sun_misc_Unsafe_getAndAddLong, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::java_util_concurrent_atomic_AtomicLong_lazySet, TR::sun_misc_Unsafe_putLongVolatile_jlObjectJJ_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {  TR::unknownMethod}
      };

   static W com_ibm_dataaccess_ByteArrayMarshaller_Dependencies[] =
      {
      {wVer(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeShort, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeShort, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeShort, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeShort, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeShort, TR::com_ibm_dataaccess_ByteArrayMarshaller_writeShort_, "com/ibm/dataaccess/ByteArrayMarshaller")},
      {wVer(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeShortLength, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeShortLength, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeShortLength, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeShortLength, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeShortLength, TR::com_ibm_dataaccess_ByteArrayMarshaller_writeShortLength_, "com/ibm/dataaccess/ByteArrayMarshaller")},
      {wVer(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeInt, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeInt, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeInt, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeInt, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeInt, TR::com_ibm_dataaccess_ByteArrayMarshaller_writeInt_, "com/ibm/dataaccess/ByteArrayMarshaller")},
      {wVer(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeIntLength, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeIntLength, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeIntLength, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeIntLength, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeIntLength, TR::com_ibm_dataaccess_ByteArrayMarshaller_writeIntLength_, "com/ibm/dataaccess/ByteArrayMarshaller")},
      {wVer(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeLong, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeLong, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeLong, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeLong, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeLong, TR::com_ibm_dataaccess_ByteArrayMarshaller_writeLong_, "com/ibm/dataaccess/ByteArrayMarshaller")},
      {wVer(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeLongLength, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeLongLength, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeLongLength, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeLongLength, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeLongLength, TR::com_ibm_dataaccess_ByteArrayMarshaller_writeLongLength_, "com/ibm/dataaccess/ByteArrayMarshaller")},
      {wVer(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeFloat, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeFloat, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeFloat, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeFloat, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeFloat, TR::com_ibm_dataaccess_ByteArrayMarshaller_writeFloat_, "com/ibm/dataaccess/ByteArrayMarshaller")},
      {w(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeFloat_, TR::java_lang_Float_floatToIntBits, "java/lang/Float")},
      {w(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeFloat_, TR::com_ibm_dataaccess_ByteArrayMarshaller_writeInt, "com/ibm/dataaccess/ByteArrayMarshaller")},
      {wVer(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeDouble, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeDouble, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeDouble, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeDouble, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeDouble, TR::com_ibm_dataaccess_ByteArrayMarshaller_writeDouble_, "com/ibm/dataaccess/ByteArrayMarshaller")},
      {w(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeDouble_, TR::java_lang_Double_doubleToLongBits, "java/lang/Double")},
      {w(TR::com_ibm_dataaccess_ByteArrayMarshaller_writeDouble_, TR::com_ibm_dataaccess_ByteArrayMarshaller_writeLong, "com/ibm/dataaccess/ByteArrayMarshaller")},
      {  TR::unknownMethod}
      };

   static W java_util_concurrent_ConcurrentHashMap_Dependencies[] =
      {
      {wVer(TR::java_util_concurrent_ConcurrentHashMap_addCount, TR::sun_misc_Unsafe_compareAndSwapLong_jlObjectJJJ_Z, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {w(TR::java_util_concurrent_ConcurrentHashMap_addCount, TR::java_util_concurrent_ConcurrentHashMap_fullAddCount, "java/util/concurrent/ConcurrentHashMap")},
      {wVer(TR::java_util_concurrent_ConcurrentHashMap_addCount, TR::sun_misc_Unsafe_compareAndSwapInt_jlObjectJII_Z, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {w(TR::java_util_concurrent_ConcurrentHashMap_addCount, TR::java_util_concurrent_ConcurrentHashMap_transfer, "java/util/concurrent/ConcurrentHashMap")},
      {wVer(TR::java_util_concurrent_ConcurrentHashMap_tryPresize, TR::sun_misc_Unsafe_compareAndSwapInt_jlObjectJII_Z, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {w(TR::java_util_concurrent_ConcurrentHashMap_tryPresize, TR::java_util_concurrent_ConcurrentHashMap_transfer, "java/util/concurrent/ConcurrentHashMap")},
      {wVer(TR::java_util_concurrent_ConcurrentHashMap_fullAddCount, TR::sun_misc_Unsafe_compareAndSwapInt_jlObjectJII_Z, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::java_util_concurrent_ConcurrentHashMap_fullAddCount, TR::sun_misc_Unsafe_compareAndSwapLong_jlObjectJJJ_Z, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::java_util_concurrent_ConcurrentHashMap_addCount, TR::sun_misc_Unsafe_compareAndSwapLong_jlObjectJJJ_Z, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::java_util_concurrent_ConcurrentHashMap_addCount, TR::sun_misc_Unsafe_compareAndSwapInt_jlObjectJII_Z, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::java_util_concurrent_ConcurrentHashMap_tryPresize, TR::sun_misc_Unsafe_compareAndSwapInt_jlObjectJII_Z, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::java_util_concurrent_ConcurrentHashMap_fullAddCount, TR::sun_misc_Unsafe_compareAndSwapInt_jlObjectJII_Z, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::java_util_concurrent_ConcurrentHashMap_fullAddCount, TR::sun_misc_Unsafe_compareAndSwapLong_jlObjectJJJ_Z, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::java_util_concurrent_ConcurrentHashMap_fullAddCount, TR::java_util_Arrays_copyOf_Object1, "java/util/Arrays", 11, AllFutureJavaVer)},
      {  TR::unknownMethod}
      };

   static W com_ibm_crypto_provider_P256PrimeField_Dependencies[] =
      {
      {wVer(TR::com_ibm_crypto_provider_P256PrimeField_divideHelper, TR::com_ibm_crypto_provider_P256PrimeField_addNoMod, "com/ibm/crypto/provider/P256PrimeField", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_crypto_provider_P256PrimeField_divideHelper, TR::com_ibm_crypto_provider_P256PrimeField_shiftRight, "com/ibm/crypto/provider/P256PrimeField", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_crypto_provider_P256PrimeField_mod, TR::com_ibm_crypto_provider_P256PrimeField_addNoMod, "com/ibm/crypto/provider/P256PrimeField", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_crypto_provider_P256PrimeField_mod, TR::com_ibm_crypto_provider_P256PrimeField_subNoMod, "com/ibm/crypto/provider/P256PrimeField", AllPastJavaVer, 8)},
      {  TR::unknownMethod}
      };

   static W com_ibm_dataaccess_ByteArrayUnmarshaller_Dependencies[] =
      {
      {wVer(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readShort, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readShort, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readShort, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readShort, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readShort, TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readShort_, "com/ibm/dataaccess/ByteArrayUnmarshaller")},
      {wVer(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readShortLength, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readShortLength, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readShortLength, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readShortLength, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readShortLength, TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readShortLength_, "com/ibm/dataaccess/ByteArrayUnmarshaller")},
      {wVer(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readInt, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readInt, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readInt, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readInt, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readInt, TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readInt_, "com/ibm/dataaccess/ByteArrayUnmarshaller")},
      {wVer(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readIntLength, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readIntLength, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readIntLength, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readIntLength, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readIntLength, TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readIntLength_, "com/ibm/dataaccess/ByteArrayUnmarshaller")},
      {wVer(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readLong, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readLong, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readLong, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readLong, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readLong, TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readLong_, "com/ibm/dataaccess/ByteArrayUnmarshaller")},
      {wVer(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readLongLength, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readLongLength, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readLongLength, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readLongLength, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readLongLength, TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readLongLength_, "com/ibm/dataaccess/ByteArrayUnmarshaller")},
      {wVer(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readFloat, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readFloat, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readFloat, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readFloat, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readFloat, TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readFloat_, "com/ibm/dataaccess/ByteArrayUnmarshaller")},
      {w(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readFloat_, TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readInt, "com/ibm/dataaccess/ByteArrayUnmarshaller")},
      {w(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readFloat_, TR::java_lang_Float_intBitsToFloat, "java/lang/Float")},
      {wVer(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readDouble, TR::java_lang_StringBuilder_init, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readDouble, TR::java_lang_StringBuilder_append_String, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readDouble, TR::java_lang_StringBuilder_append_int, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readDouble, TR::java_lang_StringBuilder_toString, "java/lang/StringBuilder", AllPastJavaVer, 8)},
      {w(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readDouble, TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readDouble_, "com/ibm/dataaccess/ByteArrayUnmarshaller")},
      {w(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readDouble_, TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readLong, "com/ibm/dataaccess/ByteArrayUnmarshaller")},
      {w(TR::com_ibm_dataaccess_ByteArrayUnmarshaller_readDouble_, TR::java_lang_Double_longBitsToDouble, "java/lang/Double")},
      {  TR::unknownMethod}
      };

   static W java_util_concurrent_atomic_AtomicInteger_Dependencies[] =
      {
      {wVer(TR::java_util_concurrent_atomic_AtomicInteger_getAndAdd, TR::sun_misc_Unsafe_getAndAddInt, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::java_util_concurrent_atomic_AtomicInteger_getAndIncrement, TR::sun_misc_Unsafe_getAndAddInt, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::java_util_concurrent_atomic_AtomicInteger_getAndDecrement, TR::sun_misc_Unsafe_getAndAddInt, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::java_util_concurrent_atomic_AtomicInteger_getAndSet, TR::sun_misc_Unsafe_getAndSetInt, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::java_util_concurrent_atomic_AtomicInteger_addAndGet, TR::sun_misc_Unsafe_getAndAddInt, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::java_util_concurrent_atomic_AtomicInteger_incrementAndGet, TR::sun_misc_Unsafe_getAndAddInt, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::java_util_concurrent_atomic_AtomicInteger_decrementAndGet, TR::sun_misc_Unsafe_getAndAddInt, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::java_util_concurrent_atomic_AtomicInteger_weakCompareAndSet, TR::sun_misc_Unsafe_compareAndSwapInt_jlObjectJII_Z, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::java_util_concurrent_atomic_AtomicInteger_lazySet, TR::sun_misc_Unsafe_putOrderedInt_jlObjectJI_V, "sun/misc/Unsafe", AllPastJavaVer, 8)},
      {wVer(TR::java_util_concurrent_atomic_AtomicInteger_getAndAdd, TR::sun_misc_Unsafe_getAndAddInt, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::java_util_concurrent_atomic_AtomicInteger_getAndIncrement, TR::sun_misc_Unsafe_getAndAddInt, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::java_util_concurrent_atomic_AtomicInteger_getAndDecrement, TR::sun_misc_Unsafe_getAndAddInt, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::java_util_concurrent_atomic_AtomicInteger_getAndSet, TR::sun_misc_Unsafe_getAndSetInt, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::java_util_concurrent_atomic_AtomicInteger_addAndGet, TR::sun_misc_Unsafe_getAndAddInt, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::java_util_concurrent_atomic_AtomicInteger_incrementAndGet, TR::sun_misc_Unsafe_getAndAddInt, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::java_util_concurrent_atomic_AtomicInteger_decrementAndGet, TR::sun_misc_Unsafe_getAndAddInt, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {wVer(TR::java_util_concurrent_atomic_AtomicInteger_lazySet, TR::sun_misc_Unsafe_putIntVolatile_jlObjectJI_V, "jdk/internal/misc/Unsafe", 9, AllFutureJavaVer)},
      {  TR::unknownMethod}
      };

   static W com_ibm_crypto_provider_AESCryptInHardware_Dependencies[] =
      {
      {wVer(TR::com_ibm_crypto_provider_AEScryptInHardware_cbcDecrypt, TR::com_ibm_jit_crypto_JITAESCryptInHardware_doAESInHardware, "com/ibm/jit/crypto/JITAESCryptInHardware", AllPastJavaVer, 8)},
      {wVer(TR::com_ibm_crypto_provider_AEScryptInHardware_cbcEncrypt, TR::com_ibm_jit_crypto_JITAESCryptInHardware_doAESInHardware, "com/ibm/jit/crypto/JITAESCryptInHardware", AllPastJavaVer, 8)},
      {  TR::unknownMethod}
      };

   static W java_lang_invoke_ConvertHandle_FilterHelpers_Dependencies[] =
      {
      {w(TR::java_lang_invoke_ConvertHandleFilterHelpers_object2J, TR::java_lang_Object_getClass, "java/lang/Object")},
      {w(TR::java_lang_invoke_ConvertHandleFilterHelpers_object2J, TR::java_lang_invoke_ConvertHandleFilterHelpers_number2J, "java/lang/invoke/ConvertHandle$FilterHelpers")},
      {w(TR::java_lang_invoke_ConvertHandleFilterHelpers_number2J, TR::java_lang_Object_getClass, "java/lang/Object")},
      {  TR::unknownMethod}
      };

   static W java_io_ObjectInputStream_BlockDataInputStream_Dependencies[] =
      {
      {w(TR::java_io_ObjectInputStream_BlockDataInputStream_read, TR::java_lang_Math_min_I, "java/lang/Math")},
      {w(TR::java_io_ObjectInputStream_BlockDataInputStream_read, TR::java_lang_System_arraycopy, "java/lang/System")},
      {  TR::unknownMethod}
      };

