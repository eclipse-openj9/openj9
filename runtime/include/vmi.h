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

/**
 * @file
 * @ingroup VMInterface
 * @brief VM interface specification
 */

#ifndef vmi_h
#define vmi_h

/* @ddr_namespace: default */
#ifdef __cplusplus
extern "C" {
#endif

#include "j9cfg.h"
#include "jni.h"
#include "j9port.h"

#if defined(J9VM_OPT_VM_LOCAL_STORAGE)
#include "j9vmls.h"
#endif

/**
 * @enum vmiError
 * Enumeration of all possible return codes from VM interface functions
 */
typedef enum 
{
	VMI_ERROR_NONE = 0,												/**< Success */
	VMI_ERROR_UNKNOWN = 1,										/**< Unknown error */
	VMI_ERROR_UNIMPLEMENTED = 2,							/**< Function has not been implemented */
	VMI_ERROR_UNSUPPORTED_VERSION = 3,			/**< The requested VM interface version is not supported */
	VMI_ERROR_OUT_OF_MEMORY= 4,							/**< Not enough memory was available to complete the request */
	VMI_ERROR_NOT_FOUND= 5,									/**< The requested system property was not found */
	VMI_ERROR_READ_ONLY= 6,									/**< An attempt was made to modify a read-only item */
	VMI_ERROR_INITIALIZATION_FAILED = 7,			/**< Initialization of the VM interface failed */

	vmiErrorEnsureWideEnum = 0x1000000						/* ensure 4-byte enum */
} vmiError;

/**
 * @enum vmiVersion
 * VM interface version identifier
 */
typedef enum
{
	VMI_VERSION_UNKNOWN = 0x00000000,					/**< Unknown VMInterface version */
	VMI_VERSION_1_0 = 0x00010000,								/**< VMInterface version 1.0 */
	VMI_VERSION_2_0 = 0x00020000,								/**< VMInterface version 2.0 */
	vmiVersionEnsureWideEnum = 0x1000000					/* ensure 4-byte enum */
} vmiVersion;

/**
 * Constant used in conjunction with GetEnv() to retrieve
 * a Harmony VM Interface table from a JNIEnv / JavaVM.
 */
#define HARMONY_VMI_VERSION_2_0 0xC01D0020

/**
 * @typedef vmiSystemPropertyIterator
 * Specification of the iterator function to provide to IterateSystemProperties
 *
 * @code void iterator(char* key, char* value, void* userData); @endcode
 */
typedef void (JNICALL *vmiSystemPropertyIterator)(char* key, char* value, void* userData);

struct VMInterface_;
struct VMInterfaceFunctions_;

/**
 * @typedef VMInterface
 * The VM interface structure. Points to the @ref VMInterfaceFunctions_ "VM interface function table". 
 * Implementations will likely choose to store opaque data off this structure.
 */
typedef const struct VMInterfaceFunctions_* VMInterface;

#if defined(J9VM_OPT_ZIP_SUPPORT)
#include "vmizip.h"
#endif

/**
 * @struct VMInterfaceFunctions_ 
 * The VM interface function table.
 * 
 * Example usage:
 * @code
 * JavaVM* vm = (*vmi)->GetJavaVM(vmi);
 * @endcode
 */
struct VMInterfaceFunctions_ 
{
	vmiError (JNICALL * CheckVersion)(VMInterface* vmi, vmiVersion* version);
	JavaVM* (JNICALL * GetJavaVM) (VMInterface* vmi);
	J9PortLibrary* (JNICALL * GetPortLibrary) (VMInterface* vmi);
#if defined(J9VM_OPT_VM_LOCAL_STORAGE)
	J9VMLSFunctionTable* (JNICALL * GetVMLSFunctions) (VMInterface* vmi);
#else
	void* reserved1;
#endif
#if defined(J9VM_OPT_ZIP_SUPPORT)
	VMIZipFunctionTable* (JNICALL * GetZipFunctions) (VMInterface* vmi);
#else
	void* reserved2;
#endif
	JavaVMInitArgs* (JNICALL * GetInitArgs) (VMInterface* vmi);
	vmiError (JNICALL * GetSystemProperty) (VMInterface* vmi, char* key, char** valuePtr);
	vmiError (JNICALL * SetSystemProperty) (VMInterface* vmi, char* key, char* value);
	vmiError (JNICALL * CountSystemProperties) (VMInterface* vmi, int* countPtr);
	vmiError (JNICALL * IterateSystemProperties) (VMInterface* vmi, vmiSystemPropertyIterator iterator, void* userData);
};


/** 
 *
 * @name VM Interface Support Functions 
 * @htmlonly <a name='VMIExports'>&nbsp;</a> @endhtmlonly Non-table VM interface functions. Directly exported from the VMI library.
 */

/*@{*/ 
/*@}*/ 

/**
 * Retrieves a VMInterface pointer given a JavaVM.
 * @param vm The VM instance from which to obtain the VMI.
 * @return  A VMInterface or NULL.
 */
VMInterface *JNICALL GetVMIFromJavaVM(JavaVM * vm);

/**
 * Retrieves a VMInterface pointer given a JNIEnv.
 * @param env The JNIEnv from which to obtain the VMI.
 * @return  A VMInterface or NULL.
 */
VMInterface *JNICALL GetVMIFromJNIEnv(JNIEnv * env);

/** @name VM Interface Access Macros
 *
 *  Convenience macros for acquiring a VMInterface
 */
/*@{*/ 
#define VMI_ACCESS_FROM_ENV(env) VMInterface* privateVMI = GetVMIFromJNIEnv(env)
#define VMI_ACCESS_FROM_JAVAVM(javaVM) VMInterface* privateVMI = GetVMIFromJavaVM(javaVM)
#define VMI privateVMI
/*@}*/ 

/**
 * @fn VMInterfaceFunctions_::CheckVersion
 * Check the version of the VM interface
 *
 * @code vmiError JNICALL CheckVersion(VMInterface* vmi, vmiVersion* version); @endcode
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
vmiError JNICALL 
CheckVersion(VMInterface* vmi, vmiVersion* version);

/**
 * @fn VMInterfaceFunctions_::GetJavaVM
 * Return the JNI JavaVM associated with the VM interface
 *
 * @code JavaVM* JNICALL GetJavaVM(VMInterface* vmi); @endcode
 *
 * @param[in] vmi  The VM interface pointer
 *
 * @return a JavaVM pointer
 */
JavaVM* JNICALL 
GetJavaVM(VMInterface* vmi);

/**
 * @fn VMInterfaceFunctions_::GetPortLibrary
 * Return a pointer to an initialized J9PortLibrary structure.
 * 
 * @code J9PortLibrary* JNICALL GetPortLibrary(VMInterface* vmi); @endcode
 *
 * The @ref j9port.h "port library" is a table of functions that implement useful platform specific
 * capability. For example, file and socket manipulation, memory management, etc.
 * It is the responsibility of the VM to create the port library.
 *
 * @param[in] vmi  The VM interface pointer
 *
 * @return the J9PortLibrary associated with the VMI
 * 
 * @see j9port.c
 */
J9PortLibrary* JNICALL 
GetPortLibrary(VMInterface* vmi);

/**
 * @fn VMInterfaceFunctions_::GetVMLSFunctions
 * Return a pointer to a J9VMLSFunctionTable. This is a table of functions for allocating,
 * freeing, getting, and setting thread local storage.
 *
 * @code J9VMLSFunctionTable* JNICALL GetVMLSFunctions(VMInterface* vmi); @endcode
 *
 * @param[in] vmi  The VM interface pointer
 *
 * @return the VM local storage function table
 */
J9VMLSFunctionTable* JNICALL 
GetVMLSFunctions(VMInterface* vmi);

#if defined(J9VM_OPT_ZIP_SUPPORT)
/**
 * @fn VMInterfaceFunctions_::GetZipFunctions
 * Return a pointer to a VMIZipFunctionTable. This is a table of functions for managing zip files.
 *
 * @code VMIZipFunctionTable* JNICALL GetZipFunctions(VMInterface* vmi); @endcode
 * 
 * @param[in] vmi  The VM interface pointer
 *
 * @return a VMIZipFunctionTable pointer
 */
VMIZipFunctionTable* JNICALL 
GetZipFunctions(VMInterface* vmi);
#endif 

/**
 * @fn VMInterfaceFunctions_::GetInitArgs
 * Return a pointer to a JavaVMInitArgs structure as defined by the 1.2 JNI
 * specification. This structure contains the arguments used to invoke the vm.
 *
 * @code JavaVMInitArgs* JNICALL GetInitArgs(VMInterface* vmi); @endcode
 *
 * @param[in] vmi  The VM interface pointer
 *
 * @return the VM invocation arguments
 */
JavaVMInitArgs* JNICALL 
GetInitArgs(VMInterface* vmi);

/**
 * @fn VMInterfaceFunctions_::GetSystemProperty
 * Retrieve the value of a VM system property. 
 * 
 * @code vmiError JNICALL GetSystemProperty (VMInterface* vmi, char* key, char** valuePtr); @endcode
 *
 * The following properties must be defined by the vm.
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
vmiError JNICALL 
GetSystemProperty (VMInterface* vmi, char* key, char** valuePtr);

/**
 * @fn VMInterfaceFunctions_::SetSystemProperty
 * Override the value of a VM system property
 *
 * @code vmiError JNICALL SetSystemProperty(VMInterface* vmi, char* key, char* value); @endcode
 * 
 * @param[in] vmi  The VM interface pointer
 * @param[in] key  The system property to override
 * @param[in] value  The value of the system property
 *
 * @return a @ref vmiError "VMI error code"
 *
 * @note New properties can be added by this mechanism, but may not be available from
 * Java after VM bootstrap is complete.
 *
 * @note See  GetSystemProperty() for the list of properties that must be defined
 * by the vm.
 */
vmiError JNICALL 
SetSystemProperty(VMInterface* vmi, char* key, char* value);

/**
 * @fn VMInterfaceFunctions_::CountSystemProperties
 * Return the number of VM system properties
 * 
 * @code vmiError JNICALL CountSystemProperties(VMInterface* vmi, int* countPtr); @endcode
 *
 * @param[in] vmi  The VM interface pointer
 * @param[out] countPtr The location to store the number of system properties
 *
 * @return a @ref vmiError "VMI error code"
 *
 * @note See  GetSystemProperty() for the list of properties that must be defined
 * by the vm.
 */
vmiError JNICALL 
CountSystemProperties(VMInterface* vmi, int* countPtr);

/**
 * @fn VMInterfaceFunctions_::IterateSystemProperties
 * Iterate over the VM system properties calling a function.
 *
 * @code vmiError JNICALL IterateSystemProperties(VMInterface* vmi, vmiSystemPropertyIterator iterator, void* userData); @endcode
 * 
 * @param[in] vmi  The VM interface pointer
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
vmiError JNICALL 
IterateSystemProperties(VMInterface* vmi, vmiSystemPropertyIterator iterator, void* userData);

#ifdef __cplusplus
}
#endif

#endif     /* vmi_h */
