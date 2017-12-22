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


#ifndef bcvcfr_h
#define bcvcfr_h

/* Provide a endian aware version of the NNSRP_GET macro.  
   This permits the bytecode verifier to be endian neutral when using
	 the SRP macros */

#include "j9comp.h"

/* Must appear before the j9.h include */
#define NNSRP_GET(field, type) ((type) (((U_8 *) &(field)) + ((I_32) (field))))

#include "j9.h"

#define PARAM_8(index, offset) \
	((index)[offset])

#ifdef J9VM_ENV_LITTLE_ENDIAN
#define PARAM_16(index, offset)	\
	( ( ((U_16) (index)[offset])			)	\
	| ( ((U_16) (index)[offset + 1]) << 8)	\
	)
#else
#define PARAM_16(index, offset)	\
	( ( ((U_16) (index)[offset]) << 8)	\
	| ( ((U_16) (index)[offset + 1])			)	\
	)
#endif

#ifdef J9VM_ENV_LITTLE_ENDIAN
#define PARAM_32(index, offset)						\
	( ( ((U_32) (index)[offset])					)	\
	| ( ((U_32) (index)[offset + 1]) << 8 )	\
	| ( ((U_32) (index)[offset + 2]) << 16)	\
	| ( ((U_32) (index)[offset + 3]) << 24)	\
	)
#else
#define PARAM_32(index, offset)						\
	( ( ((U_32) (index)[offset])		 << 24)	\
	| ( ((U_32) (index)[offset + 1]) << 16)	\
	| ( ((U_32) (index)[offset + 2]) << 8 )	\
	| ( ((U_32) (index)[offset + 3])			)	\
	)
#endif

#include "rommeth.h"

#endif     /* bcvcfr_h */


