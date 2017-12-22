/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#ifndef j9dbgext_h
#define j9dbgext_h

#include <setjmp.h>

#define TARGET_NNSRP_GET(field, type) ((type) (((U_8 *) dbgLocalToTarget(&(field))) + (field)))
#define TARGET_SRP_GET(field, type) ((field) ? TARGET_NNSRP_GET(field, type) : NULL)
#define LOCAL_NNSRP_GET(field, type) ((type) (((U_8 *)(&(field))) + (field)))
#define LOCAL_SRP_GET(field, type) ((field) ? LOCAL_NNSRP_GET(field, type) : NULL)

#define TARGET_NNWSRP_GET(field, type) ((type) (((U_8 *) dbgLocalToTarget(&(field))) + (field)))
#define TARGET_WSRP_GET(field, type) ((field) ? TARGET_NNWSRP_GET(field, type) : NULL)
#define LOCAL_NNWSRP_GET(field, type) ((type) (((U_8 *)(&(field))) + (field)))
#define LOCAL_WSRP_GET(field, type) ((field) ? LOCAL_NNWSRP_GET(field, type) : NULL)

#ifndef DONT_REDIRECT_SRP
#ifdef NNSRP_GET
#error NNSRP_GET is already defined. #include "dbgext.h" must appear above #include "j9.h"
#else
#define NNSRP_GET(field, type) TARGET_NNSRP_GET(field, type)
#endif

#ifdef NNWSRP_GET
#error NNWSRP_GET is already defined. #include "dbgext.h" must appear above #include "j9.h"
#else
#define NNWSRP_GET(field, type) TARGET_NNWSRP_GET(field, type)
#endif

#define J9DBG_AVL_SRP_GETNODE(node) ((J9AVLTreeNode *)(AVL_GETNODE(node) ? ((((U_8 *) dbgLocalToTarget(&(node))) + (J9WSRP)(AVL_GETNODE(node)))) : NULL))

#endif

#define DBGEXT_READFROMMEMORY 0
#define DBGEXT_READFROMCOREFILE 1

#include "j9.h"

/* redefine thread functions */
#include "../thread/omrthreadinspect.h"
#include "dbgext_internal.h"

/* the following macros may be used for error handling within debug extensions.
 * e.g
 * 	DBG_TRY {
 * 		doSomethingRisky();
 * 	} DBG_CATCH {
 *		dbgPrint("An error occurred\n");
 *  } DBG_FINALLY;
 *
 * All three macros MUST be used.
 * Code MUST NOT return from the TRY block, or exit it in any other abnormal way (e.g. break, continue, goto)
 */

typedef struct {
	jmp_buf buf;
} DBG_JMPBUF;

#define DBG_TRY do { \
	DBG_JMPBUF dbgJmpbuf; \
	DBG_JMPBUF *dbgOldHandler = dbgSetHandler(&dbgJmpbuf); \
	int dbgSetjmpResult = setjmp(dbgJmpbuf.buf); \
	if (dbgSetjmpResult == 0)
#define DBG_CATCH \
	dbgSetHandler(dbgOldHandler); \
	if (dbgSetjmpResult != 0)
#define DBG_FINALLY } while (0)

#ifdef __cplusplus
extern "C" {
#endif

void dbgext_walkj9pool(const char *args);

J9PortLibrary * dbgGetPortLibrary(void);

void dbgUnmapPool (J9Pool *pool);
J9Pool *dbgMapPool (UDATA addr);
extern void dbgPrint (const char* message, ...);
extern void dbgReadMemory (UDATA address, void *structure, UDATA size, UDATA *bytesRead);
extern UDATA dbgReadSlot (UDATA srcaddr, UDATA size);
extern UDATA dbgGetExpression (const char* args);
extern void dbgSetVerboseMode(int verboseMode);
extern I_32 dbg_j9port_create_library(J9PortLibrary *portLib, J9PortLibraryVersion *version, UDATA size);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif     /* j9dbgext_h */


