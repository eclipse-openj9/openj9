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
#include "vmi.h"
#include "vm_internal.h"
#include "zip_api.h"
#include <string.h>

#if (defined(J9VM_OPT_ZIP_SUPPORT)) 
static VMIZipFunctionTable* JNICALL vmi_getZipFunctions (VMInterface* vmi);
#endif /* J9VM_OPT_ZIP_SUPPORT */
static vmiError JNICALL vmi_countSystemProperties (VMInterface* vmi, int* countPtr);
static vmiError JNICALL vmi_setSystemProperty (VMInterface* vmi, char* key, char* value);
static vmiError JNICALL vmi_iterateSystemProperties (VMInterface* vmi, vmiSystemPropertyIterator iterator, void* userData);
static vmiError JNICALL vmi_checkVersion (VMInterface* vmi, vmiVersion* version);
static JavaVMInitArgs* JNICALL vmi_getInitArgs (VMInterface* vmi);
#if (defined(J9VM_OPT_VM_LOCAL_STORAGE)) 
static J9VMLSFunctionTable* JNICALL vmi_getVMLSFunctions (VMInterface* vmi);
#endif /* J9VM_OPT_VM_LOCAL_STORAGE */
static vmiError JNICALL vmi_getSystemProperty (VMInterface* vmi, char* key, char** valuePtr);
static J9PortLibrary* JNICALL vmi_getPortLibrary (VMInterface* vmi);
static JavaVM* JNICALL vmi_getJavaVM (VMInterface* vmi);
vmiError J9VMI_Initialize (J9JavaVM* vm);


struct VMInterfaceFunctions_ J9VMInterfaceFunctions = {
	vmi_checkVersion,
	vmi_getJavaVM,
	vmi_getPortLibrary,
#if defined(J9VM_OPT_VM_LOCAL_STORAGE)
	vmi_getVMLSFunctions,
#else
	NULL,
#endif
#if defined(J9VM_OPT_ZIP_SUPPORT)
	vmi_getZipFunctions,
#else
	NULL,
#endif
	vmi_getInitArgs,
	vmi_getSystemProperty,
	vmi_setSystemProperty,
	vmi_countSystemProperties,
	vmi_iterateSystemProperties,
};

/* Initialization function */
vmiError J9VMI_Initialize(J9JavaVM* vm)
{
	J9VMInterface* j9VMI = &vm->vmInterface;
	j9VMI->functions = GLOBAL_TABLE(J9VMInterfaceFunctions);
	j9VMI->javaVM = vm;
	j9VMI->portLibrary = vm->portLibrary;

	/* load the zlib */
	if (0 != initZipLibrary(vm->portLibrary, vm->j2seRootDirectory)) {
		return VMI_ERROR_INITIALIZATION_FAILED;
	}

	/* Acquire the initArgs via VMI */
#if defined(J9VM_OPT_HARMONY)
	{ /* Introduce a new scope to keep older compilers happy */
		JavaVMInitArgs* vmInitArgs = NULL;
		VMInterface* vmi = NULL;
		/* Initialize the Harmony copy of the VMI */
		HarmonyVMInterface *harmonyVMI = &vm->harmonyVMInterface;
		harmonyVMI->functions = GLOBAL_TABLE(J9VMInterfaceFunctions);
		harmonyVMI->javaVM = vm;
		harmonyVMI->portLibrary = NULL;

		/* Acquire the initArgs via VMI */
		vmi = (VMInterface*)j9VMI;
		vmInitArgs = (*vmi)->GetInitArgs(vmi);

		/* Locate the Harmony portlib (if possible) */
		if (NULL != vmInitArgs) {

			jint count = vmInitArgs->nOptions;
			JavaVMOption *option = vmInitArgs->options;

			while (count > 0) {
				if (!strcmp(option->optionString,"_org.apache.harmony.vmi.portlib")) {
					harmonyVMI->portLibrary = (struct HyPortLibrary *)option->extraInfo;
					return VMI_ERROR_NONE;
				}
				++option;
				--count;
			}
		}
	}
#endif /* J9VM_OPT_HARMONY */
	return VMI_ERROR_NONE;
}


/* Table functions */
/**
 * Check the version of the VM interface
 *
 * @param[in] vmi  The VM interface pointer
 * @param[in,out] version  Pass in the version to check, or @ref VMI_VERSION_UNKNOWN. Returns the current version.
 *
 * @return a @ref vmiError "VMI error code"
 *
 * @note The CheckVersion function allows a class library to verify that the VM provides the required interface functions.
 * If the version requested is @ref VMI_VERSION_UNKNOWN, then the function will reply with the current version and not return an error.
 * If a specific version is passed, it will be compatibility checked against the current, and @ref VMI_ERROR_UNSUPPORTED_VERSION
 * may be returned.
 */
static vmiError JNICALL 
vmi_checkVersion(VMInterface* vmi, vmiVersion* version)
{
	if(*version == VMI_VERSION_UNKNOWN) {
		*version = VMI_VERSION_2_0;
		return VMI_ERROR_NONE;
	}

	if(*version == VMI_VERSION_2_0) {
		*version = VMI_VERSION_2_0;
		return VMI_ERROR_NONE;
	}

	return VMI_ERROR_UNSUPPORTED_VERSION;
}
	


/**
 * Return the number of VM system properties
 * 
 * @param[in] vmi  The VM Interface pointer
 * @param[out] countPtr The location to store the number of system properties
 *
 * @return a @ref vmiError "VMI error code"
 *
 * @note See  GetSystemProperty() for the list of properties that must be defined
 * by the vm.
 */
static vmiError JNICALL 
vmi_countSystemProperties(VMInterface* vmi, int* countPtr)
{
	J9VMInterface* j9vmi = (J9VMInterface*)vmi;

	*countPtr = (int)pool_numElements(j9vmi->javaVM->systemProperties);
	return VMI_ERROR_NONE;
}


/**
 * Return a pointer to a JavaVMInitArgs structure as defined by the 1.2 JNI
 * specification. This structure contains the arguments used to invoke the vm.
 *
 * @param[in] vmi  The VM Interface pointer
 *
 * @return the VM invocation arguments
 */
static JavaVMInitArgs* JNICALL
vmi_getInitArgs(VMInterface* vmi)
{
	return ((J9VMInterface*) vmi)->javaVM->vmArgsArray->actualVMArgs;
}



/**
 * Return the JNI JavaVM associated with the VM Interface
 *
 * @param[in] vmi  The VM Interface pointer
 *
 * @return a JavaVM pointer
 */
static JavaVM* JNICALL
vmi_getJavaVM(VMInterface* vmi)
{
	return (JavaVM*)(((J9VMInterface*)vmi)->javaVM);
}



/**
 * Return a pointer to an initialized J9PortLibrary structure.
 * The port library is a table of functions that implement useful platform specific
 * capability. For example, file and socket manipulation, memory management, etc.
 * It is the responsibility of the vm to create the port library.
 *
 * @param[in] vmi  The VM interface pointer
 *
 * @return the J9PortLibrary associated with the VMI
 * 
 * @see j9port.c
 */
static J9PortLibrary* JNICALL
vmi_getPortLibrary(VMInterface* vmi)
{
	return ((J9VMInterface*) vmi)->portLibrary;
}



/**
 * Retrieve the value of a VM system property. The following properties must be defined
 * by the vm.
 *
 * <TABLE>
 * <TR><TD><B>Property Name</B></TD>			<TD><B>Example Value or Description</B></TD></TR>
 * <TR><TD>java.vendor</TD>			<TD>"Eclipse OpenJ9"</TD></TR>
 * <TR><TD>java.vendor.url</TD>			<TD>"http://www.eclipse.org/openj9"</TD></TR>
 * <TR><TD>java.specification.version</TD>	<TD>"1.8"</TD></TR>
 * <TR><TD>java.vm.specification.version</TD>	<TD>"1.8"</TD></TR>
 * <TR><TD>java.vm.specification.vendor</TD>	<TD>"Oracle Corporation"</TD></TR>
 * <TR><TD>java.vm.specification.name</TD>	<TD>"Java Virtual Machine Specification"</TD></TR>
 * <TR><TD>java.vm.version</TD>			<TD>"master-1ca0ab98"</TD></TR>
 * <TR><TD>java.vm.vendor</TD>			<TD>"Eclipse OpenJ9"</TD></TR>
 * <TR><TD>java.vm.name	</TD>		<TD>"Eclipse OpenJ9 VM"</TD></TR>
 * <TR><TD>java.vm.info</TD>			<TD>"JRE 1.8.0 Linux amd64-64-Bit Compressed References 20180601_201 (JIT enabled, AOT enabled)
<BR>OpenJ9   - 1ca0ab98
<BR>OMR      - 05d2b8a2
<BR>JCL      - c2aa0348 based on jdk8u172-b11"</TD></TR>
 * <TR><TD>java.compiler</TD>			<TD>"j9jit29"</TD></TR>
 * <TR><TD>java.class.version</TD>		<TD>"52.0"</TD></TR>
 * <TR><TD>java.home</TD>			<TD>the absolute path of the parent directory of the directory containing the vm
<BR>i.e. for a vm /clear/bin/vm.exe, java.home is /clear</TD></TR>
 * <TR><TD>java.class.path</TD>			<TD>the application class path</TD></TR>
 * <TR><TD>java.library.path</TD>			<TD>the application library path</TD></TR>
 * <TR><TD>&nbsp;</TD><TD>&nbsp;</TD></TR>
 * <TR><TD>com.ibm.oti.vm.bootstrap.library.path	<TD>the bootstrap library path</TD></TR>
 * <TR><TD>com.ibm.oti.vm.library.version	<TD>"29"</TD></TR>
 * </TABLE>
 *
 * @return a @ref vmiError "VMI error code"
 *
 * @note The returned string is owned by the VM, and should not be freed.
 */
static vmiError JNICALL 
vmi_getSystemProperty (VMInterface* vmi, char* key, char** valuePtr)
{
	J9VMInterface* j9vmi = (J9VMInterface*)vmi;
	J9VMSystemProperty* sysProp;

	if(j9vmi->javaVM->internalVMFunctions->getSystemProperty(j9vmi->javaVM, key, &sysProp) == J9SYSPROP_ERROR_NONE) {
		*valuePtr = sysProp->value;
		return VMI_ERROR_NONE;
	}
	*valuePtr = NULL;
	return VMI_ERROR_NOT_FOUND;
}


#if (defined(J9VM_OPT_VM_LOCAL_STORAGE)) 
/**
 * Return a pointer to a J9VMLSFunctionTable. This is a table of functions for allocating,
 * freeing, getting, and setting thread local storage.
 *
 * @param[in] vmi  The VM Interface pointer
 *
 * @return the VM local storage function table
 */
static J9VMLSFunctionTable* JNICALL
vmi_getVMLSFunctions(VMInterface* vmi)
{
	return ((J9VMInterface*) vmi)->javaVM->vmLocalStorageFunctions;
}

#endif /* J9VM_OPT_VM_LOCAL_STORAGE */


#if (defined(J9VM_OPT_ZIP_SUPPORT)) 
/**
 * Return a pointer to the VMIZipFunctionTable. This is a table of functions for 
 * operating on zip files.
 *
 * @param[in] vmi  The VM Interface pointer
 *
 * @return a VMIZipFunctionTable pointer
 */
static struct VMIZipFunctionTable* JNICALL 
vmi_getZipFunctions(VMInterface* vmi)
{
	return &VMIZipLibraryTable;
}

#endif /* J9VM_OPT_ZIP_SUPPORT */


/**
 * Iterate over the VM system properties calling a function.
 * 
 * @param[in] vmi  The VM Interface pointer
 * @param[in] iterator  The iterator function to call with each property
 * @param[in] userData  Opaque data to pass to the iterator function
 *
 * @return a @ref vmiError "VMI error code"
 *
 * @note The returned strings are owned by the VM, and should not be freed.
 *
 * @note See  GetSystemProperty() for the list of properties that must be defined
 * by the vm.
 */
static vmiError JNICALL 
vmi_iterateSystemProperties(VMInterface* vmi, vmiSystemPropertyIterator iterator, void* userData)
{
	J9JavaVM* vm = ((J9VMInterface*)vmi)->javaVM;
	pool_state walkState;

	J9VMSystemProperty* sysProp = pool_startDo(vm->systemProperties, &walkState);
	while (sysProp != NULL) {
		iterator(sysProp->name, sysProp->value, userData);
		sysProp = pool_nextDo(&walkState);
	}

	return VMI_ERROR_NONE;
}

static char*
copyString(J9PortLibrary* portLibrary, char* original)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	size_t nbyte = strlen(original);

	char* copy = j9mem_allocate_memory(nbyte + 1 /* for null terminator*/ , OMRMEM_CATEGORY_VM);
	if (NULL == copy) {
		return NULL;
	}

	memcpy(copy, original, nbyte);
	copy[nbyte] = '\0';
	return copy;
}


/**
 * Override the value of a VM system property
 * 
 * @param[in] vmi  The VM Interface pointer
 * @param[in] key  The system property to override
 * @param[in] value  The value of the system property
 *
 * @return a @ref vmiError "VMI error code"
 *
 * @note Only existing properties can be overridden. New properties cannot be added by this mechanism.
 *
 * @note See  GetSystemProperty() for the list of properties that must be defined
 * by the vm.
 */
static vmiError JNICALL 
vmi_setSystemProperty(VMInterface* vmi, char* key, char* value)
{
	J9VMInterface* j9vmi = (J9VMInterface*)vmi;
	J9VMSystemProperty* sysProp;
	UDATA rc;

	rc = j9vmi->javaVM->internalVMFunctions->getSystemProperty(j9vmi->javaVM, key, &sysProp);
	if (J9SYSPROP_ERROR_NOT_FOUND == rc) {
		/* Does not exist, add it */
		J9PortLibrary* portLibrary = j9vmi->javaVM->portLibrary;
		char* copiedKey;
		char* copiedValue;
		PORT_ACCESS_FROM_PORT(portLibrary);

		copiedKey = copyString(portLibrary, key);
		if (NULL == copiedKey) {
			return VMI_ERROR_OUT_OF_MEMORY;
		}

		copiedValue = copyString(portLibrary, value);
		if (NULL == copiedValue) {
			j9mem_free_memory(copiedKey);
			return VMI_ERROR_OUT_OF_MEMORY;
		}

		rc = j9vmi->javaVM->internalVMFunctions->addSystemProperty(j9vmi->javaVM, copiedKey, copiedValue, J9SYSPROP_FLAG_NAME_ALLOCATED | J9SYSPROP_FLAG_VALUE_ALLOCATED);
	} else {
		rc = j9vmi->javaVM->internalVMFunctions->setSystemProperty(j9vmi->javaVM, sysProp, value);
	}

	switch(rc) {
		case J9SYSPROP_ERROR_READ_ONLY:
			return VMI_ERROR_READ_ONLY;

		case J9SYSPROP_ERROR_OUT_OF_MEMORY:
			return VMI_ERROR_OUT_OF_MEMORY;

		case J9SYSPROP_ERROR_NONE:
			return VMI_ERROR_NONE;

		default: 
			return VMI_ERROR_UNKNOWN;
	}	
}

