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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
#include "j9ddr.h"
#include "ddrtable.h"

extern const J9DDRStructDefinition* getJITStructTable(void);
extern const J9DDRStructDefinition* getAlgorithmVersionStructTable(void);
/* Generated struct table functions take a portLib to allow stub functions
 * to print warning messages.*/
extern const J9DDRStructDefinition* getJITAutomatedStructTable(void* portLib);
extern const J9DDRStructDefinition* getOmrStructTable(void* portLib);
extern const J9DDRStructDefinition* getVMStructTable(void* portLib);
extern const J9DDRStructDefinition* getStackWalkerStructTable(void* portLib);
extern const J9DDRStructDefinition* getHyPortStructTable(void* portLib);
extern const J9DDRStructDefinition* getJ9PortStructTable(void* portLib);
extern const J9DDRStructDefinition* getGCStructTable(void *portLib);
extern const J9DDRStructDefinition* getDDR_CPPStructTable(void *portLib);

const J9DDRStructDefinition**
initializeDDRComponents(OMRPortLibrary* portLib)
{
	const J9DDRStructDefinition** list;
	UDATA i = 0;
	OMRPORT_ACCESS_FROM_OMRPORT(portLib);

	list = omrmem_allocate_memory((J9DDR_COMPONENT_COUNT+1) * sizeof(J9DDRStructDefinition*), OMRMEM_CATEGORY_VM);
	if(NULL == list) {
		return NULL;
	}

	list[i++] = getOmrStructTable(portLib);
	list[i++] = getVMStructTable(portLib);
	list[i++] = getStackWalkerStructTable(portLib);
	list[i++] = getGCStructTable(portLib);
	list[i++] = getHyPortStructTable(portLib);
	list[i++] = getJ9PortStructTable(portLib);
	list[i++] = getAlgorithmVersionStructTable();
	list[i++] = getJITStructTable();
	list[i++] = getJITAutomatedStructTable(portLib);
	list[i++] = getDDR_CPPStructTable(portLib);
	list[i] = NULL;
	/*increment i here if it needs to be used further*/

	return list;
}
