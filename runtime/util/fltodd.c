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



#include "j9comp.h"
#include "fltconst.h"

int isDoubleOdd (double d);




/* answer true if d represents an odd integer (used by pow) */
int isDoubleOdd(double d)  
{
	U_32 hi32 = HIGH_U32_FROM_DBL(d), lo32;
	I_32 exponent = 1075 - ((hi32 & DOUBLE_EXPONENT_MASK_HI) >>20);  /*shifted exp*/
	U_64 m,int_part;

	if(exponent>52 || exponent<0)
		return 0;

	lo32 = LOW_U32_FROM_DBL(d);
	m = (U_64)(hi32 & DOUBLE_MANTISSA_MASK_HI) << 32;
	m += lo32 & DOUBLE_MANTISSA_MASK_LO;
	m |= (U_64)0x1 << 52; /* the implicit bit */

	int_part=m >> exponent;

	/*now just see if it's integral AND then odd*/
	return m==(int_part<<exponent) && (int_part & 0x1);
}



