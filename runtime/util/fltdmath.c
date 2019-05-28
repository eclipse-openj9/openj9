/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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



static int x = 0; /* avoid empty file warning */

#if defined(WIN32) || defined(J9X86) || defined(J9_NO_DENORMAL_FLOAT_SUPPORT) || (defined(LINUXPPC)&&defined(HARDHAT)) || defined(ARMGNU) || defined(J9ZTPF)

#include <stdlib.h>
#include <stdio.h>

#include "fltconst.h"
#include "fltdmath.h"
#include "util_internal.h"


static void floatToInt (float f, U_32 * ip);
static int roundToNearest128NBits (U_64 *lup, U_64 *llp, int numBits);
void shiftLeft64 (U_64 *lp, U_64 *linp, int e);
static void oldAddDF (float f1, float f2, float *rp);
static void oldAddDD (double d1, double d2, double *rp);
static void roundDoubleAdd ( double *rp, U_64 truncValueSum, int aediff );
static void roundDoubleSubtract ( double *rp, U_64 truncValueSum, int aediff );
static void oldSubDD (double d1, double d2, double *rp);
static void make64From128 (U_64 *lup, U_64 *llp);
static void oldSubDF (float f1, float f2, float *rp);
static void scaleDownFloat (float *fp, int s);
static CANONICALFP canonicalAdd ( CANONICALFP u, CANONICALFP v );
static void oldMultiplyDD (double d1, double d2, double *rp);
static void scaleDownDouble (double *dp, int s);
static void shiftRight64RoundNearest (U_64 *lp, int e);
static void oldDivideDD (double d1, double d2, double *rp);
static U_32 numOverflowedBits32 (U_32 bits);
static CANONICALFP convertDoubleToCanonical (double u);
static void intToFloat (U_32 i, U_32 *truncValue, int e, float *rp);
static int indexLeadingOne32 (int *ip);
static int indexLeadingOne64 (U_64 * lp);
static U_32 make32From64 (U_64 * lp);
static int roundToNearestNBits (U_64 bValue, int numBits, int lsb);
static void truncateDouble (double *rp);
static U_32 shiftRight32 (U_32 * ip, int e);
static CANONICALFP canonicalSubtract ( CANONICALFP u, CANONICALFP v );
static U_64 shiftRight64 (U_64 *lp, int e);
static void shiftRight32RoundNearest (U_32 * ip, int e);
static void simpleRounding ( U_64 *rMant, U_64 *overflow );
static CANONICALFP canonicalMultiply ( CANONICALFP u, CANONICALFP v );
static void longToFloat (U_64 * lp, int e, float *rp);
static void longlongToDouble (U_64 * lup, U_64 * llp, int e, double *rp);
static float convertCanonicalToFloat (CANONICALFP r);
static void truncateToZeroFloat (U_32 i, int e, float *rp);
void scaleUpDouble (double *dp, int s);
static CANONICALFP convertFloatToCanonical (float u);
static CANONICALFP canonicalDivide ( CANONICALFP u, CANONICALFP v );
static void doubleToLong (double d, U_64 *lp);
static void scaleUpFloat (float *fp, int s);
static int shiftRight128RoundNearest (U_64 *lup, U_64 *llp, int e);
static int roundToNearest (double *dp);
static void simpleNormalizeAndRound (double *result, I_32 rSign, I_32 rExp, U_64 rMant, U_64 overflow);
static void longToDouble (U_64 l, U_64 *truncValue, int e, double *rp);
static void shiftLeft32 (U_32 * ip, U_32 *linp, int e);
static void shiftLeft128Truncate (U_64 * lup, U_64 * llp, int e, int leading);
static void canonicalNormalize ( CANONICALFP *a );
static int roundToNearest32NBits (U_32 bValue, int numBits, int lsb);
static void truncateToZeroDouble (U_64 *lp, int e, double *rp);


static void floatToInt(float f, U_32 * ip)
{
	int e;
	U_32 i;

	e = GET_SP_EXPONENT(f);
	i = GET_SP_MANTISSA(f);
	/* if the number is normalized, insert a leading 1 in position SPFRACTION_LENGTH vacated by exponent */
	if (e > 0)
		i = SET_SPNORMALIZED_BIT(i);
	*ip = i;
}



static void scaleUpFloat(float *fp, int s)
{
	int e;
	int leading, sl, sn;
	float f = *fp;

	e = GET_SP_EXPONENT(f);
	sn = GET_SP_SIGN(f);

	if (e == 0) {
		FLOAT_WORD(f) = SET_SP_SIGN(f, 0);
		leading = indexLeadingOne32((U_32 *) & f);
		sl = (s > (SPFRACTION_LENGTH - leading)) ? SPFRACTION_LENGTH - leading : s;
		shiftLeft32((U_32 *)&f, NULL, SPFRACTION_LENGTH - leading);
		if ( s > (SPFRACTION_LENGTH - leading)) e = 1;
		s -= SPFRACTION_LENGTH - leading;
	}
	e += s;
	FLOAT_WORD(f) = SET_SP_EXPONENT(f, e);
	FLOAT_WORD(f) = SET_SP_SIGN(f, sn);
	*fp = f;
}



static void scaleDownFloat(float *fp, int s)
{
	int e;
	float f = *fp;

	e = GET_SP_EXPONENT(f);

	if (e > 0 && (e + s) > 0) {
		FLOAT_WORD(f) = SET_SP_EXPONENT(f, e + s);
	} else {
		if (e > 0) {
			/* clear exponent, add normalization bit */
			FLOAT_WORD(f) = SET_SP_EXPONENT(f, 0);
			FLOAT_WORD(f) = SET_SPNORMALIZED_BIT(f);
			shiftRight32RoundNearest((U_32 *)&f, e + s - 1);
		} else {
			shiftRight32RoundNearest((U_32 *)&f, e + s);
		}
	}
	*fp = f;
}




static U_32 make32From64(U_64 * lp)
{
	int leading = 0;
	U_64 tbits=0;
	unsigned int f;

	leading = indexLeadingOne64(lp);

	if (leading < 32 + SPFRACTION_LENGTH) {
		shiftLeft64(lp, NULL, 32 + SPFRACTION_LENGTH - leading);
	}

	FLOAT_WORD(f) = HIGH_U32_FROM_LONG64_PTR(lp);

	/* truncation is whats left in ilp, see if we need to add to result for round to nearest */
    LOW_U32_FROM_LONG64(tbits) = LOW_U32_FROM_LONG64_PTR(lp);
	f = f + roundToNearestNBits(tbits, sizeof(unsigned int) * 8, f & 0x1);
	return f;
}



static void shiftLeft32(U_32 * ip, U_32 *linp, int e)
{
	if (e == 0) return;
	if (e >= 32) {
		*ip = 0;
		if ( linp ) {
			*ip = *linp;
			*linp = 0;
			if ( e >= 64 ) {
				*ip = 0;
				return;
			}
			*ip = *ip << (e - 32);
		}
		return;
	}

	*ip = *ip << e;
	if ( linp ) {
		*ip |= *linp >> (32 - e);
		*linp = *linp << e;
	}
}



static void shiftRight32RoundNearest(U_32 * ip, int e)
{

	int be = e, count = 0, trunc = 0;
	U_32 bValue, lsb;
	int mbe = -e;

	if (e < -32) {
		*ip = 0;
		return;
	}
	lsb = ((((U_32) 1) << (mbe)) & (*ip)) >> (mbe);
	bValue = ((((U_32) 1) << (mbe)) - 1) & (*ip);
	trunc = roundToNearest32NBits(bValue, -be, lsb);

	while (be < 0) {
		*(ip) = (*(ip)) >> 1;
		be++;
		count++;
	}
	if (trunc == 1) {
		*(ip) = *(ip) + 1;
	}
}



static U_32 shiftRight32(U_32 * ip, int e)
{

	int mbe = -e;
	U_32 bValue = *ip;

	if (mbe == 0) return 0;
	if (mbe >= 32) {
		*ip = 0;
		if (mbe >= 64) return 0;
		return (bValue >> (mbe - 32));
	}

	bValue <<= (32 - mbe);
	*ip >>= mbe;

	return bValue;
}



static int roundToNearest32NBits(U_32 bValue, int numBits, int lsb)
{
	U_32 twon;
	double half;

	twon = (((U_32) 1) << numBits);
	half = (((double) bValue) / twon);

	if (half > 0.5) {
		return 1;
	} else if (half < 0.5) {
		return 0;
	} else {
		/* equals 0.5 */
		return lsb;
	}

}




static int roundToNearest(double *dp)
{
	int trb, diff;

	trb = GET_TRUNCATED_BITS(dp);
	diff = DPTRUNCATED_MASK - trb;
	if (trb > diff)
		return 1;
	else
		return 0;
}



static int roundToNearestNBits(U_64 bValue, int numBits, int lsb)
{

	U_64 twonm1;
	U_64 twonm1m1;

	if (numBits == 0) {
		return (bValue) ? 1 : 0;
	}

	twonm1 = (((U_64) 1) << (numBits - 1));
	if (bValue & twonm1) {
		/* >= 0.5 */
		twonm1m1 = twonm1 - 1;
		if (bValue & twonm1m1) {
			return 1;
		} else {
			/* == 0.5 */
			return lsb;
		}
	} else {
		return 0;
	}

}




static void make64From128(U_64 *lup, U_64 *llp)
{
	U_64 lupValue, llpValue;
	int leading = 0;
	U_64 tbits=0;
	int lsb;

	/* Certain platforms do not handle de-referencing a 64-bit value
	 * from a pointer passed in from another function on the stack
	 * correctly (e.g. XScale) because the pointer may not be
	 * properly aligned, so we'll have to handle two 32-bit chunks. */
	HIGH_U32_FROM_LONG64(lupValue) = HIGH_U32_FROM_LONG64_PTR(lup);
	LOW_U32_FROM_LONG64(lupValue) = LOW_U32_FROM_LONG64_PTR(lup);
	HIGH_U32_FROM_LONG64(llpValue) = HIGH_U32_FROM_LONG64_PTR(llp);
	LOW_U32_FROM_LONG64(llpValue) = LOW_U32_FROM_LONG64_PTR(llp);

	if (lupValue == 0) {
		HIGH_U32_FROM_LONG64_PTR(lup) = HIGH_U32_FROM_LONG64_PTR(llp);
		LOW_U32_FROM_LONG64_PTR(lup) = LOW_U32_FROM_LONG64_PTR(llp);
		HIGH_U32_FROM_LONG64_PTR(llp) = 0;
		LOW_U32_FROM_LONG64_PTR(llp) = 0;
		leading = indexLeadingOne64(lup);
		if (leading < DPFRACTION_LENGTH) {
			shiftLeft64(lup, NULL, DPFRACTION_LENGTH - leading);
		}
		if (leading > DPFRACTION_LENGTH) {
			shiftRight64RoundNearest(lup, DPFRACTION_LENGTH - leading);
		}
	} else {
		leading = indexLeadingOne64(lup);
		if (leading < DPFRACTION_LENGTH) {
			shiftLeft128Truncate(lup, llp, DPFRACTION_LENGTH - leading, leading);
		}
	}
	/* truncation is whats left in llp, see if we need to add to lrp for round to nearest */
	LOW_U32_FROM_LONG64(tbits) = LOW_U32_FROM_LONG64_PTR(llp);
	HIGH_U32_FROM_LONG64(tbits) = HIGH_U32_FROM_LONG64_PTR(llp);
	lsb = (int) (lupValue & 0x1);

	lupValue += roundToNearestNBits(tbits, sizeof(unsigned int) * 8 *2, lsb);
	HIGH_U32_FROM_LONG64_PTR(lup) = HIGH_U32_FROM_LONG64(lupValue);
	LOW_U32_FROM_LONG64_PTR(lup) = LOW_U32_FROM_LONG64(lupValue);
}




static void shiftLeft128Truncate(U_64 * lup, U_64 * llp, int e, int leading)
{
	int count = 0;

	while (count < e) {
		HIGH_U32_FROM_LONG64_PTR(lup) <<= 1;
		if (LOW_U32_FROM_LONG64_PTR(lup) & 0x80000000) {
			HIGH_U32_FROM_LONG64_PTR(lup) |= 1;
		}
		LOW_U32_FROM_LONG64_PTR(lup) <<= 1;
		if (HIGH_U32_FROM_LONG64_PTR(llp) & 0x80000000) {
			LOW_U32_FROM_LONG64_PTR(lup) |= 1;
		}

		HIGH_U32_FROM_LONG64_PTR(llp) <<= 1;
		if (LOW_U32_FROM_LONG64_PTR(llp) & 0x80000000) {
			HIGH_U32_FROM_LONG64_PTR(llp) |= 1;
		}
		LOW_U32_FROM_LONG64_PTR(llp) <<= 1;

		count++;
	}
}



static U_64 shiftRight64(U_64 *lp, int e)
{
	int mbe = -e;
	U_64 b1Value, b2Value;

	/* Certain platforms do not handle de-referencing a 64-bit value
	 * from a pointer on the stack correctly (e.g. XScale)
	 * because the pointer may not be properly aligned, so we'll have
	 * to handle two 32-bit chunks. */
	HIGH_U32_FROM_LONG64(b1Value) = HIGH_U32_FROM_LONG64_PTR(lp);
	LOW_U32_FROM_LONG64(b1Value) = LOW_U32_FROM_LONG64_PTR(lp);
	b2Value = b1Value;

	if (mbe == 0) return 0;
	if (mbe >= 64) {
		HIGH_U32_FROM_LONG64_PTR(lp) = 0;
		LOW_U32_FROM_LONG64_PTR(lp) = 0;
		if (mbe >= 128) return 0;
		return (b1Value >> (mbe - 64));
	}

	b1Value <<= (64 - mbe);
	b2Value >>= mbe;

	/* Again, certain platforms do not handle de-referencing a 64-bit
	 * value from a pointer on the stack correctly (e.g. XScale)
	 * because the pointer may not be properly aligned, so we'll have
	 * to handle two 32-bit chunks. */
	HIGH_U32_FROM_LONG64_PTR(lp) = HIGH_U32_FROM_LONG64(b2Value);
	LOW_U32_FROM_LONG64_PTR(lp) = LOW_U32_FROM_LONG64(b2Value);

	return b1Value;
}



static void shiftRight64RoundNearest(U_64 *lp, int e)
{
	int be = e, count = 0, trunc = 0;
	U_64 bValue, lsb, lpValue;
	int mbe = -e;

	/* Certain platforms do not handle de-referencing a 64-bit value
	 * from a pointer on the stack correctly (e.g. XScale)
	 * because the pointer may not be properly aligned, so we'll have
	 * to handle two 32-bit chunks. */
	HIGH_U32_FROM_LONG64(lpValue) = HIGH_U32_FROM_LONG64_PTR(lp);
	LOW_U32_FROM_LONG64(lpValue) = LOW_U32_FROM_LONG64_PTR(lp);

	if (e < -64) {
		HIGH_U32_FROM_LONG64_PTR(lp) = 0;
		LOW_U32_FROM_LONG64_PTR(lp) = 0;
		return;
	}
	lsb = ((((U_64) 1) << (mbe)) & lpValue) >> (mbe);
	bValue = ((((U_64) 1) << (mbe)) - 1) & lpValue;
	trunc = roundToNearestNBits(bValue, -be, (int)lsb);

	while (be < 0) {
		LOW_U32_FROM_LONG64_PTR(lp) >>= 1;
		if (HIGH_U32_FROM_LONG64_PTR(lp) & 1) {
			LOW_U32_FROM_LONG64_PTR(lp) |= 0x80000000;
		}
		HIGH_U32_FROM_LONG64_PTR(lp) >>= 1;
		be++;
		count++;
	}
	if (trunc == 1) {
		HIGH_U32_FROM_LONG64(lpValue) = HIGH_U32_FROM_LONG64_PTR(lp);
		LOW_U32_FROM_LONG64(lpValue) = LOW_U32_FROM_LONG64_PTR(lp);
		lpValue++;
		HIGH_U32_FROM_LONG64_PTR(lp) = HIGH_U32_FROM_LONG64(lpValue);
		LOW_U32_FROM_LONG64_PTR(lp) = LOW_U32_FROM_LONG64(lpValue);
	}
}



void shiftLeft64(U_64 *lp, U_64 *linp, int e)
{
	U_64 lpValue;

	if (e == 0) return;
	if (e >= 64) {
		/*	*lp = 0;	*/
		HIGH_U32_FROM_LONG64_PTR(lp) = 0;
		LOW_U32_FROM_LONG64_PTR(lp) = 0;
		if ( linp ) {
			/*	*lp = *linp;	*/
			HIGH_U32_FROM_LONG64_PTR(lp) = HIGH_U32_FROM_LONG64_PTR(linp);
			LOW_U32_FROM_LONG64_PTR(lp) = LOW_U32_FROM_LONG64_PTR(linp);
			/*	*linp = 0;	*/
			HIGH_U32_FROM_LONG64_PTR(linp) = 0;
			LOW_U32_FROM_LONG64_PTR(linp) = 0;
			if ( e >= 128 ) {
				/*	*lp = 0;	*/
				HIGH_U32_FROM_LONG64_PTR(lp) = 0;
				LOW_U32_FROM_LONG64_PTR(lp) = 0;
				return;
			}
			/*	*lp = *lp << (e - 64);	*/
			HIGH_U32_FROM_LONG64(lpValue) = HIGH_U32_FROM_LONG64_PTR(lp);
			LOW_U32_FROM_LONG64(lpValue) = LOW_U32_FROM_LONG64_PTR(lp);
			lpValue <<= e - 64;
			HIGH_U32_FROM_LONG64_PTR(lp) = HIGH_U32_FROM_LONG64(lpValue);
			LOW_U32_FROM_LONG64_PTR(lp) = LOW_U32_FROM_LONG64(lpValue);
		}
		return;
	}

	HIGH_U32_FROM_LONG64(lpValue) = HIGH_U32_FROM_LONG64_PTR(lp);
	LOW_U32_FROM_LONG64(lpValue) = LOW_U32_FROM_LONG64_PTR(lp);

	/*
		*lp = *lp << e;
		if ( linp ) {
			*lp |= *linp >> (64 - e);
			*linp = *linp << e;
		}
	*/
	lpValue <<= e;
	HIGH_U32_FROM_LONG64_PTR(lp) = HIGH_U32_FROM_LONG64(lpValue);
	LOW_U32_FROM_LONG64_PTR(lp) = LOW_U32_FROM_LONG64(lpValue);
	if( linp ) {
		U_64 linpValue;
		HIGH_U32_FROM_LONG64(linpValue) = HIGH_U32_FROM_LONG64_PTR(linp);
		LOW_U32_FROM_LONG64(linpValue) = LOW_U32_FROM_LONG64_PTR(linp);
		lpValue |= linpValue >> (64 - e);
		linpValue <<= e;
		HIGH_U32_FROM_LONG64_PTR(lp) = HIGH_U32_FROM_LONG64(lpValue);
		LOW_U32_FROM_LONG64_PTR(lp) = LOW_U32_FROM_LONG64(lpValue);
		HIGH_U32_FROM_LONG64_PTR(linp) = HIGH_U32_FROM_LONG64(linpValue);
		LOW_U32_FROM_LONG64_PTR(linp) = LOW_U32_FROM_LONG64(linpValue);
	}
}



static int shiftRight128RoundNearest(U_64 *lup, U_64 *llp, int e)
{
	int count = 0, be=e;
	int trunc=0;

	if (e <= -128) {
		/*	*lup = 0;
			*llp = 0;
		 */
		HIGH_U32_FROM_LONG64_PTR(lup) = 0;
		LOW_U32_FROM_LONG64_PTR(lup) = 0;
		HIGH_U32_FROM_LONG64_PTR(llp) = 0;
		LOW_U32_FROM_LONG64_PTR(llp) = 0;
		return 0;
	}

	trunc = roundToNearest128NBits(lup, llp, -e);

	while (be < 0) {

		LOW_U32_FROM_LONG64_PTR(llp) >>= 1;
		if (HIGH_U32_FROM_LONG64_PTR(llp) & 1) {
			LOW_U32_FROM_LONG64_PTR(llp) = HIGH_U32_FROM_LONG64_PTR(llp) | 0x80000000;
		}
		HIGH_U32_FROM_LONG64_PTR(llp) >>= 1;

		LOW_U32_FROM_LONG64_PTR(lup) >>= 1;
		if (HIGH_U32_FROM_LONG64_PTR(lup) & 1) {
			LOW_U32_FROM_LONG64_PTR(lup) = HIGH_U32_FROM_LONG64_PTR(lup) | 0x80000000;
		}
		HIGH_U32_FROM_LONG64_PTR(lup) >>= 1;

		be++;
		count++;
	}

	return trunc;
}



static int roundToNearest128NBits(U_64 *lup, U_64 *llp, int numBits)
{
	U_64 twonm1, twonm1m1, twon;
	U_64 lupValue, llpValue;
	

	if (numBits == 0) {
		return (LOW_U32_FROM_LONG64_PTR(llp) & 0x1) ? 1 : 0;
	}

	HIGH_U32_FROM_LONG64(lupValue) = HIGH_U32_FROM_LONG64_PTR(lup);
	LOW_U32_FROM_LONG64(lupValue) = LOW_U32_FROM_LONG64_PTR(lup);
	HIGH_U32_FROM_LONG64(llpValue) = HIGH_U32_FROM_LONG64_PTR(llp);
	LOW_U32_FROM_LONG64(llpValue) = LOW_U32_FROM_LONG64_PTR(llp);

	if (numBits >= 128) {
		return (llpValue | lupValue) ? 1 : 0; /*(*(llp) | *(lup) ) ? 1 : 0;*/
	}

	if(numBits <= 64 ) {
		twonm1 = (((U_64) 1) << (numBits - 1));
		if( llpValue & twonm1 /*(*llp) & twonm1 */ ) {
			/* >= 0.5 */
			twonm1m1 = twonm1 - 1;
			if( llpValue & twonm1m1 /*(*llp) & twonm1m1 */ ) {
				return 1;
			} else {
				/* == 0.5 */
				if( twonm1 << 1 )
					return	(llpValue & (twonm1 << 1)) ? 1 : 0; /*((*llp) & (twonm1 << 1)) ? 1 : 0;*/
				else
					return (int)(LOW_U32_FROM_LONG64_PTR(lup) & 1); /*(*lup & 0x1);*/
			}
		} else {
			return 0;
		}
	} else {
		/* > 64 */
		twon =	(((U_64) 1) << (numBits - 64));
		twonm1 = twon >> 1;
		if (twonm1 != 0 ) {
			if( lupValue & twonm1 /*(*lup) & twonm1*/ ) {
				/* >= 0.5 */
				twonm1m1 = twonm1 - 1;
				if( twonm1m1 ) {
					if( (lupValue & twonm1m1) | llpValue /*((*lup) & twonm1m1) | (*llp)*/ ) {
						return 1;
					} else {
						/* == 0.5 */
						return (lupValue & twon) ? 1 : 0; /*((*lup) & (twon)) ? 1 : 0;*/
					}
				} else {
					if ( llpValue /* *llp */ ) return 1;
					else return (lupValue & twon) ? 1: 0;  /*((*lup) & twon)? 1 : 0;*/
				}
			} else {
				return 0;
			}
		} else {
			twonm1 = 0x80000000;
			if( llpValue & twonm1 /*(*llp) & twonm1*/ ) {
				/* >= 0.5 */
				twonm1m1 = twonm1 - 1;
				if( llpValue & twonm1m1 /*(*llp) & twonm1m1*/ ) {
					return 1;
				} else {
					return (int)(LOW_U32_FROM_LONG64_PTR(lup) & 1); /*(int) (*lup & 0x1);*/
				}
			} else {
				/* < 0.5 */
				return 0;
			}
		}
	}
}




static void scaleDownDouble(double *dp, int s)
{
	int e;
	U_64 l=0;

	e = GET_DP_EXPONENT(dp);

	if (e > 0 && (e + s) > 0) {
		HIGH_U32_FROM_DBL_PTR(dp) = SET_DP_EXPONENT(dp, e + s);
	} else {
		if (e > 0) {
			/* clear exponent, add normalization bit */
			HIGH_U32_FROM_DBL_PTR(dp) = SET_DP_EXPONENT(dp, 0);
			HIGH_U32_FROM_DBL_PTR(dp) = SET_DPNORMALIZED_BIT(dp);
			HIGH_U32_FROM_LONG64(l) = HIGH_U32_FROM_DBL_PTR(dp);
			LOW_U32_FROM_LONG64(l) = LOW_U32_FROM_DBL_PTR(dp);
			shiftRight64RoundNearest(&l, e + s - 1);
			HIGH_U32_FROM_DBL_PTR(dp) = HIGH_U32_FROM_LONG64(l);
			LOW_U32_FROM_DBL_PTR(dp) = LOW_U32_FROM_LONG64(l);
		} else {
			HIGH_U32_FROM_LONG64(l) = HIGH_U32_FROM_DBL_PTR(dp);
			LOW_U32_FROM_LONG64(l) = LOW_U32_FROM_DBL_PTR(dp);
			shiftRight64RoundNearest(&l, e + s);
			HIGH_U32_FROM_DBL_PTR(dp) = HIGH_U32_FROM_LONG64(l);
			LOW_U32_FROM_DBL_PTR(dp) = LOW_U32_FROM_LONG64(l);
		}
	}
}



void scaleUpDouble(double *dp, int s)
{
	int e;
	int leading, sl, sn;
	U_64 l=0;

	e = GET_DP_EXPONENT(dp);
	sn = GET_DP_SIGN(dp);
	if (e == 0) {
		HIGH_U32_FROM_LONG64(l) = HIGH_U32_FROM_DBL_PTR(dp);
		LOW_U32_FROM_LONG64(l) = LOW_U32_FROM_DBL_PTR(dp);		
		HIGH_U32_FROM_LONG64(l) = SET_LWDP_SIGN(&l, 0);
		leading = indexLeadingOne64(&l);
		sl = (s > (DPFRACTION_LENGTH - leading)) ? DPFRACTION_LENGTH - leading : s;
		shiftLeft64(&l, NULL, sl);
		HIGH_U32_FROM_DBL_PTR(dp) = HIGH_U32_FROM_LONG64(l);
		LOW_U32_FROM_DBL_PTR(dp) = LOW_U32_FROM_LONG64(l);
		if ( s > (DPFRACTION_LENGTH - leading)) e = 1;
		s -= sl;
	}
	e += s;
	HIGH_U32_FROM_DBL_PTR(dp) = SET_DP_EXPONENT(dp, e);
	HIGH_U32_FROM_DBL_PTR(dp) = SET_DP_SIGN(dp,sn); 

}



static int indexLeadingOne32(int *ip)
{
	int leading = 31;
	unsigned int mask;

	mask = 0x80000000;
	while (mask != 0 && IS_CLEAR((*ip), mask)) {
		mask = mask >> 1;
		leading--;
	}

	return leading;

}



static int indexLeadingOne64(U_64 * lp)
{
	int leading = 63;
	unsigned int mask;

	if (HIGH_U32_FROM_LONG64_PTR(lp) != 0) {
		mask = 0x80000000;
		while (mask != 0 && IS_CLEAR(HIGH_U32_FROM_LONG64_PTR(lp), mask)) {
			mask = mask >> 1;
			leading--;
		}
		return leading;
	}

	leading = 31;
	/* search LOWER word */
	mask = 0x80000000;
	while (mask != 0 && IS_CLEAR(LOW_U32_FROM_LONG64_PTR(lp), mask)) {
		mask = mask >> 1;
		leading--;
	}

	return leading;

}



static void truncateToZeroDouble(U_64 *lp, int e, double *rp)
{
	int be = e, ne=0;
	int leading = 0;
	int sl = 0, sr = 0;
	int slg0;
	U_64 mask=-1;

	leading = indexLeadingOne64(lp);

	if (leading > 52) {
		sr = leading - 52;
		shiftRight64RoundNearest(lp, -sr);
		be += sr;
	}

	/* shift left only till the normalization MSB 1 reaches the bit 20th of High word */
	if (e > 0) {
		if (leading < 52) {
			sl = 52 - leading;
			if (sl <= e)
				slg0 = sl;
			else
				slg0 = e;
			shiftLeft64(lp, NULL , slg0);
			be -= slg0;
		}
	}

	/* normalized, drop the fraction bits */
	if ( be > 0 && be < DPFRACTION_LENGTH ) {
		mask &=  (1 << (DPFRACTION_LENGTH - e)) - 1;
		(*lp) &= ~mask;
	}
	if (be <= 0) {
		leading = indexLeadingOne64(lp);
		if (leading >= 52) {
			shiftRight64RoundNearest(lp, be - 1);
			ne = GET_LWDP_EXPONENT(lp);
		}

		HIGH_U32_FROM_LONG64_PTR(lp) = SET_LWDP_EXPONENT(lp, ne);
		HIGH_U32_FROM_DBL_PTR(rp) = HIGH_U32_FROM_LONG64_PTR(lp);
		LOW_U32_FROM_DBL_PTR(rp) = LOW_U32_FROM_LONG64_PTR(lp);
	} else {
		HIGH_U32_FROM_LONG64_PTR(lp) = CLEAR_LWDPNORMALIZED_BIT(lp);
		HIGH_U32_FROM_LONG64_PTR(lp) = SET_LWDP_EXPONENT(lp, be);
		HIGH_U32_FROM_DBL_PTR(rp) = HIGH_U32_FROM_LONG64_PTR(lp);
		LOW_U32_FROM_DBL_PTR(rp) = LOW_U32_FROM_LONG64_PTR(lp);
	}

}



static void truncateToZeroFloat(U_32 i, int e, float *rp)
{
	int be = e;
	float r;
	int leading = 0;
	int sl = 0, sr = 0;
	int slg0;
	U_64 mask=-1;

	leading = indexLeadingOne32(&i);

	if (leading > 23) {
		sr = leading - 23;
		shiftRight32RoundNearest(&i, -sr);
		be += sr;
	}

	/* shift left only till the normalization MSB 1 reaches the bit 23rd of word */
	if (e > 0) {
		if (leading < 23) {
			sl = 23 - leading;
			if (sl <= e)
				slg0 = sl;
			else
				slg0 = e;
			shiftLeft32(&i, NULL, slg0);
			be -= slg0;
		}
	}

	/* normalized, drop the fraction bits */
	if ( be > 0 && be < SPFRACTION_LENGTH ) {
		mask &=  (1 << (SPFRACTION_LENGTH - e)) - 1;
		i &= ~mask;
	}

	if (be <= 0) {
		leading = indexLeadingOne32(&i);
		if (leading >= 23)
			shiftRight32RoundNearest(&i, be - 1);

		be = GET_SP_EXPONENT(i);	
		FLOAT_WORD(i) = SET_SP_EXPONENT(i, be);
		*rp = *(float *) &i;
	} else {
		FLOAT_WORD(i) = CLEAR_SPNORMALIZED_BIT(i);
		FLOAT_WORD(i) = SET_SP_EXPONENT(i, be);
		r = *(float *) &i;
		*rp = r;
	}

}




/* Does not include the sign bit in the returned value */
static void
doubleToLong(double d, U_64 *lp)
{
	int e;

	e = GET_DP_EXPONENT(&d);
	GET_DP_MANTISSA(&d, lp);
	/* if the number is normalized, insert a leading 1 in position DPFRACTION_LENGTH vacated by exponent */
	if (e > 0) {
		HIGH_U32_FROM_LONG64_PTR(lp) = SET_LWDPNORMALIZED_BIT(lp);
	}
}



static void longToDouble(U_64 l, U_64 *truncValue, int e, double *rp)
{
	int be = e, ne=0;
	int leading = 0;
	int sl = 0, sr = 0;
	int slg0;
	U_64 overflowedBits = 0, copyl = l;

	leading = indexLeadingOne64(&l);

	if (leading > 52) {
		sr = leading - 52;
		overflowedBits = shiftRight64(&copyl, -sr);
		*truncValue |= overflowedBits;
		shiftRight64RoundNearest(&l, -sr);
		be += sr;
	}

	/* shift left only till the normalization MSB 1 reaches the bit 20th of High word */
	if (e > 0) {
		if (leading < 52) {
			sl = 52 - leading;
			if (sl <= e)
				slg0 = sl;
			else
				slg0 = e;
			shiftLeft64(&l, truncValue, slg0);
			be -= slg0;
		}
	}

	if (be <= 0) {
		leading = indexLeadingOne64(&l);
		if (leading >= 52) {
			shiftRight64RoundNearest(&l, be - 1);
			ne = GET_LWDP_EXPONENT(&l);
		}

		HIGH_U32_FROM_LONG64(l) = SET_LWDP_EXPONENT(&l, ne);
		HIGH_U32_FROM_DBL_PTR(rp) = HIGH_U32_FROM_LONG64(l);
		LOW_U32_FROM_DBL_PTR(rp) = LOW_U32_FROM_LONG64(l);
	} else {
		/* Must check to see if the value still fits within the exponent and mantissa limit.
			If it is too large, return infinity. */
		if ( be <= (DPEXPONENT_BIAS << 1) ) {
			HIGH_U32_FROM_LONG64(l) = CLEAR_LWDPNORMALIZED_BIT(&l);
			HIGH_U32_FROM_LONG64(l) = SET_LWDP_EXPONENT(&l, be);
			HIGH_U32_FROM_DBL_PTR(rp) = HIGH_U32_FROM_LONG64(l);
			LOW_U32_FROM_DBL_PTR(rp) = LOW_U32_FROM_LONG64(l);
		} else {
			SET_DP_PINF(rp);
			*truncValue = 0;
		}
	}
}



static void intToFloat(U_32 i, U_32 *truncValue, int e, float *rp)
{
	int be = e;
	float r;
	int leading = 0;
	int sl = 0, sr = 0;
	int slg0;

	leading = indexLeadingOne32(&i);

	if (leading > 23) {
		sr = leading - 23;
		shiftRight32RoundNearest(&i, -sr);
		be += sr;
	}

	/* shift left only till the normalization MSB 1 reaches the bit 23rd of word */
	if (e > 0) {
		if (leading < 23) {
			sl = 23 - leading;
			if (sl <= e)
				slg0 = sl;
			else
				slg0 = e;
			shiftLeft32(&i, truncValue, slg0);
			be -= slg0;
		}
	}

	if (be <= 0) {
		leading = indexLeadingOne32(&i);
		if (leading >= 23)
			shiftRight32RoundNearest(&i, be - 1);

		be = GET_SP_EXPONENT(i);	
		FLOAT_WORD(i) = SET_SP_EXPONENT(i, be);
		*rp = *(float *) &i;
	} else {
		/* Must check to see if the value still fits within the exponent and mantissa limit.
			If it is too large, return infinity. */
		if ( be <= (SPEXPONENT_BIAS << 1) ) {
			FLOAT_WORD(i) = CLEAR_SPNORMALIZED_BIT(i);
			FLOAT_WORD(i) = SET_SP_EXPONENT(i, be);
			r = *(float *) &i;
			*rp = r;
		} else {
			SET_SP_PINF(rp);
			*truncValue = 0;
		}
	}
}



static void longToFloat(U_64 * lp, int e, float *rp)
{
	int be = e;
	float r;
	int leading = 0, nleading=0;
	int sl = 0, sr = 0;
	int slg0;
	U_32 i = 0;
	U_64 tbits = 0;

	leading = indexLeadingOne64(lp);

	if (leading > SPFRACTION_LENGTH + 32) {
		sr = leading - SPFRACTION_LENGTH - 32;
		shiftRight64RoundNearest(lp, -sr);
	}

	/* shift left only till the normalization MSB 1 reaches the bit 23rd of word */
	if (e > 0) {
		if (leading < SPFRACTION_LENGTH + 32) {
			sl = SPFRACTION_LENGTH + 32 - leading;
			slg0 = sl;
			shiftLeft64(lp, NULL, slg0);
			i = HIGH_U32_FROM_LONG64_PTR(lp);
			LOW_U32_FROM_LONG64(tbits) = LOW_U32_FROM_LONG64_PTR(lp);
			if (LOW_U32_FROM_LONG64_PTR(lp)) {
				i = i + roundToNearestNBits(tbits, sizeof(unsigned int) * 8, i & 0x1);
			}
			nleading = indexLeadingOne32(&i);
			if (nleading > SPFRACTION_LENGTH ) be +=1;
		}
	}
	if (be <= 0) {
		leading = indexLeadingOne64(lp);
		if (-be > (SPFRACTION_LENGTH + 32 - leading - 1)) {
			shiftRight64RoundNearest(lp, be + (SPFRACTION_LENGTH + 32 - leading - 1));
			i = HIGH_U32_FROM_LONG64_PTR(lp);
		} else {
			shiftLeft64(lp, NULL, be + (SPFRACTION_LENGTH + 32 - leading - 1));
			i = HIGH_U32_FROM_LONG64_PTR(lp);
			LOW_U32_FROM_LONG64(tbits) = LOW_U32_FROM_LONG64_PTR(lp);
			if (LOW_U32_FROM_LONG64_PTR(lp)) {
				i = i + roundToNearestNBits(tbits, sizeof(unsigned int) * 8, i & 0x1);
			}
			nleading = indexLeadingOne32(&i);
			if (nleading > SPFRACTION_LENGTH ) be +=1;
		}
		be = GET_SP_EXPONENT(i);
		FLOAT_WORD(i) = SET_SP_EXPONENT(i, be);
		*rp = *(float *) &i;
	} else {
		FLOAT_WORD(i) = CLEAR_SPNORMALIZED_BIT(i);
		if ( be <= (SPEXPONENT_BIAS << 1) ) {
			FLOAT_WORD(i) = SET_SP_EXPONENT(i, be);
			r = *(float *) &i;
			*rp = r;
		} else {
			SET_SP_PINF(rp);
		}
	}
}



static void longlongToDouble(U_64 * lup, U_64 * llp, int e, double *rp)
{
	int be = e;
	int leading = 0, nleading=0;
	int sl = 0;
	int slg0;
	int round=0;

	if (*lup == 0) {
		leading = indexLeadingOne64(llp);
	} else {
		leading = indexLeadingOne64(lup) + 64;
	}

	/* shift left only till the normalization MSB 1 reaches the bit 20th of High word */
	if (e > 0) {
		if (leading < DPFRACTION_LENGTH  + 64 ) {
			sl = DPFRACTION_LENGTH + 64  - leading;
			slg0 = sl;
			shiftLeft128Truncate(lup, llp, slg0, leading);
		}

		*lup = *lup + roundToNearest128NBits(lup, llp, sizeof(unsigned int) * 8 *2);
		nleading = indexLeadingOne64(lup);
		if (nleading > DPFRACTION_LENGTH) be +=1;
	}

	if (be <= 0) {
		if (-be > (DPFRACTION_LENGTH + 64 - leading - 1) ) {
			round = shiftRight128RoundNearest(lup, llp, be + (DPFRACTION_LENGTH + 64 - leading - 1));
			*lup = *lup + round;
		} else {
			shiftLeft128Truncate(lup, llp, be + (DPFRACTION_LENGTH + 64 - leading - 1), leading);
			if (*(llp)) {
				*lup = *lup + roundToNearest128NBits(lup, llp, sizeof(unsigned int) * 8 *2);
			}
			nleading = indexLeadingOne64(lup);
			if (nleading > DPFRACTION_LENGTH) be +=1;
		}
		HIGH_U32_FROM_DBL_PTR(rp) = HIGH_U32_FROM_LONG64_PTR(lup);
		LOW_U32_FROM_DBL_PTR(rp) = LOW_U32_FROM_LONG64_PTR(lup);
	} else {
		HIGH_U32_FROM_LONG64_PTR(lup) = CLEAR_LWDPNORMALIZED_BIT(lup);
		if ( be <= (DPEXPONENT_BIAS << 1) ) {
			HIGH_U32_FROM_LONG64_PTR(lup) = SET_LWDP_EXPONENT(lup, be);
			HIGH_U32_FROM_DBL_PTR(rp) = HIGH_U32_FROM_LONG64_PTR(lup);
			LOW_U32_FROM_DBL_PTR(rp) = LOW_U32_FROM_LONG64_PTR(lup);
		} else {
			SET_DP_PINF(rp);
		}
	}

}




int convertDoubleToFloat(double d, float *fp)
{
	float f;
	int e, es, sn, df;
	U_64 l;
	int roundup = 0;

	sn = GET_DP_SIGN(&d);
	e = GET_DP_EXPONENT(&d);
	e -= DPEXPONENT_BIAS;

	e += SPEXPONENT_BIAS;

	es = e;
	if (e < 0) {
		/* need to round to nearest before truncating to float */
		GET_DP_MANTISSA(&d, &l);
		HIGH_U32_FROM_DBL(l) = SET_DPNORMALIZED_BIT(&l);
		df = TRUNCATE_LONG_TO_FLOAT(l);
		roundup = roundToNearest(&d);
		/* fprintf(stderr,"roundToNearest:%d e=%d\n", roundup,e); */
		if (roundup)
			df = df + 1;
	} else {
		df = GET_FLOAT_BITS(d);
	}

	df = SET_SPNORMALIZED_BIT(df);

	if (e < 0) {
		while (e < 0) {
			df = df >> 1;
			e++;
		}
		if (es > -23)
			df = df >> 1;		/* the virtual integer */
		df = SET_SP_EXPONENT(df, e);
		df = SET_SP_SIGN(df, sn);
		f = *(float *) &df;
	} else {
		f = (float) d;
	}
	FLOAT_WORD_P(fp) = FLOAT_WORD(f); 
	return 0;
}



void convertFloatToDouble(float f, double *dp)
{
	double d = 0.0;
	int e, be, sn, df, leading;
	U_64 dmantissa;

	/* Check for some of these special cases before any conversion */
	if( IS_SP_ZERO(f) ) {
		if( IS_SP_POSITIVE(f) ){
			SETP_DP_PZERO(dp);
		} else {
			SETP_DP_NZERO(dp);
		}
		return;
	}

	if( IS_SP_INF(f) ) {
		if( IS_SP_POSITIVE(f) ) {
			SET_DP_PINF(dp);
		} else {
			SET_DP_NINF(dp);
		}
		return;
	}
    
	if( IS_SP_NAN(f) ) {
		SET_DP_NAN(dp);
		return;
	}

	sn = GET_SP_SIGN(f);
	e = GET_SP_EXPONENT(f);

	be = e - SPEXPONENT_BIAS;
	be += DPEXPONENT_BIAS;

	df = GET_SP_MANTISSA(f);

	if (e == 0) {
		leading = indexLeadingOne32(&df);
		df = (df << (23 - leading));
		be -= 23 - leading - 1;
	}

	dmantissa = (U_64)df;
	if( dmantissa > 0 ) {
		dmantissa <<= 29;
	}

	SET_DP_MANTISSA( &d, &dmantissa );
	HIGH_U32_FROM_DBL(d) = SET_DP_EXPONENT(&d, be);
	HIGH_U32_FROM_DBL(d) = SET_DP_SIGN(&d, sn);

	*dp = d;
	return;
}



void addDD( double u, double v, double *result) {
	I_32 uSign, vSign;
	CANONICALFP cu, cv, cResult;

	/* If either of the given numbers is NaN, return NaN */
	if( IS_NAN_DBL(u) || IS_NAN_DBL(v) ) {
		SET_NAN_DBL_PTR(result);
		return;
	}

	uSign = GET_DP_SIGN(&u);
	vSign = GET_DP_SIGN(&v);

	/* +Inf + +Inf ==> +Inf
	 * -Inf + -Inf ==> -Inf
	 * +Inf + -Inf ==> NaN
	 * -Inf + +Inf ==> NaN
	 */
	if( IS_INF_DBL(u) ) {
		if( IS_INF_DBL(v) ) {
			if( uSign == vSign ) {
				*result = u;
				return;
			} else {
				SET_NAN_DBL_PTR(result);
				return;
			}
		} else {
			*result = u;
			return;
		}
	} else {
		if( IS_INF_DBL(v) ) {
			*result = v;
			return;
		}
	}

	if( IS_ZERO_DBL(u) ) {
		if( IS_ZERO_DBL(v) ) {
			SET_PZERO_DBL_PTR(result);
			if( uSign == vSign ) {
				SET_DP_SIGN(result, uSign);
			}
		} else {
			*result = v;
		}
		return;
	}

	if( IS_ZERO_DBL(v) ) {
		*result = u;
		return;
	}

	cu = convertDoubleToCanonical(u);
	cv = convertDoubleToCanonical(v);
	cResult = canonicalAdd(cu, cv);

	simpleNormalizeAndRound(result, cResult.sign, cResult.exponent, cResult.mantissa, cResult.overflow);
}



void subDD(double u, double v, double *result) {
	I_32 uSign, vSign;
	CANONICALFP cu, cv, cResult;

	/* If either of the given numbers is NaN, return NaN */
	if( IS_NAN_DBL(u) || IS_NAN_DBL(v) ) {
		SET_NAN_DBL_PTR(result);
		return;
	}

	uSign = GET_DP_SIGN(&u);
	vSign = GET_DP_SIGN(&v);

	/* +Inf - +Inf ==> NaN
	 * +Inf - -Inf ==> +Inf
	 * -Inf - -Inf ==> NaN
	 * -Inf - +Inf ==> -Inf
	 */
	if( IS_INF_DBL(u) ) {
		if( IS_INF_DBL(v) ) {
			if( uSign == vSign ) {
				SET_NAN_DBL_PTR(result);
				return;
			} else {
				*result = u;
				return;
			}
		} else {
			*result = u;
			return;
		}
	} else {
		if( IS_INF_DBL(v) ) {
			if( vSign ) {
				SET_PINF_DBL_PTR(result);
			} else {
				SET_NINF_DBL_PTR(result);
			}
			return;
		}
	}

	if( IS_ZERO_DBL(u) ) {
		if( IS_ZERO_DBL(v) ) {
			if( uSign == vSign ) {
				SET_PZERO_DBL_PTR(result);
			} else {
				SET_PZERO_DBL_PTR(result);
				SET_DP_SIGN(result, uSign);
			}
		} else {
			*result = v;
			if( vSign ) {
				SET_DP_SIGN(result, 0);
			} else {
				SET_DP_SIGN(result, 1);
			}
		}
		return;
	}

	if( IS_ZERO_DBL(v) ) {
		*result = u;
		return;
	}

	cu = convertDoubleToCanonical(u);
	cv = convertDoubleToCanonical(v);
	cResult = canonicalSubtract(cu, cv);

	simpleNormalizeAndRound(result, cResult.sign, cResult.exponent, cResult.mantissa, cResult.overflow);
}



void multiplyDD( double m1, double m2, double *result ) {
	I_32 m1Sign, m2Sign;
	CANONICALFP cu, cv, cResult;

	/* If either of the given numbers is NaN, return NaN */
	if( IS_NAN_DBL(m1) || IS_NAN_DBL(m2) ) {
		SET_NAN_DBL_PTR(result);
		return;
	}

	m1Sign = GET_DP_SIGN(&m1);
	m2Sign = GET_DP_SIGN(&m2);

	/* Check for infinity */
	if (IS_INF_DBL(m1) || IS_INF_DBL(m2)) {
		if (IS_ZERO_DBL(m1) || IS_ZERO_DBL(m2)) {
			SET_NAN_DBL_PTR(result);
			return;
		}
		if (m1Sign == m2Sign) {
			SET_PINF_DBL_PTR(result);
		} else {
			SET_NINF_DBL_PTR(result);
		}
		return;
	}

	/* Check for zero */
	if (IS_ZERO_DBL(m1) || IS_ZERO_DBL(m2)) {
		if (m1Sign == m2Sign) {
			SET_PZERO_DBL_PTR(result);
		} else {
			SET_NZERO_DBL_PTR(result);
		}
		return;
	}

	/* Only one operand can be denormalized, else answer is zero */
	if (GET_DP_EXPONENT(&m1) == 0 && GET_DP_EXPONENT(&m2) == 0) {
		if (m1Sign == m2Sign) {
			SET_PZERO_DBL_PTR(result);
		} else {
			SET_NZERO_DBL_PTR(result);
		}
		return;
	}

	cu = convertDoubleToCanonical(m1);
	cv = convertDoubleToCanonical(m2);
	cResult = canonicalMultiply(cu, cv);

	simpleNormalizeAndRound(result, cResult.sign, cResult.exponent, cResult.mantissa, cResult.overflow);
}



void divideDD( double d1, double d2, double *result ) {
	I_32 d1Sign, d2Sign, resultSign;
	CANONICALFP cu, cv, cResult;

	/* If either of the given numbers is NaN, return NaN */
	if( IS_NAN_DBL(d1) || IS_NAN_DBL(d2) ) {
		SET_NAN_DBL_PTR(result);
		return;
	}

	d1Sign = GET_DP_SIGN(&d1);
	d2Sign = GET_DP_SIGN(&d2);
	resultSign = d1Sign ^ d2Sign;

	/* Check for infinity */
	if (IS_INF_DBL(d1) || IS_INF_DBL(d2)) {
		if (IS_INF_DBL(d1) && IS_INF_DBL(d2)) {
			SET_NAN_DBL_PTR(result);
			return;
		}
		if (IS_INF_DBL(d2)) {
			if (d1Sign == d2Sign) {
				SET_PZERO_DBL_PTR(result);
			} else {
				SET_NZERO_DBL_PTR(result);
			}
			return;
		}
		if (d1Sign == d2Sign) {
			SET_PINF_DBL_PTR(result);
		} else {
			SET_NINF_DBL_PTR(result);
		}
		return;
	}

	/* check for divide by zero */
	if (IS_ZERO_DBL(d2)) {
		if (IS_ZERO_DBL(d1)) {
			SET_NAN_DBL_PTR(result);
			return;
		}
		if (d1Sign == d2Sign) {
			SET_PINF_DBL_PTR(result);
		} else {
			SET_NINF_DBL_PTR(result);
		}
		return;
	}

	if (IS_ONE_DBL(d2)) {
		HIGH_U32_FROM_DBL_PTR(result) = HIGH_U32_FROM_DBL(d1);
		LOW_U32_FROM_DBL_PTR(result) = LOW_U32_FROM_DBL(d1);	
		SET_DP_SIGN(result, resultSign);
		return;
	}

	cu = convertDoubleToCanonical(d1);
	cv = convertDoubleToCanonical(d2);
	cResult = canonicalDivide(cu, cv);

	simpleNormalizeAndRound(result, cResult.sign, cResult.exponent, cResult.mantissa, cResult.overflow);
}



void remDD(double d1, double d2, double *rp)
{
	double r;
	int sn1, sn2, e1, e2;
	int cmp;
	int scale=0;

	sn1 = GET_DP_SIGN(&d1);
	sn2 = GET_DP_SIGN(&d2);
	e1 = GET_DP_EXPONENT(&d1);
	e2 = GET_DP_EXPONENT(&d2);

	HIGH_U32_FROM_DBL(d1) = SET_DP_SIGN(&d1, 0);
	HIGH_U32_FROM_DBL(d2) = SET_DP_SIGN(&d2, 0);

	/* d1 < d2 rem = d1 */
	cmp = compareDD(d1, d2);
	if ( cmp == -1 ) {
		HIGH_U32_FROM_DBL_PTR(rp) = HIGH_U32_FROM_DBL(d1);
		LOW_U32_FROM_DBL_PTR(rp) = LOW_U32_FROM_DBL(d1);	
		HIGH_U32_FROM_DBL_PTR(rp) = SET_DP_SIGN(rp, sn1);
		return;
	} else if (cmp == 0 ) {
		*rp = 0;
		HIGH_U32_FROM_DBL_PTR(rp) = SET_DP_SIGN(rp, sn1);
		return;
	}

	r = fmod( d1, d2 );
	HIGH_U32_FROM_DBL(r) = SET_DP_SIGN(&r, sn1);
	*rp = r;
}



/* d1 < d2 : -1
	d1 == d2 : 0
	d1 > d2 : 1
*/
int
compareDD(double d1, double d2)
{
	int sn1, sn2, e1, e2;
	int eq = 0, lt = -1, gt = 1, notnum = -2;
	U_64 l1, l2;

	/* Check for NaN */
	if (IS_NAN_DBL(d1) || IS_NAN_DBL(d2)) {
		return notnum;
	}

	sn1 = GET_DP_SIGN(&d1);
	sn2 = GET_DP_SIGN(&d2);
	e1 = GET_DP_EXPONENT(&d1);
	e2 = GET_DP_EXPONENT(&d2);

	if (e1 > e2) {
		if (sn1 == 0) {
			return gt;
		} else {
			return lt;
		}
	} else if (e1 < e2) {
		if (sn2 == 0) {
			return lt;
		} else {
			return gt;
		}
	} else {
		doubleToLong(d1, &l1);
		doubleToLong(d2, &l2);
		if (l1 > l2) {
			if (sn1 == 0) {
				return gt;
			} else {
				return lt;
			}
		} else if (l1 < l2) {
			if (sn2 == 0) {
				return lt;
			} else {
				return gt;
			}
		} else {
			if (sn1 == sn2) {
				return eq;
			} else {
				if (sn1 != 0) {
					return lt;
				} else {
					return gt;
				}
			}
		}
	}

}



void addDF(float f1, float f2, float *rp)
{
	int sn1, sn2;
	CANONICALFP cu, cv, cResult;

	/* Check for NaN */
	if (IS_SP_NAN(f1) || IS_SP_NAN(f2)) {
		SET_SP_NAN(rp);
		return;
	}

	sn1 = GET_SP_SIGN(f1);
	sn2 = GET_SP_SIGN(f2);

	/* check for 0 */
	if (IS_SP_ZERO(f1)) {
		if( IS_SP_ZERO(f2) ) {
			/* If both -0.0, then return -0.0. Otherwise, return +0.0 */
			if( sn1 && sn2 ) {
				SETP_SP_NZERO(rp);
			} else {
				SETP_SP_PZERO(rp);
			}
		} else {
			FLOAT_WORD_P(rp) = FLOAT_WORD(f2);
		}
		return;
	}
	if (IS_SP_ZERO(f2)) {
		FLOAT_WORD_P(rp) = FLOAT_WORD(f1); 
		return;
	}

	/* Check for infinity */
	if (IS_SP_INF(f1) || IS_SP_INF(f2)) {
		if (IS_SP_INF(f1) && IS_SP_INF(f2)) {
			if (sn1 == sn2) {
				FLOAT_WORD_P(rp) = FLOAT_WORD(f1); 
			} else {
				SET_SP_NAN(rp);
			}
			return;
		}
		if (IS_SP_INF(f1))
			FLOAT_WORD_P(rp) = FLOAT_WORD(f1); 
		else
			FLOAT_WORD_P(rp) = FLOAT_WORD(f2); 
		return;
	}

	cu = convertFloatToCanonical(f1);
	cv = convertFloatToCanonical(f2);
	cResult = canonicalAdd(cu, cv);
	*rp = convertCanonicalToFloat(cResult);
}



/* d1 - d2 */
void subDF(float f1, float f2, float *rp)
{
	int sn1, sn2;
	CANONICALFP cu, cv, cResult;

	/* Check for NaN */
	if (IS_SP_NAN(f1) || IS_SP_NAN(f2)) {
		SET_SP_NAN(rp);
		return;
	}

	sn1 = GET_SP_SIGN(f1);
	sn2 = GET_SP_SIGN(f2);

	/* check for 0 */
	if (IS_SP_ZERO(f2)) {
		if( IS_SP_ZERO(f1) ) {
			/* +0.0 - +0.0 ==> +0.0
			 * -0.0 - -0.0 ==> +0.0
			 * +0.0 - -0.0 ==> +0.0
			 * -0.0 - +0.0 ==> -0.0
			 */
			if( sn1 != 0 && sn2 == 0 ) {
				SETP_SP_NZERO(rp);
			} else {
				SETP_SP_PZERO(rp);
			}
		} else {
			FLOAT_WORD_P(rp) = FLOAT_WORD(f1);
		}
		return;
	}
	if (IS_SP_ZERO(f1)) {
		FLOAT_WORD(f2) = SET_SP_SIGN(f2, (sn2 == 1) ? 0 : 1);
		FLOAT_WORD_P(rp) = FLOAT_WORD(f2); 
		return;
	}

	/* Check for infinity */
	if (IS_SP_INF(f1) || IS_SP_INF(f2)) {
		if (IS_SP_INF(f1) && IS_SP_INF(f2)) {
			if (sn1 != sn2) {
				/* -Inf - +Inf ==> -Inf
				 * +Inf - -Inf ==> +Inf
				 * Otherwise, NaN
				 */
				if( sn1 != 0 ) {
					SET_SP_NINF(rp);
				} else {
					SET_SP_PINF(rp);
				}
			} else {
				SET_SP_NAN(rp);
			}
			return;
		}
		if (IS_SP_INF(f1)) {
			FLOAT_WORD_P(rp) = FLOAT_WORD(f1); 
		} else {
			int sres = (sn2 == 0) ? 1:0;
			FLOAT_WORD(f2) = SET_SP_SIGN(f2, sres);
			FLOAT_WORD_P(rp) = FLOAT_WORD(f2); 
		}	
		return;
	}

	cu = convertFloatToCanonical(f1);
	cv = convertFloatToCanonical(f2);
	cResult = canonicalSubtract(cu, cv);
	*rp = convertCanonicalToFloat(cResult);
}



void multiplyDF(float f1, float f2, float *rp)
{
	float r;
	int e1, e2, e;
	U_32 i1, i2;
	int be = 0;
	int sn1, sn2;
	U_64 pu = 0;
	int leading;
	int d1bits = SPFRACTION_LENGTH + 1, d2bits = SPFRACTION_LENGTH + 1, resbits = 0;

	/* Check for NaN */
	if (IS_SP_NAN(f1) || IS_SP_NAN(f2)) {
		SET_SP_NAN(rp);
		return;
	}

	e1 = GET_SP_EXPONENT(f1);
	e2 = GET_SP_EXPONENT(f2);
	sn1 = GET_SP_SIGN(f1);
	sn2 = GET_SP_SIGN(f2);
	FLOAT_WORD(f1) = SET_SP_SIGN(f1, 0);
	FLOAT_WORD(f2) = SET_SP_SIGN(f2, 0);

	/* Check for infinity */
	if (IS_SP_INF(f1) || IS_SP_INF(f2)) {
		if (IS_SP_ZERO(f1) || IS_SP_ZERO(f2)) {
			SET_SP_NAN(rp);
			return;
		}
		if (sn1 == sn2) {
			SET_SP_PINF(rp);
		} else {
			SET_SP_NINF(rp);
		}
		return;
	}

	/* Check for zero */
	if (IS_SP_ZERO(f1) || IS_SP_ZERO(f2)) {
		if (sn1 == sn2) {
			SETP_SP_PZERO(rp);
		} else {
			SETP_SP_NZERO(rp);
		}
		return;
	}


	/* Only one operand can be denormalized, else answer is zero */
	if (e1 == 0 && e2 == 0) {
		if (sn1 == sn2) {
			SETP_SP_PZERO(rp);
		} else {
			SETP_SP_NZERO(rp);
		}
		return;
	}

	i1 = GET_SP_MANTISSA(f1);
	i2 = GET_SP_MANTISSA(f2);
	if (e1 > 0)
		i1 = SET_SPNORMALIZED_BIT(i1);
	if (e2 > 0)
		i2 = SET_SPNORMALIZED_BIT(i2);

	if (e1 == 0) {
		leading = indexLeadingOne32((I_32 *)&f1);
		e1 -= SPFRACTION_LENGTH - leading - 1;
		d1bits -= SPFRACTION_LENGTH - leading;
	}

	if (e2 == 0) {
		leading = indexLeadingOne32((I_32 *)&f2);
		e2 -= SPFRACTION_LENGTH - leading - 1;
		d2bits -= SPFRACTION_LENGTH - leading;
	}

	e1 -= SPEXPONENT_BIAS;
	e2 -= SPEXPONENT_BIAS;

	pu = (U_64) i1 *(U_64) i2;
	resbits = indexLeadingOne64(&pu) + 1;

	e = e1 + e2;

	if (resbits >= (d1bits + d2bits)) {
		e += resbits - (d1bits + d2bits) + 1;
	}

	be = e + SPEXPONENT_BIAS;

	longToFloat(&pu, be, &r);
	if (sn1 != sn2)
		FLOAT_WORD(r) = SET_SP_SIGN(r, 1);

	*rp = r;
}



/* d1/d2 if d1 is denormalized, d2 cannot be normalized too where answer is 0
 * d1, d2 both can be denormalized
 */
void divideDF(float f1, float f2, float *rp)
{
	float r;
	int e1, e2;
	U_32 i1, i2;
	int sn;
	int e=0, be = 0;
	int sn1, sn2, leading;

	/* Check for NaN */
	if (IS_SP_NAN(f1) || IS_SP_NAN(f2)) {
		SET_SP_NAN(rp);
		return;
	}

	sn1 = GET_SP_SIGN(f1);
	sn2 = GET_SP_SIGN(f2);

	/* Check for infinity */
	if (IS_SP_INF(f1) || IS_SP_INF(f2)) {
		if (IS_SP_INF(f1) && IS_SP_INF(f2)) {
			SET_SP_NAN(rp);
			return;
		}
		if (IS_SP_INF(f2)) {
			if (sn1 == sn2) 
				SETP_SP_PZERO(rp);
			else
				SETP_SP_NZERO(rp);
			return;
		}
		if (sn1 == sn2) {
			SET_SP_PINF(rp);
		} else {
			SET_SP_NINF(rp);
		}
		return;
	}

	/* check for divide by zero */
	if (IS_SP_ZERO(f2)) {
		if (IS_SP_ZERO(f1)) {
			SET_SP_NAN(rp);
			return;
		}
		if (sn1 == sn2) {
			SET_SP_PINF(rp);
		} else {
			SET_SP_NINF(rp);
		}
		return;
	}

	if (IS_SP_ONE(f2)) {
		if (IS_SP_POSITIVE(f2)) {
			*rp = f1;
		} else {
			sn = GET_SP_SIGN(f1);
			FLOAT_WORD(f1) = SET_SP_SIGN(f1, (sn == 1) ? 0 : 1);
			*rp = f1;
		}
		return;
	};

	e1 = GET_SP_EXPONENT(f1);
	e2 = GET_SP_EXPONENT(f2);

	floatToInt(f1, &i1);
	floatToInt(f2, &i2);

	FLOAT_WORD(i1) = SET_SP_SIGN(i1, 0);
	FLOAT_WORD(i2) = SET_SP_SIGN(i2, 0);

	if (e1 == 0) {
		leading = indexLeadingOne32(&i1);
		e1 -= SPFRACTION_LENGTH - leading - 1;
		shiftLeft32(&i1, NULL, -e1 + 1);
	}

	if (e2 == 0) {
		leading = indexLeadingOne32(&i2);
		e2 -= SPFRACTION_LENGTH - leading - 1;
		shiftLeft32(&i2, NULL, -e2 + 1);
	}


	r = ((float) i1) / ((float)(int)i2);
	e = GET_SP_EXPONENT(r);

	be = (e1 - e2);

	if (e1 != e2) {
		if (be > 0) {
			if ((e+be) > (SPEXPONENT_BIAS<<1)) {
				SET_SP_PINF(&r);
			} else {	
				scaleUpFloat(&r, be);
			}
		} else {
			if ((e+be) < -(SPEXPONENT_BIAS<<1)) {
				SETP_SP_PZERO(&r);
			} else {	
				scaleDownFloat(&r, be);
			}
		}
	}
	if (sn1 != sn2)
		FLOAT_WORD(r) = SET_SP_SIGN(r, 1);

	*rp = r;
}



void remDF(float f1, float f2, float *rp)
{
	float q, qt, s, r;
	I_32 i1, i2, iq;
	int sn1, sn2, e1, e2, e, be, es, eq;
	int leading, cmp, scale;

	sn1 = GET_SP_SIGN(f1);
	sn2 = GET_SP_SIGN(f2);
	e1 = GET_SP_EXPONENT(f1);
	e2 = GET_SP_EXPONENT(f2);
	FLOAT_WORD(f1) = SET_SP_SIGN(f1, 0);
	FLOAT_WORD(f2) = SET_SP_SIGN(f2, 0);

	/* f1 < f2 rem = f1 */
	cmp = compareDF(f1, f2);
	if ( cmp == -1 ) {
		FLOAT_WORD_P(rp) = FLOAT_WORD(f1) ;
		FLOAT_WORD_P(rp) = SET_SP_SIGN_P(rp, sn1);
		return;
	} else if (cmp == 0 ) {
		*rp = 0;
		FLOAT_WORD_P(rp) = SET_SP_SIGN_P(rp, sn1);
		return;
	}

	/* divideDF(f1, f2, &q);  quotient > 1 */
	floatToInt(f1, &i1);
	floatToInt(f2, &i2);

	FLOAT_WORD(i1) = SET_SP_SIGN(i1, 0);
	FLOAT_WORD(i2) = SET_SP_SIGN(i2, 0);

	if (e1 == 0) {
		leading = indexLeadingOne32(&i1);
		e1 -= SPFRACTION_LENGTH - leading - 1;
		shiftLeft32(&i1, NULL, -e1 + 1);
	}

	if (e2 == 0) {
		leading = indexLeadingOne32(&i2);
		e2 -= SPFRACTION_LENGTH - leading - 1;
		shiftLeft32(&i2, NULL, -e2 + 1);
	}


	q = ((float) i1) / ((float)(int)i2);
	e = GET_SP_EXPONENT(q);

	be = (e1 - e2);
	/* divide end */

	iq = GET_SP_MANTISSA(q);
	if ( e > 0 ) SET_SPNORMALIZED_BIT(iq);
	e += be;
	if ( (e -SPEXPONENT_BIAS) > SPEXPONENT_BIAS ) {
		scale = e - (SPEXPONENT_BIAS *2);
		e -= scale;
	}
	truncateToZeroFloat(iq, e, &qt);

	if(sn1 != sn2)
		FLOAT_WORD(qt) = SET_SP_SIGN(qt, sn1);

	FLOAT_WORD(f1) = SET_SP_SIGN(f1, sn1);
	FLOAT_WORD(f2) = SET_SP_SIGN(f2, sn2);
	
	eq = GET_SP_EXPONENT(qt);
	if ( e2 <= 0 || eq <=0) {
		multiplyDF(f2, qt, &s); 
	} else {
		s = qt * f2;
	}	

	es = GET_SP_EXPONENT(s);

	if ( e1 <= 0 || es <= 0) {
		subDF(f1, s, &r);
	} else {
		r = f1 -  s;
	}

	FLOAT_WORD(r) = SET_SP_SIGN(r, sn1);

	*rp = r;
}


/*  f1 < f2 : -1
    f1 == f2 : 0
    f1 > f2 : 1
*/
int compareDF(float f1, float f2)
{
	int sn1, sn2, e1, e2;
	int eq = 0, lt = -1, gt = 1, notnum=-2;
	int i1, i2;

	/* Check for NaN */
	if (IS_SP_NAN(f1) || IS_SP_NAN(f2)) {
		return notnum;
	}


	sn1 = GET_SP_SIGN(f1);
	sn2 = GET_SP_SIGN(f2);
	e1 = GET_SP_EXPONENT(f1);
	e2 = GET_SP_EXPONENT(f2);

	if (e1 > e2) {
		if (sn1 == 0)
			return gt;
		else
			return lt;
	} else if (e1 < e2) {
		if (sn2 == 0)
			return lt;
		else
			return gt;
	} else {
		i1 = GET_SP_MANTISSA(f1);
		i2 = GET_SP_MANTISSA(f2);
		if (i1 > i2)
			if (sn1==0) return gt; else return lt;
		else if (i1 < i2)
			if (sn2==0) return lt; else return gt;
		else
			if(sn1==sn2) return eq; else return (sn1)? lt: gt;
	}

}



/* Simplified version of truncateToZeroDouble(). Remove the fractional part in the given double. */
static void truncateDouble(double *rp)
{
	int biasedExp = GET_DP_EXPONENT(rp);
	int exp = biasedExp - DPEXPONENT_BIAS;
	U_64 mantissa, mantissaMask = J9CONST64(0xfffffffffffff);

	GET_DP_MANTISSA(rp, &mantissa );
	if( exp >= 0 ) {
		if( exp < DPFRACTION_LENGTH ) {
			mantissaMask = (mantissaMask << (DPFRACTION_LENGTH - exp)) & J9CONST64(0xfffffffffffff);
		}
		mantissa &= mantissaMask;
	} else {
		mantissa = 0;
	}
	SET_DP_MANTISSA(rp, &mantissa );
}




static U_32 numOverflowedBits32(U_32 bits)
{
    U_32 nBits = 0;
    U_32 mask;

    mask = 0x80000000;
    while (mask != 0) {
        if( IS_SET(bits, mask) )
            nBits++;
        mask = mask >> 1;
    }

    return nBits;
}




static CANONICALFP convertDoubleToCanonical(double u) {
	CANONICALFP result;

	result.sign = GET_DP_SIGN(&u);
	result.exponent = GET_DP_EXPONENT(&u);
	GET_DP_MANTISSA(&u, &result.mantissa);
	if( result.exponent != 0 ) {
		/*  Prepend the leading 53rd bit of the mantissa, and adjust the
		 *  exponent appropriately. */
		result.mantissa |= J9CONST64(0x10000000000000);
		result.exponent -= DPEXPONENT_BIAS + 1;
	} else {
		while( (result.mantissa > 0) && (result.mantissa < J9CONST64(0x10000000000000)) ) {
			result.mantissa <<= 1;
			result.exponent--;
		}
		result.exponent -= DPEXPONENT_BIAS;
	}
	result.overflow = 0;
	return result;
}



static void canonicalNormalize( CANONICALFP *a ) {
	if( a->mantissa == 0 ) {
		if( a->overflow == 0 ) {
			a->exponent = 0;
		} else {
			while( (a->mantissa < J9CONST64(0x10000000000000)) ) {
				a->mantissa <<= 1;
				if( a->overflow & J9CONST64(0x8000000000000000) ) {
					a->mantissa |= 1;
				}
				a->overflow <<= 1;
				a->exponent--;
			}
		}
		return;
	}
	/* Attempt to normalize the bits by either shift right or left in
	 * order to make sure that the 53rd bit is a one. */
	while( a->mantissa >= J9CONST64(0x20000000000000) ) {
		a->overflow >>= 1;
		if( (a->mantissa & 1) != 0 ) {
			a->mantissa >>= 1;
			a->overflow |= J9CONST64(0x8000000000000000);
		} else {
			a->mantissa >>= 1;
		}
		a->exponent++;
	}
	while( (a->mantissa > 0) && (a->mantissa < J9CONST64(0x10000000000000)) ) {
		a->mantissa <<= 1;
		if( a->overflow & J9CONST64(0x8000000000000000) ) {
			a->mantissa |= 1;
		}
		a->overflow <<= 1;
		a->exponent--;
	}
}



static CANONICALFP canonicalAdd( CANONICALFP u, CANONICALFP v ) {
	I_32 expDiff;
	CANONICALFP *pu, *pv, *presult, result;

	pu = &u;
	pv = &v;
	presult = &result;

    /* If values have opposite signs, invert the sign and do
     * a subtraction instead of an addition. */
	if( pu->sign != pv->sign ) {
		if( pu->sign != 0 ) {
			pu->sign = 0;
			return canonicalSubtract(v, u);
		} else {
			pv->sign = 0;
			return canonicalSubtract(u, v);
		}
	}

	if( pu->exponent < pv->exponent ) {
		pu = &v;
		pv = &u;
	}
	presult->sign = pu->sign;
	presult->exponent = pu->exponent;
	expDiff = pu->exponent - pv->exponent;

	if( expDiff >= (DPFRACTION_LENGTH << 1) ) {
		presult->mantissa = pu->mantissa;
		presult->overflow = pu->overflow;
		return result;
	}
	shiftRight64(&pv->overflow, -expDiff);
	pv->overflow |= shiftRight64(&pv->mantissa, -expDiff);
	presult->overflow = pu->overflow + pv->overflow;
	presult->mantissa = pu->mantissa + pv->mantissa;
	if( (presult->overflow < pu->overflow) || (presult->overflow < pv->overflow) ) {
		presult->mantissa++;
	}

	canonicalNormalize(presult);
	return result;
}



static CANONICALFP canonicalSubtract( CANONICALFP u, CANONICALFP v ) {
	I_32 expDiff;
	CANONICALFP *pu, *pv, *presult, result;

	/* If values have opposite signs, invert the sign and do an
	 * addition instead of a subtraction. */
	if( v.sign ) {
		v.sign = 0;
		return canonicalAdd(u, v);
	}

	pu = &u;
	pv = &v;
	presult = &result;

	/* Make sure that |u| is always greater |v|. */
	if( pu->exponent < pv->exponent ) {
		pu = &v;
		pv = &u;
		presult->sign = pu->sign ^ 1;
	} else if( pu->exponent == pv->exponent ) {
		if( pu->mantissa < pv->mantissa ) {
			pu = &v;
			pv = &u;
			presult->sign = pu->sign ^ 1;
		} else {
			presult->sign = pu->sign;
		}
	} else {
		presult->sign = pu->sign;
	}
	presult->exponent = pu->exponent;
	expDiff = pu->exponent - pv->exponent;
	if( expDiff >= DPFRACTION_LENGTH+52 ) {
		presult->mantissa = pu->mantissa;
		presult->overflow = pu->overflow;
		return result;
	}
	shiftRight64(&pv->overflow, -expDiff);
	pv->overflow |= shiftRight64(&pv->mantissa, -expDiff);

	/* If the signs of the given values are opposite of each other, we
	 * perform an addition of the mantissa. Otherwise, we do a
	 * subtraction. */
	if( pu->sign != pv->sign ) {
		presult->overflow = pu->overflow + pv->overflow;
		presult->mantissa = pu->mantissa + pv->mantissa;
		if( (presult->overflow < pu->overflow) || (presult->overflow < pv->overflow) ) {
			presult->mantissa++;
		}
	} else {
		presult->overflow = pu->overflow - pv->overflow;
		if( presult->overflow > pu->overflow ) {
			presult->mantissa = pu->mantissa - pv->mantissa - 1;
		} else {
			presult->mantissa = pu->mantissa - pv->mantissa;
		}
	}

	canonicalNormalize(presult);
	return result;
}




static CANONICALFP canonicalMultiply( CANONICALFP u, CANONICALFP v ) {
	U_64 temp1, temp2, temp3;
	I_32 idl1;
	CANONICALFP *pu, *pv, *presult, result;

	pu = &u;
	pv = &v;
	presult = &result;

	presult->sign = pu->sign ^ pv->sign;
	presult->exponent = pu->exponent + pv->exponent + 2;

	/* If resulting value is too small to be represented as a
	 * floating-point number, just return zero with the correct sign. */
	if( presult->exponent < -(DPEXPONENT_BIAS+DPFRACTION_LENGTH) ) {
		presult->exponent = 0;
		presult->mantissa = 0;
		presult->overflow = 0;
		return result;
	}

	/* Multiplication of two 53-bit values result in a 105-bit or
	 * 106-bit number, which is stored in temp3 (high 42-bit value) and
	 * temp1 (low 64-bit value). */
	temp1 = (U_64)LOW_U32_FROM_LONG64(pu->mantissa) * (U_64)LOW_U32_FROM_LONG64(pv->mantissa);
	temp2 = (U_64)HIGH_U32_FROM_LONG64(pu->mantissa) * (U_64)LOW_U32_FROM_LONG64(pv->mantissa)
				+ (U_64)HIGH_U32_FROM_LONG64(pv->mantissa) * (U_64)LOW_U32_FROM_LONG64(pu->mantissa)
				+ HIGH_U32_FROM_LONG64(temp1);
	temp3 = (U_64)HIGH_U32_FROM_LONG64(pu->mantissa) * (U_64)HIGH_U32_FROM_LONG64(pv->mantissa)
				+ HIGH_U32_FROM_LONG64(temp2);
	HIGH_U32_FROM_LONG64(temp1) = LOW_U32_FROM_LONG64(temp2);

	idl1 = indexLeadingOne64(&temp3);

	/* Since the 105/106-bit number is stored in temp3(hi64):temp1(lo64), the index of
	 * the leading  1 must be either 40 or 41 (105|106 - 64 - 1
	 * [zero-based indexing]), the mantissa is always assumed to have
	 * 53 bits, and the remaining bits are considered as overflow. */
	if( idl1 == 41 ) {
		presult->mantissa = ((temp3 & J9CONST64(0x000003ffffffffff)) << 11) | (temp1 >> 53);
		presult->overflow = temp1 << 11;
	} else {
		presult->mantissa = ((temp3 & J9CONST64(0x000001ffffffffff)) << 12) | (temp1 >> 52);
		presult->overflow = temp1 << 12;
		presult->exponent--;
	}

	/* Again, we have to adjust the exponent if it is smaller than the
	 * negative of the exponent bias constant. */
	while( presult->exponent < -DPEXPONENT_BIAS ) {
		presult->exponent++;
		presult->overflow >>= 1;
		if( (presult->mantissa & 1) != 0 ) {
			presult->overflow |= J9CONST64(0x8000000000000000);
		}
		presult->mantissa >>= 1;
	}

	canonicalNormalize(presult);
	return result;
}



static CANONICALFP canonicalDivide( CANONICALFP u, CANONICALFP v ) {
	U_64 temp1, temp2;
	I_32 idl1ResultMant;
	CANONICALFP *pu, *pv, *presult, result;

	pu = &u;
	pv = &v;
	presult = &result;

	presult->sign = pu->sign ^ pv->sign;
	presult->mantissa = 0;
	presult->overflow = 0;

	/* Simple optimization: If dividend (first argument) is 0, then the
	 * result is zero. */
	if( (temp1 = pu->mantissa) == 0 ) {
		return result;
	}

	/* Simple optimization: If the difference of the exponents is less
	 * than the allowable limit, then the result is zero. */
	if( (pu->exponent - pv->exponent <= -(DPEXPONENT_BIAS+DPFRACTION_LENGTH)) ) {
		return result;
	}

	temp2 = pv->mantissa;
	while( (temp2 & 1) == 0 ) {
		temp2 >>= 1;
	}

	idl1ResultMant = 0;
	do {
		I_32 idl1Temp1, shiftAmount;
		if( temp1 < temp2 ) {
			idl1Temp1 = indexLeadingOne64(&temp1);
			shiftAmount = 63 - idl1Temp1;
			temp1 <<= shiftAmount;
			temp1 |= pu->overflow >> (idl1Temp1+1);
			pu->overflow <<= shiftAmount;
			idl1ResultMant = indexLeadingOne64(&presult->mantissa);
			if( idl1ResultMant > idl1Temp1 ) {
				shiftAmount = 63 - idl1ResultMant;
				presult->mantissa <<= shiftAmount;
				presult->mantissa |= (presult->overflow >> idl1ResultMant);
				presult->overflow <<= shiftAmount;
			} else {
				presult->mantissa <<= shiftAmount;
				presult->mantissa |= (presult->overflow >> idl1Temp1);
				presult->overflow <<= shiftAmount;
			}
		}
		presult->overflow += temp1 / temp2;
		idl1ResultMant = indexLeadingOne64(&presult->mantissa);
		temp1 = temp1 % temp2;
	} while( (idl1ResultMant < (DPFRACTION_LENGTH+1)) && (temp1 != 0) );

	/* Special case - where v divides evenly into u, e.g. 4.0 / 2.0 ! */
	if( presult->mantissa == 0 ) {
		presult->mantissa = presult->overflow;
		idl1ResultMant = indexLeadingOne64(&presult->mantissa);
		presult->overflow = pu->overflow;
	}

	presult->exponent = pu->exponent - pv->exponent - 1;
	presult->exponent += DPFRACTION_LENGTH - idl1ResultMant;

	if( pu->mantissa < pv->mantissa ) {
		presult->exponent--;
	}

	/* Again, we have to adjust the exponent if it is smaller than the
	 * negative of the exponent bias constant. */
	while( presult->exponent < -DPEXPONENT_BIAS ) {
		presult->exponent++;
		presult->overflow >>= 1;
		if( (presult->mantissa & 1) != 0 ) {
			presult->overflow |= J9CONST64(0x8000000000000000);
		}
		presult->mantissa >>= 1;
	}

	canonicalNormalize(presult);
	return result;
}



/* Simple round to even algorithm. If only the most significant bit of the
 * overflowed bits is set and all of the other bits are zero, then
 * round the mantissa to an even binary value.
 */
static void simpleRounding( U_64 *rMant, U_64 *overflow ) {
	U_32 mostSigDigit = (HIGH_U32_FROM_LONG64_PTR(overflow) & 0xF0000000) >> 28;

	switch(mostSigDigit) {
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
			break;
		case 8:
			if( (*overflow & J9CONST64(0xFFFFFFFFFFFFC00)) != 0 ) {
				*rMant += 1;
			} else {
				if( (*rMant & 1) != 0 ) {
					*rMant += 1;
				}
			}
			*overflow = 0;
			break;
		default:
			*rMant += 1;
			*overflow = 0;
			break;
	}
}



static void simpleNormalizeAndRound(double *result, I_32 rSign, I_32 rExp, U_64 rMant, U_64 overflow) {
	/* If mantissa is zero, then actual result is zero! */
	if( (rMant == 0) && (overflow == 0) ) {
		SET_PZERO_DBL_PTR(result);
		SET_DP_SIGN(result, rSign);
		return;
	}

	/* Attempt to normalize the bits by either shift right or left in
	 * order to make sure that the 53rd bit is a one. */
	while( rMant >= J9CONST64(0x20000000000000) ) {
		overflow >>= 1;
		if( (rMant & 1) != 0 ) {
			rMant >>= 1;
			overflow |= J9CONST64(0x8000000000000000);
		} else {
			rMant >>= 1;
		}
		rExp++;
	}
	while( (rMant > 0) && (rMant < J9CONST64(0x10000000000000)) ) {
		rMant <<= 1;
		if( overflow & J9CONST64(0x8000000000000000) ) {
			rMant |= 1;
		}
		overflow <<= 1;
		rExp--;
	}

	/* Check if it is a de-normalized floating-point number. */
	/* If the leading 1 of the mantissa is the 52nd bit or lower, it
	 * means that we have to shift the mantissa left and decrement the
	 * exponent to correspond with the left shift. So we do a quick
	 * calculation to make sure that by the mantissa can be normalized.
	 * Otherwise, we assume that it is a denormalized floating-point
	 * number. */
	if( (rExp < -DPEXPONENT_BIAS) && (rMant != 0) ) {
		/* Perform rounding before normalization. Even though this is not
		 * suggested by D.E. Knuth, The Art of Computer Programming, Vol.2,
		 * it appears to help (from some of our empirical test cases). */
		/*simpleRounding(&rMant, &overflow);*/
		/* Keeping only m-s-bit of overflow bits of denormalized
		 * floating-point number. */
		overflow &= J9CONST64(0x8000000000000000);
		while( (rExp < -DPEXPONENT_BIAS) && (rMant > 0) ) {
			overflow >>= 1;
			if( (rMant & 1) != 0 ) {
				rMant >>= 1;
				overflow |= J9CONST64(0x8000000000000000);
			} else {
				rMant >>= 1;
			}
			rExp++;
		}
		simpleRounding(&rMant, &overflow);
		/* It is possible that the rounding makes the result to be a
		 * normalized floating-point number. */
		if( (rMant & J9CONST64(0x10000000000000)) != 0 ) {
			rExp += DPEXPONENT_BIAS + 1;
		} else {
			rExp = 0;
		}
		SET_DP_EXPONENT(result, rExp);
		SET_DP_MANTISSA(result, &rMant);
		SET_DP_SIGN(result, rSign);
		return;
	}

	/* Or the result is too big to be stored as a double? */
	if( rExp > DPEXPONENT_BIAS ) {
		if( rSign ) {
			SET_NINF_DBL_PTR(result);
		} else {
			SET_PINF_DBL_PTR(result);
		}
		return;
	}

	/* Here's where we'll do the rounding!! The rounding algorithm is
	 * always try to round the mantissa to be even if it is a boundary
	 * case. */
	simpleRounding(&rMant, &overflow);

	if( rMant >= J9CONST64(0x20000000000000) ) {
		rMant >>= 1;
		rExp++;
	}

	/* Bias the exponent before proceeding. */
	/* Since we are removing the 53rd bit (assumed to be 1) from the
	 * mantissa, we have to adjust the exponent by 1. */
	rExp += DPEXPONENT_BIAS;
	if( (rMant & J9CONST64(0x10000000000000)) != 0 ) {
		rExp++;
	}

	if( rExp < 0 ) {
		if( rSign ) {
			SET_NZERO_DBL_PTR(result);
		} else {
			SET_PZERO_DBL_PTR(result);
		}
		return;
	} else if( rExp > (DPEXPONENT_BIAS << 1) ) {
		if( rSign ) {
			SET_NINF_DBL_PTR(result);
		} else {
			SET_PINF_DBL_PTR(result);
		}
		return;
	}

	SET_DP_EXPONENT(result, rExp);
	SET_DP_MANTISSA(result, &rMant);
	SET_DP_SIGN(result, rSign);
}



static CANONICALFP convertFloatToCanonical(float u) {
	CANONICALFP result;

	result.sign = GET_SP_SIGN(u);
	result.exponent = GET_SP_EXPONENT(u);
	result.mantissa = GET_SP_MANTISSA(u);
	if( result.exponent != 0 ) {
		/*	Prepend the leading 24th bit of the mantissa, and adjust the
		 *	exponent appropriately. */
		result.mantissa |= 0x800000;
		result.exponent -= SPEXPONENT_BIAS + 1;
	} else {
		while( (result.mantissa > 0) && (result.mantissa < 0x800000) ) {
			result.mantissa <<= 1;
			result.exponent--;
		}
		result.exponent -= SPEXPONENT_BIAS;
	}
	shiftLeft64(&result.mantissa, NULL, 29);
	result.overflow = 0;
	return result;
}



static float convertCanonicalToFloat(CANONICALFP r) {
	float spf = 0.0;
	U_32 spfInt, overflow;

	/* If mantissa is zero, then actual result is zero! */
	if( (r.mantissa == 0) && (r.overflow == 0) ) {
		SET_PZERO_SNGL_PTR(&spf);
		SET_SP_SIGN(spf, r.sign);
		return spf;
	}
	r.overflow = shiftRight64( &r.mantissa, -29 );

	spfInt = LOW_U32_FROM_LONG64(r.mantissa);
	overflow = HIGH_U32_FROM_LONG64(r.overflow);

	/* Check if it is a de-normalized floating-point number. */
	/* If the leading 1 of the mantissa is the 52nd bit or lower, it
	 * means that we have to shift the mantissa left and decrement the
	 * exponent to correspond with the left shift. So we do a quick
	 * calculation to make sure that by the mantissa can be normalized.
	 * Otherwise, we assume that it is a denormalized floating-point
	 * number. */
	if( (r.exponent < -SPEXPONENT_BIAS) && (spfInt != 0) ) {
		/* Perform rounding before normalization. Even though this is not
		 * suggested by D.E. Knuth, The Art of Computer Programming, Vol.2,
		 * it appears to help (from some of our empirical test cases). */
		/* Keeping only m-s-bit of overflow bits of denormalized
		 * floating-point number. */
		overflow &= 0x80000000;
		while( (r.exponent < -SPEXPONENT_BIAS) && (spfInt > 0) ) {
			overflow >>= 1;
			if( (spfInt & 1) != 0 ) {
				spfInt >>= 1;
				overflow |= 0x80000000;
			} else {
				spfInt >>= 1;
			}
			r.exponent++;
		}
		/*simpleRounding(&r.mantissa, &r.overflow);*/
		if( (overflow & 0x80000000) != 0 ) {
			if( (overflow & 0x7FF00000) != 0 ) {
				spfInt++;
			} else {
				if( (spfInt & 1) != 0 ) spfInt++;
			}
		}
		/* It is possible that the rounding makes the result to be a
		 * normalized floating-point number. */
		if( (spfInt & 0x800000) != 0 ) {
			r.exponent += SPEXPONENT_BIAS + 1;
		} else {
			r.exponent = 0;
		}
		spfInt |= r.exponent << 23;
		if( r.sign ) {
			spfInt |= 0x80000000;
		}
		*((U_32 *)&spf) = spfInt;
		return spf;
	}

	/* Do our simple rounding here! */
	if( (overflow & 0x80000000) != 0 ) {
		if( (overflow & 0x7FF00000) != 0 ) {
			spfInt++;
		} else {
			if( (spfInt & 1) != 0 ) spfInt++;
		}
	}

	while( spfInt >= 0x1000000 ) {
		spfInt >>= 1;
		r.mantissa >>= 1;
		r.exponent++;
	}

	/* Bias the exponent before proceeding. */
	/* Since we are removing the 24th bit (assumed to be 1) from the
	 * mantissa, we have to adjust the exponent by 1. */
	r.exponent += SPEXPONENT_BIAS;
	if( (spfInt & 0x800000) != 0 ) {
		r.exponent++;
	}
	spfInt &= 0x007FFFFF;

	if( r.exponent < 0 ) {
		if( r.sign ) {
			SET_NZERO_SNGL_PTR(&spf);
		} else {
			SET_PZERO_SNGL_PTR(&spf);
		}
		return spf;
	} else if( r.exponent > (SPEXPONENT_BIAS << 1) ) {
		if( r.sign ) {
			SET_NINF_SNGL_PTR(&spf);
		} else {
			SET_PINF_SNGL_PTR(&spf);
		}
		return spf;
	}

	spfInt |= r.exponent << 23;
	if( r.sign ) {
		spfInt |= 0x80000000;
	}
	*((U_32 *)&spf) = spfInt;
	return spf;
}




#endif

#if defined(OLD_DBL_MATH)
static void oldAddDD(double d1, double d2, double *rp)
{
	double r;
	int e1, e2, ediff, aediff;
	U_64 l1, l2, lr;
	int sn1, sn2, sres;
	int sr = 0;
	int be = 0, leading1, leading2, leadingr, tmp;
	U_64 truncValue1=0, truncValue2=0, truncValueSum=0;
	U_64 round=0, borrow=0;

	/* Check for NaN */
	if (IS_NAN_DBL(d1) || IS_NAN_DBL(d2)) {
		SET_DP_NAN(rp);
		return;
	}

	sn1 = GET_DP_SIGN(&d1);
	sn2 = GET_DP_SIGN(&d2);

	/* check for 0 */
	if (IS_ZERO_DBL(d1)) {
		if( IS_ZERO_DBL(d2) ) {
			/* If both -0.0, then return -0.0. Otherwise, return +0.0 */
			if( sn1 && sn2 ) {
				SETP_DP_NZERO(rp);
			} else {
				SETP_DP_PZERO(rp);
			}
		} else {
			HIGH_U32_FROM_DBL_PTR(rp) = HIGH_U32_FROM_DBL(d2);
			LOW_U32_FROM_DBL_PTR(rp) = LOW_U32_FROM_DBL(d2);
		}
		return;
	}

	if (IS_ZERO_DBL(d2)) {
		HIGH_U32_FROM_DBL_PTR(rp) = HIGH_U32_FROM_DBL(d1);
		LOW_U32_FROM_DBL_PTR(rp) = LOW_U32_FROM_DBL(d1);	
		return;
	}

	/* Check for infinity */
	if (IS_INF_DBL(d1) || IS_INF_DBL(d2)) {
		if (IS_INF_DBL(d1) && IS_INF_DBL(d2)) {
			if (sn1 == sn2) {
				HIGH_U32_FROM_DBL_PTR(rp) = HIGH_U32_FROM_DBL(d1);
				LOW_U32_FROM_DBL_PTR(rp) = LOW_U32_FROM_DBL(d1);	
			} else {
				SET_DP_NAN(rp);
			}
			return;
		}
		if (IS_INF_DBL(d1)) {
			HIGH_U32_FROM_DBL_PTR(rp) = HIGH_U32_FROM_DBL(d1);
			LOW_U32_FROM_DBL_PTR(rp) = LOW_U32_FROM_DBL(d1);	
		} else {
			HIGH_U32_FROM_DBL_PTR(rp) = HIGH_U32_FROM_DBL(d2);
			LOW_U32_FROM_DBL_PTR(rp) = LOW_U32_FROM_DBL(d2);	
		}
		return;
	}

	e1 = GET_DP_EXPONENT(&d1);
	e2 = GET_DP_EXPONENT(&d2);

	ediff = e1 - e2;
	aediff = abs(ediff);
	if (aediff > (DPEXPONENT_LENGTH + DPFRACTION_LENGTH)) {
		if (e1 > e2) {
		   HIGH_U32_FROM_DBL_PTR(rp) = HIGH_U32_FROM_DBL(d1);
		   LOW_U32_FROM_DBL_PTR(rp) = LOW_U32_FROM_DBL(d1);	
			return;
		} else {
    		HIGH_U32_FROM_DBL_PTR(rp) = HIGH_U32_FROM_DBL(d2);
	    	LOW_U32_FROM_DBL_PTR(rp) = LOW_U32_FROM_DBL(d2);	
			return;
		}
	}

	HIGH_U32_FROM_DBL(d1) = SET_DP_SIGN(&d1, 0);
	HIGH_U32_FROM_DBL(d2) = SET_DP_SIGN(&d2, 0);

	doubleToLong(d1, &l1);
	doubleToLong(d2, &l2);

	leading1 = indexLeadingOne64(&l1);
	leading2 = indexLeadingOne64(&l2);

	if (ediff > 0) {
		sr = (e2!=0) ? -ediff : -ediff+1;
		truncValue2 = shiftRight64(&l2, sr);
		e2 += ediff;
	} else if (ediff < 0) {
		sr = (e1 != 0)? ediff: ediff + 1;
		truncValue1 = shiftRight64(&l1, sr);
		e1 += -ediff;
	}

	e1 -= DPEXPONENT_BIAS;
	e2 -= DPEXPONENT_BIAS;

	/* exponents must be equal */
	if (sn1 == sn2) {
		lr = l1 + l2;
		sres = sn1;
		truncValueSum = truncValue1 + truncValue2;
	} else {
		if (l1 > l2) {
			lr = l1 - l2;
			sres = (lr != 0) ? sn1 : 0;
			if (truncValue2 != 0) {
				borrow = 1;
				truncValueSum = (~truncValue2) + 1;
			}
		} else {
			lr = l2 - l1;
			sres = (lr != 0) ? sn2 : 0;
			if (truncValue1 != 0) {
				borrow = 1;
				truncValueSum = (~truncValue1) + 1;
			}
		}
	}
	leadingr = indexLeadingOne64(&lr);
	tmp = ( leading1 > leading2) ? leading1: leading2;

	be = (e1 > e2) ? e1 : e2;
	be += DPEXPONENT_BIAS;

	/* was a carry out generated */	
	e1 += DPEXPONENT_BIAS;
	e2 += DPEXPONENT_BIAS;
	if ( (e1 ==0 && e2==0 ) && leadingr > tmp && leadingr >= DPFRACTION_LENGTH) be += leadingr - tmp;
	/* if a denormal was generated */
	if ( tmp >= DPFRACTION_LENGTH && (be +leadingr) < DPFRACTION_LENGTH ) be -=1;

	if( lr != 0 || borrow != 0 || round != 0 ) {
		if(borrow) {
			lr = lr - 1;
		}

		longToDouble(lr, &truncValueSum, be, &r);
        roundDoubleAdd( &r, truncValueSum, aediff );

		HIGH_U32_FROM_DBL(r) = SET_DP_SIGN(&r, sres);
		HIGH_U32_FROM_DBL_PTR(rp) = HIGH_U32_FROM_DBL(r);
		LOW_U32_FROM_DBL_PTR(rp) = LOW_U32_FROM_DBL(r);	
	} else {
		HIGH_U32_FROM_DBL_PTR(rp) = 0x0;
		LOW_U32_FROM_DBL_PTR(rp) = 0x0;	
	}
}



/* d1/d2 if d1 is denormalized, d2 cannot be normalized too where answer is 0
 * d1, d2 both can be denormalized
 */
static void oldDivideDD(double d1, double d2, double *rp)
{
	double r;
	int e1, e2;
	U_64 l1, l2;
	int sn;
	int e, be = 0;
	int sn1, sn2, leading;

	/* Check for NaN */
	if (IS_NAN_DBL(d1) || IS_NAN_DBL(d2)) {
		SET_DP_NAN(rp);
		return;
	}

	sn1 = GET_DP_SIGN(&d1);
	sn2 = GET_DP_SIGN(&d2);

	/* Check for infinity */
	if (IS_INF_DBL(d1) || IS_INF_DBL(d2)) {
		if (IS_INF_DBL(d1) && IS_INF_DBL(d2)) {
			SET_DP_NAN(rp);
			return;
		}
		if (IS_INF_DBL(d2)) {
			if (sn1 == sn2) {
				SETP_DP_PZERO(rp);
			} else {
				SETP_DP_NZERO(rp);
			}
			return;
		}
		if (sn1 == sn2) {
			SET_DP_PINF(rp);
		} else {
			SET_DP_NINF(rp);
		}
		return;
	}

	/* check for divide by zero */
	if (IS_ZERO_DBL(d2)) {
		if (IS_ZERO_DBL(d1)) {
			SET_DP_NAN(rp);
			return;
		}
		if (sn1 == sn2) {
			SET_DP_PINF(rp);
		} else {
			SET_DP_NINF(rp);
		}
		return;
	}

	if (IS_ONE_DBL(d2)) {
		if (IS_POSITIVE_DBL(d2)) {
		   HIGH_U32_FROM_DBL_PTR(rp) = HIGH_U32_FROM_DBL(d1);
		   LOW_U32_FROM_DBL_PTR(rp) = LOW_U32_FROM_DBL(d1);	
		} else {
			sn = GET_DP_SIGN(&d1);
			HIGH_U32_FROM_DBL(d1) = SET_DP_SIGN(&d1, (sn == 1) ? 0 : 1);
		   HIGH_U32_FROM_DBL_PTR(rp) = HIGH_U32_FROM_DBL(d1);
		   LOW_U32_FROM_DBL_PTR(rp) = LOW_U32_FROM_DBL(d1);
		}
		return;
	};

	e1 = GET_DP_EXPONENT(&d1);
	e2 = GET_DP_EXPONENT(&d2);

	doubleToLong(d1, &l1);
	doubleToLong(d2, &l2);

	HIGH_U32_FROM_LONG64(l1) = SET_LWDP_SIGN(&l1, 0);
	HIGH_U32_FROM_LONG64(l2) = SET_LWDP_SIGN(&l2, 0);

	if (e1 == 0) {
		leading = indexLeadingOne64(&l1);
		e1 -= DPFRACTION_LENGTH - leading - 1;
		shiftLeft64(&l1, NULL, -e1 + 1);
	}

	if (e2 == 0) {
		leading = indexLeadingOne64(&l2);
		e2 -= DPFRACTION_LENGTH - leading - 1;
		shiftLeft64(&l2, NULL, -e2 + 1);
	}


	r = ((double) ((I_64) l1)) / ((I_64)l2);
	e = GET_DP_EXPONENT(&r);

	be = (e1 - e2);

	/* check for over(under)flow */
	if (e1 != e2) {
		if (be > 0) {
			if ((e+be) > (DPEXPONENT_BIAS<<1)) {
				SET_DP_PINF(&r);
			} else {	
				scaleUpDouble(&r, be);
			}
		} else {
			if ( (e+be) < -(DPEXPONENT_BIAS<<1)) {
				SETP_DP_PZERO(&r);
			} else {	
				scaleDownDouble(&r, be);
			}
		}
	}
	if (sn1 != sn2)
		HIGH_U32_FROM_DBL(r) = SET_DP_SIGN(&r, 1);

	*rp = r;
}




static void oldMultiplyDD(double d1, double d2, double *rp)
{
	double r;
	int e1, e2, e;
	U_64 l1, l2, lr;
	int be = 0;
	int sn1, sn2;
	int carry = 0;
	U_64 pl = 0, pm = 0, pu = 0, tmp;
	unsigned int i1l, i1h, i2l, i2h, leading;
	int d1bits = DPFRACTION_LENGTH + 1, d2bits = DPFRACTION_LENGTH + 1, resbits = 0;
	int leading1, leading2, leadingr, tmpl;

	/* Check for NaN */
	if (IS_NAN_DBL(d1) || IS_NAN_DBL(d2)) {
		SET_DP_NAN(rp);
		return;
	}

	e1 = GET_DP_EXPONENT(&d1);
	e2 = GET_DP_EXPONENT(&d2);
	sn1 = GET_DP_SIGN(&d1);
	sn2 = GET_DP_SIGN(&d2);
	HIGH_U32_FROM_DBL(d1) = SET_DP_SIGN(&d1, 0);
	HIGH_U32_FROM_DBL(d2) = SET_DP_SIGN(&d2, 0);

	/* Check for infinity */
	if (IS_INF_DBL(d1) || IS_INF_DBL(d2)) {
		if (IS_ZERO_DBL(d1) || IS_ZERO_DBL(d2)) {
			SET_DP_NAN(rp);
			return;
		}
		if (sn1 == sn2) {
			SET_DP_PINF(rp);
		} else {
			SET_DP_NINF(rp);
		}
		return;
	}

	/* Check for zero */
	if (IS_ZERO_DBL(d1) || IS_ZERO_DBL(d2)) {
		if (sn1 == sn2) {
			SETP_DP_PZERO(rp);
		} else {
			SETP_DP_NZERO(rp);
		}
		return;
	}

	/* Only one operand can be denormalized, else answer is zero */
	if (e1 == 0 && e2 == 0) {
		if (sn1 == sn2) {
			SETP_DP_PZERO(rp);
		} else {
			SETP_DP_NZERO(rp);
		}
		return;
	}

	doubleToLong(d1, &l1);
	doubleToLong(d2, &l2);
	leading1 = indexLeadingOne64(&l1);
	leading2 = indexLeadingOne64(&l2);

	if (e1 == 0) {
		leading = indexLeadingOne64(&l1);
		e1 -= DPFRACTION_LENGTH - leading - 1;
		d1bits--;
	}

	if (e2 == 0) {
		leading = indexLeadingOne64(&l2);
		e2 -= DPFRACTION_LENGTH - leading - 1;
		d2bits--;
	}

	e1 -= DPEXPONENT_BIAS;
	e2 -= DPEXPONENT_BIAS;

	i1l = LOW_U32_FROM_LONG64(l1);
	i1h = HIGH_U32_FROM_LONG64(l1);
	i2l = LOW_U32_FROM_LONG64(l2);
	i2h = HIGH_U32_FROM_LONG64(l2);

	pl = (U_64) i1l *(U_64) i2l;
	pm = pl;
	tmp = ((U_64) i1l * (U_64) i2h);
	pm += ((U_64) (LOW_U32_FROM_LONG64(tmp)) << 32);
	if (pm < (((U_64) (LOW_U32_FROM_LONG64(tmp)) << 32)))
		carry++;
	pu = (U_64) (HIGH_U32_FROM_LONG64(tmp));

	tmp = ((U_64) i2l * (U_64) i1h);
	pm += ((U_64) (LOW_U32_FROM_LONG64(tmp)) << 32);
	if (pm < (((U_64) (LOW_U32_FROM_LONG64(tmp))) << 32))
		carry++;
	pu += (U_64) (HIGH_U32_FROM_LONG64(tmp));
	pu += (U_64) carry;

	pu += ((U_64) i1h * (U_64) i2h);

	if (pu != 0) {
		resbits = 64 + indexLeadingOne64(&pu) + 1;
	} else {
		resbits = indexLeadingOne64(&pm) + 1;
	}
	leadingr = resbits - 1;
	tmpl = leading1+ leading2;

	lr = pu;
	e = e1 + e2;

	/* If a carry was generated */
	if ( leadingr > tmpl && leadingr >= DPFRACTION_LENGTH )  e += leadingr - tmpl;
	/* if a denormal was generated */
	if ( tmpl >= DPFRACTION_LENGTH && leadingr < DPFRACTION_LENGTH ) e -=1;

	be = e + DPEXPONENT_BIAS;

	longlongToDouble(&pu, &pm, be, &r);
	if (sn1 != sn2)
		HIGH_U32_FROM_DBL(r) = SET_DP_SIGN(&r, 1);

	HIGH_U32_FROM_DBL_PTR(rp) = HIGH_U32_FROM_DBL(r); 
	LOW_U32_FROM_DBL_PTR(rp) = LOW_U32_FROM_DBL(r);
}



/* both numbers should be denorm else the result is the larger normalized no*/
/* d1 - d2 */
static void oldSubDD(double d1, double d2, double *rp)
{
	double r;
	int e1, e2, ediff, aediff;
	U_64 l1, l2, lr;
	int sn = 0, be;
	int sn1, sn2, sres;
	int sr = 0;
	int  leading1, leading2, leadingr, tmp;
	U_64 truncValue1=0, truncValue2=0, truncValueSum=0;
	U_64 round=0, borrow=0;

	/* Check for NaN */
	if (IS_NAN_DBL(d1) || IS_NAN_DBL(d2)) {
		SET_DP_NAN(rp);
		return;
	}

	sn1 = GET_DP_SIGN(&d1);
	sn2 = GET_DP_SIGN(&d2);

	/* check for 0 */
	if( IS_ZERO_DBL(d1) ) {
		if( IS_ZERO_DBL(d2) ) {
			if( sn1 != 0 && sn2 == 0 ) {
				SETP_DP_NZERO(rp);
			} else {
				SETP_DP_PZERO(rp);
			}
		} else {
			HIGH_U32_FROM_DBL(d2) = SET_DP_SIGN(&d2, (sn2 != 0) ? 0 : 1);
			HIGH_U32_FROM_DBL_PTR(rp) = HIGH_U32_FROM_DBL(d2);
			LOW_U32_FROM_DBL_PTR(rp) = LOW_U32_FROM_DBL(d2);
		}
		return;
	};

	if (IS_ZERO_DBL(d2)) {
		HIGH_U32_FROM_DBL_PTR(rp) = HIGH_U32_FROM_DBL(d1);
		LOW_U32_FROM_DBL_PTR(rp) = LOW_U32_FROM_DBL(d1);
		return;
	}

	/* Check for infinity */
	if (IS_INF_DBL(d1) || IS_INF_DBL(d2)) {
		if (IS_INF_DBL(d1) && IS_INF_DBL(d2)) {
			if (sn1 != sn2) {
				/* -Inf - +Inf ==> -Inf
				 * +Inf - -Inf ==> +Inf
				 * Otherwise, NaN
				 */
				if( sn1 != 0 ) {
					SET_DP_NINF(rp);
				} else {
					SET_DP_PINF(rp);
				}
			} else {
				SET_DP_NAN(rp);
			}
			return;
		}
		if (IS_INF_DBL(d1)) {
			HIGH_U32_FROM_DBL_PTR(rp) = HIGH_U32_FROM_DBL(d1);
			LOW_U32_FROM_DBL_PTR(rp) = LOW_U32_FROM_DBL(d1);	
		} else {
			sres = (sn2 == 0) ? 1:0;
			HIGH_U32_FROM_DBL_PTR(rp) = HIGH_U32_FROM_DBL(d2);
			LOW_U32_FROM_DBL_PTR(rp) = LOW_U32_FROM_DBL(d2);	
			HIGH_U32_FROM_DBL_PTR(rp) = SET_DP_SIGN(rp, sres);
		}
		return;
	}

	e1 = GET_DP_EXPONENT(&d1);
	e2 = GET_DP_EXPONENT(&d2);

	ediff = e1 - e2;
	aediff = abs(ediff);

	HIGH_U32_FROM_DBL(d1) = SET_DP_SIGN(&d1, 0);
	HIGH_U32_FROM_DBL(d2) = SET_DP_SIGN(&d2, 0);

	/* both operands are denormalized */
	doubleToLong(d1, &l1);
	doubleToLong(d2, &l2);

	leading1 = indexLeadingOne64(&l1);
	leading2 = indexLeadingOne64(&l2);

	if (ediff > 0) {
		sr = (e2 != 0) ? -ediff : -ediff+1;
		truncValue2=shiftRight64(&l2, sr);
		e2 += ediff;
	} else if (ediff < 0) {
		sr = (e1 != 0) ? ediff : ediff+1;
		truncValue1=shiftRight64(&l1, sr);
		e1 += -ediff;
	}

	e1 -= DPEXPONENT_BIAS;
	e2 -= DPEXPONENT_BIAS;

	if (sn2 == 1) {
		/* +-l1+l2 */
		if (sn1 == 0) {
			lr = l1 + l2;
			sres = 0;
			truncValueSum = truncValue1 + truncValue2;
		} else {
			/* lr = l2 - l1; */
			/* sres = (l1<=l2)?0:1; */
			if (l1 > l2) {
				lr = l1 - l2;
				sres = 1;
				sres = (lr != 0) ? sres : 0;
				if (truncValue2 != 0) {
					borrow = 1;
					truncValueSum = (~truncValue2) + 1;
				}
			} else {
				lr = l2 - l1;
				sres = 0;
				if (truncValue1 != 0) {
					borrow = 1;
					truncValueSum = (~truncValue1) + 1;
				}
			}

		}
	} else {
		/* +-l1-l2 */
		if (sn1 == 0) {
			/* lr = l1 - l2; */
			/* sres = (l2<=l1)?0:1; */
			if (l1 > l2) {
				lr = l1 - l2;
				sres = 0;
				if (truncValue2 != 0) {
					borrow = 1;
					truncValueSum = (~truncValue2) + 1;
				}
			} else {
				lr = l2 - l1;
				sres = 1;
				sres = (lr != 0) ? sres : 0;
				if (truncValue1 != 0) {
					borrow = 1;
					truncValueSum = (~truncValue1) + 1;
				}
			}
		} else {
			lr = l1 + l2;
			sres = 1;
			truncValueSum = truncValue1 + truncValue2;
		}
	}

	HIGH_U32_FROM_LONG64(lr) = SET_LWDP_SIGN(&lr, 0);

	leadingr = indexLeadingOne64(&lr);
	tmp = ( leading1 > leading2) ? leading1: leading2;
	be = e1;
	be += DPEXPONENT_BIAS;

	/* was a carry out generated 	*/
	e1 += DPEXPONENT_BIAS;
	e2 += DPEXPONENT_BIAS;
	if ( (e1 ==0 && e2==0 ) && leadingr > tmp && leadingr >= DPFRACTION_LENGTH) be += leadingr - tmp;
	/* if a denormal was generated */
	if ( tmp >= DPFRACTION_LENGTH && (be+leadingr) < DPFRACTION_LENGTH ) be -=1;

	if ( lr != 0 || borrow != 0 || round != 0 ) {
		if(borrow) {
			lr = lr - 1;
		}

		longToDouble(lr, &truncValueSum, be, &r);
		roundDoubleSubtract( &r, truncValueSum, aediff );

		HIGH_U32_FROM_DBL(r) = SET_DP_SIGN(&r, sres);
		HIGH_U32_FROM_DBL_PTR(rp) = HIGH_U32_FROM_DBL(r);
		LOW_U32_FROM_DBL_PTR(rp) = LOW_U32_FROM_DBL(r);
	} else {
		HIGH_U32_FROM_DBL_PTR(rp) = 0x0; 
		LOW_U32_FROM_DBL_PTR(rp) = 0x0; 
	}
}



static void roundDoubleAdd( double *rp, U_64 truncValueSum, int aediff )
{
	U_64 mantissa = 0;
	U_32 round = 0;
	int rExp = GET_DP_EXPONENT(rp) - DPEXPONENT_BIAS;
	U_64 copyMant;
	U_32 lastDigit, r1Digit, r2Digit, searchDigit, curDigit;
	int iCount;

	/* No rounding is required if overflowed bits are all zero. */
	if( truncValueSum == 0 ) {
		return;
	}

	GET_DP_MANTISSA(rp, &mantissa);
	copyMant = mantissa;
	r1Digit = searchDigit = (U_32)(copyMant & 0xF0) >> 4;
	r2Digit = lastDigit = (U_32)(copyMant & 0xF);
	copyMant >>= 8;
	for( iCount=0; iCount<(DPFRACTION_LENGTH>>2)-2; iCount++ ) {
		curDigit = (U_32)(copyMant & 0xF);
		if( curDigit == searchDigit )
			break;
		r2Digit = r1Digit;
		r1Digit = curDigit;
		copyMant >>= 4;
	}

	if( (truncValueSum & J9CONST64(0x8000000000000000)) != 0 ) {
		/* These special cases indicate that there is still some problem
		 * in our addDD() code which will require further debugging at
		 * a later time.
		 */
		switch( rExp ) {
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
				/* Special case where last digit is affected before
				 * rounding, i.e. 1.3d + 16.4d
				 */
				if( (iCount == 0) && (r1Digit - r2Digit == 1) ) {
					round = 1;
				} else if( (iCount < (DPFRACTION_LENGTH>>2)-2) && (r1Digit == lastDigit) ) {
					round = r2Digit >= 8;
				} else {
					if( (truncValueSum & J9CONST64(0x4000000000000000))
							&& (truncValueSum & J9CONST64(0x3F00000000000000)) ) {
						round = 1;
					} else {
						round = (mantissa & 1) != 0;
					}
				}
				break;
			case 10:
				if( aediff < 2 ) {
					round = 1;
				} else {
					round = 0;
				}
				break;
			case 11:
				if( aediff < 2 ) {
					round = (lastDigit >= 8);
				} else {
					round = (mantissa & 1) != 0;
				}
				break;
			default:
				if( (truncValueSum & J9CONST64(0x7FFFFFFFFFFFFF00)) != 0 ) {
					round = 1;
				} else {
					round = (mantissa & 1) != 0;
				}
		}
	} else {
		round = 0;
	}

	if(round) {
		/* Increment the mantissa by 1. */
		mantissa++;
		SET_DP_MANTISSA(rp, &mantissa);
		/* It is possible for the mantissa to overflow, exceeding the 52-bit limit */
		if( mantissa > J9CONST64(0x000FFFFFFFFFFFFF) ) {
			U_32 exp = GET_DP_EXPONENT(rp) + 1;
			HIGH_U32_FROM_DBL_PTR(rp) = SET_DP_EXPONENT(rp, exp);
		}
	}
}



static void roundDoubleSubtract( double *rp, U_64 truncValueSum, int aediff )
{
	U_64 mantissa = 0;
	U_32 round = 0;

	/* No rounding is required if overflowed bits are all zero. */
	if( truncValueSum == 0 ) {
		return;
	}

	GET_DP_MANTISSA(rp, &mantissa);

	if( (truncValueSum & J9CONST64(0x8000000000000000)) != 0 ) {
		if( (truncValueSum & J9CONST64(0x7FFFFFFFFFFFFF00)) != 0 ) {
			round = 1;
		} else {
			round = (mantissa & 1) != 0;
		}
	} else {
		round = 0;
	}

	if(round) {
		/* Increment the mantissa by 1. */
		mantissa++;
		SET_DP_MANTISSA(rp, &mantissa);
		/* It is possible for the mantissa to overflow, exceeding the 52-bit limit */
		if( mantissa > J9CONST64(0x000FFFFFFFFFFFFF) ) {
			U_32 exp = GET_DP_EXPONENT(rp) + 1;
			HIGH_U32_FROM_DBL_PTR(rp) = SET_DP_EXPONENT(rp, exp);
		}
	}
}



static void oldAddDF(float f1, float f2, float *rp)
{
	float r;
	int e1, e2, ediff, aediff;
	U_32 i1, i2, ir;
	int sn1, sn2, sres;
	int sr = 0;
	int be = 0;
	int leading1, leading2, leadingr, tmp;
	U_32 truncValue1=0, truncValue2=0, truncValueSum=0;
	U_32 round=0, borrow=0;

	/* Check for NaN */
	if (IS_SP_NAN(f1) || IS_SP_NAN(f2)) {
		SET_SP_NAN(rp);
		return;
	}

	sn1 = GET_SP_SIGN(f1);
	sn2 = GET_SP_SIGN(f2);

	/* check for 0 */
	if (IS_SP_ZERO(f1)) {
		if( IS_SP_ZERO(f2) ) {
			/* If both -0.0, then return -0.0. Otherwise, return +0.0 */
			if( sn1 && sn2 ) {
				SETP_SP_NZERO(rp);
			} else {
				SETP_SP_PZERO(rp);
			}
		} else {
			FLOAT_WORD_P(rp) = FLOAT_WORD(f2);
		}
		return;
	}
	if (IS_SP_ZERO(f2)) {
		FLOAT_WORD_P(rp) = FLOAT_WORD(f1); 
		return;
	}

	/* Check for infinity */
	if (IS_SP_INF(f1) || IS_SP_INF(f2)) {
		if (IS_SP_INF(f1) && IS_SP_INF(f2)) {
			if (sn1 == sn2) {
				FLOAT_WORD_P(rp) = FLOAT_WORD(f1); 
			} else {
				SET_SP_NAN(rp);
			}
			return;
		}
		if (IS_SP_INF(f1))
			FLOAT_WORD_P(rp) = FLOAT_WORD(f1); 
		else
			FLOAT_WORD_P(rp) = FLOAT_WORD(f2); 
		return;
	}

	e1 = GET_SP_EXPONENT(f1);
	e2 = GET_SP_EXPONENT(f2);

	ediff = e1 - e2;
	aediff = abs(ediff);
	if (aediff > (SPEXPONENT_LENGTH + SPFRACTION_LENGTH)) {
		if (e1 > e2) {
			FLOAT_WORD_P(rp) = FLOAT_WORD(f1); 
			return;
		} else {
			FLOAT_WORD_P(rp) = FLOAT_WORD(f2); 
			return;
		}
	}

	FLOAT_WORD(f1) = SET_SP_SIGN(f1, 0);
	FLOAT_WORD(f2) = SET_SP_SIGN(f2, 0);
	floatToInt(f1, &i1);
	floatToInt(f2, &i2);

	leading1 = indexLeadingOne32(&i1);
	leading2 = indexLeadingOne32(&i2);

	if (ediff > 0) {
		sr = (e2 != 0) ? -ediff : -ediff +1;
		truncValue2 = shiftRight32(&i2, sr );
		e2 += ediff;
	} else if (ediff < 0) {
		sr = (e1 != 0) ? ediff : ediff +1;
		truncValue1 = shiftRight32(&i1, sr);
		e1 += -ediff;
	}
	e1 -= SPEXPONENT_BIAS;
	e2 -= SPEXPONENT_BIAS;

	/* exponents must be equal */
	if (sn1 == sn2) {
		ir = i1 + i2;
		sres = sn1;
		truncValueSum = truncValue1 + truncValue2;
	} else {
		if (i1 > i2) {
			ir = i1 - i2;
			if (truncValue2 != 0) {
				borrow = 1;
				truncValueSum = (~truncValue2) + 1;
			}
			sres = (ir != 0) ? sn1 : 0;
		} else {
			ir = i2 - i1;
			if (truncValue1 != 0) {
				borrow = 1;
				truncValueSum = (~truncValue1) + 1;
			}
			sres = (ir != 0) ? sn2 : 0;
		}
	}

	be = (e1 > e2) ? e1 : e2;
	be += SPEXPONENT_BIAS;

	leadingr = indexLeadingOne32(&ir);
	tmp = ( leading1 > leading2) ? leading1: leading2;

	/* was a carry out generated */
	e1 += SPEXPONENT_BIAS;
	e2 += SPEXPONENT_BIAS;
	if ( (e1 ==0 && e2==0 ) && leadingr > tmp && leadingr >= SPFRACTION_LENGTH) be += leadingr - tmp;

	/* if a denormal was generated 	*/
	if ( tmp >= SPFRACTION_LENGTH && (be + leadingr) < SPFRACTION_LENGTH  ) be -=1;

	if ( ir != 0 || borrow != 0 || round != 0 ) {
		int nOvflBits = 0;
		if(borrow) {
			*(U_32 *)&ir = *(U_32 *)&ir -1;
		}
		intToFloat(ir, &truncValueSum, be, &r);
		nOvflBits = numOverflowedBits32(truncValueSum);

		/* We round single-precision floating-point numbers based on
		 * the difference in the exponents of the two given values and
		 * the overflowed bits. The most significant bit (bit 31), or
		 * the two adjacent ones (bits 30 and 29) and some of the
		 * lesser significant bits (bits 28 to 0) must be set for us to
		 * round up.
		 */
		if( aediff <= SPFRACTION_LENGTH ) {
			round = (truncValueSum & 0x80000000)
							|| ((truncValueSum & 0x40000000) && (truncValueSum & 0x20000000)
						&& (truncValueSum & 0x7fffffff));
		} else if( aediff == (SPFRACTION_LENGTH + 1) ) {
			/* This is a boundary case where the difference in the
			 * exponents is greater than the single-precision
			 * fractional length. If there is only one overflowed bit,
			 * then it must be the most significant one, and we round
			 * up if the last bit in the float is set. Otherwise, we
			 * do not round up.
			 */
			if( nOvflBits < 2 ) {
				round = (truncValueSum & 0x80000000) && ((FLOAT_WORD(r) & 1) != 0);
			} else {
				round = (truncValueSum & 0x80000000)
								|| ((truncValueSum & 0x40000000) && (truncValueSum & 0x20000000)
									&& (truncValueSum & 0x7fffffff));
			}
		} else {
			round = (truncValueSum & 0x80000000) && (truncValueSum & 0x7fffffff);
		}

		if(round) {
			*(U_32 *)&r = *(U_32 *)&r +1;
		}

		FLOAT_WORD(r) = SET_SP_SIGN(r, sres);
		FLOAT_WORD_P(rp) = FLOAT_WORD(r); 
	} else {
		FLOAT_WORD_P(rp) = 0x0; 
	}
}



/* d1 - d2 */
static void oldSubDF(float f1, float f2, float *rp)
{
	float r;
	int e1, e2, ediff, aediff;
	U_32 i1, i2, ir;
	int sn = 0, be;
	int sn1, sn2, sres;
	int sr = 0;
	int leading1, leading2, leadingr, tmp;
	U_32 truncValue1=0, truncValue2=0, truncValueSum=0;
	U_32 round=0, borrow=0;

	/* Check for NaN */
	if (IS_SP_NAN(f1) || IS_SP_NAN(f2)) {
		SET_SP_NAN(rp);
		return;
	}

	sn1 = GET_SP_SIGN(f1);
	sn2 = GET_SP_SIGN(f2);

	/* check for 0 */
	if (IS_SP_ZERO(f2)) {
		if( IS_SP_ZERO(f1) ) {
			/* +0.0 - +0.0 ==> +0.0
			 * -0.0 - -0.0 ==> +0.0
			 * +0.0 - -0.0 ==> +0.0
			 * -0.0 - +0.0 ==> -0.0
			 */
			if( sn1 != 0 && sn2 == 0 ) {
				SETP_SP_NZERO(rp);
			} else {
				SETP_SP_PZERO(rp);
			}
		} else {
			FLOAT_WORD_P(rp) = FLOAT_WORD(f1);
		}
		return;
	}
	if (IS_SP_ZERO(f1)) {
		sn = GET_SP_SIGN(f2);
		FLOAT_WORD(f2) = SET_SP_SIGN(f2, (sn == 1) ? 0 : 1);
		FLOAT_WORD_P(rp) = FLOAT_WORD(f2); 
		return;
	}

	/* Check for infinity */
	if (IS_SP_INF(f1) || IS_SP_INF(f2)) {
		if (IS_SP_INF(f1) && IS_SP_INF(f2)) {
			if (sn1 != sn2) {
				/* -Inf - +Inf ==> -Inf
				 * +Inf - -Inf ==> +Inf
				 * Otherwise, NaN
				 */
				if( sn1 != 0 ) {
					SET_SP_NINF(rp);
				} else {
					SET_SP_PINF(rp);
				}
			} else {
				SET_SP_NAN(rp);
			}
			return;
		}
		if (IS_SP_INF(f1)) {
			FLOAT_WORD_P(rp) = FLOAT_WORD(f1); 
		} else {
			sres = (sn2 == 0) ? 1:0;
			FLOAT_WORD(f2) = SET_SP_SIGN(f2, sres);
			FLOAT_WORD_P(rp) = FLOAT_WORD(f2); 
		}   
		return;
	}

	e1 = GET_SP_EXPONENT(f1);
	e2 = GET_SP_EXPONENT(f2);

	ediff = e1 - e2;
	aediff = abs(ediff);
	/* TBN - 20040511 PR# CMVC-73344
		This optimization below causes the short-cuts the actual work, but it causes the some rounding
		problem, so we will just comment it out and let the actual calculation and rounding to proceed.
	 */
	/*
	if (aediff > (SPFRACTION_LENGTH)) {
		if (e1 > e2) {
			FLOAT_WORD_P(rp) = FLOAT_WORD(f1); 
			return;
		} else {
			sn = GET_SP_SIGN(f2);
			FLOAT_WORD(f2) = SET_SP_SIGN(f2, (sn == 1) ? 0 : 1);
			FLOAT_WORD_P(rp) = FLOAT_WORD(f2); 
			return;
		}
	}
	*/

	FLOAT_WORD(f1) = SET_SP_SIGN(f1, 0);
	FLOAT_WORD(f2) = SET_SP_SIGN(f2, 0);

	// both operands are denormalized
	floatToInt(f1, &i1);
	floatToInt(f2, &i2);

	leading1 = indexLeadingOne32(&i1);
	leading2 = indexLeadingOne32(&i2);

	if (ediff > 0) {
		sr = (e2 != 0) ? -ediff : -ediff +1;
		truncValue2=shiftRight32(&i2, sr);
		e2 += ediff;
	} else if (ediff < 0) {
		sr = (e1 != 0) ? ediff : ediff +1;
		truncValue1=shiftRight32(&i1, sr);
		e1 += -ediff;
	}
	e1 -= SPEXPONENT_BIAS;
	e2 -= SPEXPONENT_BIAS;

	if (sn2 == 1) {
		// +-l1+l2 
		if (sn1 == 0) {
			ir = i1 + i2;
			sres = 0;
			truncValueSum = truncValue1 + truncValue2;
		} else {
			// lr = l2 - l1;
			// sres = (i1<=i2)?0:1;
			if (i1 > i2) {
				ir = i1 - i2;
				sres = 1;
				sres = (ir != 0) ? sres : 0;
				if (truncValue2 != 0) {
					borrow = 1;
					truncValueSum = (~truncValue2) + 1;
				}

			} else {
				ir = i2 - i1;
				sres = 0;
				if (truncValue1 != 0) {
					borrow = 1;
					truncValueSum = (~truncValue1) + 1;
				}
			}

		}
	} else {
		// +-l1-l2 
		if (sn1 == 0) {
			// lr = l1 - l2;
			// sres = (l2<=l1)?0:1;
			if (i1 > i2) {
				ir = i1 - i2;
				sres = 0;
				if (truncValue2 != 0) {
					borrow = 1;
					truncValueSum = (~truncValue2) + 1;
				}

			} else {
				ir = i2 - i1;
				sres = 1;
				sres = (ir != 0) ? sres : 0;
				if (truncValue1 != 0) {
					borrow = 1;
					truncValueSum = (~truncValue1) + 1;
				}
			}
		} else {
			ir = i1 + i2;
			sres = 1;
			truncValueSum = truncValue1 + truncValue2;
		}
	}

	ir = SET_SP_SIGN(ir, 0);

	be = e1;
	be += SPEXPONENT_BIAS;

	leadingr = indexLeadingOne32(&ir);
	tmp = ( leading1 > leading2) ? leading1: leading2;

	/* was a carry out generated */
	e1 += SPEXPONENT_BIAS;
	e2 += SPEXPONENT_BIAS;
	if ( (e1 ==0 && e2==0 ) && leadingr > tmp && leadingr >= SPFRACTION_LENGTH) be += leadingr - tmp;

	/* if a denormal was generated */
	if ( tmp >= SPFRACTION_LENGTH && (be+leadingr) < SPFRACTION_LENGTH ) be -=1;

	if ( ir != 0 || borrow != 0 || round != 0 ) {
		int nOvflBits = 0;
		if(borrow) {
			*(U_32 *)&ir = *(U_32 *)&ir -1;
		}
		intToFloat(ir, &truncValueSum, be, &r);
		nOvflBits = numOverflowedBits32(truncValueSum);

		/* We round single-precision floating-point numbers based on
		 * the difference in the exponents of the two given values and
		 * the overflowed bits. The most significant bit (bit 31), or
		 * the two adjacent ones (bits 30 and 29) and some of the
		 * lesser significant bits (bits 28 to 0) must be set for us to
		 * round up.
		 */
		if( aediff <= 8 ) {
			if( (truncValueSum & 0x80000000) != 0 ) {
				if( (truncValueSum & 0x7fff0000) != 0 ) {
					round = 1;
				} else {
					round = (FLOAT_WORD(r) & 1) != 0;
				}
			} else {
				round = 0;
			}
		} else if( aediff <= SPFRACTION_LENGTH ) {
			if( (truncValueSum & 0x80000000) != 0 ) {
				if( (truncValueSum & 0x7f00000) != 0 ) {
					round = 1;
				} else {
					round = (FLOAT_WORD(r) & 1) != 0;
				}
			} else {
				round = 0;
			}
		} else if( aediff == (SPFRACTION_LENGTH + 1) ) {
			/* This is a boundary case where the difference in the
			 * exponents is greater than the single-precision
			 * fractional length. If there is only one overflowed bit,
			 * then it must be the most significant one, and we round
			 * up if the last bit in the float is set. Otherwise, we
			 * do not round up.
			 */
			if( nOvflBits < 2 ) {
				round = (truncValueSum & 0x80000000) && ((FLOAT_WORD(r) & 1) != 0);
			} else {
				round = (truncValueSum & 0x80000000)
								|| ((truncValueSum & 0x40000000) && (truncValueSum & 0x20000000)
									&& (truncValueSum & 0x7fffffff));
			}
		} else {
			if( (truncValueSum & 0x80000000) != 0 ) {
				if( (truncValueSum & 0x7f00000) != 0 ) {
					round = 1;
				} else {
					round = (FLOAT_WORD(r) & 1) != 0;
				}
			} else {
				round = 0;
			}
		}

		if(round) {
			*(U_32 *)&r = *(U_32 *)&r +1;
		}

		FLOAT_WORD(r) = SET_SP_SIGN(r, sres);
		FLOAT_WORD_P(rp) = FLOAT_WORD(r); 
	} else {
		FLOAT_WORD_P(rp) = 0x0; 
	}
}



#endif /* define(OLD_DBL_MATH) */


