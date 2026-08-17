/*******************************************************************************
 * Copyright IBM Corp. and others 2025
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

#include "ctest.h"
#include "cassume_api.h"

/* U_64 following a smaller field */

typedef struct J9AlignU64AfterU32 {
	U_32 before;
	U_64 after;
} J9AlignU64AfterU32;

typedef struct J9AlignU64AfterBOOLEAN {
	BOOLEAN before;
	U_64 after;
} J9AlignU64AfterBOOLEAN;

typedef struct J9AlignU64AfterU16 {
	U_16 before;
	U_64 after;
} J9AlignU64AfterU16;

typedef struct J9AlignU64AfterU8 {
	U_8 before;
	U_64 after;
} J9AlignU64AfterU8;

/* UDATA following a smaller field */

typedef struct J9AlignUDATAAfterU32 {
	U_32 before;
	UDATA after;
} J9AlignUDATAAfterU32;

typedef struct J9AlignUDATAAfterBOOLEAN {
	BOOLEAN before;
	UDATA after;
} J9AlignUDATAAfterBOOLEAN;

typedef struct J9AlignUDATAAfterU16 {
	U_16 before;
	UDATA after;
} J9AlignUDATAAfterU16;

typedef struct J9AlignUDATAAfterU8 {
	U_8 before;
	UDATA after;
} J9AlignUDATAAfterU8;

/* U_32 following a smaller field */

typedef struct J9AlignU32AfterU16 {
	U_16 before;
	U_32 after;
} J9AlignU32AfterU16;

typedef struct J9AlignU32AfterU8 {
	U_8 before;
	U_32 after;
} J9AlignU32AfterU8;

/* BOOLEAN following a smaller field */

typedef struct J9AlignBOOLEANAfterU16 {
	U_16 before;
	BOOLEAN after;
} J9AlignBOOLEANAfterU16;

typedef struct J9AlignBOOLEANAfterU8 {
	U_8 before;
	BOOLEAN after;
} J9AlignBOOLEANAfterU8;

void
verifyJ9StructAlignment(void)
{
	PORT_ACCESS_FROM_PORT(cTestPortLib);
	j9tty_printf(PORTLIB, "Verifying J9 struct field alignment\n");

	/* U_64 after U_32 */
	j9_assume(offsetof(J9AlignU64AfterU32, before) % sizeof(U_32), 0);
	j9_assume(offsetof(J9AlignU64AfterU32, after) % sizeof(U_64), 0);

	/* U_64 after BOOLEAN */
	j9_assume(offsetof(J9AlignU64AfterBOOLEAN, before) % sizeof(BOOLEAN), 0);
	j9_assume(offsetof(J9AlignU64AfterBOOLEAN, after) % sizeof(U_64), 0);

	/* U_64 after U_16 */
	j9_assume(offsetof(J9AlignU64AfterU16, before) % sizeof(U_16), 0);
	j9_assume(offsetof(J9AlignU64AfterU16, after) % sizeof(U_64), 0);

	/* U_64 after U_8 */
	j9_assume(offsetof(J9AlignU64AfterU8, before) % sizeof(U_8), 0);
	j9_assume(offsetof(J9AlignU64AfterU8, after) % sizeof(U_64), 0);

	/* UDATA after U_32 */
	j9_assume(offsetof(J9AlignUDATAAfterU32, before) % sizeof(U_32), 0);
	j9_assume(offsetof(J9AlignUDATAAfterU32, after) % sizeof(UDATA), 0);

	/* UDATA after BOOLEAN */
	j9_assume(offsetof(J9AlignUDATAAfterBOOLEAN, before) % sizeof(BOOLEAN), 0);
	j9_assume(offsetof(J9AlignUDATAAfterBOOLEAN, after) % sizeof(UDATA), 0);

	/* UDATA after U_16 */
	j9_assume(offsetof(J9AlignUDATAAfterU16, before) % sizeof(U_16), 0);
	j9_assume(offsetof(J9AlignUDATAAfterU16, after) % sizeof(UDATA), 0);

	/* UDATA after U_8 */
	j9_assume(offsetof(J9AlignUDATAAfterU8, before) % sizeof(U_8), 0);
	j9_assume(offsetof(J9AlignUDATAAfterU8, after) % sizeof(UDATA), 0);

	/* U_32 after U_16 */
	j9_assume(offsetof(J9AlignU32AfterU16, before) % sizeof(U_16), 0);
	j9_assume(offsetof(J9AlignU32AfterU16, after) % sizeof(U_32), 0);

	/* U_32 after U_8 */
	j9_assume(offsetof(J9AlignU32AfterU8, before) % sizeof(U_8), 0);
	j9_assume(offsetof(J9AlignU32AfterU8, after) % sizeof(U_32), 0);

	/* BOOLEAN after U_16 */
	j9_assume(offsetof(J9AlignBOOLEANAfterU16, before) % sizeof(U_16), 0);
	j9_assume(offsetof(J9AlignBOOLEANAfterU16, after) % sizeof(BOOLEAN), 0);

	/* BOOLEAN after U_8 */
	j9_assume(offsetof(J9AlignBOOLEANAfterU8, before) % sizeof(U_8), 0);
	j9_assume(offsetof(J9AlignBOOLEANAfterU8, after) % sizeof(BOOLEAN), 0);
}
