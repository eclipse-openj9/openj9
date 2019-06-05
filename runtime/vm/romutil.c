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

/* File renamed to romutil.c */

#include <string.h>
#include "j9.h"
#include "j9protos.h"
#include "j9consts.h"
#include "romcookie.h"
#include "objhelp.h"
#include "j9vmnls.h"
#include "vm_internal.h"
#include "omrlinkedlist.h"
#include "ut_j9vm.h"

J9ROMClass* 
checkRomClassForError( J9ROMClass *romClass, J9VMThread *vmThread ) 
{
	/* check for error'ed rom class */
	if ( romClass->singleScalarStaticCount != 0xFFFFFFFF ) return romClass;

	Trc_VM_checkRomClassForError_ErrorFound(vmThread, romClass);

	/* found an error'ed rom class tell the folks back home*/
	return 0; 
}

void 
setExceptionForErroredRomClass( J9ROMClass *romClass, J9VMThread *vmThread ) 
{
	J9ROMClassCfrError *romError = (J9ROMClassCfrError *)J9ROMCLASS_SUPERCLASSNAME(romClass);
	J9UTF8 *className = J9ROMCLASS_CLASSNAME(romClass);
	const char *errorString = NULL;
	j9object_t errorStringObject = NULL;
	J9CfrError localError;

	PORT_ACCESS_FROM_VMC( vmThread );

	/* check for error'ed rom class */
	if ( romClass->singleScalarStaticCount != 0xFFFFFFFF ) {
		return;
	}

	localError.errorCode = romError->errorCode;
	localError.errorAction = romError->errorAction;
	localError.errorCatalog = romError->errorCatalog;
	localError.errorOffset = romError->errorOffset;
	localError.errorMethod = -1;
	localError.errorPC = romError->errorPC;
	localError.errorMember = NULL;
	localError.constantPool = NULL;

	/* found an error'ed rom class, now setup an exception */
	if ( romError->errorMethod == -1 ) {
		errorString = getJ9CfrErrorDetailMessageNoMethod(
			PORTLIB,
			&localError, 
			J9UTF8_DATA(className),
			J9UTF8_LENGTH(className));
	} else if ( romError->errorMethod == -2 ) {
		/* do nothing here */
	} else {
		J9ROMClassCfrConstantPoolInfo *name, *sig;
		J9ROMClassCfrConstantPoolInfo *base = J9ROMCLASSCFRERROR_CONSTANTPOOL(romError);
		struct J9ROMClassCfrMember *errMember = J9ROMCLASSCFRERROR_ERRORMEMBER(romError);

		name = &(base[errMember->nameIndex]);
		sig = &(base[errMember->descriptorIndex]);

		errorString = getJ9CfrErrorDetailMessageForMethod(
			PORTLIB, 
			&localError, 
			J9UTF8_DATA(className),
			J9UTF8_LENGTH(className),
			J9ROMCLASSCFRCONSTANTPOOLINFO_BYTES(name),
			name->slot1,
			J9ROMCLASSCFRCONSTANTPOOLINFO_BYTES(sig),
			sig->slot1,
			NULL,
			0);
	}
	
	if ( errorString != NULL) {
		errorStringObject = vmThread->javaVM->memoryManagerFunctions->j9gc_createJavaLangString(vmThread, (U_8 *)errorString, strlen(errorString), 0);
	}
	vmThread->javaVM->internalVMFunctions->setCurrentException( vmThread, romError->errorAction, (UDATA *)errorStringObject );

	j9mem_free_memory((void *) errorString);
}

J9ROMClass *
romClassLoadFromCookie (J9VMThread *vmStruct, U_8 *clsName, UDATA clsNameLength, U_8 *romClassBytes, UDATA romClassLength)
{
	J9ROMClass *loadedClass = NULL;
	J9ROMClassCookie *cookie;

	/* check for Cookie in the romClassBytes */
	if ( romClassLength >= sizeof(J9ROMClassCookie) ) {
		U_8 romClassCookieSig[] = J9_ROM_CLASS_COOKIE_SIG;
		cookie = (J9ROMClassCookie *)romClassBytes;
		if (memcmp(cookie->signature, romClassCookieSig, J9_ROM_CLASS_COOKIE_SIG_LENGTH) != 0) {
			return NULL;
		}
	}
	else {
		return NULL;
	}

	Trc_VM_romClassLoadFromCookie_Entry1(vmStruct, vmStruct, clsName, clsNameLength, romClassBytes, romClassLength);

	if ( cookie->version != J9_ROM_CLASS_COOKIE_VERSION ) {
		return NULL;
	}

	switch ( cookie->type ) {
#if defined(J9VM_OPT_SHARED_CLASSES)
		case J9_ROM_CLASS_COOKIE_TYPE_SHARED_CLASS: {
			J9ROMClassCookieSharedClass *jxeCookie = (J9ROMClassCookieSharedClass *)cookie;
			BOOLEAN verbose = (vmStruct->javaVM->dynamicLoadBuffers->flags & BCU_VERBOSE) != 0;

			if ( clsName != NULL ) {
				J9ROMClass* romClass = jxeCookie->romClass;
				J9UTF8 *utf = J9ROMCLASS_CLASSNAME(jxeCookie->romClass);

				if ( !J9UTF8_DATA_EQUALS( clsName, clsNameLength, J9UTF8_DATA(utf), J9UTF8_LENGTH(utf) ) ) {
					return NULL;
				}
				/* Check that the cookie originated from shared.c and has not been created in Java */
				if (jxeCookie->magic != J9_ROM_CLASS_COOKIE_MAGIC(vmStruct->javaVM, romClass)) {
					return NULL;
				}
			} 

			if (verbose) {
				/* Don't report dynload events for shared classes - it is useful to see dynload verbose for those classes which are not cached */
				memset(&vmStruct->javaVM->dynamicLoadBuffers->dynamicLoadStats->nameLength, 0, sizeof(J9DynamicLoadStats) - sizeof(UDATA) - sizeof(U_8 *));
			}

			loadedClass = jxeCookie->romClass;

			break;
		}
#endif
		default:
			return NULL;
	}

	Trc_VM_romClassLoadFromCookie_Exit(vmStruct, loadedClass);
	
	return loadedClass;
}
