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



#include "j9.h"

/* include '[' for arrays (character after 'Z') */
const U_8 argCountCharConversion[] = {
	0,  /* A */
	1,  /* B */
	1,  /* C */
	2,  /* D */
	0,  /* E */
	1,  /* F */
	0,  /* G */
	0,  /* H */
	1,  /* I */
	2,  /* J */
	0,  /* K */
	1,  /* L */
	0,  /* M */
	0,  /* N */
	0,  /* O */
	0,  /* P */
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
	1,  /* Q */
#else
	0,  /* Q */
#endif /* J9VM_OPT_VALHALLA_VALUE_TYPES */
	0,  /* R */
	1,  /* S */
	0,  /* T */
	0,  /* U */
	0,  /* V */
	0,  /* W */
	0,  /* X */
	0,  /* Y */
	1,  /* Z */
	1,  /* [ */
	0   /* \ */
}; 



                 

