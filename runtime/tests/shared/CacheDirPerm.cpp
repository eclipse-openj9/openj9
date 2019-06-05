/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
#if !defined(WIN32)
extern "C" {
#include "shrinit.h"
}

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "SCTestCommon.h"
#include "OpenCacheHelper.h"
#include "j9port.h"

#define TEST_PARENTDIR "testParentDir"
#define TEST_TEMPDIR "testCacheDirPermDir"
#define J9SH_DEFAULT_CTRL_ROOT "/tmp"

#define USER_PERMISSION_STR "0700"
#define USER_PERMISSION      0700
#define NEW_DIR_PERM         0760

#define EXISTING_DIR_PERM         NEW_DIR_PERM
#if defined(OPENJ9_BUILD)
#define NON_EXISTING_DEFAULT_DIR_PERM	J9SH_DIRPERM
#define EXISTING_DEFAULT_DIR_PERM	NEW_DIR_PERM
#else
#define NON_EXISTING_DEFAULT_DIR_PERM	J9SH_DIRPERM_DEFAULT_TMP
#if defined(J9ZOS390)
/* on z/OS permission of existing default directory under /tmp is not changed to J9SH_DIRPERM_DEFAULT_TMP. */
#define EXISTING_DEFAULT_DIR_PERM	NEW_DIR_PERM
#else /* defined(J9ZOS390) */
#define EXISTING_DEFAULT_DIR_PERM	J9SH_DIRPERM_DEFAULT_TMP
#endif /* defined(J9ZOS390) */
#endif /* defined(OPENJ9_BUILD) */

extern "C" IDATA removeTempDir(J9JavaVM *vm, char *dir);

class CacheDirPerm : public OpenCacheHelper {
public:
	const char *cacheName;
	char cacheDir[J9SH_MAXPATH];
	char parentDir[J9SH_MAXPATH];

	CacheDirPerm(J9JavaVM *vm) :
		OpenCacheHelper(vm)
	{
	}

	IDATA getTempCacheDir(J9JavaVM *vm, I_32 cacheType, bool useDefaultDir, bool groupAccess);
	IDATA createTempCacheDir(I_32 cacheType, bool useDefaultDir);
	IDATA getCacheDirPerm(I_32 cacheType, bool isDefaultDir);
	IDATA getParentDirPerm(void);
};

IDATA
CacheDirPerm::getTempCacheDir(J9JavaVM *vm, I_32 cacheType, bool useDefaultDir, bool groupAccess)
{
	IDATA rc = PASS;
	char baseDir[J9SH_MAXPATH];
	const char *testName = "getTempCacheDir";
	U_32 flags = 0;
	PORT_ACCESS_FROM_JAVAVM(vm);

	if (useDefaultDir) {
		flags |= J9SHMEM_GETDIR_APPEND_BASEDIR;
#if defined(OPENJ9_BUILD)
		if (!groupAccess) {
			flags |= J9SHMEM_GETDIR_USE_USERHOME;
		}
#endif /* defined(OPENJ9_BUILD) */
	}

	memset(cacheDir, 0, J9SH_MAXPATH);
	memset(parentDir, 0, J9SH_MAXPATH);
	rc = j9shmem_getDir(NULL, flags, baseDir, J9SH_MAXPATH);
	if (0 > rc) {
		ERRPRINTF("Failed to get a temporary directory\n");
		rc = FAIL;
		goto _end;
	}
	if (useDefaultDir) {
		j9str_printf(PORTLIB, cacheDir, sizeof(cacheDir), "%s", baseDir);
	} else {
		j9str_printf(PORTLIB, cacheDir, sizeof(cacheDir), "%s%s/%s/", baseDir, TEST_PARENTDIR, TEST_TEMPDIR);
		if (J9PORT_SHR_CACHE_TYPE_NONPERSISTENT == cacheType) {
			/* for non-persistent cache, actual cache dir is cacheDir/J9SH_BASEDIR. So parent dir is same as cacheDir. */
			j9str_printf(PORTLIB, parentDir, sizeof(parentDir), "%s%s/%s/", baseDir, TEST_PARENTDIR, TEST_TEMPDIR);
		} else {
			j9str_printf(PORTLIB, parentDir, sizeof(parentDir), "%s%s/", baseDir, TEST_PARENTDIR);
		}
	}
_end:
	return rc;
}

IDATA
CacheDirPerm::createTempCacheDir(I_32 cacheType, bool useDefaultDir)
{
	char actualCacheDir[J9SH_MAXPATH];
	const char *testName = "createTempCacheDir";
	struct stat statbuf;
	IDATA rc = PASS;
	PORT_ACCESS_FROM_JAVAVM(vm);

	if (useDefaultDir) {
		j9str_printf(PORTLIB, actualCacheDir, sizeof(actualCacheDir), "%s", cacheDir);
	} else {
		if (J9PORT_SHR_CACHE_TYPE_NONPERSISTENT == cacheType) {
			j9str_printf(PORTLIB, actualCacheDir, sizeof(actualCacheDir), "%s/%s", cacheDir, J9SH_BASEDIR);
		} else {
			j9str_printf(PORTLIB, actualCacheDir, sizeof(actualCacheDir), "%s", cacheDir);
		}
	}

	rc = j9shmem_createDir(actualCacheDir, NEW_DIR_PERM, FALSE);
	if (-1 == rc) {
		ERRPRINTF("Failed to create a temporary directory\n");
		rc = FAIL;
		goto _end;
	}
	if(-1 == stat(actualCacheDir, &statbuf)) {
		ERRPRINTF1("Failed to get stats for cache directory, actualCacheDir: %s", actualCacheDir);
		return -1;
	}
	rc = PASS;
_end:
	return rc;
}

IDATA
CacheDirPerm::getCacheDirPerm(I_32 cacheType, bool isDefaultDir)
{
	struct stat statbuf;
	char actualCacheDir[J9SH_MAXPATH];
	const char *testName = "getCacheDirPerm";
	PORT_ACCESS_FROM_JAVAVM(vm);

	if (isDefaultDir) {
		j9str_printf(PORTLIB, actualCacheDir, sizeof(actualCacheDir), "%s", cacheDir);
	} else {
		if (J9PORT_SHR_CACHE_TYPE_NONPERSISTENT == cacheType) {
			j9str_printf(PORTLIB, actualCacheDir, sizeof(actualCacheDir), "%s/%s", cacheDir, J9SH_BASEDIR);
		} else {
			j9str_printf(PORTLIB, actualCacheDir, sizeof(actualCacheDir), "%s", cacheDir);
		}
	}

	if(-1 == stat(actualCacheDir, &statbuf)) {
		ERRPRINTF1("Failed to get stats for cache directory, actualCacheDir: %s", actualCacheDir);
		return -1;
	}
	return (statbuf.st_mode & (S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO));
}

IDATA
CacheDirPerm::getParentDirPerm(void)
{
	struct stat statbuf;
	const char *testName = "getParentDirPerm";
	PORT_ACCESS_FROM_JAVAVM(vm);
	if(-1 == stat(parentDir, &statbuf)) {
		ERRPRINTF1("Failed to get stats for parent directory: %s", parentDir);
		return -1;
	}
	return (statbuf.st_mode & (S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO));
}

extern "C" {

IDATA
removeTempDir(J9JavaVM *vm, char *dir)
{
	struct J9FileStat buf;
	IDATA rc;
	const char *testName = "removeTempDir";
	PORT_ACCESS_FROM_JAVAVM(vm);

	rc = j9file_stat(dir, (U_32)0, &buf);
	if (-1 == rc) {
		ERRPRINTF("Error in getting stats for cache dir");
		rc = FAIL;
		goto _end;
	}

	if (buf.isDir != 1) {
		rc = j9file_unlink(dir);
		if (-1 == rc) {
			ERRPRINTF1("Failed to unlink %s\n", dir);
			rc = FAIL;
			goto _end;
		}
	} else {
		char baseFilePath[J9SH_MAXPATH];
		char resultBuffer[J9SH_MAXPATH];
		UDATA rc, handle;

		j9str_printf(PORTLIB, baseFilePath, sizeof(baseFilePath), "%s", dir);
		rc = handle = j9file_findfirst(baseFilePath, resultBuffer);
		while ((UDATA)-1 != rc) {
			char nextEntry[J9SH_MAXPATH];
			/* skip current and parent dir */
			if (resultBuffer[0] == '.') {
				rc = j9file_findnext(handle, resultBuffer);
				continue;
			}
			j9str_printf(PORTLIB, nextEntry, sizeof(nextEntry), "%s/%s", baseFilePath, resultBuffer);
			removeTempDir(vm, nextEntry);
			rc = j9file_findnext(handle, resultBuffer);
		}
		if (handle != (UDATA)-1) {
			j9file_findclose(handle);
		}
		rc = j9file_unlinkdir(baseFilePath);
		if ((UDATA)-1 == rc) {
			ERRPRINTF1("Failed to unlink %s\n", baseFilePath);
			rc = FAIL;
			goto _end;
		}
	}
	rc = PASS;
_end:
	return rc;
}

IDATA
testCacheDirPerm(J9JavaVM *vm)
{
	IDATA rc = PASS;
	const char *testName = "testCacheDirPerm";
	PORT_ACCESS_FROM_JAVAVM(vm);

	REPORT_START("CacheDirPerm");

	CacheDirPerm cdp(vm);
	cdp.cacheName = "CacheDirPermCache";

	for (int i = 0; i <= 19; i++) {
		const char *cacheDirPermStr, *cacheTypeString;
		bool useExistingDir, useDefaultDir;
		IDATA expectedDirPerm;
		IDATA perm;
		IDATA cacheType = 0;
		bool groupAccess = false;
		U_64 extraRuntimeFlag = 0;

		switch(i) {
		case 0:
#if !(defined(J9ZOS390))
			/* persistent cache; use existing non-default directory; don't use cacheDirPerm */
			cacheType = J9PORT_SHR_CACHE_TYPE_PERSISTENT;
			cacheTypeString = "persistent";
			useExistingDir = true;
			cacheDirPermStr = NULL;
			expectedDirPerm = EXISTING_DIR_PERM;
			useDefaultDir = false;
#endif
			break;
		case 1:
#if !defined(J9SHR_CACHELET_SUPPORT)
			/* non-persistent cache; use existing non-default directory; don't use cacheDirPerm */
			cacheType = J9PORT_SHR_CACHE_TYPE_NONPERSISTENT;
			cacheTypeString = "non persistent";
			useExistingDir = true;
			cacheDirPermStr = NULL;
			expectedDirPerm = EXISTING_DIR_PERM;
			useDefaultDir = false;
#endif
			break;
		case 2:
#if !(defined(J9ZOS390))
			/* persistent cache; use non-existing non-default directory; don't use cacheDirPerm */
			cacheType = J9PORT_SHR_CACHE_TYPE_PERSISTENT;
			cacheTypeString = "persistent";
			useExistingDir = false;
			cacheDirPermStr = NULL;
			expectedDirPerm = J9SH_DIRPERM;
			useDefaultDir = false;
#endif
			break;
		case 3:
#if !defined(J9SHR_CACHELET_SUPPORT)
			/* non-persistent cache; use non-existing non-default directory; don't use cacheDirPerm */
			cacheType = J9PORT_SHR_CACHE_TYPE_NONPERSISTENT;
			cacheTypeString = "non persistent";
			useExistingDir = false;
			cacheDirPermStr = NULL;
			expectedDirPerm = J9SH_DIRPERM;
			useDefaultDir = false;
#endif
			break;
		case 4:
#if !(defined(J9ZOS390))
			/* persistent cache; use existing non-default directory; use cacheDirPerm */
			cacheType = J9PORT_SHR_CACHE_TYPE_PERSISTENT;
			cacheTypeString = "persistent";
			useExistingDir = true;
			cacheDirPermStr = USER_PERMISSION_STR;
			expectedDirPerm = NEW_DIR_PERM;
			useDefaultDir = false;
#endif
			break;
		case 5:
#if !defined(J9SHR_CACHELET_SUPPORT)
			/* non-persistent cache; use existing non-default directory; use cacheDirPerm */
			cacheType = J9PORT_SHR_CACHE_TYPE_NONPERSISTENT;
			cacheTypeString = "non persistent";
			useExistingDir = true;
			cacheDirPermStr = USER_PERMISSION_STR;
			expectedDirPerm = NEW_DIR_PERM;
			useDefaultDir = false;
#endif
			break;
		case 6:
#if !(defined(J9ZOS390))
			/* persistent cache; use non-existing non-default directory; use cacheDirPerm */
			cacheType = J9PORT_SHR_CACHE_TYPE_PERSISTENT;
			cacheTypeString = "persistent";
			useExistingDir = false;
			cacheDirPermStr = USER_PERMISSION_STR;
			expectedDirPerm = USER_PERMISSION;
			useDefaultDir = false;
#endif
			break;
		case 7:
#if !defined(J9SHR_CACHELET_SUPPORT)
			/* non-persistent cache; use non-existing non-default directory; use cacheDirPerm */
			cacheType = J9PORT_SHR_CACHE_TYPE_NONPERSISTENT;
			cacheTypeString = "non persistent";
			useExistingDir = false;
			cacheDirPermStr = USER_PERMISSION_STR;
			expectedDirPerm = USER_PERMISSION;
			useDefaultDir = false;
#endif
			break;
		case 8:
#if !(defined(J9ZOS390))
			/* persistent cache; use existing default directory; don't use cacheDirPerm */
			cacheType = J9PORT_SHR_CACHE_TYPE_PERSISTENT;
			cacheTypeString = "persistent";
			useExistingDir = true;
			cacheDirPermStr = NULL;
			expectedDirPerm = EXISTING_DEFAULT_DIR_PERM;
			useDefaultDir = true;
#endif
			break;
		case 9:
#if !defined(J9SHR_CACHELET_SUPPORT)
			/* non-persistent cache; use existing default directory; don't use cacheDirPerm */
			cacheType = J9PORT_SHR_CACHE_TYPE_NONPERSISTENT;
			cacheTypeString = "non persistent";
			useExistingDir = true;
			cacheDirPermStr = NULL;
			expectedDirPerm = EXISTING_DEFAULT_DIR_PERM;
			useDefaultDir = true;
#endif
			break;
		case 10:
#if !(defined(J9ZOS390))
			/* persistent cache; use existing default directory; use cacheDirPerm */
			cacheType = J9PORT_SHR_CACHE_TYPE_PERSISTENT;
			cacheTypeString = "persistent";
			useExistingDir = true;
			cacheDirPermStr = USER_PERMISSION_STR;
			expectedDirPerm = EXISTING_DEFAULT_DIR_PERM;
			useDefaultDir = true;
#endif
			break;
		case 11:
#if !defined(J9SHR_CACHELET_SUPPORT)
			/* non-persistent cache; use existing default directory; use cacheDirPerm */
			cacheType = J9PORT_SHR_CACHE_TYPE_NONPERSISTENT;
			cacheTypeString = "non persistent";
			useExistingDir = true;
			cacheDirPermStr = USER_PERMISSION_STR;
			expectedDirPerm = EXISTING_DEFAULT_DIR_PERM;
			useDefaultDir = true;
#endif
			break;
		case 12:
#if !(defined(J9ZOS390))
			/* persistent cache; use non-existing default directory; don't use cacheDirPerm */
			cacheType = J9PORT_SHR_CACHE_TYPE_PERSISTENT;
			cacheTypeString = "persistent";
			useExistingDir = false;
			cacheDirPermStr = NULL;
			expectedDirPerm = NON_EXISTING_DEFAULT_DIR_PERM;
			useDefaultDir = true;
#endif
			break;
		case 13:
#if !defined(J9SHR_CACHELET_SUPPORT)
			/* non-persistent cache; use non-existing default directory; don't use cacheDirPerm */
			cacheType = J9PORT_SHR_CACHE_TYPE_NONPERSISTENT;
			cacheTypeString = "non persistent";
			useExistingDir = false;
			cacheDirPermStr = NULL;
			expectedDirPerm = NON_EXISTING_DEFAULT_DIR_PERM;
			useDefaultDir = true;
#endif
			break;
		case 14:
#if !(defined(J9ZOS390))
			/* persistent cache; use non-existing default directory; use cacheDirPerm */
			cacheType = J9PORT_SHR_CACHE_TYPE_PERSISTENT;
			cacheTypeString = "persistent";
			useExistingDir = false;
			cacheDirPermStr = USER_PERMISSION_STR;
			expectedDirPerm = NON_EXISTING_DEFAULT_DIR_PERM;
			useDefaultDir = true;
#endif /* !(defined(J9ZOS390)) */
			break;
		case 15:
#if !defined(J9SHR_CACHELET_SUPPORT)
			/* non-persistent cache; use non-existing default directory; use cacheDirPerm */
			cacheType = J9PORT_SHR_CACHE_TYPE_NONPERSISTENT;
			cacheTypeString = "non persistent";
			useExistingDir = false;
			cacheDirPermStr = USER_PERMISSION_STR;
			expectedDirPerm = NON_EXISTING_DEFAULT_DIR_PERM;
			useDefaultDir = true;
#endif
			break;
		case 16:
#if !defined(J9ZOS390)
			/* persistent cache; use non-existing default directory; don't use cacheDirPerm, use groupaccess*/
			cacheType = J9PORT_SHR_CACHE_TYPE_PERSISTENT;
			cacheTypeString = "persistent";
			useExistingDir = false;
			cacheDirPermStr = NULL;
			expectedDirPerm = J9SH_DIRPERM_DEFAULT_TMP;
			useDefaultDir = true;
			groupAccess = true;
#endif /*!defined(J9ZOS390) */
			break;
		case 17:
			/* non-persistent cache; use non-existing default directory; don't use cacheDirPerm, use groupaccess */
			cacheType = J9PORT_SHR_CACHE_TYPE_NONPERSISTENT;
			cacheTypeString = "non persistent";
			useExistingDir = false;
			cacheDirPermStr = NULL;
			expectedDirPerm = J9SH_DIRPERM_DEFAULT_TMP;
			useDefaultDir = true;
			groupAccess = true;
			break;
		case 18:
#if !defined(J9ZOS390)
			/* persistent cache; use non-existing non-default directory; don't use cacheDirPerm, use groupaccess*/
			cacheType = J9PORT_SHR_CACHE_TYPE_PERSISTENT;
			cacheTypeString = "persistent";
			useExistingDir = false;
			cacheDirPermStr = NULL;
			expectedDirPerm = J9SH_DIRPERM_GROUPACCESS;
			useDefaultDir = false;
			groupAccess = true;
#endif /* !defined(J9ZOS390) */
			break;
		case 19:
			/* non-persistent cache; use non-existing non-default directory; don't use cacheDirPerm, use groupaccess*/
			cacheType = J9PORT_SHR_CACHE_TYPE_NONPERSISTENT;
			cacheTypeString = "non persistent";
			useExistingDir = false;
			cacheDirPermStr = NULL;
			expectedDirPerm = J9SH_DIRPERM_GROUPACCESS;
			useDefaultDir = false;
			groupAccess = true;
			break;
		}

		if (0 != cacheType) {
			INFOPRINTF6("Test %d: cacheType: %s, useExistingDir: %d, cacheDirPermStr: %s, expectedDirPerm: %d, useDefaultDir: %d\n", i, cacheTypeString, useExistingDir, cacheDirPermStr, expectedDirPerm, useDefaultDir);

			rc = cdp.getTempCacheDir(vm, cacheType, useDefaultDir, groupAccess);
			if (FAIL == rc) {
				ERRPRINTF("createTempCacheDir failed\n");
				goto _end;
			}
			INFOPRINTF("Initial cleanup\n");

			if (0 != strlen(cdp.parentDir)) {
				rc = removeTempDir(vm, cdp.parentDir);
			} else {
				rc = removeTempDir(vm, cdp.cacheDir);
			}

			/* if we want to use existing dir, then create it before opening the cache */
			if (useExistingDir) {
				rc = cdp.createTempCacheDir(cacheType, useDefaultDir);
				if (FAIL == rc) {
					ERRPRINTF("createTempCacheDir failed\n");
					goto _end;
				} else {
					INFOPRINTF1("Created cache dir %s\n", cdp.cacheDir);
				}
			}
			if (groupAccess) {
				extraRuntimeFlag = J9SHR_RUNTIMEFLAG_ENABLE_GROUP_ACCESS;
			} else {
				extraRuntimeFlag = 0;
			}

			if (useDefaultDir) {
				rc = cdp.openTestCache(cacheType, CACHE_SIZE, cdp.cacheName, false, NULL, NULL, cacheDirPermStr, extraRuntimeFlag, 0);
			} else {
				rc = cdp.openTestCache(cacheType, CACHE_SIZE, cdp.cacheName, false, NULL, cdp.cacheDir, cacheDirPermStr, extraRuntimeFlag, 0);
			}
			if (FAIL == rc) {
				ERRPRINTF("openTestCache failed\n");
				goto _close;
			}
			perm = cdp.getCacheDirPerm(cacheType, useDefaultDir);
			if (-1 == perm) {
				ERRPRINTF("getCacheDirPerms failed\n");
				rc = FAIL;
				goto _close;
			}
			if (perm != expectedDirPerm) {
				ERRPRINTF2("Incorrect permission on cache directory. Expected: %d, found: %d\n", expectedDirPerm, perm);
				rc = FAIL;
				goto _close;
			}
			if ((false == useExistingDir) && (false == useDefaultDir) && (NULL == cacheDirPermStr)) {
				perm = cdp.getParentDirPerm();
				if (-1 == perm) {
					rc = FAIL;
					goto _close;
				}
				if (J9SH_PARENTDIRPERM != perm) {
					ERRPRINTF2("Incorrect permission on parent directory. Expected: %d, found: %d ", J9SH_PARENTDIRPERM, perm);
					rc = FAIL;
					goto _close;
				}
			}
_close:
			if (FAIL == rc) {
				cdp.closeTestCache(false);
			}
			if (FAIL == rc) {
				removeTempDir(vm, cdp.cacheDir);
				goto _end;
			} else {
				rc = cdp.closeTestCache(false);
				if (FAIL == rc) {
					ERRPRINTF("cdp.closeTestCache failed\n");
					removeTempDir(vm, cdp.cacheDir);
					goto _end;
				}
			}
			INFOPRINTF("Test passed\n");
		}
	}

_end:
	REPORT_SUMMARY("CacheDirPerm", rc);
	return rc;
}

} /* extern "C" */

#endif /* !defined(WIN32) */
