/*******************************************************************************
 * Copyright (c) 2009, 2019 IBM Corp. and others
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

#include "j9port.h"
#include "j9.h"
#include "ut_j9jcl.h"
#include "stackwalk.h"
#include "jclglob.h"
#include "jclprots.h"

extern char * getTmpDir(JNIEnv *env, char**envSpace);

/**
 * Test if the file is owned by this process's owner or the process is running as root.
 * @param pathUTF pathname of the file.  Must be a valid path
 * @return TRUE if the file is owned by this process or the user is root.
 */
static BOOLEAN
isFileOwnedByMe(JNIEnv *env, const char *pathUTF) {

	PORT_ACCESS_FROM_VMC( ((J9VMThread *) env) );

	struct J9FileStat fileStat;
	jlong ownerUid = -1;
	jlong myUid = (jlong) j9sysinfo_get_euid();
	I_32 statRc;

	if (0 == myUid) { /* I am root */
		return TRUE;
	}

	statRc = j9file_stat(pathUTF, 0, &fileStat);
	if (statRc == 0) {
		ownerUid = fileStat.ownerUid;
	}
	return (ownerUid == myUid);

}

/* all native methods defined herein are static */

/**
 * Get the system temporary directory path (/tmp on all but Windows).
 * Note that this may be different from the java.io.tmpdir system property.
 * @return jstring object with the path
 */

#define TMP_PATH_BUFFER_SIZE 128
jstring JNICALL
Java_com_ibm_tools_attach_target_IPC_getTempDirImpl(JNIEnv *env, jclass clazz)
{
	PORT_ACCESS_FROM_VMC( ((J9VMThread *) env) );
	jstring result = NULL;
	char *envSpace = NULL;
	char *charResult = getTmpDir(env, &envSpace);
	if (NULL != charResult) {
		uint8_t defaultConversionBuffer[TMP_PATH_BUFFER_SIZE];
		uint8_t *conversionBuffer = defaultConversionBuffer;
		size_t pathLength = strlen(charResult);
		int32_t conversionResult = 0;
		Trc_JCL_attach_getTmpDir(env, charResult);
		conversionResult = 	j9str_convert(J9STR_CODE_PLATFORM_OMR_INTERNAL, J9STR_CODE_MUTF8, charResult, pathLength, (char*)defaultConversionBuffer, sizeof(defaultConversionBuffer));
		if (OMRPORT_ERROR_STRING_BUFFER_TOO_SMALL == conversionResult) {
			/* determine how much space we really need */
			int32_t requiredSize = 	j9str_convert(J9STR_CODE_PLATFORM_OMR_INTERNAL, J9STR_CODE_MUTF8, charResult, pathLength, NULL, 0);
			if (requiredSize > 0) {
				requiredSize += 1; /* leave room for null */
				conversionBuffer = j9mem_allocate_memory(requiredSize, OMRMEM_CATEGORY_VM);
				if (NULL != conversionBuffer) {
					j9str_convert(J9STR_CODE_PLATFORM_OMR_INTERNAL, J9STR_CODE_MUTF8, charResult, pathLength, (char*)conversionBuffer, requiredSize);
				}
			} else {
				conversionBuffer = NULL; /* string is bogus */
			}
		} else if (conversionResult < 0) {
			Trc_JCL_stringConversionFailed(env, charResult, conversionResult);
			conversionBuffer = NULL; /* string conversion failed */
		}
		if (NULL != conversionBuffer) {
			result =  (*env)->NewStringUTF(env, (char*)conversionBuffer);
			if (defaultConversionBuffer != conversionBuffer) {
				jclmem_free_memory(env,conversionBuffer);
			}
		}
		if (NULL != envSpace) {
			jclmem_free_memory(env,envSpace);
		}
	}
	return result;
}


/**
 * @param path file system path
 * @param mode the file's new access permissions (posix format)
 * @return actual file permissions, which may be different than those requested due to OS limitations
 */

jint JNICALL
Java_com_ibm_tools_attach_target_IPC_chmod(JNIEnv *env, jclass clazz, jstring path, jint mode)
{
	PORT_ACCESS_FROM_VMC( ((J9VMThread *) env) );

	jint result = JNI_ERR;
	const char *pathUTF;

	pathUTF = (*env)->GetStringUTFChars(env, path, NULL);
	if (NULL != pathUTF) {
		if (isFileOwnedByMe(env, pathUTF)) {
			result = j9file_chmod(pathUTF, mode);
			Trc_JCL_attach_chmod(env, pathUTF, mode, result);
		}
		(*env)->ReleaseStringUTFChars(env, path, pathUTF);
	}
	return result;
}

/**
 * @param path file system path
 * @param uid user identity to which to chown the file. This was upcast from UDATA.
 * @return result of chown() operation
 */
jint JNICALL
Java_com_ibm_tools_attach_target_IPC_chownFileToTargetUid(JNIEnv *env, jclass clazz, jstring path, jlong uid)
{
	PORT_ACCESS_FROM_VMC( ((J9VMThread *) env) );

	jint result = JNI_OK;
	const char *pathUTF;

	pathUTF = (*env)->GetStringUTFChars(env, path, NULL);
	if (NULL != pathUTF) {
		if (isFileOwnedByMe(env, pathUTF)) { /* j9file_chown takes a UTF8 string for the path */
			/* uid value was upcast from a UDATA to jlong. */
			result = (jint)j9file_chown(pathUTF, (UDATA) uid, J9PORT_FILE_IGNORE_ID);
			Trc_JCL_attach_chown(env, pathUTF, uid, result);
		}
		(*env)->ReleaseStringUTFChars(env, path, pathUTF);
	} else {
		result = JNI_ERR;
	}
	return result;
}

/**
 * Call file stat and get the effective UID of the file owner.
 * @param path file system path
 * @return Returns 0 on Windows and the effective UID of owner on other operation systems. Returns -1 in the case of an error.
 */
jlong JNICALL
Java_com_ibm_tools_attach_target_CommonDirectory_getFileOwner(JNIEnv *env, jclass clazz, jstring path) {
	PORT_ACCESS_FROM_VMC( ((J9VMThread *) env) );

	jlong ownerUid = -1;
	const char *pathUTF;

	pathUTF = (*env)->GetStringUTFChars(env, path, NULL);
	if (NULL != pathUTF) {
		struct J9FileStat fileStat;
		I_32 statRc = j9file_stat(pathUTF, 0, &fileStat);

		if (statRc == 0) {
			ownerUid = fileStat.ownerUid;
		}
		Trc_JCL_attach_getFileOwner(env, pathUTF, ownerUid);
		(*env)->ReleaseStringUTFChars(env, path, pathUTF);
	}
	return ownerUid;
}

/**
 * This crawls the z/OS control blocks to determine if the current process is using the default UID.
 * This handles both job- and task-level security.
 * For information on the control blocks, see the z/OS V1R9.0 MVS Data Areas documentation.
 * @return JNI_TRUE if the process is running on z/OS and is using the default UID
 * @note A batch process runs as the default UID if it has no USS segment.
 */
jboolean JNICALL
Java_com_ibm_tools_attach_target_IPC_isUsingDefaultUid(JNIEnv *env, jclass clazz)
{

	jboolean usingDuid = JNI_FALSE;
#if defined(J9ZOS390)

	/* all offsets are byte offsets */
	U_32* PSATOLD_ADDR = (U_32 *)(UDATA) 0x21c;  /* z/OS Pointer to current TCB or zero if in SRB mode. Field fixed by architecture. */
	U_32 tcbBase; /* base of the z/OS Task Control Block */
	const TCBSENV_OFFSET = 0x154; /* offset of the TCBSENV field in the TCB.  This field contains a pointer to the ACEE. */
	U_32 aceeBaseAddr; /* Address of a control block field which contains a pointer to the base of the RACF Accessor Environment Element (ACEE) */
	U_32 aceeBase; /* absolute address of the start of the ACEE */
	U_32 aceeflg3Addr; /* address of the "Miscellaneous flags" byte of the ACEE */
	U_8 aceeflg3Value; /* contents of the "Miscellaneous flags" byte of the ACEE */
	const U_32 ACEEFLG3_OFFSET = 0x28; /* offset of flag 3 from the base of the ACEE */
	const U_8 ACEE_DUID = 0X02; /* 1 if current thread is using the defaultUID */

	tcbBase = *PSATOLD_ADDR;
	aceeBaseAddr = tcbBase+TCBSENV_OFFSET;
	aceeBase = *((U_32 *)(UDATA) aceeBaseAddr);
	if (0 == aceeBase) { /* not using task-level ACEE */
		U_32* PSAAOLD_ADDR= (U_32 *) 0x224; /* Pointer to the home (current) ASCB. */
		U_32 ascbAddr; /* address of the ACSB (ADDRESS SPACE CONTROL BLOCK) */
		const U_32 ASCBASXB_OFFSET = 0x6c; /* offset of the address space extension control block (ASXB) field in the ASCB */
		U_32 asxbBaseAddr; /* absolute address of the ASXB field in the ASCB */
		U_32 asxbBase; /* Contents of the ASXB field in the ASCB. This points the ASXB itself. */
		const U_32 ASXBSENV_OFFSET = 0xc8; /* offset of the ASXBSENV field in the ASXB. This field contains a pointer to the ACEE. */

		ascbAddr = *PSAAOLD_ADDR;

		asxbBaseAddr = ascbAddr+ASCBASXB_OFFSET;
		asxbBase = *((U_32 *)(UDATA) asxbBaseAddr);

		aceeBaseAddr = asxbBase+ASXBSENV_OFFSET;
		aceeBase = *((U_32 *)(UDATA) aceeBaseAddr);
	}

	aceeflg3Addr = aceeBase + ACEEFLG3_OFFSET;
	aceeflg3Value = *((U_8 *)(UDATA) aceeflg3Addr);
	Trc_JCL_com_ibm_tools_attach_javaSE_IPC_isUsingDefaultUid(env, aceeBase, aceeflg3Addr, aceeflg3Value);

	if (0 != (aceeflg3Value & ACEE_DUID)) { /* Running with default UID.*/
		usingDuid = JNI_TRUE;
	}
#endif
	return usingDuid;
}


/**
 * Create a directory with specified file permissions.
 * These permissions are masked with the user's umask
 * @param absolutePath path of the directory to create
 * @param cdPerms file access permissions (posix format) for the new directory
 * @return JNI_OK if success
 */

jint JNICALL
Java_com_ibm_tools_attach_target_IPC_mkdirWithPermissionsImpl(JNIEnv *env, jclass clazz, jstring absolutePath, jint cdPerms)
{
	PORT_ACCESS_FROM_VMC( ((J9VMThread *) env) );
	jint status = JNI_OK;
	const char *absolutePathUTF = (*env)->GetStringUTFChars(env, absolutePath, NULL);

	if (NULL != absolutePathUTF) {
		status = j9file_mkdir(absolutePathUTF); /* file perms ignored for now */
		Java_com_ibm_tools_attach_target_IPC_chmod(env, clazz, absolutePath, cdPerms);
		/* ensure that the directory group is the same as this process's.  Some OS's set the group to that of the parent directory */
		j9file_chown(absolutePathUTF, J9PORT_FILE_IGNORE_ID, j9sysinfo_get_egid());
		Trc_JCL_attach_mkdirWithPermissions(env, absolutePathUTF, cdPerms, status);
		(*env)->ReleaseStringUTFChars(env, absolutePath, absolutePathUTF);
	} else {
		status = JNI_ERR;
	}
	return status;
}

/**
 * @param path file system path
 * @param mode file access permissions (posix format) for the new file
 * @return JNI_OK on success, JNI_ERR on failure
 * Create the file, set the permissions (to override umask) and close it
 */
jint JNICALL
Java_com_ibm_tools_attach_target_IPC_createFileWithPermissionsImpl(JNIEnv *env, jclass clazz, jstring path, jint mode)
{
	PORT_ACCESS_FROM_VMC( ((J9VMThread *) env) );

	jint status = JNI_OK;
	const char *pathUTF = (*env)->GetStringUTFChars(env, path, NULL);

	if (NULL != pathUTF) {
		IDATA fd = j9file_open(pathUTF, EsOpenCreateNew | EsOpenWrite | EsOpenTruncate , mode);
		if (-1 == fd) {
			status = JNI_ERR;
		} else {
			j9file_close(fd);
		}
		Trc_JCL_attach_createFileWithPermissions(env, pathUTF, mode, status);
		(*env)->ReleaseStringUTFChars(env, path, pathUTF);
	} else {
		status = JNI_ERR;
	}
	return status;
}

/**
 * @return JNI_OK if no error, JNI_ERR otherwise
 */
#define SHRDIR_NAME_LENGTH 255

static jint createSharedResourcesDir(JNIEnv *env, jstring ctrlDirName)
{
	IDATA status = JNI_ERR;
	const char *ctrlDirUTF = NULL;

	PORT_ACCESS_FROM_VMC( ((J9VMThread *) env) );

	/* TODO: should use OS native encoding for the file path */
	ctrlDirUTF = (*env)->GetStringUTFChars(env, ctrlDirName, NULL);
	if (NULL != ctrlDirUTF) {
		I_32 statRc=0;
		struct J9FileStat ctrlDirStat;
		statRc = j9file_stat(ctrlDirUTF, 0, &ctrlDirStat);

		if ((statRc == 0) && ctrlDirStat.isFile) {
			j9file_unlink(ctrlDirUTF);
			status = j9file_mkdir(ctrlDirUTF);
		} else if (statRc < 0){ /* directory does not exist */
			status = j9file_mkdir(ctrlDirUTF);
		} else {
			status = JNI_OK;
		}
		Trc_JCL_attach_createSharedResourcesDir(env, ctrlDirUTF, status);
		(*env)->ReleaseStringUTFChars(env, ctrlDirName, ctrlDirUTF);
	}
	return (jint)status;
}

/**
 * prepare to use semaphores, creating resources as required
 * @param ctrlDirName location of semaphore control files
 * @return JNI_OK if no error, JNI_ERR otherwise
 */

jint JNICALL
Java_com_ibm_tools_attach_target_IPC_setupSemaphore(JNIEnv *env, jclass clazz, jstring ctrlDirName)
{


	jint status;

	status = createSharedResourcesDir(env, ctrlDirName);
	return status;
}

/**
 * open the semaphore and save into a specified struct
 * @param ctrlDirName directory for the control file
 * @param semaName name for the control file
 * @param semaphore out parameter for the resulting handle
 * @return JNI_ERR if cannot allocate string, j9shsem_open status otherwise
 */

static jint JNICALL
openSemaphore(JNIEnv *env, jclass clazz, jstring ctrlDirName, jstring semaName, BOOLEAN global, struct j9shsem_handle** semaphore)
{

	PORT_ACCESS_FROM_VMC((J9VMThread *) env );

	jint status = JNI_OK;
	const char *semaNameUTF = (*env)->GetStringUTFChars(env, semaName, NULL);
	const char *ctrlDirNameUTF = (*env)->GetStringUTFChars(env, ctrlDirName, NULL);

	if ((NULL != semaNameUTF) && (NULL != ctrlDirNameUTF)) {
		J9PortShSemParameters openParams;
		j9shsem_params_init(&openParams);
		openParams.semName = semaNameUTF;
		openParams.setSize = 1;
		openParams.permission = 0666;
		openParams.controlFileDir = ctrlDirNameUTF;
		openParams.proj_id = 0xa1;
		openParams.deleteBasefile = 0;
		if (global) {
			openParams.global = 1;
		}

		status = (jint) j9shsem_open(semaphore, &openParams);
		Trc_JCL_attach_openSemaphore(env, semaNameUTF, ctrlDirNameUTF, status);
	} else {
		status = JNI_ERR;
	}
	if (NULL != semaNameUTF) {
		(*env)->ReleaseStringUTFChars(env, semaName, semaNameUTF);
	}
	if (NULL != ctrlDirNameUTF){
		(*env)->ReleaseStringUTFChars(env, ctrlDirName, ctrlDirNameUTF);
	}
	return status;
}

/**
 * @param ctrlDirName path to directory holding the semaphore files
 * @param semaName name of the notification semaphore for this process
 * @return JNI_OK on success, j9shsem_open status otherwise
 * Saves the semaphore handle into the VM struct.
 */

jint JNICALL
Java_com_ibm_tools_attach_target_IPC_openSemaphore(JNIEnv *env, jclass clazz, jstring ctrlDirName, jstring semaName)
{
	jint rc = 0;
	J9JavaVM* javaVM = ((J9VMThread*) env)->javaVM;

	Trc_JCL_attach_openSemaphoreEntry(env);
	rc = openSemaphore(env, clazz, ctrlDirName, semaName, TRUE, &(javaVM->attachContext.semaphore));

	if ((J9PORT_INFO_SHSEM_OPENED == rc) || (J9PORT_INFO_SHSEM_OPENED_STALE == rc) || (J9PORT_INFO_SHSEM_CREATED == rc)) {
		rc = JNI_OK;
	}
	Trc_JCL_attach_openSemaphoreExit(env, rc);
	return rc;
}

/**
 * Open a semaphore, post to it, and close it.  Do not store the semaphore handle.
 * @param ctrlDirName path to directory holding the semaphore files
 * @param semaName name of the notification semaphore for this process
 * @param numberOfPosts the number of times to increment the semaphore
 * @param global use the global semaphore namespace (Windows only)
 * @return JNI_OK on success, j9shsem_open or close status otherwise
 */
jint JNICALL
Java_com_ibm_tools_attach_target_IPC_notifyVm(JNIEnv *env, jclass clazz, jstring ctrlDirName, jstring semaName, jint numberOfPosts, jboolean global)
{

	PORT_ACCESS_FROM_VMC( ((J9VMThread *) env) );

	jint status;
	struct j9shsem_handle* semaphore;

	Trc_JCL_attach_notifyVmEntry(env);
	status = openSemaphore(env, clazz, ctrlDirName, semaName, global, &semaphore);
	if ((J9PORT_INFO_SHSEM_OPENED == status) || (J9PORT_INFO_SHSEM_OPENED_STALE == status)) {
		while (numberOfPosts > 0) {
			status = (jint) j9shsem_post(semaphore, 0, J9PORT_SHSEM_MODE_DEFAULT);
			--numberOfPosts;
		}
		j9shsem_close(&semaphore);
		status = JNI_OK;
	} else if (J9PORT_INFO_SHSEM_CREATED == status) {
		/* Jazz 27080. the semaphore should already have been created.  We are probably shutting down. */
		status = (jint) j9shsem_destroy(&semaphore);
	}
	Trc_JCL_attach_notifyVmExit(env, status);
	return status;
}

/**
 * Open a semaphore, decrement it N times (non-blocking)  to it, and close it.  Do not store the semaphore handle.
 * @param ctrlDirName path to directory holding the semaphore files
 * @param semaName name of the notification semaphore for this process
 * @param numberOfDecrements the number of times to decrement the semaphore
 * @return JNI_OK on success, j9shsem_open or close status otherwise
 */
jint JNICALL
Java_com_ibm_tools_attach_target_IPC_cancelNotify(JNIEnv *env, jclass clazz, jstring ctrlDirName, jstring semaName, jint numberOfDecrements, jboolean global)
{

	PORT_ACCESS_FROM_VMC( ((J9VMThread *) env) );

	jint status;
	struct j9shsem_handle* semaphore;

	Trc_JCL_attach_cancelNotifyVmEntry(env);
	status = openSemaphore(env, clazz, ctrlDirName, semaName, global, &semaphore);
	if ((J9PORT_INFO_SHSEM_OPENED == status) || (J9PORT_INFO_SHSEM_OPENED_STALE == status)) {
		while (numberOfDecrements > 0) {
			status = (jint) j9shsem_wait(semaphore, 0, J9PORT_SHSEM_MODE_NOWAIT);
			--numberOfDecrements;
		}
		j9shsem_close(&semaphore);
		status = JNI_OK;
	} else if (J9PORT_INFO_SHSEM_CREATED == status) {
		/* Jazz 27080. this was supposed to be consuming posts on an existing semaphore, but it appears to have disappeared */
		status = (jint) j9shsem_destroy(&semaphore);
	}
	Trc_JCL_attach_cancelNotifyVmExit(env, status);
	return status;
}

/**
 * @return result of j9shsem_wait: 0 on success, -1 on failure
 *  Block until semaphore becomes non-zero.
 */
jint JNICALL
Java_com_ibm_tools_attach_target_IPC_waitSemaphore(JNIEnv *env, jclass clazz)
{

	PORT_ACCESS_FROM_VMC( ((J9VMThread *) env) );

	J9JavaVM* javaVM = ((J9VMThread*) env)->javaVM;
	jint status;

	Trc_JCL_attach_waitSemaphoreEntry(env);
	status = (jint) j9shsem_wait(javaVM->attachContext.semaphore, 0, 0);
	Trc_JCL_attach_waitSemaphoreExit(env, status);
	return status;
}

/**
 * Close semaphore
 */
void JNICALL
Java_com_ibm_tools_attach_target_IPC_closeSemaphore(JNIEnv *env, jclass clazz)
{

	PORT_ACCESS_FROM_VMC( ((J9VMThread *) env) );

	J9JavaVM* javaVM = ((J9VMThread*) env)->javaVM;

	j9shsem_close(&javaVM->attachContext.semaphore);
	Trc_JCL_attach_closeSemaphore(env);
	return;
}

/**
 * @return 0 on success, -1 on failure
 */
jint JNICALL
Java_com_ibm_tools_attach_target_IPC_destroySemaphore(JNIEnv *env, jclass clazz)
{

	PORT_ACCESS_FROM_VMC( ((J9VMThread *) env) );

	jint status = 0; /* return success if the semaphore is already closed or destroyed */
	struct j9shsem_handle **handle;

	J9JavaVM* javaVM = ((J9VMThread*) env)->javaVM;

	handle = &javaVM->attachContext.semaphore;
	if (NULL != handle) {
		status = (jint) j9shsem_destroy(handle);
	}
	Trc_JCL_attach_destroySemaphore(env, status);
	return  status;
}

/**
 * @return numeric user ID of the caller. This is upcast from a UDATA.
 */
jlong JNICALL
Java_com_ibm_tools_attach_target_IPC_getUid(JNIEnv *env, jclass clazz)
{

	PORT_ACCESS_FROM_VMC( ((J9VMThread *) env) );

	jlong uid;

	uid =  (jlong) j9sysinfo_get_euid();
	Trc_JCL_attach_getUid(env, uid);
	return uid;
}

/**
 * @return process ID of the caller.  This is upcast from UDATA.
 */
jlong JNICALL
Java_com_ibm_tools_attach_target_IPC_getProcessId(JNIEnv *env, jclass clazz)
{

	PORT_ACCESS_FROM_VMC( ((J9VMThread *) env) );

	jlong pid;

	pid =  (jlong) j9sysinfo_get_pid();
	Trc_JCL_attach_getProcessId(env, pid);
	return pid;
}

/**
 * Indicate if a specific process exists. Non-positive process IDs and processes owned by
 * other users return an error.
 * @param pid process ID
 * @return positive value if the process exists, 0 if the process does not exist, otherwise negative error code
 */
jint JNICALL
Java_com_ibm_tools_attach_target_IPC_processExistsImpl(JNIEnv *env, jclass clazz, jlong pid)
{
	PORT_ACCESS_FROM_VMC( ((J9VMThread *) env) );
	/* PID value was upcast from a UDATA to jlong. */
	jint rc = (pid > 0) ? (jint) j9sysinfo_process_exists((UDATA) pid) : -1;
	Trc_JCL_attach_processExists(env, pid, rc);
	return rc;
}

/**
 * Opens and locks a file.  Creates the file if it does not exist.
 * @param path file path to the file
 * @param mode file mode to create the file with
 * @param blocking true if process is to wait for the file to be unlocked
 * @return file descriptor if the file is successfully locked, otherwise negative value
 * @note J9PORT_ERROR_FILE_OPFAILED (-300) indicates file cannot be opened or the path could not be converted, J9PORT_ERROR_FILE_LOCK_BADLOCK (-316) indicates that the file could not be locked.
 */
jlong JNICALL
Java_com_ibm_tools_attach_target_FileLock_lockFileImpl(JNIEnv *env, jclass clazz, jstring path, jint mode, jboolean blocking)
{
    PORT_ACCESS_FROM_VMC( ((J9VMThread *) env) );

    jlong result = JNI_OK;
	const char *pathUTF;

	pathUTF = (*env)->GetStringUTFChars(env, path, NULL); /* j9file_open takes a UTF8 string for the path */
	if (NULL != pathUTF) {
		IDATA fd = j9file_open(pathUTF, EsOpenCreate | EsOpenWrite, mode);
		if (isFileOwnedByMe(env, pathUTF)) {
			j9file_chmod(pathUTF, mode); /* override UMASK */
		}
		Trc_JCL_attach_lockFileImpl(env, pathUTF, mode, blocking, fd);
		(*env)->ReleaseStringUTFChars(env, path, pathUTF);
		if (0 >= fd) {
			result = J9PORT_ERROR_FILE_OPFAILED;
		} else {
			IDATA lockStatus;
			/*
			 * Must be a write lock to get exclusive access.
			 * Must lock at least one byte on Windows, otherwise it returns a false positive.
			 * Both Posix and Windows allow the lock range to extend past the end of the file.
			 */
			lockStatus = j9file_lock_bytes(fd, J9PORT_FILE_WRITE_LOCK | ((0 == blocking)? J9PORT_FILE_NOWAIT_FOR_LOCK: J9PORT_FILE_WAIT_FOR_LOCK), 0, 1);
			if (0 != lockStatus) {
				j9file_close(fd);
				result = J9PORT_ERROR_FILE_LOCK_BADLOCK;
			} else {
				result = fd;
			}
		}
	} else {
		result = J9PORT_ERROR_FILE_OPFAILED;
	}
	Trc_JCL_attach_lockFileStatus(env, result);
	return result;
}

 /* use for debugging or to record normal events. Use message to distinguish errors  */
#define TRACEPOINT_STATUS_NORMAL 0
#define TRACEPOINT_STATUS_LOGGING  1

/* use for debugging only. Use message to distinguish errors */
#define TRACEPOINT_STATUS_ERROR  -1

#define TRACEPOINT_STATUS_OOM_DURING_WAIT  -2
#define TRACEPOINT_STATUS_OOM_DURING_TERMINATE  -3
/**
 * generate a tracepoint with a status code and message.
 *
 * @param statusCode numeric status value
 * @param message descriptive message. May be null.
 */
void JNICALL
Java_com_ibm_tools_attach_target_IPC_tracepoint(JNIEnv *env, jclass clazz, jint statusCode, jstring message) {

		const char *msgUTF = NULL;
		const char *statusText = "STATUS_NORMAL";

		if (NULL != message) {
			msgUTF = (*env)->GetStringUTFChars(env, message, NULL);
		}

		switch (statusCode) {
		case TRACEPOINT_STATUS_LOGGING:
			statusText = "STATUS_LOGGING";
			break;
		case TRACEPOINT_STATUS_NORMAL:
			statusText = "STATUS_NORMAL";
			break;
		case TRACEPOINT_STATUS_OOM_DURING_WAIT:
			statusText = "STATUS_OOM_DURING_WAIT"; /* from wait loop */
			break;
		case TRACEPOINT_STATUS_OOM_DURING_TERMINATE:
			statusText = "STATUS_OOM_DURING_TERMINATE"; /* from terminate code */
			break;
		default: /* FALLTHROUGH */
		case -1:
			statusText = "STATUS_ERROR"; /* unspecified error */
			break;
		}
		if (NULL != msgUTF) {
			Trc_JCL_com_ibm_tools_attach_javaSE_IPC_tracepoint(env, statusCode, statusText, msgUTF);
			(*env)->ReleaseStringUTFChars(env, message, msgUTF);
		} else {
			Trc_JCL_com_ibm_tools_attach_javaSE_IPC_tracepoint(env, statusCode, statusText, "<unavailable>");
		}
 }

/**
 * Unlock and close a file.
 * @param file descriptor
 * @return 0 if successful, -1 if failed
 */
jint JNICALL
Java_com_ibm_tools_attach_target_FileLock_unlockFileImpl(JNIEnv *env, jclass clazz, jlong fd) {
    PORT_ACCESS_FROM_VMC( ((J9VMThread *) env) );

    jint result = JNI_OK;
    j9file_unlock_bytes((IDATA) fd, 0, 0);
    result = j9file_close((IDATA) fd);
	Trc_JCL_attach_unlockFileWithStatus(env, (IDATA) fd, result);
	return result;
}
