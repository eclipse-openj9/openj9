/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

#include "j9cfg.h"
#include "j9comp.h"
#include "util_internal.h"

void helperLongDivideLong (I_64 *p1, I_64 *p2, I_64 *pRes);
void helperLongRemainderLong (I_64 *p1, I_64 *p2, I_64 *pRes);
void helperLongMultiplyLong (I_64 *p1, I_64 *p2, I_64 *pRes);


I_64 
helperCLongDivideLong(I_64 a, I_64 b)
{
	I_64 result;

#if defined(LINUX) && defined(J9X86)
	
	/* Library bug does not correctly divide with negative divisors 
	 */
	if (b < 0) {
		if (b == I_64_MIN) {
			if (a == I_64_MIN) {
				result = 1;
			} else {
				result = 0;
			}
		} else {
			result = -(a / -b);
		}
	} else {
		result = a / b;
	}
#elif defined(J9ZOS390)
	
	/* On zOS, avoid fixed point exception for this special case. 
	 */
	if (b == -1 && a == I_64_MIN) {
		result = a;
	} else {
		result = a / b;
	}
#else
	result = a / b;
#endif

	return result;
}


void 
helperLongDivideLong(I_64 *a, I_64 *b, I_64 *c)
{
	I_64 tmpA, tmpB, tmpC;

	PTR_LONG_VALUE(a, &tmpA);
	PTR_LONG_VALUE(b, &tmpB);
	tmpC = helperCLongDivideLong (tmpA, tmpB);
	PTR_LONG_STORE(c, &tmpC);
}
	

I_64 
helperCLongRemainderLong(I_64 a, I_64 b)
{
	I_64 result;

#if defined(LINUX) && defined(J9X86)
	
	/* Library bug does not correctly remainder with negative divisors
	 */
	if (b < 0) {
		if (b == I_64_MIN) {
			if (a == I_64_MIN) {
				result = 0;
			} else {
				result = a;
			}
		} else {
			result = a % -b;
		}
	} else {
		result = a % b;
	}
#elif defined(J9ZOS390)
	
	/* On zOS, avoid fixed point exception for this special case. 
	 */
	if (b == -1 && a == I_64_MIN) {
		result = 0;
	} else {
		result = a % b;
	}
#else
	result = a % b;
#endif

	return result;
}


void 
helperLongRemainderLong(I_64 *a, I_64 *b, I_64 *c)
{
	I_64 tmpA, tmpB, tmpC;

	PTR_LONG_VALUE(a, &tmpA);
	PTR_LONG_VALUE(b, &tmpB);
	tmpC = helperCLongRemainderLong (tmpA, tmpB);
	PTR_LONG_STORE(c, &tmpC);
}


I_64 
helperCLongMultiplyLong(I_64 a, I_64 b)
{
	return a * b;
}


void 
helperLongMultiplyLong(I_64 *a, I_64 *b, I_64 *c)
{
	I_64 tmpA, tmpB, tmpC;

	PTR_LONG_VALUE(a, &tmpA);
	PTR_LONG_VALUE(b, &tmpB);
	tmpC = helperCLongMultiplyLong (tmpA, tmpB);
	PTR_LONG_STORE(c, &tmpC);
}


