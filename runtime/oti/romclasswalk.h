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

#ifndef romclasswalk_h
#define romclasswalk_h

#include "j9.h"

#define J9ROM_U8 0
#define J9ROM_U16 1
#define J9ROM_U32 2
#define J9ROM_U64 3
#define J9ROM_SRP 4
#define J9ROM_UTF8 5
#define J9ROM_NAS 6
#define J9ROM_AOT 7
#define J9ROM_WSRP 8
#define J9ROM_UTF8_NOSRP 9
#define J9ROM_INTERMEDIATECLASSDATA 10

typedef void(*J9ROMClassSlotCallback)(J9ROMClass*, U_32, void*, const char*, void*);
typedef void(*J9ROMClassSectionCallback)(J9ROMClass*, void*, UDATA, const char*, void*);
typedef BOOLEAN(*J9ROMClassValidateRangeCallback)(J9ROMClass*, void*, UDATA, void*);

typedef struct J9ROMClassWalkCallbacks {
	J9ROMClassSlotCallback slotCallback;
	J9ROMClassSectionCallback sectionCallback;
	J9ROMClassValidateRangeCallback validateRangeCallback;
} J9ROMClassWalkCallbacks;

#endif     /* romclasswalk_h */
