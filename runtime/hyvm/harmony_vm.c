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

#include <string.h>
#include "hyport.h"
/* To prevent clashes between hyport.h and j9port.h on certain defines undef them here. */
#undef PORT_ACCESS_FROM_ENV
#undef PORT_ACCESS_FROM_JAVAVM
#undef PORT_ACCESS_FROM_VMI
#undef PORT_ACCESS_FROM_PORT
#undef PORTLIB 

#include "harmony_vm.h"
#include "jni.h"
#include "j9port.h"
#include "j9lib.h"

#define HARMONY_PORTLIB_ARG "_org.apache.harmony.vmi.portlib"


typedef I_32 (*hyport_allocate_library_maptoj9) (struct J9PortLibrary *j9PortLib, struct HyPortLibraryVersion *expectedVersion, struct HyPortLibrary **portLibrary);
static hyport_allocate_library_maptoj9 GlobalHyPortAllocateLibraryMapToJ9 = NULL;

/* A single port library used for all invocations */
static struct HyPortLibrary* harmonyPortLibrary = NULL;


static struct HyPortLibrary *
createHarmonyPortLibrary(J9PortLibrary *j9PortLib)
{
	HyPortLibraryVersion portLibraryVersion;
	I_32 rc;
	UDATA hyprtshimLibHandle = 0;
	
	if ( harmonyPortLibrary != NULL ) return harmonyPortLibrary;
	
	if ( GlobalHyPortAllocateLibraryMapToJ9 == NULL ) {
		PORT_ACCESS_FROM_PORT(j9PortLib);
		if ( j9sl_open_shared_library( J9_HARMONY_PORT_LIBRARY_SHIM_DLL_NAME, &hyprtshimLibHandle, TRUE ) != 0 ) {
				return NULL;
			}
		if ( j9sl_lookup_name(hyprtshimLibHandle, "hyport_allocate_library_maptoj9", (void *) &GlobalHyPortAllocateLibraryMapToJ9, "ILLL")) {
			return NULL;
		}
	}
	
	/* Figure out the default version and allocate */
	HYPORT_SET_VERSION(&portLibraryVersion, HYPORT_CAPABILITY_MASK);

	rc = GlobalHyPortAllocateLibraryMapToJ9 (j9PortLib, &portLibraryVersion, &harmonyPortLibrary);
	if (0 != rc) {
		return NULL;
	}
	
	return harmonyPortLibrary;
}

JavaVMOption *addHarmonyPortLibToVMArgs(J9PortLibrary *portLib, JavaVMOption *optionCursor, JavaVMInitArgs *vm_args, struct HyPortLibrary **hyPortLib)
{
	int i;
	
	/* Search for Harmony Port Library */
	
	for ( i=0; i<vm_args->nOptions; i++ ) {
		if (0 == strcmp(HARMONY_PORTLIB_ARG, vm_args->options[i].optionString)) {
			return optionCursor;
		}
	}
	
	/* Create and add a Harmony Port Library */
	optionCursor->extraInfo = createHarmonyPortLibrary(portLib);
	if ( optionCursor->extraInfo == NULL ) {
		/* Couldn't create the Harmony port library */
		return optionCursor;
	}
	optionCursor->optionString = HARMONY_PORTLIB_ARG;

	*hyPortLib = (struct HyPortLibrary *)optionCursor->extraInfo;

	return ++optionCursor;
}



