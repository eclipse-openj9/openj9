/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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

#if !defined(LINUX) || defined(J9ZTPF) || defined(J9ARM) || defined(J9AARCH64)
#error "jitserver is only supported on Linux OS, excluding ARM and ZTPF archs"
#endif

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dlfcn.h>
#include <inttypes.h>
#include <linux/limits.h>
#include "j9cfg.h"
#include "j9arch.h"
#include "jitserver_api.h"
#include "jitserver_error.h"

#undef JITSERVER_LAUNCHER_DEBUG

#define DIR_SEPARATOR_CHAR '/'

#define PATH_BUFFER_LEN 512
#define SELF_EXE "/proc/self/exe"

typedef int32_t (*createJITServer)(JITServer **jitServer, void *vm_args);

static void truncateOneLevel(char *path);
static int32_t append(char **buffer, uint32_t bufLen, char *str);
static int32_t isDir(const char *path);
static int32_t isRegularFile(const char *path);
static int32_t getJvmPath(char *pathBuffer, int32_t pathBufLen);
static char * getJvmLibPath(void);

static const char *jvmOptions[] = {	"-Xms8m",
					"-Xmx8m",
					"-Xgcthreads1",
				};

static void
truncateOneLevel(char *path)
{
	char *lastLevel = NULL;
	uint32_t index = strlen(path) - 1;

#if defined(JITSERVER_LAUNCHER_DEBUG)
	fprintf(stdout, "truncateOneLevel> on entry, path: %s\n", path);
#endif

	/* Consume any trailing '/' */
	while (path[index] == DIR_SEPARATOR_CHAR) {
		path[index] = '\0';
		index -= 1;
	}

	/* Now consume the last directory if present */
	lastLevel = strrchr(path, DIR_SEPARATOR_CHAR);
	if (NULL != lastLevel) {
		*lastLevel = '\0';
	}

#if defined(JITSERVER_LAUNCHER_DEBUG)
	fprintf(stdout, "truncateOneLevel> on exit, path: %s\n", path);
#endif
}

static int32_t
append(char **buffer, uint32_t bufLen, char *str)
{
	uint32_t strLen = strlen(str);

	if (strlen(*buffer) + strLen >= bufLen) {
		uint32_t newBufLen = bufLen + strLen + PATH_BUFFER_LEN;
		char *newBuf = realloc(*buffer, newBufLen);
		if (NULL == newBuf) {
			return -1;
		}
		strcat(newBuf, str);
		*buffer = newBuf;
		return 0;
	} else {
		strcat(*buffer, str);
		return 0;
	}
}

static int32_t
isDir(const char *path)
{
	int32_t rc = 0;
	struct stat statBuf = {0};

	rc = stat(path, &statBuf);
	if ((0 == rc) && (S_ISDIR(statBuf.st_mode))) {
		return 1;
	}
	return 0;
}

static int32_t
isRegularFile(const char *path)
{
	int32_t rc = 0;
	struct stat statBuf = {0};

	rc = stat(path, &statBuf);
	if ((0 == rc) && (S_ISREG(statBuf.st_mode))) {
		return 1;
	}
	return 0;
}

static int32_t
getJvmPath(char *pathBuffer, int32_t pathBufLen)
{
	int32_t rc = 0;

#if JAVA_SPEC_VERSION == 8
	/* pathBuffer can be either <jdk_home> or <jdk_home>/jre or <jre_home> */
	char *lastLevel = strrchr(pathBuffer, DIR_SEPARATOR_CHAR);

	if ((NULL != lastLevel) && (strcmp(lastLevel + 1, "jre") == 0)) {
		/* pathBuffer is at <jdk_home>/jre */
		rc = append(&pathBuffer, pathBufLen, "/lib/" OPENJ9_ARCH_DIR);
	} else {
		/* pathBuffer is at <jdk_home> or <jre_home> */
		rc = append(&pathBuffer, pathBufLen, "/jre");
		if (0 != rc) {
			fprintf(stderr, "Failed to allocated memory for path buffer\n");
			goto _end;
		}
		if (isDir(pathBuffer)) {
			/* pathBuffer is at <jdk_home>/jre */
			rc = append(&pathBuffer, pathBufLen, "/lib/" OPENJ9_ARCH_DIR);
		} else {
			/* pathBuffer is at <jre_home> */
			truncateOneLevel(pathBuffer);
			rc = append(&pathBuffer, pathBufLen, "/lib/" OPENJ9_ARCH_DIR);
		}
	}
#else
	rc = append(&pathBuffer, pathBufLen, "/lib");
#endif /* JAVA_SPEC_VERSION */
	if (0 != rc) {
		fprintf(stderr, "Failed to allocated memory for path buffer\n");
		goto _end;
	}

	/* try "compressedrefs" first */
	rc = append(&pathBuffer, pathBufLen, "/" OPENJ9_CR_JVM_DIR);
	if (0 != rc) {
		fprintf(stderr, "Failed to allocated memory for path buffer\n");
		goto _end;
	}
	rc = isDir(pathBuffer);
	if (0 == rc) {
		/* "compressedrefs" is not present, try "noncr" now */
		truncateOneLevel(pathBuffer);
#if defined(JITSERVER_LAUNCHER_DEBUG)
		fprintf(stdout, "getJvmPath> failed to find \"compressedrefs\" dir, looking for \"default\" dir now\n");
#endif /* JITSERVER_LAUNCHER_DEBUG */
		rc = append(&pathBuffer, pathBufLen, "/" OPENJ9_NOCR_JVM_DIR);
		if (0 != rc) {
			fprintf(stderr, "Failed to allocated memory for path buffer\n");
			goto _end;
		}
		rc = isDir(pathBuffer);
		if (0 == rc) {
			fprintf(stderr, "Failed to find any recognized JVM\n");
			rc = -1;
			goto _end;
		}
	}
#if defined(JITSERVER_LAUNCHER_DEBUG)
	fprintf(stdout, "getJvmPath> path: %s\n", pathBuffer);
#endif
	rc = 0; /* success */

_end:
	return rc;
}

static char *
getJvmLibPath(void)
{
	char *jvmLibPath = NULL;
	uint32_t jvmLibPathLen = 0;
	int32_t rc = 0;

	do {
		jvmLibPathLen += PATH_BUFFER_LEN;
		jvmLibPath = malloc(jvmLibPathLen);
		if (NULL == jvmLibPath) {
			fprintf(stderr, "Failed to allocate memory for jvm library\n");
			goto _end;
		}
		rc = readlink(SELF_EXE, jvmLibPath, jvmLibPathLen);
		if (-1 == rc) {
			free(jvmLibPath);
			jvmLibPath = NULL;
			goto _end;
		} else if (jvmLibPathLen == rc) {
			/* path is probably truncated, try again with larger buffer */
			free(jvmLibPath);
		}
	} while (jvmLibPathLen == rc);

	jvmLibPath[rc] = '\0';

	/* selfPath could be one of the following:
	 *  - <jdk_home>/bin/jitserver for Java 9+
	 *  - <jdk_home>/jre/bin/jitserver for Java 8
	 *  - <jdk_home>/bin/jitserver for Java 8
	 *  - <jre_home>/bin/jitserver for Java 8
	 *
	 * libjvm.so can be at the following locations:
	 *  - <jdk_home>/lib/<jvm_type>/libjvm.so for Java 9+
	 *  - <jdk_home>/jre/lib/amd64/<jvm_type>/libjvm.so for Java 8
	 *  - <jre_home>/lib/amd64/<jvm_type>/libjvm.so for Java 8
	 * where <jvm_type> can be "compressedrefs" or "default"
	 */

	/* Remove at least two entries from the path. */
	truncateOneLevel(jvmLibPath);
	truncateOneLevel(jvmLibPath);
	/* Now jvmLibPath is either <jdk_home> (Java 9+, Java 8) or <jdk_home>/jre (Java 8) or <jre_home> (Java 8) */

	rc = getJvmPath(jvmLibPath, jvmLibPathLen);
	if (0 != rc) {
		goto _end;
	}

	rc = append(&jvmLibPath, jvmLibPathLen, "/libjvm.so");
	if (0 != rc) {
		fprintf(stderr, "Failed to allocated memory for path buffer\n");
		free(jvmLibPath);
		jvmLibPath = NULL;
		goto _end;
	}

#if defined(JITSERVER_LAUNCHER_DEBUG)
	fprintf(stdout, "getJvmLibPath> jvmLibPath: %s\n", jvmLibPath);
#endif

	/* Final check for libjvm.so */
	rc = isRegularFile(jvmLibPath);
	if (1 != rc) {
		fprintf(stderr, "Failed to find JVM library libjvm.so at expected location %s\n", jvmLibPath);
		goto _end;
	}
	rc = 0; /* success */

_end:
	if ((0 != rc) && (NULL != jvmLibPath)) {
		free(jvmLibPath);
		jvmLibPath = NULL;
	}
	return jvmLibPath;
}

int
main(int argc, char *argv[])
{
	JITServer *server = NULL;
	JavaVMOption *options = NULL;
	JavaVMInitArgs vmArgs = {0};
	void *libHandle = NULL;
	char *jvmLibPath = NULL;
	int32_t rc = 0;
	int32_t i = 0;
	int32_t argIndex = 0;
	int32_t numJvmOptions = 0;
	createJITServer createServer = NULL;

	fprintf(stderr, "JITServer is currently a technology preview. Its use is not yet supported.\n");

	jvmLibPath = getJvmLibPath();
	if (NULL == jvmLibPath) {
		fprintf(stderr, "Failed to find libjvm.so\n");
		rc = -1;
		goto _end;
	}
	dlerror(); /* clear any old error conditions */
	libHandle = (void*)dlopen(jvmLibPath, RTLD_NOW);
	if (NULL == libHandle) {
		fprintf(stderr, "%s\n", dlerror());
		rc = -1;
		goto _end;
	}

	createServer = (createJITServer) dlsym(libHandle, "JITServer_CreateServer");
	if (NULL == createServer) {
		fprintf(stderr, "%s\n", dlerror());
		rc = -1;
		goto _end;
	}

	vmArgs.version = JNI_VERSION_1_6;
	vmArgs.nOptions = 0;
	vmArgs.ignoreUnrecognized = JNI_TRUE;
	vmArgs.options = NULL;

	numJvmOptions = (argc - 1) + (sizeof(jvmOptions) / sizeof(char *));
	options = malloc(sizeof(JavaVMOption) * numJvmOptions);
	if (NULL == options) {
		fprintf(stderr, "Insufficient memory to allocate JavaVMOption\n");
		rc = -1;
		goto _end;
	}
	vmArgs.options = options;

	for (i = 0, argIndex = 0; i < (sizeof(jvmOptions) / sizeof(char *)); i++, argIndex++) {
		options[argIndex].optionString = (char *)jvmOptions[i];
	}
	for (i = 1; i < argc; i++, argIndex++) {
		options[argIndex].optionString = argv[i];
	}
	vmArgs.nOptions = argIndex;

	rc = createServer(&server, &vmArgs);
	if (JITSERVER_OK != rc) {
		fprintf(stderr, "JITServer_CreateServer failed, error code=%d\n", rc);
		goto _end;
	}

	rc = server->startJITServer(server);
	if (JITSERVER_OK != rc) {
		fprintf(stderr, "startJITServer failed, error code=%d\n", rc);
		goto _end;
	}
	fprintf(stderr, "\nJITServer is ready to accept incoming requests\n");

	rc = server->waitForJITServerTermination(server);
	if (JITSERVER_OK != rc) {
		fprintf(stderr, "waitForJITServerTermination failed, error code=%d\n", rc);
		goto _end;
	}

	server->destroyJITServer(&server);

	free(jvmLibPath);
	free(options);

_end:
	return rc;
}
