/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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
#include <stdio.h>
#include <math.h>

#include <float.h>

#include "j9port.h"
#include "j9comp.h"
#include "j9protos.h"
#include "fltconst.h"
#include "fltdmath.h"
#include "util_internal.h"

#if (defined(J9VM_INTERP_FLOAT_SUPPORT))  /* File Level Build Flags */

#if (defined(LINUXPPC) || defined(ARMGNU)) && defined(HARDHAT)
#include "fltdmath.h"
#endif

static int fltconv_indexLeadingOne32 (U_32 u32val);


jfloat 
helperCConvertDoubleToFloat(jdouble src)
{
	jfloat result;

#ifdef J9_NO_DENORMAL_FLOAT_SUPPORT
	if (IS_DENORMAL_DBL(src))  {
		convertDoubleToFloat(src, &result);
		return result;
	}
#endif

#if (defined(LINUXPPC) || defined(ARMGNU)) && defined(HARDHAT)
	convertDoubleToFloat(src, &result);
#else 
	result = (jfloat)src;
#endif
	return result;
}


void 
helperConvertDoubleToFloat(jdouble *srcPtr, jfloat *dstPtr)
{
	*dstPtr = helperCConvertDoubleToFloat(*srcPtr);
}


I_32 
helperCConvertDoubleToInteger(jdouble src)
{
	I_32 result;

	if (IS_NAN_DBL(src)) {
		return 0;
	}

#ifdef J9_NO_DENORMAL_FLOAT_SUPPORT
	if (IS_DENORMAL_DBL(src)) {
		return 0;		/* this is the correct answer anyway */
	}
#endif

	/* If op > 2^31, the result is 2^31 - 1.
	 * If op < -2^31, the result is -2^31.
	 * Otherwise, result is simple conversion from jdouble to integer.
	 */
	if (src >= 2147483648.0) {
		result = 0x7FFFFFFF;
	} else if (src <= -2147483648.0) {
		result = (I_32)(((U_32)-1) << 31);
	} else {
		result = (I_32)src;
	}
	
	return result;
}


void 
helperConvertDoubleToInteger(jdouble *srcPtr, I_32 *dstPtr)
{
	*dstPtr = helperCConvertDoubleToInteger(*srcPtr);
}


I_64 
helperCConvertDoubleToLong(jdouble src)
{
	I_64 result;

	if (IS_NAN_DBL(src)) {
		return 0;
	}
#ifdef J9_NO_DENORMAL_FLOAT_SUPPORT
	if (IS_DENORMAL_DBL(src)) {
		return 0;
	}
#endif

	/* If op > 2^63, the result is 2^63 - 1.
	 * If op < -2^63, the result is -2^63.
	 * Otherwise, result is simple conversion from jdouble to long.
	 */
	if (src >= 9223372036854775808.0) {
		result = J9CONST64(0x7FFFFFFFFFFFFFFF);
	} else if (src <= -9223372036854775808.0) {
		result = J9CONST64(0x8000000000000000);
	} else {
		result = (I_64)src;
	}

	return result;
}


void 
helperConvertDoubleToLong(jdouble *srcPtr, I_64 *dstPtr)
{
	*dstPtr = helperCConvertDoubleToLong(*srcPtr);
}


jdouble 
helperCConvertFloatToDouble(jfloat src)
{

#if defined(J9_NO_DENORMAL_FLOAT_SUPPORT)
	if (IS_DENORMAL_SNGL(src))  {
		jdouble result;
		/* SINGLE_STORE_POS_ZERO(dst);	 return incorrect, but non crashing answer (zero) */
		convertFloatToDouble(src, &result);
		return result;
	}
#endif

	return (jdouble)src;
}


void 
helperConvertFloatToDouble(jfloat *srcPtr, jdouble *dstPtr)
{
	jfloat tmpSrc;
	jdouble tmpDst;
	
	PTR_SINGLE_VALUE(srcPtr, &tmpSrc);
	tmpDst = helperCConvertFloatToDouble(tmpSrc);
	PTR_DOUBLE_STORE(dstPtr, &tmpDst);
}


I_32 
helperCConvertFloatToInteger(jfloat src)
{
	I_32 result;

	if (IS_NAN_SNGL(src)) {
		return 0;
	}
#ifdef J9_NO_DENORMAL_FLOAT_SUPPORT
	if (IS_DENORMAL_SNGL(src)) {
		return 0;				/* this is the correct answer anyway */
	}
#endif

	/* If op > 2^31, the result is 2^31 - 1
	 * If op < -2^31, the result is -2^31 
	 * Otherwise, result is simple conversion of float to integer.
	 */
	if (src >= 2147483648.0) {
		result = 0x7FFFFFFF;
	}else if (src <= -2147483648.0) {
		result = (I_32)(((U_32)-1) << 31);
	} else {
		result = (I_32)src;
	}
	
	return result;
}


void 
helperConvertFloatToInteger(jfloat *srcPtr, I_32 *dstPtr)
{
	jfloat tmpSrc;
	I_32 tmpDst;

	PTR_SINGLE_VALUE(srcPtr, &tmpSrc);
	tmpDst = helperCConvertFloatToInteger(tmpSrc);
	PTR_SINGLE_STORE(dstPtr, &tmpDst);
}


I_64 
helperCConvertFloatToLong(jfloat src)
{
	I_64 result;

	if (IS_NAN_SNGL(src)) {
		return 0;
	}
#ifdef J9_NO_DENORMAL_FLOAT_SUPPORT
	if (IS_DENORMAL_SNGL(src)) {
		return 0;
	}
#endif

	/* If op > 2^63, the result is 2^63 - 1.
	 * If op < -2^63, the result is -2^63.
	 * Otherwise, the result is a simple conversion from float to long.
	 */
	if (src >= 9223372036854775808.0) {
		result = J9CONST64(0x7FFFFFFFFFFFFFFF);
	} else if (src <= -9223372036854775808.0) {
		result = J9CONST64(0x8000000000000000);
	} else {
		result = (I_64)src;
	}

	return result;
}


void 
helperConvertFloatToLong(jfloat *srcPtr, I_64 *dstPtr)
{
	jfloat tmpSrc;
	I_64 tmpDst;

	PTR_SINGLE_VALUE(srcPtr, &tmpSrc);
	tmpDst = helperCConvertFloatToLong(tmpSrc);
	PTR_LONG_STORE(dstPtr, &tmpDst);
}


jdouble 
helperCConvertIntegerToDouble(I_32 src)
{
	return (jdouble)src;
}


void 
helperConvertIntegerToDouble(I_32 *srcPtr, jdouble *dstPtr)
{
	jdouble tmpDst;
	
	tmpDst = helperCConvertIntegerToDouble(*srcPtr);
	PTR_DOUBLE_STORE(dstPtr, &tmpDst);
}


jfloat 
helperCConvertIntegerToFloat(I_32 src)
{
	jfloat tmpDst;

#if defined(USE_NATIVE_CAST) || defined(ARM)
	/* Enabling native cast for ARM avoids a compiler bug in
	 * gcc-linaro-4.9.4-2017.01
	 */
	tmpDst = (jfloat)src;
#else
	{
		int idl1, sign;
		U_32 spfInt, overflow;
	
		if (src == 0) {
			return 0.0f;
		}
	
		if (src < 0) {
			spfInt = (U_32)(0 - src);
			sign = 1;
		} else {
			spfInt = (U_32)src;
			sign = 0;
		}
		
		/* Find out where the most significant bit is in the integer value.
		 * We are only interested in keeping 24 of those bits. 
		 */
		idl1 = fltconv_indexLeadingOne32(spfInt);
		if (idl1 >= 24) {
			
			/* If it's more than 24 bits, we shift right and keep track of
			 * the overflow for some possible rounding. 
			 */
			overflow = spfInt << (32 - (idl1 - 23));
			spfInt >>= (idl1 - 23);
			spfInt &= 0x007FFFFF;
			spfInt |= ((idl1+SPEXPONENT_BIAS) << 23);
			if ((overflow & 0x80000000) != 0) {
				if ((overflow & 0x7FF00000) != 0) {
					spfInt++;
				} else {
					if((spfInt & 1) != 0) spfInt++;
				}
			}
		} else if (idl1 < 23) {
			
			/* If it's less than 24 bits, we shift left, and no
			 * overflow and rounding to worry about. 
			 */
			spfInt <<= (23 - idl1);
			spfInt &= 0x007FFFFF;
			spfInt |= ((idl1+SPEXPONENT_BIAS) << 23);
		} else {
			
			/* It must be exactly 24 bits, so no shift is required. 
			 */
			spfInt &= 0x007FFFFF;
			spfInt |= ((idl1+SPEXPONENT_BIAS) << 23);
		}
		if (sign) {
			spfInt |= 0x80000000;
		}
		
		/* Cast tmpDst to an integer to avoid float conversion.
		 */
		*((U_32 *)&tmpDst) = spfInt;
	}
#endif
	
	return tmpDst;
}


void 
helperConvertIntegerToFloat(I_32 *srcPtr, jfloat *dstPtr)
{
	I_32 tmpSrc = *srcPtr;
	jfloat tmpDst;

	tmpDst = helperCConvertIntegerToFloat(tmpSrc);
	PTR_SINGLE_STORE(dstPtr, &tmpDst);
}


jdouble 
helperCConvertLongToDouble(I_64 src)
{

	return (jdouble)src;
}


void 
helperConvertLongToDouble(I_64 *srcPtr, jdouble *dstPtr)
{
	I_64 tmpSrc;
	jdouble tmpDst;

	PTR_LONG_VALUE(srcPtr, &tmpSrc);
	tmpDst = helperCConvertLongToDouble(tmpSrc);
	PTR_DOUBLE_STORE(dstPtr, &tmpDst);

}


jfloat 
helperCConvertLongToFloat(I_64 src)
{
	jfloat tmpDst;

	tmpDst = (jfloat)src;

	return tmpDst;
}


void 
helperConvertLongToFloat(I_64 *srcPtr, jfloat * dstPtr)
{
	I_64 tmpSrc;
	jfloat tmpDst;

	PTR_LONG_VALUE(srcPtr, &tmpSrc);
	tmpDst = helperCConvertLongToFloat(tmpSrc);
	PTR_SINGLE_STORE(dstPtr, &tmpDst);

}


static int 
fltconv_indexLeadingOne32(U_32 u32val)
{
	int leading;
	U_32 mask;
	
	if (u32val == 0) {
		return -1;
	}
	if (u32val & 0xFF000000) {
		leading = 31;
		mask = 0x80000000;
	} else if (u32val & 0x00FF0000) {
		leading = 23;
		mask = 0x00800000;
	} else if (u32val & 0x0000FF00) {
		leading = 15;
		mask = 0x00008000;
	} else {
		leading = 7;
		mask = 0x00000080;
	}
	/* WARNING This loop will busy hang when compiled with
	 * gcc-linaro-4.9.4-2017.01 for armhf if the match should happen on
	 * the first iteration. The loop exit condition is moved to the end of
	 * the loop and doesn't execute before the first shift of mask.
	 */
	while((mask & u32val) == 0) {
		mask >>= 1;
		leading--;
	}
	return leading;
}


#endif /* J9VM_INTERP_FLOAT_SUPPORT */ /* End File Level Build Flags */
