/*******************************************************************************
 * Copyright IBM Corp. and others 2002
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

#if 0
#define DEBUG
#define DEBUG_TEST
#endif

#if defined(J9ZTPF)
/*
 * setup USE_BSD_SELECT_PROTOTYPE so z/tpf's sys/socket.h gives us the *right* select() api.
*/
#include <tpf/icstk.h>
#include <tpf/c_eb0eb.h>
#define USE_BSD_SELECT_PROTOTYPE 1
#include <tpf/sysapi.h>
#include <tpf/tpfapi.h>
#include <time.h>
#endif /* defined(J9ZTPF) */

#include "j9vm_internal.h"
#include "j9pool.h"
#include "util_api.h"
#include "j9lib.h"
#include "exelib_api.h"
#include "j9.h"
#include "j9protos.h"
#include "j9consts.h"
#include "fltconst.h"
#include "j9comp.h"
#include "omrthread.h"
#include "j9cp.h"
#include "j2sever.h"
#include "j2senls.h"
#include "hashtable_api.h"
#include "rommeth.h"
#include "util_api.h"
#include "vmargs_api.h"
#include "mmhook.h"
#include "jvminit.h"
#include "sunvmi_api.h"
#include "omrformatconsts.h"
#ifdef J9VM_OPT_ZIP_SUPPORT
#include "vmi.h"
#include "vmzipcachehook.h"
#endif
#if defined(J9VM_OPT_JITSERVER)
#include "jitserver_api.h"
#include "jitserver_error.h"
#endif /* J9VM_OPT_JITSERVER */

#if JAVA_SPEC_VERSION < 15
#include <assert.h>
#endif /* JAVA_SPEC_VERSION < 15 */

#if defined(AIXPPC)
#include <procinfo.h>
#include <sys/types.h>
#include <unistd.h>
#endif /* defined(AIXPPC) */

#if defined(J9ZOS390)
#include <stdlib.h>

/*
 * These offsets and constants are found in "Language Environment Vendor Interfaces", downloaded from
 * https://www.ibm.com/servers/resourcelink/svc00100.nsf/pages/zosV2R4SA380688/$file/ceev100_v2r4.pdf
 */
#if defined(J9VM_ENV_DATA64)
#define OFFSET_CAA_CEDB 0x348
#define OFFSET_CEDB_LANG 0x18
#else /* defined(J9VM_ENV_DATA64) */
#define OFFSET_CAA_CEDB 0x218
#define OFFSET_CEDB_LANG 0x10
#endif /* defined(J9VM_ENV_DATA64) */

/* An enclave data block should begin with this eye-catcher. */
#pragma convlit(suspend)
static const char cedb_eye[4] = "CEDB";
#pragma convlit(resume)

/*
 * Determine whether the mainline language is C/C++.
 */
static BOOLEAN
mainlineLanguageIsC(void)
{
	BOOLEAN result = FALSE;
	/* Locate the common anchor area. */
	const char *caa = (const char *)_gtca();
	/* Fetch the pointer to the C/C++ environment data block. */
	const char *cedb = *(const char * const *)(caa + OFFSET_CAA_CEDB);

	/* Do some basic checking of the cedb pointer. */
	if ((NULL != cedb) && (0 == memcmp(cedb, cedb_eye, sizeof(cedb_eye)))) {
		int16_t language = *(const int16_t *)(cedb + OFFSET_CEDB_LANG);

		/* The code for C/C++ is 3. */
		if (3 == language) {
			result = TRUE;
		}
	}

	return result;
}

struct arg_string
{
	uint32_t length; /* length of value, including NUL terminator */
	char value[];
};

struct arg_list
{
	uint32_t argc; /* number of command-line arguments */
	uint32_t env_size; /* number of environment variables */
	const struct arg_string *string[];
};
#endif /* defined(J9ZOS390) */

#if defined(OSX)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif /* defined(OSX) */

#if defined(WIN32)
#include <processenv.h>
#include <stdlib.h>
#endif /* defined(WIN32) */

/* Must include this after j9vm_internal.h */
#include <string.h>

#define _UTE_STATIC_
#include "ut_j9scar.h"

#ifdef J9OS_I5
#include "Xj9I5OSDebug.H"
#include "Xj9I5OSInterface.H"
#endif

#if defined(DEBUG)
#define DBG_MSG(x) printf x
#else
#define DBG_MSG(x)
#endif

#if defined(LINUX) && !defined(J9VM_ENV_DATA64)
#include <unistd.h>
/* glibc 2.4 and on provide lseek64(), do not use the syscall hack */
#if !__GLIBC_PREREQ(2,4)
/* Support code for CMVC 104382. */
#include <linux/unistd.h>
_syscall5(int, _llseek, uint, fd, ulong, hi, ulong, lo, loff_t *, res, uint, wh);
#endif
#endif

#ifdef WIN32
#define IBM_MALLOCTRACE_STR L"IBM_MALLOCTRACE"
#define ALT_JAVA_HOME_DIR_STR L"_ALT_JAVA_HOME_DIR"
#else /* WIN32 */
#define IBM_MALLOCTRACE_STR "IBM_MALLOCTRACE"
#endif /* WIN32 */

#define MIN_GROWTH 128
/* DIR_SEPARATOR is defined in j9comp.h */
#define jclSeparator DIR_SEPARATOR

#define CREATE_JAVA_VM_ENTRYPOINT "J9_CreateJavaVM"
#define GET_JAVA_VMS_ENTRYPOINT "J9_GetCreatedJavaVMs"

typedef jint (JNICALL *CreateVM)(JavaVM ** p_vm, void ** p_env, J9CreateJavaVMParams *createparams);
typedef jint (JNICALL *GetVMs)(JavaVM**, jsize, jsize*);
typedef jint (JNICALL *DetachThread)(JavaVM *);

typedef jint (JNICALL *DestroyVM)(JavaVM *);
typedef int (*SigAction) (int signum, const struct sigaction *act, struct sigaction *oldact);

#ifdef J9ZOS390
typedef jint (JNICALL *pGetStringPlatform)(JNIEnv*, jstring, char*, jint, const char *);
typedef jint (JNICALL *pGetStringPlatformLength)(JNIEnv*, jstring, jint *, const char *);
typedef jint (JNICALL *pNewStringPlatform)(JNIEnv*, const char*, jstring *, const char *);
typedef jint (JNICALL *p_a2e_vsprintf)(char *, const char *, va_list);
#endif

typedef UDATA* (*ThreadGlobal)(const char* name);
typedef IDATA (*ThreadAttachEx)(omrthread_t *handle, omrthread_attr_t *attr);
typedef void (*ThreadDetach)(omrthread_t thread);
typedef IDATA (*MonitorEnter)(omrthread_monitor_t monitor);
typedef IDATA (*MonitorExit)(omrthread_monitor_t monitor);
typedef IDATA (*MonitorInit)(omrthread_monitor_t* handle, UDATA flags, char* name);
typedef IDATA (*MonitorDestroy)(omrthread_monitor_t monitor);
typedef IDATA (* ThreadLibControl)(const char * key, UDATA value);
typedef IDATA (*SetCategory)(omrthread_t thread, UDATA category, UDATA type);
typedef IDATA (*LibEnableCPUMonitor)(omrthread_t thread);

typedef I_32 (*PortInitLibrary)(J9PortLibrary *portLib, J9PortLibraryVersion *version, UDATA size);
typedef UDATA (*PortGetSize)(struct J9PortLibraryVersion *version);
typedef I_32 (*PortGetVersion)(struct J9PortLibrary *portLibrary, struct J9PortLibraryVersion *version);

typedef void* (*J9GetInterface)(J9_INTERFACE_SELECTOR interfaceSelector, J9PortLibrary* portLib, void *userData);

#if defined(J9ZOS390)
#define ZOS_THR_WEIGHT_NOT_FOUND   ((UDATA) 0U)
#define ZOS_THR_WEIGHT_MEDIUM      ((UDATA) 1U)
#define ZOS_THR_WEIGHT_HEAVY       ((UDATA) 2U)

static UDATA checkZOSThrWeightEnvVar(void);
static BOOLEAN setZOSThrWeight(void);
#endif /* J9ZOS390 */

#ifdef WIN32
#define J9_MAX_ENV 32767
#define J9_MAX_PATH _MAX_PATH
static HINSTANCE j9vm_dllHandle = (HINSTANCE) 0;
static HINSTANCE thread_dllHandle = (HINSTANCE) 0;
static HANDLE jvm_dllHandle;
#else /* WIN32 */
#define J9_MAX_PATH PATH_MAX
static void *j9vm_dllHandle = NULL;
static void *thread_dllHandle = NULL;
#endif /* WIN32 */
static J9StringBuffer * j9binBuffer = NULL;
static J9StringBuffer * jrebinBuffer = NULL;
static J9StringBuffer * j9libBuffer = NULL;
static J9StringBuffer * j9libvmBuffer = NULL;
static J9StringBuffer * j9Buffer = NULL;
static BOOLEAN jvmInSubdir=TRUE;

static CreateVM globalCreateVM;
static GetVMs globalGetVMs;

static J9GetInterface f_j9_GetInterface = NULL;

static DestroyVM globalDestroyVM;

#ifdef J9ZOS390
static pGetStringPlatform globalGetStringPlatform;
static pGetStringPlatformLength globalGetStringPlatformLength;
static pNewStringPlatform globalNewStringPlatform;
static p_a2e_vsprintf global_a2e_vsprintf;
#endif

static ThreadGlobal f_threadGlobal;
static ThreadAttachEx f_threadAttachEx;
static ThreadDetach f_threadDetach;
/* Share these with the other *.c in the DLL */
MonitorEnter f_monitorEnter;
MonitorExit f_monitorExit;
static MonitorInit f_monitorInit;
static MonitorDestroy f_monitorDestroy;
static PortInitLibrary portInitLibrary;
static PortGetSize portGetSizeFn;
static PortGetVersion portGetVersionFn;
static ThreadLibControl f_threadLibControl;
static SetCategory f_setCategory;
static LibEnableCPUMonitor f_libEnableCPUMonitor;

static UDATA monitor = 0;
static J9InternalVMFunctions globalInvokeInterface;

static struct J9PortLibrary j9portLibrary;
static char * newPath = NULL;

/* cannot be static because it's declared extern in vmi.c */
J9JavaVM *BFUjavaVM = NULL;

static jclass jlClass = NULL;
#ifdef J9VM_IVE_RAW_BUILD
static jfieldID classNameFID = NULL;
#endif /* J9VM_IVE_RAW_BUILD */
static jmethodID classDepthMID = NULL;
static jmethodID classLoaderDepthMID = NULL;
static jmethodID currentClassLoaderMID = NULL;
static jmethodID currentLoadedClassMID = NULL;
static jmethodID getNameMID = NULL;

static jclass jlThread = NULL;
#if JAVA_SPEC_VERSION < 21
static jmethodID sleepMID = NULL;
#else /* JAVA_SPEC_VERSION < 21 */
static jmethodID sleepNanosMID = NULL;
#endif /* JAVA_SPEC_VERSION < 21 */

static jmethodID waitMID = NULL;
static jmethodID notifyMID = NULL;
static jmethodID notifyAllMID = NULL;

static UDATA jvmSEVersion = -1;

static void *omrsigDLL = NULL;

static void addToLibpath(const char *, BOOLEAN isPrepend);

/*
 * PMR56610: allow CL to permanently add directories to LIBPATH during JVM initialization,
 * while still removing directories added by the VM
 */
#if defined(AIXPPC)
typedef struct J9LibpathBackup {
	char *fullpath; /* the whole libpath */
	size_t j9prefixLen; /* length of the j9 prefix, including trailing ':' when needed */
} J9LibpathBackup;

static J9LibpathBackup libpathBackup = { NULL, 0 };

static void backupLibpath(J9LibpathBackup *, size_t origLibpathLen);
static void freeBackupLibpath(J9LibpathBackup *);
static void restoreLibpath(J9LibpathBackup *);
static const char *findInLibpath(const char *libpath, const char *path);
static char *deleteDirsFromLibpath(const char *const libpath, const char *const deleteStart, const size_t deleteLen);
static void setLibpath(const char *libpath);

#ifdef DEBUG_TEST
static void testBackupAndRestoreLibpath(void);
#endif /* DEBUG_TEST */
#endif  /* AIXPPC */

/* Defined in j9memcategories.c */
extern OMRMemCategorySet j9MainMemCategorySet;
extern void resetThreadCategories(void);

void exitHook(J9JavaVM *vm);
static jint JNI_CreateJavaVM_impl(JavaVM **pvm, void **penv, void *vm_args, BOOLEAN isJITServer);
static void* preloadLibrary(char* dllName, BOOLEAN inJVMDir);
static UDATA protectedStrerror(J9PortLibrary* portLib, void* savedErrno);
static UDATA strerrorSignalHandler(struct J9PortLibrary* portLibrary, U_32 gpType, void* gpInfo, void* userData);
static void throwNewUnsatisfiedLinkError(JNIEnv *env, char *message);
static int findDirContainingFile(J9StringBuffer **result, char *paths, char pathSeparator, char *fileToFind, int elementsToSkip);
static int findDirUplevelToDirContainingFile(J9StringBuffer **result, char *pathEnvar, char pathSeparator, char *fileInPath, int upLevels, int elementsToSkip);
static void truncatePath(char *inputPath);

#if defined(WIN32)
static jint formatErrorMessage(int errorCode, char *inBuffer, jint inBufferLength);
#endif

#if defined(J9VM_OPT_JITSERVER)
static int32_t startJITServer(struct JITServer *);
static int32_t waitForJITServerTermination(struct JITServer *);
static int32_t destroyJITServer(struct JITServer **);
#endif /* J9VM_OPT_JITSERVER */

/**
 * Preload the VM, Thread, and Port libraries using platform-specific
 * shared library functions, and modifying the process environment as
 * required.
 * @return TRUE on success, FALSE otherwise.
 */
static BOOLEAN preloadLibraries(void);

/**
 * Check if the libraries have been preloaded
 * @return TRUE if libraries have been loaded, false otherwise
 */
static BOOLEAN librariesLoaded(void);

#if defined(WIN32) || defined(WIN64)
#define strdup _strdup
#define write _write
#define close _close
#define read _read
#define open _open
#endif

#if defined(J9ZTPF)
#define LD_ENV_PATH "LD_LIBRARY_PATH"
#elif defined(J9ZOS390)
#define LD_ENV_PATH "LIBPATH"
#endif /* defined(J9ZTPF) */

#define J9_SIG_ERR -1
#define J9_SIG_IGNORED 1

#define J9_PRE_DEFINED_HANDLER_CHECK 2
#define J9_USE_OLD_JAVA_SIGNAL_HANDLER 2

#define J9_SIG_PREFIX "SIG"

/* Chosen based upon signal names listed in signalMap below. */
#define J9_SIGNAME_BUFFER_LENGTH 16

static void dummySignalHandler(jint sigNum);
static BOOLEAN isSignalUsedForShutdown(jint sigNum);
static BOOLEAN isSignalReservedByJVM(jint sigNum);

typedef struct {
	const char *signalName;
	jint signalValue;
} J9SignalMapping;

#if defined(WIN32)
#define J9_SIGNAL_MAP_ENTRY(name, value) { name, value }
#else /* defined(WIN32) */
#define J9_SIGNAL_MAP_ENTRY(name, value) { "SIG" name, value }
#endif /* defined(WIN32) */

static const J9SignalMapping signalMap[] = {
#if defined(SIGABRT)
	J9_SIGNAL_MAP_ENTRY("ABRT", SIGABRT),
#endif /* defined(SIGABRT) */
#if defined(SIGALRM)
	J9_SIGNAL_MAP_ENTRY("ALRM", SIGALRM),
#endif /* defined(SIGALRM) */
#if defined(SIGBREAK)
	J9_SIGNAL_MAP_ENTRY("BREAK", SIGBREAK),
#endif /* defined(SIGBREAK) */
#if defined(SIGBUS)
	J9_SIGNAL_MAP_ENTRY("BUS", SIGBUS),
#endif /* defined(SIGBUS) */
#if defined(SIGCHLD)
	J9_SIGNAL_MAP_ENTRY("CHLD", SIGCHLD),
#endif /* defined(SIGCHLD) */
#if defined(SIGCONT)
	J9_SIGNAL_MAP_ENTRY("CONT", SIGCONT),
#endif /* defined(SIGCONT) */
#if defined(SIGFPE)
	J9_SIGNAL_MAP_ENTRY("FPE", SIGFPE),
#endif /* defined(SIGFPE) */
#if defined(SIGHUP)
	J9_SIGNAL_MAP_ENTRY("HUP", SIGHUP),
#endif /* defined(SIGHUP) */
#if defined(SIGILL)
	J9_SIGNAL_MAP_ENTRY("ILL", SIGILL),
#endif /* defined(SIGILL) */
#if defined(SIGINT)
	J9_SIGNAL_MAP_ENTRY("INT", SIGINT),
#endif /* defined(SIGINT) */
#if defined(SIGIO)
	J9_SIGNAL_MAP_ENTRY("IO", SIGIO),
#endif /* defined(SIGIO) */
#if defined(SIGPIPE)
	J9_SIGNAL_MAP_ENTRY("PIPE", SIGPIPE),
#endif /* defined(SIGPIPE) */
#if defined(SIGPROF)
	J9_SIGNAL_MAP_ENTRY("PROF", SIGPROF),
#endif /* defined(SIGPROF) */
#if defined(SIGQUIT)
	J9_SIGNAL_MAP_ENTRY("QUIT", SIGQUIT),
#endif /* defined(SIGQUIT) */
#if defined(SIGSEGV)
	J9_SIGNAL_MAP_ENTRY("SEGV", SIGSEGV),
#endif /* defined(SIGSEGV) */
#if defined(SIGSYS)
	J9_SIGNAL_MAP_ENTRY("SYS", SIGSYS),
#endif /* defined(SIGSYS) */
#if defined(SIGTERM)
	J9_SIGNAL_MAP_ENTRY("TERM", SIGTERM),
#endif /* defined(SIGTERM) */
#if defined(SIGTRAP)
	J9_SIGNAL_MAP_ENTRY("TRAP", SIGTRAP),
#endif /* defined(SIGTRAP) */
#if defined(SIGTSTP)
	J9_SIGNAL_MAP_ENTRY("TSTP", SIGTSTP),
#endif /* defined(SIGTSTP) */
#if defined(SIGTTIN)
	J9_SIGNAL_MAP_ENTRY("TTIN", SIGTTIN),
#endif /* defined(SIGTTIN) */
#if defined(SIGTTOU)
	J9_SIGNAL_MAP_ENTRY("TTOU", SIGTTOU),
#endif /* defined(SIGTTOU) */
#if defined(SIGURG)
	J9_SIGNAL_MAP_ENTRY("URG", SIGURG),
#endif /* defined(SIGURG) */
#if defined(SIGUSR1)
	J9_SIGNAL_MAP_ENTRY("USR1", SIGUSR1),
#endif /* defined(SIGUSR1) */
#if defined(SIGUSR2)
	J9_SIGNAL_MAP_ENTRY("USR2", SIGUSR2),
#endif /* defined(SIGUSR2) */
#if defined(SIGVTALRM)
	J9_SIGNAL_MAP_ENTRY("VTALRM", SIGVTALRM),
#endif /* defined(SIGVTALRM) */
#if defined(SIGWINCH)
	J9_SIGNAL_MAP_ENTRY("WINCH", SIGWINCH),
#endif /* defined(SIGWINCH) */
#if defined(SIGXCPU)
	J9_SIGNAL_MAP_ENTRY("XCPU", SIGXCPU),
#endif /* defined(SIGXCPU) */
#if defined(SIGXFSZ)
	J9_SIGNAL_MAP_ENTRY("XFSZ", SIGXFSZ),
#endif /* defined(SIGXFSZ) */
#if defined(SIGRECONFIG)
	J9_SIGNAL_MAP_ENTRY("RECONFIG", SIGRECONFIG),
#endif /* defined(SIGRECONFIG) */
#if defined(SIGKILL)
	J9_SIGNAL_MAP_ENTRY("KILL", SIGKILL),
#endif /* defined(SIGKILL) */
#if defined(SIGSTOP)
	J9_SIGNAL_MAP_ENTRY("STOP", SIGSTOP),
#endif /* defined(SIGSTOP) */
#if defined(SIGINFO)
	J9_SIGNAL_MAP_ENTRY("INFO", SIGINFO),
#endif /* defined(SIGINFO) */
#if defined(SIGIOT)
	J9_SIGNAL_MAP_ENTRY("IOT", SIGIOT),
#endif /* defined(SIGIOT) */
#if defined(SIGPOLL)
	J9_SIGNAL_MAP_ENTRY("POLL", SIGPOLL),
#endif /* defined(SIGPOLL) */
	{NULL, J9_SIG_ERR}
};

/*
 * Attempt to update or remove the value of OPENJ9_JAVA_COMMAND_LINE in the
 * environment. Passing NULL is equivalent to providing an empty string which
 * indicates that the environment variable should be removed. The function
 * returns whether the change was successfully applied.
 *
 * @param value the value to be stored; "" or NULL if the variable should be removed
 *
 * @return TRUE if successful, FALSE otherwise
 */
#if defined(WIN32)
static BOOLEAN
storeCommandLine(const wchar_t *value)
{
	return 0 == _wputenv_s(L"OPENJ9_JAVA_COMMAND_LINE", (NULL != value) ? value : L"");
}
#else /* defined(WIN32) */
static BOOLEAN
storeCommandLine(const char *value)
{
#if defined(J9ZOS390)
#pragma convlit(suspend)
#endif /* defined(J9ZOS390) */
	return 0 == setenv("OPENJ9_JAVA_COMMAND_LINE", (NULL != value) ? value : "", 1 /* overwrite */);
#if defined(J9ZOS390)
#pragma convlit(resume)
#endif /* defined(J9ZOS390) */
}
#endif /* defined(WIN32) */

/*
 * Attempt to capture the command line of this process in the environment
 * variable 'OPENJ9_JAVA_COMMAND_LINE'. If the command line is not available,
 * try to remove that variable to avoid possibly incorrect information.
 */
static void
captureCommandLine(void)
{
	BOOLEAN captured = FALSE;

#if defined(AIXPPC)
	long int bufferSize = sysconf(_SC_ARG_MAX);

	if (bufferSize > 0) {
		char *buffer = malloc(bufferSize);

		if (NULL != buffer) {
#if defined(J9VM_ENV_DATA64)
			struct procsinfo64 info;
#else /* defined(J9VM_ENV_DATA64) */
			struct procsinfo info;
#endif /* defined(J9VM_ENV_DATA64) */

			memset(&info, '\0', sizeof(info));
			info.pi_pid = getpid();

			if (0 == getargs(&info, sizeof(info), buffer, bufferSize)) {
				char *cursor = buffer;

				/* replace the internal NULs with spaces */
				for (;; ++cursor) {
					if ('\0' == *cursor) {
						if ('\0' == cursor[1]) {
							/* the list ends with two NUL characters */
							break;
						}
						*cursor = ' ';
					}
				}

				captured = storeCommandLine(buffer);
			}

			free(buffer);
		}
	}
#elif defined(J9ZOS390) /* defined(AIXPPC) */
#pragma convlit(suspend)
	/*
	 * First, make sure the mainline language is C/C++ which uses parameter passing
	 * style 3 as described by [1]; other mainlines may use different styles.
	 *
	 * [1] https://www.ibm.com/docs/en/zos/2.4.0?topic=formats-c-c-parameter-passing-considerations
	 */
	if (mainlineLanguageIsC()) {
		void *plist = __osplist;
		if (NULL != plist) {
			const struct arg_list *args = *(const struct arg_list **)plist;
			uint32_t argc = args->argc;
			uint32_t i = 0;
			size_t length = 0;
			char *buffer = NULL;

			for (i = 0; i < argc; ++i) {
				length += args->string[i]->length;
			}

			buffer = malloc(length);
			if (NULL != buffer) {
				char *cursor = buffer;

				for (i = 0; i < argc; ++i) {
					const struct arg_string *arg = args->string[i];
					if (0 != i) {
						*cursor = ' ';
						cursor += 1;
					}
					memcpy(cursor, arg->value, arg->length - 1);
					cursor += arg->length - 1;
				}

				*cursor = '\0';

				captured = storeCommandLine(buffer);
				free(buffer);
			}
		}
	}
#pragma convlit(resume)
#elif defined(LINUX) /* defined(AIXPPC) */
	int fd = open("/proc/self/cmdline", O_RDONLY, 0);

	if (fd >= 0) {
		char *buffer = NULL;
		size_t length = 0;
		for (;;) {
			char small_buffer[512];
			ssize_t count = read(fd, small_buffer, sizeof(small_buffer));
			if (count <= 0) {
				break;
			}
			length += (size_t)count;
		}
		if (length < 2) {
			goto done;
		}
		/* final NUL is already included in length */
		buffer = malloc(length);
		if (NULL == buffer) {
			goto done;
		}
		if ((off_t)-1 == lseek(fd, 0, SEEK_SET)) {
			goto done;
		}
		if (read(fd, buffer, length) != length) {
			goto done;
		}
		/* replace the internal NULs with spaces */
		for (length -= 2;; length -= 1) {
			if (0 == length) {
				break;
			}
			if ('\0' == buffer[length]) {
				buffer[length] = ' ';
			}
		}
		captured = storeCommandLine(buffer);
done:
		if (NULL != buffer) {
			free(buffer);
		}
		close(fd);
	}
#elif defined(OSX) /* defined(AIXPPC) */
	int argmax = 0;
	size_t length = 0;
	int mib[3];

	/* query the argument space limit */
	mib[0] = CTL_KERN;
	mib[1] = KERN_ARGMAX;
	length = sizeof(argmax);
	if (0 == sysctl(mib, 2, &argmax, &length, NULL, 0)) {
		char *buffer = malloc(argmax);

		if (NULL != buffer) {
			int argc = 0;
			int pid = getpid();

			/* query the argument count */
			mib[0] = CTL_KERN;
			mib[1] = KERN_PROCARGS2;
			mib[2] = pid;
			length = argmax;
			if ((argmax >= sizeof(argc)) && (0 == sysctl(mib, 3, buffer, &length, NULL, 0))) {
				memcpy(&argc, buffer, sizeof(argc));

				/* retrieve the arguments */
				mib[0] = CTL_KERN;
				mib[1] = KERN_PROCARGS;
				mib[2] = pid;
				length = argmax;
				if (0 == sysctl(mib, 3, buffer, &length, NULL, 0)) {
					char *cursor = buffer;
					char *start = NULL;
					char *limit = buffer + length;

					for (; (cursor < limit) && ('\0' != *cursor); ++cursor) {
						/* skip past the program path */
					}

					for (; (cursor < limit) && ('\0' == *cursor); ++cursor) {
						/* skip past the padding after the program path */
					}

					/*
					 * remember the start of the first argument (this is the beginning
					 * of argv[0] which is often the same as the path we skipped above)
					 */
					start = cursor;

					/* replace the internal NULs with spaces */
					for (; cursor < limit; ++cursor) {
						if ('\0' == *cursor) {
							argc -= 1;
							if (0 == argc) {
								break;
							}
							*cursor = ' ';
						}
					}

					captured = storeCommandLine(start);
				}
			}

			free(buffer);
		}
	}
#elif defined(WIN32) /* defined(OSX) */
	const wchar_t *commandLine = GetCommandLineW();

	if (NULL != commandLine) {
		captured = storeCommandLine(commandLine);
	}
#endif /* defined(AIXPPC) */

	if (!captured) {
		/*
		 * If we were unable to find the command line or store it in the
		 * environment, try to remove any existing value (which is unlikely
		 * to be correct). It's not fatal if this fails, so don't bother
		 * checking.
		 */
		storeCommandLine(NULL);
	}
}

static void freeGlobals(void)
{
	free(newPath);
	newPath = NULL;

	free(j9binBuffer);
	j9binBuffer = NULL;

	free(jrebinBuffer);
	jrebinBuffer = NULL;

	free(j9libBuffer);
	j9libBuffer = NULL;

	free(j9libvmBuffer);
	j9libvmBuffer = NULL;

	free(j9Buffer);
	j9Buffer = NULL;

	resetThreadCategories();
}

static J9StringBuffer* jvmBufferEnsure(J9StringBuffer* buffer, UDATA len) {

	if (buffer == NULL) {
		UDATA newSize = len > MIN_GROWTH ? len : MIN_GROWTH;
		buffer = (J9StringBuffer*) malloc( newSize + 1 + sizeof(UDATA));	/* 1 for null terminator */
		if (buffer != NULL) {
			buffer->remaining = newSize;
			buffer->data[0] = '\0';
		}
		return buffer;
	}

	if (len > buffer->remaining) {
		UDATA newSize = len > MIN_GROWTH ? len : MIN_GROWTH;
		J9StringBuffer* new = (J9StringBuffer*) malloc( strlen((const char*)buffer->data) + newSize + sizeof(UDATA) + 1 );
		if (new) {
			new->remaining = newSize;
			strcpy(new->data, (const char*)buffer->data);
		}
		free(buffer);
		return new;
	}

	return buffer;
}


static J9StringBuffer*
jvmBufferCat(J9StringBuffer* buffer, const char* string)
{
	UDATA len = strlen(string);

	buffer = jvmBufferEnsure(buffer, len);
	if (buffer) {
		strcat(buffer->data, string);
		buffer->remaining -= len;
	}

	return buffer;
}


static char* jvmBufferData(J9StringBuffer* buffer) {
	return buffer ? buffer->data : NULL;
}

jint JNICALL DestroyJavaVM(JavaVM * javaVM)
{
	jint rc;

	UT_MODULE_UNLOADED(J9_UTINTERFACE_FROM_VM((J9JavaVM*)javaVM));
	rc = globalDestroyVM(javaVM);

	if (JNI_OK == rc) {
		if (NULL != j9portLibrary.port_shutdown_library) {
			j9portLibrary.port_shutdown_library(&j9portLibrary);
		}

		freeGlobals();

#ifdef WIN32
		FreeLibrary(j9vm_dllHandle);
		FreeLibrary(thread_dllHandle);
#else /* WIN32 */
		dlclose(j9vm_dllHandle);
		dlclose(thread_dllHandle);
#endif /* WIN32 */
		j9vm_dllHandle = 0;
		thread_dllHandle = 0;

		BFUjavaVM = NULL;
	} else {
		/* We are not shutting down the  port library but we still
		 * need to make sure memcheck gets a chance to print its
		 * report.
		 */
		memoryCheck_print_report(&j9portLibrary);
	}

	return rc;
}

/**
 * Check if the libraries have been preloaded
 * @return TRUE if libraries have been loaded, false otherwise
 */
static BOOLEAN
librariesLoaded(void)
{
	if ((NULL == globalCreateVM) || (NULL == globalGetVMs)) {
		return FALSE;
	}
	return TRUE;
}

#ifdef WIN32

BOOL APIENTRY DllMain(HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	BOOL rcDllMain = TRUE;
	jvm_dllHandle = hModule;

	if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
		/* Disable DLL_THREAD_ATTACH and DLL_THREAD_DETACH notifications for WIN32*/
		DisableThreadLibraryCalls(hModule);
	}

	return rcDllMain;
}


static BOOLEAN
preloadLibraries(void)
{
	char* tempchar = 0;
	char jvmDLLName[J9_MAX_PATH];
	J9StringBuffer * jvmDLLNameBuffer = NULL;
	wchar_t unicodeDLLName[J9_MAX_PATH + 1] = {0}; /*extra character is added to check if path is truncated by GetModuleFileNameW()*/
	DWORD unicodeDLLNameLength = 0;
	UINT prevMode;
	HINSTANCE vmDLL = 0;
	HINSTANCE threadDLL = 0;
	HINSTANCE portDLL = 0;
	char* vmDllName = J9_VM_DLL_NAME;
	char* vmName = NULL;
	static beenRun=FALSE;
	if(beenRun) {
		return TRUE;
	}
	beenRun = TRUE;

	unicodeDLLNameLength = GetModuleFileNameW(jvm_dllHandle, unicodeDLLName, (J9_MAX_PATH + 1));
	/* Don't use truncated path */
	if (unicodeDLLNameLength > (DWORD)J9_MAX_PATH) {
		fprintf(stderr,"ERROR: failed to load library. Path is too long: %ls\n", unicodeDLLName);
		return FALSE;
	}

	/* Use of jvmDLLName is safe as the length is passed in */
	WideCharToMultiByte(OS_ENCODING_CODE_PAGE, OS_ENCODING_WC_FLAGS, unicodeDLLName, -1,  jvmDLLName, J9_MAX_PATH, NULL, NULL);

	jvmDLLNameBuffer = jvmBufferCat(NULL, jvmDLLName);
	j9binBuffer = jvmBufferCat(j9binBuffer, jvmDLLName);

	/* detect if we're in a subdir or not - is the j9vm dll in the subdir? */
	truncatePath(jvmBufferData(jvmDLLNameBuffer));
	jvmDLLNameBuffer = jvmBufferCat(jvmDLLNameBuffer, "\\");
	jvmDLLNameBuffer = jvmBufferCat(jvmDLLNameBuffer, vmDllName);
	jvmDLLNameBuffer = jvmBufferCat(jvmDLLNameBuffer, ".dll");

	/* Last argument tells Windows the size of the buffer */
	MultiByteToWideChar(OS_ENCODING_CODE_PAGE, OS_ENCODING_MB_FLAGS, jvmBufferData(jvmDLLNameBuffer), -1, unicodeDLLName, J9_MAX_PATH);
	if(INVALID_FILE_ATTRIBUTES != GetFileAttributesW(unicodeDLLName)) {
		jvmInSubdir = TRUE;
	} else {
		jvmInSubdir = FALSE;
	}

	prevMode = SetErrorMode( SEM_NOOPENFILEERRORBOX|SEM_FAILCRITICALERRORS );

	free(jvmDLLNameBuffer);
	jvmDLLNameBuffer = NULL;
	free(jrebinBuffer);
	jrebinBuffer = NULL;

	if(jvmInSubdir) {
		/* truncate the \<my vm name>\jvm.dll from the string */
		truncatePath(jvmBufferData(j9binBuffer));
		jrebinBuffer = jvmBufferCat(NULL, jvmBufferData(j9binBuffer));
		truncatePath(jvmBufferData(jrebinBuffer));
	} else {
		/* in this config, j9binBuffer and jrebinBuffer point to the same place - jre/bin/ */
		truncatePath(jvmBufferData(j9binBuffer));
		truncatePath(jvmBufferData(j9binBuffer));
		jrebinBuffer = jvmBufferCat(NULL, jvmBufferData(j9binBuffer));
	}

	j9libBuffer = jvmBufferCat(NULL, jvmBufferData(jrebinBuffer));
	truncatePath(jvmBufferData(j9libBuffer));
	j9Buffer = jvmBufferCat(NULL, jvmBufferData(j9libBuffer));
	truncatePath(jvmBufferData(j9Buffer));
	j9libBuffer = jvmBufferCat(j9libBuffer, DIR_SEPARATOR_STR "lib");

	j9libvmBuffer = jvmBufferCat(NULL, jvmBufferData(j9libBuffer));
	/* append /<vm name> to j9libBuffer to get location of j9ddr.dat file */
	vmName = strrchr(jvmBufferData(j9binBuffer), DIR_SEPARATOR);
	if (NULL != vmName) {
		j9libvmBuffer = jvmBufferCat(j9libvmBuffer, vmName);
	}

	DBG_MSG(("j9binBuffer   = <%s>\n", jvmBufferData(j9binBuffer)));
	DBG_MSG(("jrebinBuffer  = <%s>\n", jvmBufferData(jrebinBuffer)));
	DBG_MSG(("j9libBuffer   = <%s>\n", jvmBufferData(j9libBuffer)));
	DBG_MSG(("j9libvmBuffer = <%s>\n", jvmBufferData(j9libvmBuffer)));
	DBG_MSG(("j9Buffer      = <%s>\n", jvmBufferData(j9Buffer)));

	vmDLL = (HINSTANCE) preloadLibrary(vmDllName, TRUE);
	preloadLibrary(J9_HOOKABLE_DLL_NAME, TRUE);

	SetErrorMode( prevMode );

	if (vmDLL < (HINSTANCE)HINSTANCE_ERROR) {
		fprintf(stderr,"jvm.dll failed to load: %s\n", vmDllName);
		return FALSE;
	}
	globalCreateVM = (CreateVM) GetProcAddress (vmDLL, (LPCSTR) CREATE_JAVA_VM_ENTRYPOINT );
	globalGetVMs = (GetVMs) GetProcAddress (vmDLL, (LPCSTR) GET_JAVA_VMS_ENTRYPOINT);
	if (!globalCreateVM || !globalGetVMs) {
		FreeLibrary(vmDLL);
		fprintf(stderr,"jvm.dll failed to load: global entrypoints not found\n");
		return FALSE;
	}
	j9vm_dllHandle = vmDLL;

	threadDLL = (HINSTANCE) preloadLibrary(J9_THREAD_DLL_NAME, TRUE);
	f_threadGlobal = (ThreadGlobal) GetProcAddress (threadDLL, (LPCSTR) "omrthread_global");
	f_threadAttachEx = (ThreadAttachEx) GetProcAddress (threadDLL, (LPCSTR) "omrthread_attach_ex");
	f_threadDetach = (ThreadDetach) GetProcAddress (threadDLL, (LPCSTR) "omrthread_detach");
	f_monitorEnter = (MonitorEnter) GetProcAddress (threadDLL, (LPCSTR) "omrthread_monitor_enter");
	f_monitorExit = (MonitorExit) GetProcAddress (threadDLL, (LPCSTR) "omrthread_monitor_exit");
	f_monitorInit = (MonitorInit) GetProcAddress (threadDLL, (LPCSTR) "omrthread_monitor_init_with_name");
	f_monitorDestroy = (MonitorDestroy) GetProcAddress (threadDLL, (LPCSTR) "omrthread_monitor_destroy");
	f_threadLibControl = (ThreadLibControl) GetProcAddress (threadDLL, (LPCSTR) "omrthread_lib_control");
	f_setCategory = (SetCategory) GetProcAddress (threadDLL, (LPCSTR) "omrthread_set_category");
	f_libEnableCPUMonitor = (LibEnableCPUMonitor) GetProcAddress (threadDLL, (LPCSTR) "omrthread_lib_enable_cpu_monitor");
	if (!f_threadGlobal || !f_threadAttachEx || !f_threadDetach || !f_monitorEnter || !f_monitorExit || !f_monitorInit
		|| !f_monitorDestroy || !f_threadLibControl || !f_setCategory || !f_libEnableCPUMonitor
	) {
		FreeLibrary(vmDLL);
		FreeLibrary(threadDLL);
		fprintf(stderr,"jvm.dll failed to load: thread library entrypoints not found\n");
		return FALSE;
	}
	thread_dllHandle = threadDLL;

	/* pre-load port library for memorycheck */
	portDLL = (HINSTANCE) preloadLibrary(J9_PORT_DLL_NAME, TRUE);
	portInitLibrary = (PortInitLibrary) GetProcAddress (portDLL, (LPCSTR) "j9port_init_library");
	portGetSizeFn = (PortGetSize) GetProcAddress (portDLL, (LPCSTR) "j9port_getSize");
	portGetVersionFn = (PortGetVersion) GetProcAddress (portDLL, (LPCSTR) "j9port_getVersion");
	if (!portInitLibrary) {
		FreeLibrary(vmDLL);
		FreeLibrary(threadDLL);
		FreeLibrary(portDLL);
		fprintf(stderr,"jvm.dll failed to load: %s entrypoints not found\n", J9_PORT_DLL_NAME);
		return FALSE;
	}
	preloadLibrary(J9_ZIP_DLL_NAME, TRUE);

	/* CMVC 152702: with other JVM on the path this library can get loaded from the wrong
	 * location if not preloaded. */
	return TRUE;
}

static void
bfuThreadStart(J9HookInterface** vmHooks, UDATA eventNum, void* eventData, void* userData)
{
	J9VMThreadCreatedEvent* event = eventData;
	if (event->continueInitialization) {
		HANDLE win32Event = CreateEvent(NULL, FALSE, FALSE, NULL);

		if (win32Event == NULL) {
			/* abort the creation of this thread */
			event->continueInitialization = 0;
		} else {
			event->vmThread->sidecarEvent = win32Event;
		}
	}
}


static void
bfuThreadEnd(J9HookInterface** vmHooks, UDATA eventNum, void* eventData, void* userData)
{
	J9VMThread* vmThread = ((J9VMThreadDestroyEvent*)eventData)->vmThread;
	HANDLE event = (HANDLE) vmThread->sidecarEvent;

	if (event) {
		CloseHandle(event);
		vmThread->sidecarEvent = NULL;
	}
}


static void bfuInterrupt(J9VMThread * vmThread)
{
	HANDLE event = (HANDLE) vmThread->sidecarEvent;

	if (event) {
		SetEvent(event);
	}
}

static void bfuClearInterrupt(J9VMThread * vmThread)
{
	HANDLE event = (HANDLE) vmThread->sidecarEvent;

	if (event) {
		ResetEvent(event);
	}
}

static jint
initializeWin32ThreadEvents(J9JavaVM* javaVM)
{
	J9VMThread* aThread;
	J9HookInterface** hookInterface = javaVM->internalVMFunctions->getVMHookInterface(javaVM);

	javaVM->sidecarInterruptFunction = bfuInterrupt;
	javaVM->sidecarClearInterruptFunction = bfuClearInterrupt;

	if ((*hookInterface)->J9HookRegisterWithCallSite(hookInterface, J9HOOK_VM_THREAD_CREATED, bfuThreadStart, OMR_GET_CALLSITE(), NULL)) {
		return JNI_ERR;
	}
	if ((*hookInterface)->J9HookRegisterWithCallSite(hookInterface, J9HOOK_VM_THREAD_DESTROY, bfuThreadEnd, OMR_GET_CALLSITE(), NULL)) {
		return JNI_ERR;
	}

	/* make sure that all existing threads gets an event, too */
	aThread = javaVM->mainThread;
	do {
		aThread->sidecarEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (aThread->sidecarEvent == NULL) {
			return JNI_ERR;
		}
		aThread = aThread->linkNext;
	} while (aThread != javaVM->mainThread);

	return JNI_OK;
}

#endif

#ifndef WIN32
J9StringBuffer *
getj9bin()
{
	/* misnamed - returns the directory that the jvm DLL is found in, NOT the directory that the J9 VM itself is in. */

	J9StringBuffer *result = NULL;
#if (defined(LINUX) && !defined(J9ZTPF)) || defined(OSX)
	Dl_info libraryInfo;
	int rc = dladdr((void *)getj9bin, &libraryInfo);

	if (0 == rc) {
		fprintf(stderr, "ERROR: cannot determine JAVA home directory\n");
		abort();
	}

	result = jvmBufferCat(NULL, libraryInfo.dli_fname);
	/* remove libjvm.so */
	truncatePath(jvmBufferData(result));
#elif defined(J9ZOS390) || defined(J9ZTPF)
#define VMDLL_NAME J9_VM_DLL_NAME

	int foundPosition = 0;

	/* assumes LIBPATH (or LD_LIBRARY_PATH for z/TPF) points to where all libjvm.so can be found */
	while(foundPosition = findDirUplevelToDirContainingFile(&result, LD_ENV_PATH, ':', "libjvm.so", 0, foundPosition)) {
		/* now screen to see if match is the right libjvm.so - it needs to have a j9vm DLL either in this dir, or in the parent. */
		DBG_MSG(("found a libjvm.so at offset %d - looking at elem: %s\n", foundPosition, result));

		/* first try this dir - this will be true for 'vm in subdir' cases, and is the likely Java 6 case as of SR1. */
		if (isFileInDir(jvmBufferData(result), "lib" VMDLL_NAME J9PORT_LIBRARY_SUFFIX)) {
			return result;
		}

		truncatePath(jvmBufferData(result));

		/* trying parent */
		if (isFileInDir(jvmBufferData(result), "lib" VMDLL_NAME J9PORT_LIBRARY_SUFFIX)) {
			return result;
		}
	}

	fprintf(stderr, "ERROR: cannot determine JAVA home directory\n");
	abort();

#else /* must be AIX / RS6000 */
	struct ld_info *linfo, *linfop;
	int             linfoSize, rc;
	char           *myAddress, *filename, *membername;

	/* get loader information */
	linfoSize = 1024;
	linfo = malloc(linfoSize);
	for(;;) {
		rc = loadquery(L_GETINFO, linfo, linfoSize);
		if (rc != -1) {
			break;
		}
		linfoSize *=2; /* insufficient buffer size - increase */
		linfo = realloc(linfo, linfoSize);
	}

	/* find entry for my loaded object */
	myAddress = ((char **)&getj9bin)[0];
	for (linfop = linfo;;) {
		char *textorg  = (char *)linfop->ldinfo_textorg;
		char *textend  = textorg + (unsigned long)linfop->ldinfo_textsize;
		if (myAddress >=textorg && (myAddress < textend)) {
			break;
		}
		if (!linfop->ldinfo_next) {
			abort();
		}
		linfop = (struct ld_info *)((char *)linfop + linfop->ldinfo_next);
	}

	filename   = linfop->ldinfo_filename;
	membername = filename+strlen(filename)+1;
#ifdef DEBUG
	printf("ldinfo: filename is %s. membername is %s\n",  filename, membername);
#endif

	result = jvmBufferCat(NULL, filename);
	/* remove '/libjvm.a' */
	truncatePath(jvmBufferData(result));

	free(linfo);
#endif  /* defined(LINUX) && !defined(J9ZTPF) */
	return result;
}
#endif /* ifndef WIN32*/

#if defined(RS6000) || defined(LINUXPPC)
#ifdef PPC64
#ifdef J9VM_ENV_LITTLE_ENDIAN
#define JVM_ARCH_DIR "ppc64le"
#else /* J9VM_ENV_LITTLE_ENDIAN */
#define JVM_ARCH_DIR "ppc64"
#endif /* J9VM_ENV_LITTLE_ENDIAN */
#else
#define JVM_ARCH_DIR "ppc"
#endif /* PPC64 */
#elif defined(J9X86) || defined(WIN32)
#define JVM_ARCH_DIR "i386"
#elif defined(S390) || defined(J9ZOS390)
#if defined(S39064) || defined(J9ZOS39064)
#define JVM_ARCH_DIR "s390x"
#else
#define JVM_ARCH_DIR "s390"
#endif /* S39064 || J9ZOS39064 */
#elif defined(J9HAMMER)
#define JVM_ARCH_DIR "amd64"
#elif defined(J9ARM)
#define JVM_ARCH_DIR "arm"
#elif defined(J9AARCH64)
#define JVM_ARCH_DIR "aarch64"
#elif defined(RISCV64)
#define JVM_ARCH_DIR "riscv64"
#else
#error "Must define an architecture"
#endif

/* We use forward slashes here because J9VM_LIB_ARCH_DIR is not used on Windows. */
#if (JAVA_SPEC_VERSION >= 9) || defined(OSX)
/* On OSX, <arch> doesn't exist, so JVM_ARCH_DIR shouldn't be included in J9VM_LIB_ARCH_DIR. */
#define J9VM_LIB_ARCH_DIR "/lib/"
#else /* (JAVA_SPEC_VERSION >= 9) || defined(OSX) */
#define J9VM_LIB_ARCH_DIR "/lib/" JVM_ARCH_DIR "/"
#endif /* (JAVA_SPEC_VERSION >= 9) || defined(OSX) */

#if JAVA_SPEC_VERSION == 8
/*
 * Remove the suffix from string if present.
 */
static void
removeSuffix(char *string, const char *suffix)
{
	size_t stringLength = strlen(string);
	size_t suffixLength = strlen(suffix);

	if (stringLength >= suffixLength) {
		char *tail = &string[stringLength - suffixLength];

		if (0 == strcmp(tail, suffix)) {
			*tail = '\0';
		}
	}
}
#endif /* JAVA_SPEC_VERSION == 8 */

#if defined(J9UNIX) || defined(J9ZOS390)
static BOOLEAN
preloadLibraries(void)
{
	void *vmDLL, *threadDLL, *portDLL;
	char* lastSep = 0;
	char* tempchar = 0;
	char* exeInBuf;
	J9StringBuffer * jvmDLLNameBuffer = NULL;
	char *lastDirName;
	char* vmDllName = J9_VM_DLL_NAME;
	struct stat statBuf;
#if defined(J9ZOS390)
	void *javaDLL;
#endif

#if defined(AIXPPC)
	size_t origLibpathLen = 0;
	const char *origLibpath = NULL;
#endif /* AIXPPC */

	if (j9vm_dllHandle != 0) {
		return FALSE;
	}

#if defined(J9ZOS390)
	iconv_init();
#endif

#if defined(AIXPPC)
	origLibpath = getenv("LIBPATH");
	if (NULL != origLibpath) {
		origLibpathLen = strlen(origLibpath);
	}
#endif /* AIXPPC */

	jvmDLLNameBuffer = getj9bin();
	j9binBuffer = jvmBufferCat(NULL, jvmBufferData(jvmDLLNameBuffer));

	addToLibpath(jvmBufferData(j9binBuffer), TRUE);

	/* Are we in classic? If so, point to the right arch dir. */
	lastDirName = strrchr(jvmBufferData(j9binBuffer), DIR_SEPARATOR);

	if (NULL == lastDirName) {
		fprintf(stderr, "Preload libraries failed to find a valid J9 binary location\n" );
		exit( -1 ); /* failed */
	}

	if (0 == strcmp(lastDirName + 1, "classic")) {
		truncatePath(jvmBufferData(j9binBuffer)); /* at jre/bin or jre/lib/<arch> */
		truncatePath(jvmBufferData(j9binBuffer)); /* at jre     or jre/lib        */
#if JAVA_SPEC_VERSION == 8
		/* remove /lib if present */
		removeSuffix(jvmBufferData(j9binBuffer), "/lib"); /* at jre */
#endif /* JAVA_SPEC_VERSION == 8 */
		j9binBuffer = jvmBufferCat(j9binBuffer, J9VM_LIB_ARCH_DIR "j9vm/");
		if (-1 != stat(jvmBufferData(j9binBuffer), &statBuf)) {
			/* does exist, carry on */
			jvmDLLNameBuffer = jvmBufferCat(NULL, jvmBufferData(j9binBuffer));
		} else {
			/* does not exist, rewind (likely a 5.0 build) */
			free(j9binBuffer);
			j9binBuffer = NULL;
			j9binBuffer = jvmBufferCat(NULL, jvmBufferData(jvmDLLNameBuffer));
		}
	}

	/* detect if we're in a subdir or not */
	jvmDLLNameBuffer = jvmBufferCat(jvmDLLNameBuffer, "/lib");
	jvmDLLNameBuffer = jvmBufferCat(jvmDLLNameBuffer, vmDllName);
	jvmDLLNameBuffer = jvmBufferCat(jvmDLLNameBuffer, J9PORT_LIBRARY_SUFFIX);

	if(-1 != stat (jvmBufferData(jvmDLLNameBuffer), &statBuf)) {
		jvmInSubdir = TRUE;
	} else {
		jvmInSubdir = FALSE;
	}

	DBG_MSG(("jvmInSubdir: %d\n", jvmInSubdir));

	free(jrebinBuffer);
	jrebinBuffer = NULL;

	/* set up jre bin based on result of subdir knowledge */
	if(jvmInSubdir) {
		jrebinBuffer = jvmBufferCat(NULL, jvmBufferData(j9binBuffer));
		truncatePath(jvmBufferData(jrebinBuffer));
	} else {
		truncatePath(jvmBufferData(j9binBuffer));
		jrebinBuffer = jvmBufferCat(NULL, jvmBufferData(j9binBuffer));
	}
	j9libBuffer = jvmBufferCat(NULL, jvmBufferData(jrebinBuffer));
#if !defined(OSX)
	/* <arch> directory doesn't exist on OSX so j9libBuffer shouldn't
	 * be truncated on OSX for removing <arch>.
	 */
#if JAVA_SPEC_VERSION == 8
	/* Remove <arch> */
	truncatePath(jvmBufferData(j9libBuffer));
#endif /* JAVA_SPEC_VERSION == 8 */
#endif /* !defined(OSX) */
	j9libvmBuffer = jvmBufferCat(NULL, jvmBufferData(j9binBuffer));
	j9Buffer = jvmBufferCat(NULL, jvmBufferData(jrebinBuffer));
	truncatePath(jvmBufferData(j9Buffer));

	DBG_MSG(("j9binBuffer   = <%s>\n", jvmBufferData(j9binBuffer)));
	DBG_MSG(("jrebinBuffer  = <%s>\n", jvmBufferData(jrebinBuffer)));
	DBG_MSG(("j9libBuffer   = <%s>\n", jvmBufferData(j9libBuffer)));
	DBG_MSG(("j9libvmBuffer = <%s>\n", jvmBufferData(j9libvmBuffer)));
	DBG_MSG(("j9Buffer      = <%s>\n", jvmBufferData(j9Buffer)));

	addToLibpath(jvmBufferData(jrebinBuffer), TRUE);

#if defined(AIXPPC)
	backupLibpath(&libpathBackup, origLibpathLen);
#endif /* AIXPPC */

	omrsigDLL = preloadLibrary("omrsig", TRUE);
	if (NULL == omrsigDLL) {
		fprintf(stderr, "libomrsig failed to load: omrsig\n" );
		exit( -1 ); /* failed */
	}

	vmDLL = preloadLibrary(vmDllName, TRUE);
	if (NULL == vmDLL) {
		fprintf(stderr,"libjvm.so failed to load: %s\n", vmDllName);
		exit( -1 );	/* failed */
	}

	globalCreateVM = (CreateVM) dlsym (vmDLL, CREATE_JAVA_VM_ENTRYPOINT );
	globalGetVMs = (GetVMs) dlsym (vmDLL,  GET_JAVA_VMS_ENTRYPOINT);
	if ((NULL == globalCreateVM) || (NULL == globalGetVMs)) {
		dlclose(vmDLL);
		fprintf(stderr,"libjvm.so failed to load: global entrypoints not found\n");
		exit( -1 );	/* failed */
	}
	j9vm_dllHandle = vmDLL;

#ifdef J9ZOS390
	/* pre-load libjava.so for IMBZOS functions */
	javaDLL = preloadLibrary("java", FALSE);
	if (!javaDLL) {
	   fprintf(stderr,"libjava.dll failed to load: %s\n", "java");
	   exit( -1 );     /* failed */
	}
	globalGetStringPlatform = (pGetStringPlatform) dlsym (javaDLL,  "IBMZOS_GetStringPlatform");
	globalGetStringPlatformLength = (pGetStringPlatformLength) dlsym (javaDLL,  "IBMZOS_GetStringPlatformLength");
	globalNewStringPlatform = (pNewStringPlatform) dlsym (javaDLL,  "IBMZOS_NewStringPlatform");
	global_a2e_vsprintf = (p_a2e_vsprintf) dlsym (javaDLL,  "IBMZOS_a2e_vsprintf");
	if (!globalGetStringPlatform || !globalGetStringPlatformLength || !globalNewStringPlatform || !global_a2e_vsprintf) {
	   dlclose(vmDLL);
	   dlclose(javaDLL);
	   fprintf(stderr,"libjava.dll failed to load: global entrypoints not found\n");
	   exit( -1 );     /* failed */
	}
#endif

	threadDLL = preloadLibrary(J9_THREAD_DLL_NAME, TRUE);
	f_threadGlobal = (ThreadGlobal) dlsym (threadDLL, "omrthread_global");
	f_threadAttachEx = (ThreadAttachEx) dlsym (threadDLL, "omrthread_attach_ex");
	f_threadDetach = (ThreadDetach) dlsym (threadDLL, "omrthread_detach");
	f_monitorEnter = (MonitorEnter) dlsym (threadDLL, "omrthread_monitor_enter");
	f_monitorExit = (MonitorExit) dlsym (threadDLL, "omrthread_monitor_exit");
	f_monitorInit = (MonitorInit) dlsym (threadDLL, "omrthread_monitor_init_with_name");
	f_monitorDestroy = (MonitorDestroy) dlsym (threadDLL, "omrthread_monitor_destroy");
	f_threadLibControl = (ThreadLibControl) dlsym (threadDLL, "omrthread_lib_control");
	f_setCategory = (SetCategory) dlsym (threadDLL, "omrthread_set_category");
	f_libEnableCPUMonitor = (LibEnableCPUMonitor) dlsym (threadDLL, "omrthread_lib_enable_cpu_monitor");
	if (!f_threadGlobal || !f_threadAttachEx || !f_threadDetach || !f_monitorEnter || !f_monitorExit || !f_monitorInit
		|| !f_monitorDestroy || !f_threadLibControl || !f_setCategory || !f_libEnableCPUMonitor
	) {
		dlclose(vmDLL);
#ifdef J9ZOS390
		dlclose(javaDLL);
#endif
		dlclose(threadDLL);
		fprintf(stderr,"libjvm.so failed to load: thread library entrypoints not found\n");
		exit( -1 );	/* failed */
	}
	thread_dllHandle = threadDLL;

	portDLL = preloadLibrary(J9_PORT_DLL_NAME, TRUE);
	portInitLibrary = (PortInitLibrary) dlsym (portDLL, "j9port_init_library");
	portGetSizeFn = (PortGetSize) dlsym (portDLL, "j9port_getSize");
	portGetVersionFn = (PortGetVersion) dlsym (portDLL, "j9port_getVersion");
	if (!portInitLibrary) {
		dlclose(vmDLL);
#ifdef J9ZOS390
		dlclose(javaDLL);
#endif
		dlclose(threadDLL);
		dlclose(portDLL);
		fprintf(stderr,"libjvm.so failed to load: %s entrypoints not found\n", J9_PORT_DLL_NAME);
		exit( -1 );	/* failed */
	}

	return TRUE;
}
#endif /* defined(J9UNIX) || defined(J9ZOS390) */




int jio_fprintf(FILE * stream, const char * format, ...) {
	va_list args;

	Trc_SC_fprintf_Entry();

	va_start( args, format );
	vfprintf(stream, format, args);
	va_end( args );

	Trc_SC_fprintf_Exit(0);

	return 0;
}



int
jio_snprintf(char * str, int n, const char * format, ...)
{
	va_list args;
	int result;

	Trc_SC_snprintf_Entry();

	va_start(args, format);
	result = vsnprintf( str, n, format, args );
	va_end(args);

	Trc_SC_snprintf_Exit(result);

	return result;

}



int
jio_vfprintf(FILE * stream, const char * format, va_list args)
{

	Trc_SC_vfprintf_Entry(stream, format);

	vfprintf(stream, format, args);

	Trc_SC_vfprintf_Exit(0);

	return 0;
}



int
jio_vsnprintf(char * str, int n, const char * format, va_list args)
{
	int result;

	Trc_SC_vsnprintf_Entry(str, n, format);

	result = vsnprintf( str, n, format, args );

	Trc_SC_vsnprintf_Exit(result);

	return result;
}

typedef struct J9SpecialArguments {
	UDATA localVerboseLevel;
	IDATA *xoss;
	UDATA *argEncoding;
	IDATA *ibmMallocTraceSet;
	const char *executableJarPath;
	BOOLEAN captureCommandLine;
} J9SpecialArguments;
/**
 * Look for special options:
 * -verbose:init
 * -Xoss
 * argument encoding
 * -Xcheck:memory
 * and return the total size required for the strings
 */
static UDATA
initialArgumentScan(JavaVMInitArgs *args, J9SpecialArguments *specialArgs)
{
	BOOLEAN xCheckFound = FALSE;
	const char *xCheckString = "-Xcheck";
	const char *javaCommand = "-Dsun.java.command=";
	const char *javaCommandValue = NULL;
	const char *classPath = "-Djava.class.path=";
	const char *classPathValue = NULL;
	jint argCursor;
	UDATA argumentsSize = 0;

#ifdef WIN32
	Assert_SC_notNull(specialArgs->argEncoding);
#endif
	for (argCursor=0; argCursor < args->nOptions; argCursor++) {
		argumentsSize += strlen(args->options[argCursor].optionString) + 1; /* add space for the \0 */
		/* scan for -Xoss */
		if (0 == strncmp(args->options[argCursor].optionString, "-Xoss", 5)) {
			*(specialArgs->xoss) = argCursor;
		} else if (strncmp(args->options[argCursor].optionString, OPT_VERBOSE_INIT, strlen(OPT_VERBOSE_INIT))==0) {
			specialArgs->localVerboseLevel = VERBOSE_INIT;
		} else if (0 == strncmp(args->options[argCursor].optionString, javaCommand, strlen(javaCommand))) {
			javaCommandValue = args->options[argCursor].optionString + strlen(javaCommand);
		} else if (0 == strncmp(args->options[argCursor].optionString, classPath, strlen(classPath))) {
			classPathValue = args->options[argCursor].optionString + strlen(classPath);
		} else if (0 == strncmp(args->options[argCursor].optionString, xCheckString, strlen(xCheckString))) {
			xCheckFound = TRUE;
		} else if (0 == strcmp(args->options[argCursor].optionString, VMOPT_XARGENCODING)) {
			*(specialArgs->argEncoding) = ARG_ENCODING_PLATFORM;
		} else if (0 == strcmp(args->options[argCursor].optionString, VMOPT_XNOARGSCONVERSION)) {
			*(specialArgs->argEncoding) = ARG_ENCODING_LATIN;
		} else if (0 == strcmp(args->options[argCursor].optionString, VMOPT_XARGENCODINGUTF8)) {
			*(specialArgs->argEncoding) = ARG_ENCODING_UTF;
		} else if (0 == strcmp(args->options[argCursor].optionString, VMOPT_XARGENCODINGLATIN)) {
			*(specialArgs->argEncoding) = ARG_ENCODING_LATIN;
		} else if (0 == strcmp(args->options[argCursor].optionString, VMOPT_XXOPENJ9COMMANDLINEENV)) {
			specialArgs->captureCommandLine = TRUE;
		} else if (0 == strcmp(args->options[argCursor].optionString, VMOPT_XXNOOPENJ9COMMANDLINEENV)) {
			specialArgs->captureCommandLine = FALSE;
		}
	}

	if ((NULL != classPathValue) && (NULL != javaCommandValue) && (strcmp(javaCommandValue, classPathValue) == 0)) {
		specialArgs->executableJarPath = javaCommandValue;
	}

	if (TRUE == xCheckFound) {
		/* scan backwards for -Xcheck:memory.  There may be multiple -Xcheck options, so check them all, stop when we hit -Xcheck:memory */
		for( argCursor = args->nOptions - 1 ; argCursor >= 0; argCursor-- ) {
			char* memcheckArgs[2];

			/* fake up command-line args for parseCmdLine */
			memcheckArgs[0] = ""; /* program name -- unused */
			memcheckArgs[1] = args->options[argCursor].optionString;
			if (memoryCheck_parseCmdLine(&j9portLibrary, 1, memcheckArgs) != 0) {
				/* Abandon -Xcheck processing */
				/* -Xcheck:memory overrides env var */
				*(specialArgs->ibmMallocTraceSet) = FALSE;
				break;
			}
		}
	}
	return argumentsSize;
}

static void
printVmArgumentsList(J9VMInitArgs *argList)
{
	UDATA i;
	JavaVMInitArgs *actualArgs = argList->actualVMArgs;
	for (i = 0; i < argList->nOptions; ++i) {
		J9CmdLineOption* j9Option = &(argList->j9Options[i]);
		char *envVar = j9Option->fromEnvVar;

		if (NULL == envVar) {
			envVar = "N/A";
		}
		fprintf(stderr, "Option %" OMR_PRIuPTR " optionString=\"%s\" extraInfo=%p from environment variable =\"%s\"\n", i,
				actualArgs->options[i].optionString,
				actualArgs->options[i].extraInfo,
				envVar);
	}
}

static void
setNLSCatalog(struct J9PortLibrary* portLib)
{
	J9StringBuffer *nlsSearchPathBuffer = NULL;
	const char *nlsSearchPaths = NULL;
	PORT_ACCESS_FROM_PORT(portLib);

	if (J2SE_CURRENT_VERSION >= J2SE_V11) {
		/*
		 * j9libBuffer doesn't end in a slash, but j9nls_set_catalog ignores everything after
		 * the last slash. Append a slash to our local copy of j9libBuffer
		 */
		nlsSearchPathBuffer = jvmBufferCat(nlsSearchPathBuffer, jvmBufferData(j9libBuffer));
	} else {
		/*
		 * j9binBuffer doesn't end in a slash, but j9nls_set_catalog ignores everything after
		 * the last slash. Append a slash to our local copy of j9bin
		 */
		nlsSearchPathBuffer = jvmBufferCat(nlsSearchPathBuffer, jvmBufferData(j9binBuffer));
	}
	nlsSearchPathBuffer = jvmBufferCat(nlsSearchPathBuffer, DIR_SEPARATOR_STR);
	nlsSearchPaths = jvmBufferData(nlsSearchPathBuffer);

	j9nls_set_catalog(&nlsSearchPaths, 1, "java", "properties");
	free(nlsSearchPathBuffer);
	nlsSearchPathBuffer = NULL;
}


static jint initializeReflectionGlobals(JNIEnv * env, BOOLEAN includeAccessors) {
	J9VMThread *vmThread = (J9VMThread *) env;
	J9JavaVM * vm = vmThread->javaVM;
	jclass clazz, clazzConstructorAccessorImpl, clazzMethodAccessorImpl;

	clazz = (*env)->FindClass(env, "java/lang/Class");
	if (!clazz) {
		return JNI_ERR;
	}

	jlClass = (*env)->NewGlobalRef(env, clazz);
	if (!jlClass) {
		return JNI_ERR;
	}

#ifdef J9VM_IVE_RAW_BUILD /* J9VM_IVE_RAW_BUILD is not enabled by default */
	classNameFID = (*env)->GetFieldID(env, clazz, "name", "Ljava/lang/String;");
	if (!classNameFID) {
		return JNI_ERR;
	}
#else /* J9VM_IVE_RAW_BUILD */
	classDepthMID = (*env)->GetStaticMethodID(env, clazz, "classDepth", "(Ljava/lang/String;)I");
	if (!classDepthMID) {
		return JNI_ERR;
	}

	classLoaderDepthMID = (*env)->GetStaticMethodID(env, clazz, "classLoaderDepth", "()I");
	if (!classLoaderDepthMID) {
		return JNI_ERR;
	}

	currentClassLoaderMID = (*env)->GetStaticMethodID(env, clazz, "currentClassLoader", "()Ljava/lang/ClassLoader;");
	if (!currentClassLoaderMID) {
		return JNI_ERR;
	}

	currentLoadedClassMID = (*env)->GetStaticMethodID(env, clazz, "currentLoadedClass", "()Ljava/lang/Class;");
	if (!currentLoadedClassMID) {
		return JNI_ERR;
	}
#endif /* J9VM_IVE_RAW_BUILD */

	getNameMID = (*env)->GetMethodID(env, clazz, "getName", "()Ljava/lang/String;");
	if (!getNameMID) {
		return JNI_ERR;
	}

	clazz = (*env)->FindClass(env, "java/lang/Thread");
	if (!clazz) {
		return JNI_ERR;
	}

	jlThread = (*env)->NewGlobalRef(env, clazz);
	if (!jlThread) {
		return JNI_ERR;
	}

#if JAVA_SPEC_VERSION < 21
	sleepMID = (*env)->GetStaticMethodID(env, clazz, "sleep", "(J)V");
	if (!sleepMID) {
		return JNI_ERR;
	}
#else /* JAVA_SPEC_VERSION < 21 */
	sleepNanosMID = (*env)->GetStaticMethodID(env, clazz, "sleep", "(JI)V");
	if (!sleepNanosMID) {
		return JNI_ERR;
	}
#endif /* JAVA_SPEC_VERSION < 21 */

	clazz = (*env)->FindClass(env, "java/lang/Object");
	if (!clazz) {
		return JNI_ERR;
	}

	waitMID = (*env)->GetMethodID(env, clazz, "wait", "(J)V");
	if (!waitMID) {
		return JNI_ERR;
	}

	notifyMID = (*env)->GetMethodID(env, clazz, "notify", "()V");
	if (!notifyMID) {
		return JNI_ERR;
	}

	notifyAllMID = (*env)->GetMethodID(env, clazz, "notifyAll", "()V");
	if (!notifyAllMID) {
		return JNI_ERR;
	}

	if (includeAccessors) {
		if (J2SE_VERSION(vm) >= J2SE_V11) {
			clazzConstructorAccessorImpl = (*env)->FindClass(env, "jdk/internal/reflect/ConstructorAccessorImpl");
			clazzMethodAccessorImpl = (*env)->FindClass(env, "jdk/internal/reflect/MethodAccessorImpl");
		} else {
			clazzConstructorAccessorImpl = (*env)->FindClass(env, "sun/reflect/ConstructorAccessorImpl");
			clazzMethodAccessorImpl = (*env)->FindClass(env, "sun/reflect/MethodAccessorImpl");
		}
		if ( (NULL == clazzConstructorAccessorImpl) || (NULL == clazzMethodAccessorImpl))	{
			return JNI_ERR;
		}
		vm->srConstructorAccessor = (*env)->NewGlobalRef(env, clazzConstructorAccessorImpl);
		if (NULL == vm->srConstructorAccessor) {
			return JNI_ERR;
		}
		vm->srMethodAccessor = (*env)->NewGlobalRef(env, clazzMethodAccessorImpl);
		if (NULL == vm->srMethodAccessor) {
			return JNI_ERR;
		}
	}
	return JNI_OK;
}

/**
 * jint JNICALL JNI_CreateJavaVM(JavaVM **pvm, void **penv, void *vm_args)
 * Load and initialize a virtual machine instance.
 * This provides an invocation API that runs the J9 VM in BFU/sidecar mode.
 *
 * @param pvm pointer to the location where the JavaVM interface
 *			pointer will be placed
 * @param penv pointer to the location where the JNIEnv interface
 *			pointer for the main thread will be placed
 * @param vm_args java virtual machine initialization arguments
 *
 * @returns zero on success; otherwise, return a negative number
 *
 * DLL: jvm
 */
jint JNICALL
JNI_CreateJavaVM(JavaVM **pvm, void **penv, void *vm_args)
{
	return JNI_CreateJavaVM_impl(pvm, penv, vm_args, FALSE);
}

#if defined(J9VM_OPT_JITSERVER)
int32_t JNICALL
JITServer_CreateServer(JITServer **jitServer, void *serverArgs)
{
	JNIEnv *env = NULL;
	JITServer *server = NULL;
	jint rc = JITSERVER_OK;

	server = malloc(sizeof(JITServer));
	if (NULL == server) {
		rc = JITSERVER_OOM;
		goto _end;
	}
	server->startJITServer = startJITServer;
	server->waitForJITServerTermination = waitForJITServerTermination;
	server->destroyJITServer = destroyJITServer;
	rc = JNI_CreateJavaVM_impl(&server->jvm, (void **)&env, serverArgs, TRUE);

	if (JNI_OK != rc) {
		rc = JITSERVER_CREATE_FAILED;
		goto _end;
	}
	*jitServer = server;
	rc = JITSERVER_OK; /* success */

_end:
	if ((JITSERVER_OK != rc) && (NULL != server)) {
		free(server);
		*jitServer = NULL;
	}
	return rc;
}

/**
 * Starts an instance of JITServer.
 *
 * @param jitServer pointer to the JITServer interface
 *
 * @returns JITSERVER_OK on success, else negative error code
 */
static int32_t
startJITServer(JITServer *jitServer)
{
	JavaVM *vm = jitServer->jvm;
	JNIEnv *env = NULL;
	int32_t rc = JITSERVER_OK;

	rc = (*vm)->AttachCurrentThread(vm, (void **)&env, NULL);
	if (JNI_OK == rc) {
		J9JITConfig *jitConfig = ((J9JavaVM *)vm)->jitConfig;
		rc = jitConfig->startJITServer(jitConfig);
		if (0 != rc) {
			rc = JITSERVER_STARTUP_FAILED;
		} else {
			rc = JITSERVER_OK;
		}
		(*vm)->DetachCurrentThread(vm);
	} else {
		rc = JITSERVER_THREAD_ATTACH_FAILED;
	}
	return rc;
}

/**
 * Wait for JITServer to terminate.
 *
 * @param jitServer pointer to the JITServer interface
 *
 * @returns JITSERVER_OK on success, else negative error code
 */
static int32_t
waitForJITServerTermination(JITServer *jitServer)
{
	JavaVM *vm = jitServer->jvm;
	JNIEnv *env = NULL;
	int32_t rc = JITSERVER_OK;

	rc = (*vm)->AttachCurrentThread(vm, (void **)&env, NULL);
	if (JNI_OK == rc) {
		J9JITConfig *jitConfig = ((J9JavaVM *)vm)->jitConfig;
		rc = jitConfig->waitJITServerTermination(jitConfig);
		if (0 != rc) {
			rc = JITSERVER_WAIT_TERM_FAILED;
		} else {
			rc = JITSERVER_OK;
		}
		(*vm)->DetachCurrentThread(vm);
	} else {
		rc = JITSERVER_THREAD_ATTACH_FAILED;
	}
	return rc;
}

/**
 * Frees the resources allocated by JITServer_CreateServer.
 *
 * @param jitServer double pointer to the JITServer interface. Must not be NULL
 *
 * @returns JITSERVER_OK on success, else negative error code
 *
 * @note on return *jitServer is set to NULL
 */
static int32_t
destroyJITServer(JITServer **jitServer)
{
	JavaVM *vm = (*jitServer)->jvm;
	jint rc = (*vm)->DestroyJavaVM(vm);
	free(*jitServer);
	*jitServer = NULL;
	if (JNI_OK == rc) {
		rc = JITSERVER_OK;
	} else {
		rc = JITSERVER_DESTROY_ERROR;
	}
	return rc;
}

#endif /* J9VM_OPT_JITSERVER */

/*
 * Helper method to load and initialize a virtual machine instance.
 *
 * @param pvm pointer to the location where the JavaVM interface
 *			pointer will be placed
 * @param penv pointer to the location where the JNIEnv interface
 *			pointer for the main thread will be placed
 * @param vm_args java virtual machine initialization arguments
 *
 * @returns zero on success; otherwise, return a negative number
 */
static jint
JNI_CreateJavaVM_impl(JavaVM **pvm, void **penv, void *vm_args, BOOLEAN isJITServer)
{
	jint result = JNI_OK;
	IDATA xoss = -1;
	IDATA ibmMallocTraceSet = FALSE;
 	char cwd[J9_MAX_PATH];
#if defined(WIN32)
	wchar_t unicodeTemp[J9_MAX_PATH];
	char *altLibraryPathBuffer = NULL;
	char *executableJarPath = NULL;
#endif /* defined(WIN32) */
	UDATA argEncoding = ARG_ENCODING_DEFAULT;
	UDATA altJavaHomeSpecified = 0; /* not used on non-Windows */
	J9PortLibraryVersion portLibraryVersion;
#if defined(AIXPPC)
	char *origLibpath = NULL;
#endif /* AIXPPC */
	I_32 portLibraryInitStatus;
	UDATA expectedLibrarySize;
	UDATA localVerboseLevel = 0;
	J9CreateJavaVMParams createParams = {0};
	JavaVMInitArgs *args = NULL;
	UDATA launcherArgumentsSize = 0;
	J9VMInitArgs *j9ArgList = NULL;
	char *xServiceBuffer = NULL;
	const char *libpathValue = NULL;
	const char *ldLibraryPathValue = NULL;
	J9SpecialArguments specialArgs;
	omrthread_t attachedThread = NULL;
	specialArgs.localVerboseLevel = localVerboseLevel;
	specialArgs.xoss = &xoss;
	specialArgs.argEncoding = &argEncoding;
	specialArgs.executableJarPath = NULL;
	specialArgs.ibmMallocTraceSet = &ibmMallocTraceSet;
	specialArgs.captureCommandLine = TRUE;
#if defined(J9ZOS390)
	/*
	 * Temporarily disable capturing the command line on z/OS.
	 * See the discussion in https://github.com/eclipse-openj9/openj9/pull/14634.
	 */
	specialArgs.captureCommandLine = FALSE;
#endif /* defined(J9ZOS390) */
#ifdef J9ZTPF

	result = tpf_eownrc(TPF_SET_EOWNR, "IBMRT4J                        ");

	if (result < 0)  {
		fprintf(stderr, "Failed to issue tpf_eownrc.");
		return JNI_ERR;
	}
	result = tmslc(TMSLC_ENABLE+TMSLC_HOLD, "IBMRT4J");
	if (result < 0)  {
		fprintf(stderr, "Failed to start time slicing.");
		return JNI_ERR;
	}
#endif /* defined(J9ZTPF) */
	/*
	 * Linux uses LD_LIBRARY_PATH
	 * z/OS uses LIBPATH
	 * AIX uses both
	 * use native OS calls because port library isn't ready.
	 * Copy the strings because the value returned by getenv may change.
	 */
#if defined(AIXPPC) || defined(J9ZOS390)
	libpathValue = getenv(ENV_LIBPATH);
	if (NULL != libpathValue) {
		size_t pathLength = strlen(libpathValue) +1;
		char *envTemp = malloc(pathLength);
		if (NULL == envTemp) {
			result = JNI_ERR;
			goto exit;
		}
		strcpy(envTemp, libpathValue);
		libpathValue = envTemp;
	}
#endif
#if defined(J9UNIX)
	ldLibraryPathValue = getenv(ENV_LD_LIB_PATH);
	if (NULL != ldLibraryPathValue) {
		size_t pathLength = strlen(ldLibraryPathValue) +1;
		char *envTemp = malloc(pathLength);
		if (NULL == envTemp) {
			result = JNI_ERR;
			goto exit;
		}
		strcpy(envTemp, ldLibraryPathValue);
		ldLibraryPathValue = envTemp;
	}
#endif /* defined(J9UNIX) */

	if (BFUjavaVM != NULL) {
		result = JNI_ERR;
		goto exit;
	}

#ifdef J9OS_I5
	/* debug code */
	Xj9BreakPoint("jvm");
	/* Force iSeries create JVM flow */
	if (Xj9IleCreateJavaVmCalled() == 0) {
	    result = Xj9CallIleCreateJavaVm(pvm, penv, vm_args);
	    goto exit;
	}
#endif

#if defined(AIXPPC)
	/* CMVC 137180:
	 * in some cases the LIBPATH does not contain /usr/lib, when
	 * trying to load application native libraries that are linked against
	 * libraries in /usr/lib we could fail to find those libraries if /usr/lib
	 * is not on the LIBPATH.
	 */
	{
		/* github #8504 shows tests fail if the libpath is modified
		 * when creating a new process from Java.  The easy fix is to only
		 * append to the libpath if /usr/lib isn't already present.
		 * Example libpath:
		 * LIBPATH=/jre/lib/ppc64/j9vm:/jre/lib/ppc64:/jre/lib/ppc64/jli:/jre/../lib/ppc64:/usr/lib
		 */

		const char *currentLibPath = getenv("LIBPATH");
		BOOLEAN appendToLibPath = TRUE;
		if (NULL != currentLibPath) {
			const size_t currentLibPathLength = strlen(currentLibPath);
			const char *usrLib = "/usr/lib";
			const UDATA usrLibLength = LITERAL_STRLEN("/usr/lib");
			const char *needle = strstr(currentLibPath, usrLib);
			while (NULL != needle) {
				/* Note, inside the loop we're guaranteed to have
				 * usrLibLength of string to operate on so we can
				 * always peek needle[usrLibLength] and will get
				 * either a value or '\0'
				 */
				const ptrdiff_t offsetFromStart = needle - currentLibPath;
				if ((0 == offsetFromStart) || (':' == needle[-1])) {
					if  ((':' == needle[usrLibLength]) || ('\0' == needle[usrLibLength])) {
						/* Found a match */
						appendToLibPath = FALSE;
						break;
					}
				}
				needle = strstr(needle + usrLibLength, usrLib);
			}
		}
		if (appendToLibPath) {
			addToLibpath("/usr/lib", FALSE);
		}
	}
	/* CMVC 135358.
	 * This function modifies LIBPATH while dlopen()ing J9 shared libs.
	 * Save the original, with appended /usr/lib so that it can be
	 * restored at the end of the function.  Can't reuse the getenv
	 * result from above.
	 */
	origLibpath = getenv("LIBPATH");
#endif

	/* no tracing for this function, since it's unlikely to be used once the VM is running and the trace engine is initialized */
	preloadLibraries();

#ifdef WIN32
	if (GetCurrentDirectoryW(J9_MAX_PATH, unicodeTemp) == 0) {
		strcpy(cwd, "\\");
	} else {
		WideCharToMultiByte(OS_ENCODING_CODE_PAGE, OS_ENCODING_WC_FLAGS, unicodeTemp, -1,  cwd, J9_MAX_PATH, NULL, NULL);
	}
#else
	if (getcwd(cwd, J9_MAX_PATH) == NULL) {
		strcpy(cwd, ".");
	}
#endif
#ifdef DEBUG
	printf("cwd = %s\n", cwd);
#endif

	if (0 != f_threadAttachEx(&attachedThread, J9THREAD_ATTR_DEFAULT)) {
		result = JNI_ERR;
	}
	if (JNI_OK != result) {
		goto exit;
	}

	f_libEnableCPUMonitor(attachedThread);

#if defined(J9ZOS390)
	/*
	 * When we init the port lib, the 'Signal Reporter' thread will be spawned.
	 * On z/OS, we need to know whether this thread should be spawned as a medium
	 * or heavy weight thread. We do this here but we will only take into
	 * account JAVA_THREAD_MODEL - i.e., if the customer is using '-Xthr:tw=heavy'
	 * instead of the env var, the 'Signal Reporter' thread will still be launched
	 * as a medium weight thread (see PR100512).
	 */
	if (!setZOSThrWeight()) {
		return JNI_ERR;
	}
#endif /* defined(J9ZOS390) */

	/* Use portlibrary version which we compiled against, and have allocated space
	 * for on the stack.  This version may be different from the one in the linked DLL.
	 */
	J9PORT_SET_VERSION(&portLibraryVersion, J9PORT_CAPABILITY_MASK);

	expectedLibrarySize = sizeof(J9PortLibrary);
	portLibraryInitStatus = portInitLibrary(&j9portLibrary, &portLibraryVersion, expectedLibrarySize);

	if (0 != portLibraryInitStatus) {
		J9PortLibraryVersion actualVersion;
		/* port lib init failure */
		switch (portLibraryInitStatus) {
		case J9PORT_ERROR_INIT_WRONG_MAJOR_VERSION: {
			portGetVersionFn(&j9portLibrary, &actualVersion);

			fprintf(stderr,"Error: Port Library failed to initialize: expected major version %u, actual version is %u\n",
					portLibraryVersion.majorVersionNumber, actualVersion.majorVersionNumber);
			break;
		}
		case J9PORT_ERROR_INIT_WRONG_SIZE: {
			UDATA actualSize = portGetSizeFn(&portLibraryVersion);
			fprintf(stderr,"Error: Port Library failed to initialize: expected library size %" OMR_PRIuPTR ", actual size is %" OMR_PRIuPTR "\n",
					expectedLibrarySize, actualSize);
			break;
		}
		case J9PORT_ERROR_INIT_WRONG_CAPABILITIES: {
			fprintf(stderr,"Error: Port Library failed to initialize: capabilities do not match\n");
			break;
		}
		default: fprintf(stderr,"Error: Port Library failed to initialize: %i\n", portLibraryInitStatus); break;
		/* need this to handle legacy port libraries */
		}

#if defined(AIXPPC)
		/* Release memory if allocated by strdup() in backupLibpath() previously */
		freeBackupLibpath(&libpathBackup);

		/* restore LIBPATH to avoid polluting child processes */
		setLibpath(origLibpath);
#endif /* AIXPPC */
		result = JNI_ERR;
		goto exit;
	}

	/* Get the thread library memory categories */
	{
		IDATA threadCategoryResult = f_threadLibControl(J9THREAD_LIB_CONTROL_GET_MEM_CATEGORIES, (UDATA)&j9MainMemCategorySet);

		if (threadCategoryResult) {
			fprintf(stderr,"Error: Couldn't get memory categories from thread library.\n");
#if defined(AIXPPC)
			/* Release memory if allocated by strdup() in backupLibpath() previously */
			freeBackupLibpath(&libpathBackup);

			/* restore LIBPATH to avoid polluting child processes */
			setLibpath(origLibpath);
#endif /* AIXPPC */
			result = JNI_ERR;
			goto exit;
		}
	}
	/* Register the J9 memory categories with the port library */
	j9portLibrary.omrPortLibrary.port_control(&j9portLibrary.omrPortLibrary, J9PORT_CTLDATA_MEM_CATEGORIES_SET, (UDATA)&j9MainMemCategorySet);

	Assert_SC_true(J2SE_CURRENT_VERSION >= J2SE_18);
	setNLSCatalog(&j9portLibrary);


#ifdef WIN32
	if (GetEnvironmentVariableW(IBM_MALLOCTRACE_STR, NULL, 0) > 0)
		ibmMallocTraceSet = TRUE;
	altJavaHomeSpecified = (GetEnvironmentVariableW(ALT_JAVA_HOME_DIR_STR, NULL, 0) > 0);
#endif
#if defined(J9UNIX) || defined(J9ZOS390)
	if (getenv(IBM_MALLOCTRACE_STR)) {
		ibmMallocTraceSet = TRUE;
	}
#endif /* defined(J9UNIX) || defined(J9ZOS390) */

	args = (JavaVMInitArgs *)vm_args;
	launcherArgumentsSize = initialArgumentScan(args, &specialArgs);
	localVerboseLevel = specialArgs.localVerboseLevel;

	if (VERBOSE_INIT == localVerboseLevel) {
		createParams.flags |= J9_CREATEJAVAVM_VERBOSE_INIT;
	}

	/* [RTC 147146] Allow a Java option to disable capturing command line
	 * in the OPENJ9_JAVA_COMMAND_LINE environment variable.
	 */
	if (specialArgs.captureCommandLine) {
		captureCommandLine();
	}
#if defined(J9VM_OPT_JITSERVER)
	if (isJITServer) {
		createParams.flags |= J9_CREATEJAVAVM_START_JITSERVER;
	}
#endif /* J9VM_OPT_JITSERVER */

	if (ibmMallocTraceSet) {
		/* We have no access to the original command line, so cannot
		 * pass in a valid argv to this function.
		 * Currently this function only used argv to help set the NLS
		 * catalog.
		 * The catalog has already been set above. */
		memoryCheck_initialize(&j9portLibrary, "all", NULL);
	}

	{
		char *optionsDefaultFileLocation = NULL;
		BOOLEAN doAddExtDir = FALSE;
		J9JavaVMArgInfoList vmArgumentsList;
		J9ZipFunctionTable *zipFuncs = NULL;
		vmArgumentsList.pool = pool_new(sizeof(J9JavaVMArgInfo),
				32, /* expect at least ~16 arguments, overallocate to accommodate user arguments */
				0, 0,
				J9_GET_CALLSITE(), OMRMEM_CATEGORY_VM, POOL_FOR_PORT(&j9portLibrary));
		if (NULL == vmArgumentsList.pool) {
			result = JNI_ERR;
			goto exit;
		}
		vmArgumentsList.head = NULL;
		vmArgumentsList.tail = NULL;
		if (NULL != specialArgs.executableJarPath) {
			if (NULL == f_j9_GetInterface) {
#ifdef WIN32
				f_j9_GetInterface = (J9GetInterface) GetProcAddress (j9vm_dllHandle, (LPCSTR) "J9_GetInterface");
#else
				f_j9_GetInterface = (J9GetInterface) dlsym (j9vm_dllHandle, "J9_GetInterface");
#endif /* WIN32 */
			}
			/* j9binBuffer->data is null terminated */
			zipFuncs = (J9ZipFunctionTable*) f_j9_GetInterface(IF_ZIPSUP, &j9portLibrary, j9binBuffer->data);
#if defined(WIN32)
			{
				BOOLEAN conversionSucceed = FALSE;
				int32_t size = 0;
				UDATA pathLen = strlen(specialArgs.executableJarPath);

				PORT_ACCESS_FROM_PORT(&j9portLibrary);
				/* specialArgs.executableJarPath is retrieved from JavaVMInitArgs.options[i].optionString
				 * which was set by Java Launcher in system default code page encoding.
				 */
				size = j9str_convert(J9STR_CODE_WINDEFAULTACP, J9STR_CODE_MUTF8, specialArgs.executableJarPath, pathLen, NULL, 0);
				if (size > 0) {
					size += 1; /* leave room for null */
					executableJarPath = j9mem_allocate_memory(size, OMRMEM_CATEGORY_VM);
					if (NULL != executableJarPath) {
						size = j9str_convert(J9STR_CODE_WINDEFAULTACP, J9STR_CODE_MUTF8, specialArgs.executableJarPath, pathLen, executableJarPath, size);
						if (size > 0) {
							conversionSucceed = TRUE;
						}
					}
				}
				if (!conversionSucceed) {
					result = JNI_ERR;
					goto exit;
				}
				/* specialArgs.executableJarPath was assigned with javaCommandValue which is
				 * args->options[argCursor].optionString + strlen(javaCommand) within initialArgumentScan().
				 * The optionString will be freed later at destroyJvmInitArgs().
				 * Hence overwriting specialArgs.executableJarPath won't cause memory leak.
				 */
				specialArgs.executableJarPath = executableJarPath;
			}
#endif /* defined(WIN32) */
		}
		if (J2SE_CURRENT_VERSION >= J2SE_V11) {
			optionsDefaultFileLocation = jvmBufferData(j9libBuffer);
		} else {
			optionsDefaultFileLocation = jvmBufferData(j9binBuffer);
			doAddExtDir = TRUE;
		}
		/* now add implicit VM arguments */
		if (
			/* Add the default options file */
			(0 != addOptionsDefaultFile(&j9portLibrary, &vmArgumentsList, optionsDefaultFileLocation, localVerboseLevel))
			|| (0 != addXjcl(&j9portLibrary, &vmArgumentsList, J2SE_CURRENT_VERSION))
			|| (0 != addBootLibraryPath(&j9portLibrary, &vmArgumentsList, "-Dcom.ibm.oti.vm.bootstrap.library.path=",
					jvmBufferData(j9binBuffer), jvmBufferData(jrebinBuffer)))
			|| (0 != addBootLibraryPath(&j9portLibrary, &vmArgumentsList, "-Dsun.boot.library.path=",
					jvmBufferData(j9binBuffer), jvmBufferData(jrebinBuffer)))
			|| (0 != addJavaLibraryPath(&j9portLibrary, &vmArgumentsList, argEncoding, jvmInSubdir,
					jvmBufferData(j9binBuffer), jvmBufferData(jrebinBuffer),
					libpathValue, ldLibraryPathValue))
			|| (0 != addJavaHome(&j9portLibrary, &vmArgumentsList, altJavaHomeSpecified, jvmBufferData(j9libBuffer)))
			|| (doAddExtDir && (0 != addExtDir(&j9portLibrary, &vmArgumentsList, jvmBufferData(j9libBuffer), args, J2SE_CURRENT_VERSION)))
			|| (0 != addUserDir(&j9portLibrary, &vmArgumentsList, cwd))
#if !defined(OPENJ9_BUILD)
			|| (0 != addJavaPropertiesOptions(&j9portLibrary, &vmArgumentsList, localVerboseLevel))
#endif /* defined(OPENJ9_BUILD) */
			|| (0 != addJarArguments(&j9portLibrary, &vmArgumentsList, specialArgs.executableJarPath, zipFuncs, localVerboseLevel))
			|| (0 != addEnvironmentVariables(&j9portLibrary, args, &vmArgumentsList, localVerboseLevel))
			|| (0 != addLauncherArgs(&j9portLibrary, args, launcherArgumentsSize, &vmArgumentsList,
					&xServiceBuffer, argEncoding, localVerboseLevel))
#if (JAVA_SPEC_VERSION != 8) || defined(OPENJ9_BUILD)
			|| (0 != addEnvironmentVariableArguments(&j9portLibrary, ENVVAR_JAVA_OPTIONS, &vmArgumentsList, localVerboseLevel))
#endif /* (JAVA_SPEC_VERSION != 8) || defined(OPENJ9_BUILD) */
			|| (0 != addXserviceArgs(&j9portLibrary, &vmArgumentsList, xServiceBuffer, localVerboseLevel))
		) {
			result = JNI_ERR;
			goto exit;
		}

		if (NULL != libpathValue) {
			free((void *)libpathValue);
		}
		if (NULL != ldLibraryPathValue) {
			free((void *)ldLibraryPathValue);
		}
		j9ArgList = createJvmInitArgs(&j9portLibrary, args, &vmArgumentsList, &argEncoding);
		if (ARG_ENCODING_LATIN == argEncoding) {
			createParams.flags |= J9_CREATEJAVAVM_ARGENCODING_LATIN;
		} else if (ARG_ENCODING_UTF == argEncoding) {
			createParams.flags |= J9_CREATEJAVAVM_ARGENCODING_UTF8;
		} else if (ARG_ENCODING_PLATFORM == argEncoding) {
			createParams.flags |= J9_CREATEJAVAVM_ARGENCODING_PLATFORM;
		}

		pool_kill(vmArgumentsList.pool);
		if (NULL == j9ArgList) {
			result = JNI_ERR;
			goto exit;
		}
	}

	createParams.j2seVersion = J2SE_CURRENT_VERSION;
	if (jvmInSubdir) {
		createParams.j2seVersion |= J2SE_LAYOUT_VM_IN_SUBDIR;
	}
	createParams.threadDllHandle = (UDATA)thread_dllHandle;
	createParams.j2seRootDirectory = jvmBufferData(j9binBuffer);
	createParams.j9libvmDirectory = jvmBufferData(j9libvmBuffer);

	createParams.portLibrary = &j9portLibrary;
	createParams.globalJavaVM = &BFUjavaVM;

	if (VERBOSE_INIT == localVerboseLevel) {
		fprintf(stderr, "VM known paths\t- j9libvm directory: %s\n\t\t- j2seRoot directory: %s\n",
			createParams.j9libvmDirectory,
			createParams.j2seRootDirectory);

		printVmArgumentsList(j9ArgList);
	}
	createParams.vm_args = j9ArgList;

	result = globalCreateVM((JavaVM**)&BFUjavaVM, penv, &createParams);

#ifdef DEBUG
	fprintf(stdout,"Finished, result %d, env %llx\n", result, (long long)*penv);
	fflush(stdout);
#endif
	if (result == JNI_OK) {
		BOOLEAN initializeReflectAccessors = TRUE;
		JavaVM * vm = (JavaVM*)BFUjavaVM;
		*pvm = vm;

		/* Initialize the Sun VMI */
		ENSURE_VMI();

		memcpy(&globalInvokeInterface, *vm, sizeof(J9InternalVMFunctions));
		globalDestroyVM = globalInvokeInterface.DestroyJavaVM;
		globalInvokeInterface.DestroyJavaVM = DestroyJavaVM;
		issueWriteBarrier();
		*vm = (struct JNIInvokeInterface_ *) &globalInvokeInterface;

#ifdef WIN32
		result = initializeWin32ThreadEvents(BFUjavaVM);
		if (result != JNI_OK) {
			(**pvm)->DestroyJavaVM(*pvm);
			goto exit;
		}
#endif

		/* Initialize the VM interface */
		result = initializeReflectionGlobals(*penv, initializeReflectAccessors);
		if (result != JNI_OK) {
			(**pvm)->DestroyJavaVM(*pvm);
			goto exit;
		}
	} else {
		freeGlobals();
	}

	if ((result == JNI_OK) && (BFUjavaVM->runtimeFlags & J9_RUNTIME_SHOW_VERSION)) {
		JNIEnv * env = *penv;
		jclass clazz = (*env)->FindClass(env, "sun/misc/Version");

		if (clazz == NULL) {
			(*env)->ExceptionClear(env);
		} else {
			jmethodID mid = (*env)->GetStaticMethodID(env, clazz, "print", "()V");

			if (mid != NULL) {
				(*env)->CallStaticVoidMethod(env, clazz, mid);
				if (!(*env)->ExceptionCheck(env)) {
					j9portLibrary.omrPortLibrary.tty_printf(&j9portLibrary.omrPortLibrary, "\n");
				}
			}
			(*env)->ExceptionClear(env);
			(*env)->DeleteLocalRef(env, clazz);
		}
	}

#if defined(AIXPPC)
	/* restore LIBPATH to avoid polluting child processes */
	restoreLibpath(&libpathBackup);
#ifdef DEBUG_TEST
	testBackupAndRestoreLibpath();
#endif /* DEBUG_TEST */
#endif /* AIXPPC */

	if (JNI_OK == result) {
		J9JavaVM *env = (J9JavaVM *) BFUjavaVM;
		J9VMThread *currentThread = env->mainThread;
		omrthread_t thread = currentThread->osThread;
		f_setCategory(thread, J9THREAD_CATEGORY_APPLICATION_THREAD, J9THREAD_TYPE_SET_MODIFY);
	}

exit:
	if (NULL != attachedThread) {
		f_threadDetach(attachedThread);
	}
#if defined(WIN32)
	if (NULL != executableJarPath) {
		PORT_ACCESS_FROM_PORT(&j9portLibrary);
		/* specialArgs.executableJarPath (overwritten with executableJarPath) is only used by
		 * addJarArguments(... specialArgs.executableJarPath, ...) which uses the path to load the jar files
		 * and doesn't require the path afterwards.
		 * So executableJarPath can be freed after that usage.
		 */
		j9mem_free_memory(executableJarPath);
	}
#endif /* defined(WIN32) */

	return result;
}


/**
 *	jint JNICALL JNI_GetCreatedJavaVMs(JavaVM **vmBuf, jsize bufLen, jsize *nVMs)
 *  Return pointers to all the virtual machine instances that have been
 *  created.
 *	This provides an invocation API that runs the J9 VM in BFU/sidecar mode
 *
 *  @param vmBuf pointer to the buffer where the pointer to virtual
 *			machine instances will be placed
 *  @param bufLen the length of the buffer
 *  @param nVMs a pointer to and integer
 *
 *  @returns zero on success; otherwise, return a negative number
 *
 *	DLL: jvm
 */


jint JNICALL
JNI_GetCreatedJavaVMs(JavaVM **vmBuf, jsize bufLen, jsize *nVMs)
{
	jint result;
	Trc_SC_GetCreatedJavaVMs_Entry(vmBuf, bufLen, nVMs);

	/* for proxy we cannot preload the libraries as we don't know whether proxy is enabled or not at this point
	 * However, if the libraries have not already been preloaded then we know that no vms have been created and
	 * can return the right answer
	 */
	if (librariesLoaded()) {
		result = globalGetVMs(vmBuf, bufLen, nVMs);
	} else {
		/* simply return that there have been no JVMs created */
		result = JNI_OK;
		*nVMs = 0;
	}

	Trc_SC_GetCreatedJavaVMs_Exit(result, *nVMs);
	return result;
}


/**
 *	jint JNICALL JNI_GetDefaultJavaVMInitArgs(void *vm_args)
 *  Return a default configuration for the java virtual machine
 *  implementation.
 *	This provides an invocation API that runs the J9 VM in BFU/sidecar mode
 *
 *  @param vm_args pointer to a vm-specific initialization structure
 *			into which the default arguments are filled.
 *
 *  @returns zero on success; otherwise, return a negative number
 *
 *	DLL: jvm
 */

jint JNICALL JNI_GetDefaultJavaVMInitArgs(void *vm_args)
{
	jint requestedVersion = ((JavaVMInitArgs *)vm_args)->version;

	switch (requestedVersion) {
	case JNI_VERSION_1_1:
#if defined(OPENJ9_BUILD)
		((JDK1_1InitArgs *)vm_args)->javaStackSize = J9_OS_STACK_SIZE;
#endif /* defined(OPENJ9_BUILD) */
		break;
	case JNI_VERSION_1_2:
	case JNI_VERSION_1_4:
	case JNI_VERSION_1_6:
	case JNI_VERSION_1_8:
#if JAVA_SPEC_VERSION >= 9
	case JNI_VERSION_9:
#endif /* JAVA_SPEC_VERSION >= 9 */
#if JAVA_SPEC_VERSION >= 10
	case JNI_VERSION_10:
#endif /* JAVA_SPEC_VERSION >= 10 */
#if JAVA_SPEC_VERSION >= 19
	case JNI_VERSION_19:
#endif /* JAVA_SPEC_VERSION >= 19 */
#if JAVA_SPEC_VERSION >= 20
	case JNI_VERSION_20:
#endif /* JAVA_SPEC_VERSION >= 20 */
#if JAVA_SPEC_VERSION >= 21
	case JNI_VERSION_21:
#endif /* JAVA_SPEC_VERSION >= 21 */
		return JNI_OK;
	}

	return JNI_EVERSION;
}




#ifdef J9ZOS390
/**
 *      jint JNICALL GetStringPlatform(JNIEnv*, jstring, char*, jint, const char *);
 *      This provides an invocation API that runs the J9 VM in BFU/sidecar mode
 *
 *  @returns zero on success; otherwise, return a negative number
 *
 *      DLL: java
 */

jint
GetStringPlatform(JNIEnv* env, jstring instr, char* outstr, jint outlen, const char* encoding)
{
	jint result;

	preloadLibraries();

	Trc_SC_GetStringPlatform_Entry(env, instr, outstr, outlen, encoding);

	result = globalGetStringPlatform(env, instr, outstr, outlen, encoding);

	Trc_SC_GetStringPlatform_Exit(env, result);

	return result;
}



/**
 *      jint JNICALL GetStringPlatformLength(JNIEnv*, jstring, char*, jint, const char *);
 *      This provides an invocation API that runs the J9 VM in BFU/sidecar mode
 *
 *  @returns zero on success; otherwise, return a negative number
 *
 *      DLL: java
 */

jint
GetStringPlatformLength(JNIEnv* env, jstring instr, jint* outlen, const char* encoding)
{
	jint result;

	preloadLibraries();

	Trc_SC_GetStringPlatformLength_Entry(env, instr, outlen, encoding);
	result = globalGetStringPlatformLength(env, instr, outlen, encoding);
	Trc_SC_GetStringPlatformLength_Exit(env, result, *outlen);

	return result;
}



/**
 *      jint JNICALL NewStringPlatform(JNIEnv*, const char *, jstring*, const char*);
 *      This provides an invocation API that runs the J9 VM in BFU/sidecar mode
 *
 *  @returns zero on success; otherwise, return a negative number
 *
 *      DLL: java
 */

jint
NewStringPlatform(JNIEnv* env, const char* instr, jstring* outstr, const char* encoding)
{
	jint result;

	preloadLibraries();
	Trc_SC_NewStringPlatform_Entry(env, instr, outstr, encoding);
	result = globalNewStringPlatform(env, instr, outstr, encoding);
	Trc_SC_NewStringPlatform_Exit(env, result);

	return result;
}



/**
 *      jint JNICALL JNI_a2e_vsprintf(char *, const char *, va_list)
 *      This provides an invocation API that runs the J9 VM in BFU/sidecar mode
 *
 *  @returns zero on success; otherwise, return a negative number
 *
 *      DLL: java
 */

jint
JNI_a2e_vsprintf(char *target, const char *format, va_list args)
{
	jint result;

	preloadLibraries();

	Trc_SC_a2e_vsprintf_Entry(target, format);

	result = global_a2e_vsprintf(target, format, args);

	Trc_SC_a2e_vsprintf_Exit(result);

	return result;
}
#endif /* J9ZOS390 */


int isFileInDir(char *dir, char *file){
	size_t length, dirLength;
	char *fullpath = NULL;
	FILE *f = NULL;
	int foundFile = 0;

	dirLength = strlen(dir);
	/* Construct 'full' path */
	if (dir[dirLength-1] == DIR_SEPARATOR) {
		/* remove trailing '/' */
		dir[dirLength-1] = '\0';
		dirLength--;
	}

	length = dirLength + strlen(file) + 2; /* 2= '/' + null char */
	fullpath = malloc(length);
	if (NULL != fullpath) {
		strcpy(fullpath, dir);
		fullpath[dirLength] = DIR_SEPARATOR;
		strcpy(fullpath+dirLength+1, file);

		/* See if file exists - use fopen() for portability */
		f = fopen(fullpath, "rb");
		if (NULL != f) {
			foundFile = 1;
			fclose(f);
		}
		free(fullpath);
	}
	return foundFile;
}


/*
 * find directory containing a given file.
 * @returns 0 for not found, and a positive integer on success, which represents which path element the file was found in.
 **/
int findDirContainingFile(J9StringBuffer **result, char *paths, char pathSeparator, char *fileToFind, int elementsToSkip) {
	char *startOfDir, *endOfDir, *pathsCopy;
	int   isEndOfPaths, foundIt, count=elementsToSkip;

	/* Copy input as it is modified */
	paths = strdup(paths);
	if (!paths) {
		return FALSE;
	}

	pathsCopy = paths;
	while(elementsToSkip--) {
		pathsCopy = strchr(pathsCopy, pathSeparator);
		if(pathsCopy) {
			pathsCopy++; /* advance past separator */
		} else {
			free(paths);
			return 0;
		}
	}

	/* Search each dir in the list for fileToFind */
	startOfDir = endOfDir = pathsCopy;
	for (isEndOfPaths=FALSE, foundIt=FALSE; !foundIt && !isEndOfPaths; endOfDir++) {

		isEndOfPaths = endOfDir[0] == '\0';
		if (isEndOfPaths || (endOfDir[0] == pathSeparator))  {
			endOfDir[0] = '\0';
			if (strlen(startOfDir) && isFileInDir(startOfDir, fileToFind)) {
				foundIt = TRUE;
				if (NULL != *result) {
					free(*result);
					*result = NULL;
				}
				*result = jvmBufferCat(NULL, startOfDir);
			}
			startOfDir = endOfDir+1;
			count+=1;
		}
	}

	free(paths); /* from strdup() */
	if(foundIt) {
		return count;
	} else {
		return 0;
	}
}



int findDirUplevelToDirContainingFile(J9StringBuffer **result, char *pathEnvar, char pathSeparator, char *fileInPath, int upLevels, int elementsToSkip) {
	char *paths;
	int   rc;

	/* Get the list of paths */
	paths = getenv(pathEnvar);
	if (!paths) {
		return FALSE;
	}

	/* find the directory */
	rc = findDirContainingFile(result, paths, pathSeparator, fileInPath, elementsToSkip);

	/* Now move upLevel to it - this may not work for directories of form
		/aaa/bbb/..      ... and so on.
		If that is a problem, could always use /.. to move up.
	*/
	if (rc) {
		for (; upLevels > 0; upLevels--) {
			truncatePath(jvmBufferData(*result));
		}
	}
	return rc;
}


void
exitHook(J9JavaVM *vm)
{
	while (f_monitorEnter(vm->vmThreadListMutex), vm->sidecarExitFunctions) {
		J9SidecarExitFunction * current = vm->sidecarExitFunctions;

		vm->sidecarExitFunctions = current->next;
		f_monitorExit(vm->vmThreadListMutex);
		current->func();
		free(current);
	}

	f_monitorExit(vm->vmThreadListMutex);
}

static void*
preloadLibrary(char* dllName, BOOLEAN inJVMDir)
{
	J9StringBuffer *buffer = NULL;
	void* handle = NULL;
#ifdef WIN32
	wchar_t unicodePath[J9_MAX_PATH];
	char * bufferData;
	size_t bufferLength;
#endif

	if(inJVMDir) {
		buffer = jvmBufferCat(buffer, jvmBufferData(j9binBuffer));
	} else {
		buffer = jvmBufferCat(buffer, jvmBufferData(jrebinBuffer));
	}
#ifdef WIN32
	buffer = jvmBufferCat(buffer, "\\");
	buffer = jvmBufferCat(buffer, dllName);
	buffer = jvmBufferCat(buffer, ".dll");
	bufferData = jvmBufferData(buffer);
	bufferLength = strlen(bufferData);
	MultiByteToWideChar(OS_ENCODING_CODE_PAGE, OS_ENCODING_MB_FLAGS, bufferData, -1, unicodePath, (int)bufferLength + 1);
	handle = (void*)LoadLibraryExW (unicodePath, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
	if (handle == NULL) {
		fprintf(stderr,"jvm.dll preloadLibrary: LoadLibrary(%s) error: %x\n", buffer->data, GetLastError());
	}
#endif
#if defined(J9UNIX)
	buffer = jvmBufferCat(buffer, "/lib");
	buffer = jvmBufferCat(buffer, dllName);
	buffer = jvmBufferCat(buffer, J9PORT_LIBRARY_SUFFIX);
#ifdef AIXPPC
	loadAndInit(jvmBufferData(buffer), L_RTLD_LOCAL, NULL);
#endif
 	handle = (void*)dlopen(jvmBufferData(buffer), RTLD_NOW);
#ifdef AIXPPC
	if (handle == NULL) {
		int len = strlen(buffer->data);
		buffer->data[len - 2] = 'a';
		buffer->data[len - 1] = '\0';
		loadAndInit(buffer->data, L_RTLD_LOCAL, NULL);
		handle = (void*)dlopen(buffer->data, RTLD_NOW);
		if (handle == NULL) {
			/* get original error, otherwise the error displayed will be for a missing .a library. */
			buffer->data[len - 2] = 's';
			buffer->data[len - 1] = 'o';
			loadAndInit(buffer->data, L_RTLD_LOCAL, NULL);
			handle = (void*)dlopen(buffer->data, RTLD_NOW);
		}
	}
#endif /* AIXPPC */
	if (handle == NULL) {
		fprintf(stderr,"libjvm.so preloadLibrary(%s): %s\n", buffer->data, dlerror());
	}
#endif /* defined(J9UNIX) */
#ifdef J9ZOS390
	buffer = jvmBufferCat(buffer, "/lib");
	buffer = jvmBufferCat(buffer, dllName);
	buffer = jvmBufferCat(buffer, ".so");
	handle = (void*)dllload(jvmBufferData(buffer));
	if (handle == NULL) {
		perror("libjvm.so preloadLibrary: dllload() failed");
	}
#endif

	free(buffer);
	return handle;
}

/*
 *	This section contains a bunch of 'extra' symbols that Linux needs above and beyond Windows.
 *	Included everywhere because the makefile generator makes it hard to create different export lists
 *	for different platforms.
 */
int
pre_block(pre_block_t buf)
{
	return 0;
}



int
post_block() {
	return 0;
}

#if defined(AIXPPC)
static void
setLibpath(const char *libpath)
{
	setenv("LIBPATH", libpath, 1);
}
#endif /* AIXPPC */

static void
addToLibpath(const char *dir, BOOLEAN isPrepend)
{
#if defined(AIXPPC) || defined(J9ZOS390)
	char *oldPath, *newPath;
	int rc, newSize;
#if defined(J9ZOS390)
	char *putenvPath;
	int putenvSize;
#endif

	oldPath = getenv("LIBPATH");
#ifdef DEBUG
	printf("\nLIBPATH before = %s\n", oldPath ? oldPath : "<empty>");
#endif
	newSize = (oldPath ? strlen(oldPath) : 0) + strlen(dir) + 2;  /* 1 for :, 1 for \0 terminator */
	newPath = malloc(newSize);

	if(!newPath) {
		fprintf(stderr, "addToLibpath malloc(%d) 1 failed, aborting\n", newSize);
		abort();
	}
#if defined(AIXPPC)
	if (oldPath) {
		if (isPrepend) {
			strcpy(newPath, dir);
			strcat(newPath, ":");
			strcat(newPath, oldPath);
		} else {
			strcpy(newPath, oldPath);
			strcat(newPath, ":");
			strcat(newPath, dir);
		}
	} else {
		strcpy(newPath, dir);
	}

#else
	/* ZOS doesn't like it when we pre-pend to LIBPATH */
	if (oldPath) {
		strcpy(newPath, oldPath);
		strcat(newPath, ":");
	} else {
		newPath[0] = '\0';
	}
	strcat(newPath, dir);
#endif

#if defined(J9ZOS390)
	putenvSize = newSize + strlen("LIBPATH=");
	putenvPath = malloc(putenvSize);
	if(!putenvPath) {
		fprintf(stderr, "addToLibpath malloc(%d) 2 failed, aborting\n", putenvSize);
		abort();
	}

	strcpy(putenvPath,"LIBPATH=");
	strcat(putenvPath, newPath);
	rc = putenv(putenvPath);
	free(putenvPath);
#else
	rc = setenv("LIBPATH", newPath, 1);
#endif

#ifdef DEBUG
	printf("\nLIBPATH after = %s\n", getenv("LIBPATH"));
#endif
	free(newPath);
#endif
}

#if defined(AIXPPC)
/**
 * Backup the entire LIBPATH after we have prepended VM directory.
 *
 * @pre libpathBackup can't be NULL.
 *
 * @param libpathBackup  The structure that stores a copy of the current LIBPATH.
 * @param origLibpathLen The length of the original LIBPATH before prepending VM dir.
 */
static void
backupLibpath(J9LibpathBackup *libpathBackup, size_t origLibpathLen)
{
	const char *curLibpath = getenv("LIBPATH");

	libpathBackup->fullpath = NULL;
	libpathBackup->j9prefixLen = 0;

	if (NULL != curLibpath) {
		/* If j9prefix is not at the end of LIBPATH, then trailing ':' is included in j9prefixLen */
		libpathBackup->j9prefixLen = strlen(curLibpath) - origLibpathLen;

		/* If j9prefixLen == 0, there is no VM prefix to remove. We don't
		 * need to save a copy of the LIBPATH because we don't need to
		 * do any processing in restoreLibpath().
		 */
		if (libpathBackup->j9prefixLen > 0) {
			libpathBackup->fullpath = strdup(curLibpath);
			if (NULL == libpathBackup->fullpath) {
				fprintf(stderr, "backupLibpath: strdup() failed to allocate memory for the backup path\n");
				abort();
			}
		}
	}
}
#endif /* AIXPPC */

#if defined(AIXPPC)
/**
 * Release memory if allocated by strdup() in backupLibpath().
 *
 * @pre libpathBackup can't be NULL.
 *
 * @param libpathBackup  The structure that stores a copy of the current LIBPATH.
 */
static void
freeBackupLibpath(J9LibpathBackup *libpathBackup)
{
	if (NULL != libpathBackup->fullpath) {
		free(libpathBackup->fullpath);
		libpathBackup->fullpath = NULL;
	}
	libpathBackup->j9prefixLen = 0;
}
#endif /* AIXPPC */

#if defined(AIXPPC)
/**
 * We allow CL (Class Library) to prepend or append to the existing LIBPATH,
 * but it can't change the original one.
 * Remove any VM prefix from the current LIBPATH while keeping other parts unchanged.
 *
 * @pre libpathBackup can't be NULL.
 *
 * @param libpathBackup Ptr to the LIBPATH before being modified by CL.
 */
static void
restoreLibpath(J9LibpathBackup *libpathBackup)
{
	if ((NULL != libpathBackup->fullpath) && (libpathBackup->j9prefixLen > 0)) {
		const char *curPath = getenv("LIBPATH");

		if (NULL != curPath) {
			const char *j9pathLoc = findInLibpath(curPath, libpathBackup->fullpath);

			if (NULL != j9pathLoc) {
				char *newPath = deleteDirsFromLibpath(curPath, j9pathLoc, libpathBackup->j9prefixLen);

#ifdef DEBUG
				printf("restoreLibpath: old LIBPATH = <%s>\n", curPath);
				printf("restoreLibpath: new LIBPATH = <%s>\n", newPath);
#endif
				setLibpath(newPath);
				free(newPath);
			}
		}

		free(libpathBackup->fullpath);
		libpathBackup->fullpath = NULL;
		libpathBackup->j9prefixLen = 0;
	}
}
#endif /* AIXPPC */

#if defined(AIXPPC)
/**
 * Search the current LIBPATH for the backup LIBPATH.
 *
 * @pre libpath is properly formed.
 * @pre backupPath is a non-NULL, non-empty string.
 *
 * @param libpath    The ptr to the current LIBPATH.
 * @param backupPath The ptr to the backup LIBPATH.
 * @return ptr to the match start if found, NULL if there is no match.
 */
static const char *
findInLibpath(const char *libpath, const char *backupPath)
{
	size_t backupPathLen = strlen(backupPath);
	BOOLEAN leadingColon = FALSE;
	BOOLEAN trailingColon = FALSE;
	const char *libpathdir = libpath;

	if (':' == backupPath[0]) {
		/* Shouldn't happen, but this will work if it does */
		leadingColon = TRUE;
	}
	if (':' == backupPath[backupPathLen - 1]) {
		trailingColon = TRUE;
	}

	while (NULL != libpathdir) {
		if (!leadingColon) {
			/* Skip leading ':'s from libpathdir */
			while (':' == libpathdir[0]) {
				libpathdir += 1;
			}
		}
		if (0 == strncmp(libpathdir, backupPath, backupPathLen)) {
			if (('\0' == libpathdir[backupPathLen]) || (':' == libpathdir[backupPathLen]) || trailingColon) {
				return libpathdir;
			}
		}

		/*
		 * The pointer moves forward by one step if a mismatch occurs previously,
		 * otherwise strchr() would repeatedly return the first char ':' of the original
		 * libpathdir if there is no match. Under such circumstance, it would end up
		 * being in an infinite loop (e.g. libpath = ::x:y, path = b:x:y, prefixlen = 1).
		 * The change works even if the first char of libpathdir isn't ':'.
		 */
		libpathdir = strchr(libpathdir + 1, ':');
	}
	return NULL;
}
#endif /* AIXPPC */

#if defined(AIXPPC)
/**
 * Copy a libpath and delete a section from it.
 *
 * @pre libpath is properly formed.
 * @pre The section to be deleted consists of one or more complete directories in the libpath.
 * @pre deleteStart is a ptr into libpath string
 * @pre deleteLen is greater than 0
 *
 * @param libpath     The value of the current LIBPATH, a null-terminated string.
 * @param deleteStart Ptr to the position of libpath to start deleting from.
 * @param deleteLen   Number of chars to delete. It does not include null-terminators.
 *                    It may include leading or trailing ':'s.
 *
 * @return a new libpath string
 */
static char *
deleteDirsFromLibpath(const char *const libpath, const char *const deleteStart, const size_t deleteLen)
{
	char *newPath = NULL;
	size_t preLen = deleteStart - libpath;
	const char *postStart = deleteStart + deleteLen;
	size_t postLen = strlen(postStart);
	size_t delim = 0;

	/* Remove trailing : from the prefix */
	while ((preLen > 0) && (':' == libpath[preLen - 1])) {
		preLen -= 1;
	}

	if (postLen > 0) {
		/* Remove leading : from the postfix */
		while (':' == postStart[0]) {
			postStart += 1;
			postLen -= 1;
		}
	}

	if ((preLen > 0) && (postLen > 0)) {
		/* Add delimiter : */
		delim = 1;
	}

	newPath = malloc(preLen + delim + postLen + 1);
	if (NULL == newPath) {
		fprintf(stderr, "deleteDirsFromLibpath: malloc(%zu) failed, aborting\n", preLen + delim + postLen + 1);
		abort();
	}

	memcpy(newPath, libpath, preLen);
	if (delim > 0) {
		newPath[preLen] = ':';
	}

	memcpy(newPath + preLen + delim, postStart, postLen);

	/* Set the NUL terminator at the end */
	newPath[preLen + delim + postLen] = '\0';

	return newPath;
}
#endif /* AIXPPC */

#if defined(AIXPPC) && defined(DEBUG_TEST)
static void
testBackupAndRestoreLibpath(void)
{
	int failed = 0;
	int passed = 0;
	char *origLibpath = getenv("LIBPATH");

	J9LibpathBackup bkp = {NULL, 0};

	printf("testBackupAndRestoreLibpath:------------ BEGIN ------------------------\n");

	/*
	 * Typical tests
	 */

	/* Remove the path added by VM when restoring LIBPATH */
	printf("TESTCASE_1: Remove the path added by VM when restoring LIBPATH\n");
	setLibpath("compressedrefs:/usr/lib");
	backupLibpath(&bkp, strlen("/usr/lib"));
	restoreLibpath(&bkp);
	if (0 == strcmp(getenv("LIBPATH"), "/usr/lib")) {
		printf("Test result: PASSED at </usr/lib>\n");
		passed++;
	} else {
		fprintf(stderr, "Test result: FAILED at </usr/lib>: LIBPATH = <%s>\n", getenv("LIBPATH"));
		failed++;
	}

	/* Ignore multiple colons prefixing the backup path when restoring LIBPATH */
	printf("TESTCASE_2: Ignore colons prefixing the backup path when restoring LIBPATH\n");
	setLibpath("::abc");
	backupLibpath(&bkp, strlen("abc"));
	restoreLibpath(&bkp);
	if (0 == strcmp(getenv("LIBPATH"), "abc")) {
		printf("Test result: PASSED at <abc>\n");
		passed++;
	} else {
		fprintf(stderr, "Test result: FAILED at <abc>: LIBPATH = <%s>\n", getenv("LIBPATH"));
		failed++;
	}

	/*
	 * Test fullpath searching
	 */

	/* Set up LIBPATH again with exactly the same path */
	printf("TESTCASE_3: Set up libpath again with exactly the same path\n");
	setLibpath("x:y");
	backupLibpath(&bkp, 0);
	setLibpath("x:y");
	restoreLibpath(&bkp);
	if (0 == strcmp(getenv("LIBPATH"), "")) {
		printf("testcase_3: PASSED at <>\n");
		passed++;
	} else {
		fprintf(stderr, "testcase_3: FAILED at <>: LIBPATH = <%s>\n", getenv("LIBPATH"));
		failed++;
	}

	/* The backup path is followed by a single colon */
	printf("TESTCASE_4: The backup path is followed by a single colon\n");
	setLibpath("x:y");
	backupLibpath(&bkp, 0);
	setLibpath("x:y:");
	restoreLibpath(&bkp);
	if (0 == strcmp(getenv("LIBPATH"), "")) {
		printf("testcase_4: PASSED at <>\n");
		passed++;
	} else {
		fprintf(stderr, "testcase_4: FAILED at <>: LIBPATH = <%s>\n", getenv("LIBPATH"));
		failed++;
	}

	/* The backup path is followed by multiple colons */
	printf("TESTCASE_5: The backup path is followed by multiple colons\n");
	setLibpath("x:y");
	backupLibpath(&bkp, 0);
	setLibpath("x:y:::");
	restoreLibpath(&bkp);
	if (0 == strcmp(getenv("LIBPATH"), "")) {
		printf("TESTCASE_5: PASSED at <>\n");
		passed++;
	} else {
		fprintf(stderr, "TESTCASE_5: FAILED at <>: LIBPATH = <%s>\n", getenv("LIBPATH"));
		failed++;
	}

	/* The backup path is followed by multiple paths */
	printf("TESTCASE_6: The backup path is followed by multiple paths\n");
	setLibpath("x:y");
	backupLibpath(&bkp, 0);
	setLibpath("x:y:def:ghi");
	restoreLibpath(&bkp);
	if (0 == strcmp(getenv("LIBPATH"), "def:ghi")) {
		printf("TESTCASE_6: PASSED at <def:ghi>\n");
		passed++;
	} else {
		fprintf(stderr, "TESTCASE_6: FAILED at <def:ghi>: LIBPATH = <%s>\n", getenv("LIBPATH"));
		failed++;
	}

	/* The backup path stays at the start except for a single colon */
	printf("TESTCASE_7: The backup path stays at the start except for a single colon\n");
	setLibpath("x:y");
	backupLibpath(&bkp, 0);
	setLibpath(":x:y:def:ghi");
	restoreLibpath(&bkp);
	if (0 == strcmp(getenv("LIBPATH"), "def:ghi")) {
		printf("TESTCASE_7: PASSED at <def:ghi>\n");
		passed++;
	} else {
		fprintf(stderr, "TESTCASE_7: FAILED at <def:ghi>: LIBPATH = <%s>\n", getenv("LIBPATH"));
		failed++;
	}

	/* The backup path stays at the start except for multiple colons */
	printf("TESTCASE_8: The backup path stays at the start except for multiple colons\n");
	setLibpath("x:y");
	backupLibpath(&bkp, 0);
	setLibpath(":::x:y:def:ghi");
	restoreLibpath(&bkp);
	if (0 == strcmp(getenv("LIBPATH"), "def:ghi")) {
		printf("TESTCASE_8: PASSED at <def:ghi>\n");
		passed++;
	} else {
		fprintf(stderr, "TESTCASE_8: FAILED at <def:ghi>: LIBPATH = <%s>\n", getenv("LIBPATH"));
		failed++;
	}

	/* Partial match at the start */
	printf("TESTCASE_9: Partial match at the start\n");
	setLibpath("x:y");
	backupLibpath(&bkp, 0);
	setLibpath("::x:ydef:ghi");
	restoreLibpath(&bkp);
	if (0 == strcmp(getenv("LIBPATH"), "::x:ydef:ghi")) {
		printf("TESTCASE_9: PASSED at <::x:ydef:ghi>\n");
		passed++;
	} else {
		fprintf(stderr, "TESTCASE_9: FAILED at <::x:ydef:ghi>: LIBPATH = <%s>\n", getenv("LIBPATH"));
		failed++;
	}

	/* Partial match at the end */
	printf("TESTCASE_10: Partial match at the end\n");
	setLibpath("x:y");
	backupLibpath(&bkp, 0);
	setLibpath("::abc:defx:y::");
	restoreLibpath(&bkp);
	if (0 == strcmp(getenv("LIBPATH"), "::abc:defx:y::")) {
		printf("TESTCASE_10: PASSED at <::abc:defx:y::>\n");
		passed++;
	} else {
		fprintf(stderr, "TESTCASE_10: FAILED at <::abc:defx:y::>: LIBPATH = <%s>\n", getenv("LIBPATH"));
		failed++;
	}

	/* Partial match in the middle */
	printf("TESTCASE_11: Partial match in the middle \n");
	setLibpath("x:y");
	backupLibpath(&bkp, 0);
	setLibpath("::abc:def::x:yghi:jkl");
	restoreLibpath(&bkp);
	if (0 == strcmp(getenv("LIBPATH"), "::abc:def::x:yghi:jkl")) {
		printf("TESTCASE_11: PASSED at <::abc:def::x:yghi:jkl>\n");
		passed++;
	} else {
		fprintf(stderr, "TESTCASE_11: FAILED at <::abc:def::x:yghi:jkl>: LIBPATH = <%s>\n", getenv("LIBPATH"));
		failed++;
	}

	/* The backup path stays at the end with a single colon ahead of it */
	printf("TESTCASE_12: The backup path stays at the end with a single colon ahead of it\n");
	setLibpath("x:y");
	backupLibpath(&bkp, 0);
	setLibpath(":x:y");
	restoreLibpath(&bkp);
	if (0 == strcmp(getenv("LIBPATH"), "")) {
		printf("TESTCASE_12: PASSED at <>\n");
		passed++;
	} else {
		fprintf(stderr, "TESTCASE_12: FAILED at <>: LIBPATH = <%s>\n", getenv("LIBPATH"));
		failed++;
	}

	/* The backup path stays at the end with multiple colons ahead of it */
	printf("TESTCASE_13: The backup path stays at the end with multiple colons ahead of it\n");
	setLibpath("x:y");
	backupLibpath(&bkp, 0);
	setLibpath("::x:y");
	restoreLibpath(&bkp);
	if (0 == strcmp(getenv("LIBPATH"), "")) {
		printf("TESTCASE_13: PASSED at <>\n");
		passed++;
	} else {
		fprintf(stderr, "TESTCASE_13: FAILED at <>: LIBPATH = <%s>\n", getenv("LIBPATH"));
		failed++;
	}

	/* The backup path stays at the end with multiple paths ahead of it */
	printf("TESTCASE_14: The backup path stays at the end with multiple paths ahead of it\n");
	setLibpath("x:y");
	backupLibpath(&bkp, 0);
	setLibpath("abc:def:x:y");
	restoreLibpath(&bkp);
	if (0 == strcmp(getenv("LIBPATH"), "abc:def")) {
		printf("TESTCASE_14: PASSED at <abc:def>\n");
		passed++;
	} else {
		fprintf(stderr, "TESTCASE_14: FAILED at <abc:def>: LIBPATH = <%s>\n", getenv("LIBPATH"));
		failed++;
	}

	/* The backup path stays at the end with multiple empty paths ahead of it */
	printf("TESTCASE_15: The backup path stays at the end with multiple empty paths ahead of it\n");
	setLibpath("x:y");
	backupLibpath(&bkp, 0);
	setLibpath("abc:::def:x:y");
	restoreLibpath(&bkp);
	if (0 == strcmp(getenv("LIBPATH"), "abc:::def")) {
		printf("TESTCASE_15: PASSED at <abc:::def>\n");
		passed++;
	} else {
		fprintf(stderr, "TESTCASE_15: FAILED at <abc:::def>: LIBPATH = <%s>\n", getenv("LIBPATH"));
		failed++;
	}

	/* An empty path plus a single colon at the start stays ahead of the backup path */
	printf("TESTCASE_16: An empty path plus a single colon at the start stays ahead of the backup path\n");
	setLibpath("x:y");
	backupLibpath(&bkp, 0);
	setLibpath(":abc::def:x:y");
	restoreLibpath(&bkp);
	if (0 == strcmp(getenv("LIBPATH"), ":abc::def")) {
		printf("TESTCASE_16: PASSED at <:abc::def>\n");
		passed++;
	} else {
		fprintf(stderr, "TESTCASE_16: FAILED at <:abc::def>: LIBPATH = <%s>\n", getenv("LIBPATH"));
		failed++;
	}

	/* Multiple empty paths plus a single colon at the start stays ahead of the backup path */
	printf("TESTCASE_17: Multiple empty paths plus a single colon at the start stays ahead of the backup patht\n");
	setLibpath("x:y");
	backupLibpath(&bkp, 0);
	setLibpath(":abc::def::x:y");
	restoreLibpath(&bkp);
	if (0 == strcmp(getenv("LIBPATH"), ":abc::def")) {
		printf("TESTCASE_17: PASSED at <:abc::def>\n");
		passed++;
	} else {
		fprintf(stderr, "TESTCASE_17: FAILED at <:abc::def>: LIBPATH = <%s>\n", getenv("LIBPATH"));
		failed++;
	}

	/* The backup path stays at the end except for a single colon */
	printf("TESTCASE_18: The backup path stays at the end except for a single colon\n");
	setLibpath("x:y");
	backupLibpath(&bkp, 0);
	setLibpath("abc:def:x:y:");
	restoreLibpath(&bkp);
	if (0 == strcmp(getenv("LIBPATH"), "abc:def")) {
		printf("TESTCASE_18: PASSED at <abc:def>\n");
		passed++;
	} else {
		fprintf(stderr, "TESTCASE_18: FAILED at <abc:def>: LIBPATH = <%s>\n", getenv("LIBPATH"));
		failed++;
	}

	/* The backup path stays at the end except for an empty path */
	printf("TESTCASE_19: The backup path stays at the end except for an empty path\n");
	setLibpath("x:y");
	backupLibpath(&bkp, 0);
	setLibpath("abc:def:x:y::");
	restoreLibpath(&bkp);
	if (0 == strcmp(getenv("LIBPATH"), "abc:def")) {
		printf("TESTCASE_19: PASSED at <abc:def>\n");
		passed++;
	} else {
		fprintf(stderr, "TESTCASE_19: FAILED at <abc:def>: LIBPATH = <%s>\n", getenv("LIBPATH"));
		failed++;
	}

	/* The backup path comes with a trailing colon */
	printf("TESTCASE_20: The backup path comes with a trailing colon\n");
	setLibpath("x:y:");
	backupLibpath(&bkp, 0);
	setLibpath("abc:x:y:ghi");
	restoreLibpath(&bkp);
	if (0 == strcmp(getenv("LIBPATH"), "abc:ghi")) {
		printf("TESTCASE_20: PASSED at <abc:ghi>\n");
		passed++;
	} else {
		fprintf(stderr, "TESTCASE_20: FAILED at <abc:ghi>: LIBPATH = <%s>\n", getenv("LIBPATH"));
		failed++;
	}

	/* The backup path comes with a leading colon */
	printf("TESTCASE_21: The backup path comes with a leading colon\n");
	setLibpath(":x:y");
	backupLibpath(&bkp, 0);
	setLibpath("::abc:def::x:y:ghi");
	restoreLibpath(&bkp);
	if (0 == strcmp(getenv("LIBPATH"), "::abc:def:ghi")) {
		printf("TESTCASE_21: PASSED at <::abc:def:ghi>\n");
		passed++;
	} else {
		fprintf(stderr, "TESTCASE_21: FAILED at <::abc:def:ghi>: LIBPATH = <%s>\n", getenv("LIBPATH"));
		failed++;
	}

	/*
	 * Test path deletion
	 */

	/* Delete from the start when restoring LIBPATH */
	printf("TESTCASE_22: Delete from the start when restoring LIBPATH\n");
	setLibpath("abc::def:xyz");
	backupLibpath(&bkp, strlen("xyz"));
	setLibpath("abc::def:xyz:morestuff");
	restoreLibpath(&bkp);
	if (0 == strcmp(getenv("LIBPATH"), "xyz:morestuff")) {
		printf("TESTCASE_22: PASSED at <xyz:morestuff>\n");
		passed++;
	} else {
		fprintf(stderr, "TESTCASE_22: FAILED at <xyz:morestuff>: LIBPATH = <%s>\n", getenv("LIBPATH"));
		failed++;
	}

	/* Delete from the middle when restoring LIBPATH */
	printf("TESTCASE_23: Delete from the middle when restoring LIBPATH\n");
	setLibpath("abc::def:xyz");
	backupLibpath(&bkp, strlen("xyz"));
	setLibpath(":stuff:abc::def:xyz:morestuff");
	restoreLibpath(&bkp);
	if (0 == strcmp(getenv("LIBPATH"), ":stuff:xyz:morestuff")) {
		printf("TESTCASE_23: PASSED at <:stuff:xyz:morestuff>\n");
		passed++;
	} else {
		fprintf(stderr, "TESTCASE_23: FAILED at <:stuff:xyz:morestuff>: LIBPATH = <%s>\n", getenv("LIBPATH"));
		failed++;
	}

	/* Delete from the end when restoring LIBPATH */
	printf("TESTCASE_24: Delete from the end when restoring LIBPATH\n");
	setLibpath("abc::def:xyz");
	backupLibpath(&bkp, strlen("xyz"));
	setLibpath(":stuff:morestuff:abc::def:xyz");
	restoreLibpath(&bkp);
	if (0 == strcmp(getenv("LIBPATH"), ":stuff:morestuff:xyz")) {
		printf("TESTCASE_24: PASSED at <:stuff:morestuff:xyz>\n");
		passed++;
	} else {
		fprintf(stderr, "TESTCASE_24: FAILED at <:stuff:morestuff:xyz>: LIBPATH = <%s>\n", getenv("LIBPATH"));
		failed++;
	}

	/* Ignore colons in the prefix when restoring LIBPATH */
	printf("TESTCASE_25: Ignore colons in the prefix when restoring LIBPATH\n");
	setLibpath("abc::def::xyz");
	backupLibpath(&bkp, strlen("xyz"));
	setLibpath("stuff:abc::def::xyz:morestuff");
	restoreLibpath(&bkp);
	if (0 == strcmp(getenv("LIBPATH"), "stuff:xyz:morestuff")) {
		printf("TESTCASE_25: PASSED at <stuff:xyz:morestuff>\n");
		passed++;
	} else {
		fprintf(stderr, "TESTCASE_25: FAILED at <stuff:xyz:morestuff>: LIBPATH = <%s>\n", getenv("LIBPATH"));
		failed++;
	}

	/* Skip colons from both start and end when restoring LIBPATH */
	printf("TESTCASE_26: Skip colons from both start and end when restoring LIBPATH\n");
	setLibpath("abc::def:");
	backupLibpath(&bkp, 0);
	setLibpath("::abc::def::");
	restoreLibpath(&bkp);
	if (0 == strcmp(getenv("LIBPATH"), "")) {
		printf("TESTCASE_26: PASSED at <>\n");
		passed++;
	} else {
		fprintf(stderr, "TESTCASE_26: FAILED at <>: LIBPATH = <%s>\n", getenv("LIBPATH"));
		failed++;
	}

	/* Insert colon from the end when restoring LIBPATH */
	printf("TESTCASE_27: Insert colon from the end when restoring LIBPATH\n");
	setLibpath(":abc::def:xyz");
	backupLibpath(&bkp, strlen("xyz"));
	setLibpath("stuff:abc::def:xyz:morestuff");
	restoreLibpath(&bkp);
	if (0 == strcmp(getenv("LIBPATH"), "stuff:xyz:morestuff")) {
		printf("TESTCASE_27: PASSED at <stuff:xyz:morestuff>\n");
		passed++;
	} else {
		fprintf(stderr, "TESTCASE_27: FAILED at <stuff:xyz:morestuff>: LIBPATH = <%s>\n", getenv("LIBPATH"));
		failed++;
	}

	/* Empty backup path before restoring LIBPATH */
	printf("TESTCASE_28: Empty backup path before restoring LIBPATH\n");
	setLibpath("");
	backupLibpath(&bkp, 0);
	setLibpath("stuff:abc::def:xyz:morestuff");
	restoreLibpath(&bkp);
	if (0 == strcmp(getenv("LIBPATH"), "stuff:abc::def:xyz:morestuff")) {
		printf("TESTCASE_28: PASSED at <stuff:abc::def:xyz:morestuff>\n");
		passed++;
	} else {
		fprintf(stderr, "TESTCASE_28: FAILED at <stuff:abc::def:xyz:morestuff>: LIBPATH = <%s>\n", getenv("LIBPATH"));
		failed++;
	}

	/* Keep backup path and empty prefix when restoring LIBPATH */
	printf("TESTCASE_29: Keep backup path and empty prefix when restoring LIBPATH\n");
	setLibpath("abc:def");
	backupLibpath(&bkp, strlen("abc:def"));
	setLibpath("stuff:abc:def:xyz:morestuff");
	restoreLibpath(&bkp);
	if (0 == strcmp(getenv("LIBPATH"), "stuff:abc:def:xyz:morestuff")) {
		printf("TESTCASE_29: PASSED at <stuff:abc:def:xyz:morestuff>\n");
		passed++;
	} else {
		fprintf(stderr, "TESTCASE_29: FAILED at <stuff:abc:def:xyz:morestuff>: LIBPATH = <%s>\n", getenv("LIBPATH"));
		failed++;
	}

	/* The length of current path is greater than the backup path when restoring LIBPATH */
	printf("TESTCASE_30: The length of current path is greater than the backup path when restoring LIBPATH\n");
	setLibpath("abc:def:ghi");
	backupLibpath(&bkp, 0);
	setLibpath("abc:def");
	restoreLibpath(&bkp);
	if (0 == strcmp(getenv("LIBPATH"), "abc:def")) {
		printf("TESTCASE_30: PASSED at <abc:def>\n");
		passed++;
	} else {
		fprintf(stderr, "TESTCASE_30: FAILED at <abc:def>: LIBPATH = <%s>\n", getenv("LIBPATH"));
		failed++;
	}

	/* The backup path is prefixed by multiple colons */
	printf("TESTCASE_31: The backup path is prefixed by multiple colons\n");
	setLibpath(":::abc");
	backupLibpath(&bkp, strlen("abc"));
	setLibpath("abc");
	restoreLibpath(&bkp);
	if (0 == strcmp(getenv("LIBPATH"), "abc")) {
		printf("TESTCASE_31: PASSED at <abc>\n");
		passed++;
	} else {
		fprintf(stderr, "TESTCASE_31: FAILED at <abc>: LIBPATH = <%s>\n", getenv("LIBPATH"));
		failed++;
	}

	/* Only VM prefix exists in the backup path when restoring LIBPATH */
	printf("TESTCASE_32: Only VM prefix exists in the backup path when restoring LIBPATH\n");
	setLibpath(":::abc");
	backupLibpath(&bkp, 0);
	setLibpath("abc");
	restoreLibpath(&bkp);
	if (0 == strcmp(getenv("LIBPATH"), "abc")) {
		printf("TESTCASE_32: PASSED at <abc>\n");
		passed++;
	} else {
		fprintf(stderr, "TESTCASE_32: FAILED at <abc>: LIBPATH = <%s>\n", getenv("LIBPATH"));
		failed++;
	}

	/* Colons prefixing the backup LIBPATH is more than that of the current LIBPATH */
	printf("TESTCASE_33: Colons prefixing the backup LIBPATH is more than that of the current LIBPATH\n");
	setLibpath(":::abc:xyz");
	backupLibpath(&bkp, strlen("abc:xyz"));
	setLibpath("def::abc:xyz");
	restoreLibpath(&bkp);
	if (0 == strcmp(getenv("LIBPATH"), "def::abc:xyz")) {
		printf("TESTCASE_33: PASSED at <def::abc:xyz>\n");
		passed++;
	} else {
		fprintf(stderr, "TESTCASE_33: FAILED at <def::abc:xyz>: LIBPATH = <%s>\n", getenv("LIBPATH"));
		failed++;
	}

	/* Colons prefixing the backup LIBPATH is less than that of the current LIBPATH (1) */
	printf("TESTCASE_34: Colons prefixing the backup LIBPATH is less than that of the current LIBPATH (1)\n");
	setLibpath(":::abc:xyz");
	backupLibpath(&bkp, strlen("abc:xyz"));
	setLibpath("def:::abc:xyz");
	restoreLibpath(&bkp);
	if (0 == strcmp(getenv("LIBPATH"), "def:abc:xyz")) {
		printf("TESTCASE_34: PASSED at <def:abc:xyz>\n");
		passed++;
	} else {
		fprintf(stderr, "TESTCASE_34: FAILED at <def:abc:xyz>: LIBPATH = <%s>\n", getenv("LIBPATH"));
		failed++;
	}

	/* Colons prefixing the backup LIBPATH is less than that of the current LIBPATH (2) */
	printf("TESTCASE_35: Colons prefixing the backup LIBPATH is less than that of the current LIBPATH (2)\n");
	setLibpath(":::abc:xyz");
	backupLibpath(&bkp, strlen("abc:xyz"));
	setLibpath("def::::abc:xyz");
	restoreLibpath(&bkp);
	if (0 == strcmp(getenv("LIBPATH"), "def:abc:xyz")) {
		printf("TESTCASE_35: PASSED at <def:abc:xyz>\n");
		passed++;
	} else {
		fprintf(stderr, "TESTCASE_35: FAILED at <def:abc:xyz>: LIBPATH = <%s>\n", getenv("LIBPATH"));
		failed++;
	}

	printf("---------- TEST RESULTS ----------\n");
	printf("Number of PASSED tests: %d\n", passed);
	printf("Number of FAILED tests: %d\n", failed);

	if (0 == failed) {
		printf("testBackupAndRestoreLibpath:---- TEST_PASSED ---------\n");
	} else {
		printf("testBackupAndRestoreLibpath:---- TEST_FAILED ---------\n");
	}

	setLibpath(origLibpath);
}
#endif /* AIXPPC and DEBUG_TEST */

#if defined(WIN32)

static jint
formatErrorMessage(int errorCode, char *inBuffer, jint inBufferLength)
{
	size_t i=0;
	int rc = 0, j=0;
	size_t outLength, lastChar;
	_TCHAR buffer[JVM_DEFAULT_ERROR_BUFFER_SIZE];
	_TCHAR noCRLFbuffer[JVM_DEFAULT_ERROR_BUFFER_SIZE];

	if (inBufferLength <= 1) {
		if (inBufferLength == 1) {
			inBuffer[0] = '\0';
		}
		return 0;
	}

	rc = FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer, JVM_DEFAULT_ERROR_BUFFER_SIZE, NULL );
	if (rc == 0) {
		inBuffer[0] = '\0';
		return 0;
	}

	j=0;
	for (i = 0; i < _tcslen(buffer)+1 ; i++) {
		if(buffer[i] == _TEXT('\n') ) {
			noCRLFbuffer[j++]=_TEXT(' ');
		} else if(buffer[i] == _TEXT('\r')) {
			continue;
		} else {
			noCRLFbuffer[j++]=buffer[i];
		}
	}

	lastChar = _tcslen(noCRLFbuffer)-1;

	if(_istspace(noCRLFbuffer[lastChar])) {
		noCRLFbuffer[lastChar] = _TEXT('\0');
	}

/* We always return multibyte */

#ifdef UNICODE
	outLength = WideCharToMultiByte(CP_UTF8, 0, noCRLFbuffer, -1, inBuffer, inBufferLength-1, NULL, NULL);
#else
	outLength = strlen(noCRLFbuffer)+1;
	if(outLength > (size_t)inBufferLength) {
		outLength = (size_t)inBufferLength;
	}
	strncpy(inBuffer, noCRLFbuffer, outLength);
	inBuffer[inBufferLength-1]='\0';
#endif

	Assert_SC_true(outLength <= I_32_MAX);
	return (jint)outLength;
}

#endif /* WIN32 */

static UDATA
protectedStrerror(J9PortLibrary* portLib, void* savedErrno)
{
	return (UDATA) strerror((int) (IDATA) savedErrno);
}


static UDATA
strerrorSignalHandler(struct J9PortLibrary* portLibrary, U_32 gpType, void* gpInfo, void* userData)
{
	return J9PORT_SIG_EXCEPTION_RETURN;
}

static void
throwNewUnsatisfiedLinkError(JNIEnv *env, char *message)
{
	jclass exceptionClass = (*env)->FindClass(env, "java/lang/UnsatisfiedLinkError");
	if (NULL != exceptionClass) {
		(*env)->ThrowNew(env, exceptionClass, message);
	}
}

/*
 * Remove one segment of a path.
 */
static void
truncatePath(char *inputPath) {
	char *lastOccurence = strrchr(inputPath, DIR_SEPARATOR);
	/* strrchr() returns NULL if it cannot find the character */
	if (NULL != lastOccurence) {
		*lastOccurence = '\0';
	}
}

/**********************************************************************
 * JVM_ functions start here
 **********************************************************************/

/**
 *	void JNICALL JVM_OnExit(void *ptr)
 *	This function seems to be required by fontmanager.dll but is not
 *	called?
 *
 *	@param exitCode?
 *
 *  @returns ?
 *
 *	DLL: jvm
 */
void JNICALL
JVM_OnExit(void (*func)(void))
{
	J9SidecarExitFunction * newFunc;

	Trc_SC_OnExit_Entry(func);

	newFunc = (J9SidecarExitFunction *) malloc(sizeof(J9SidecarExitFunction));
	if (newFunc) {
		newFunc->func = func;

		f_monitorEnter(BFUjavaVM->vmThreadListMutex);

		newFunc->next = BFUjavaVM->sidecarExitFunctions;
		BFUjavaVM->sidecarExitFunctions = newFunc;

		f_monitorExit(BFUjavaVM->vmThreadListMutex);

		BFUjavaVM->sidecarExitHook = &exitHook;
	} else {
		Trc_SC_OnExit_OutOfMemory();
	}

	Trc_SC_OnExit_Exit(newFunc);
}

/**
 *  void * JNICALL JVM_LoadSystemLibrary(char *libName)
 *
 *	Attempts to load the shared library specified by libName.  If
 *	successful, returns the file handle, otherwise returns NULL.
 *
 *	@param libName a null terminated string containing the libName.
 *
 *	@return the shared library's handle if successful, throws java/lang/UnsatisfiedLinkError on failure
 */
void* JNICALL
JVM_LoadSystemLibrary(const char *libName)
{
#ifdef WIN32
	UDATA dllHandle = 0;
	size_t libNameLen = strlen(libName);
	UDATA flags = 0;

	Trc_SC_LoadSystemLibrary_Entry(libName);

	if ((libNameLen <= 4) ||
			('.' != libName[libNameLen - 4]) ||
			('d' != j9_cmdla_tolower(libName[libNameLen - 3])) ||
			('l' != j9_cmdla_tolower(libName[libNameLen - 2])) ||
			('l' != j9_cmdla_tolower(libName[libNameLen - 1])))
	{
		flags = J9PORT_SLOPEN_DECORATE;
	}

	if (0 == j9util_open_system_library((char *)libName, &dllHandle, flags)) {
		Trc_SC_LoadSystemLibrary_Exit(dllHandle);
		return (void *)dllHandle;
	}
#endif

#if defined(J9UNIX) || defined(J9ZOS390)
	void *dllHandle = NULL;

	Trc_SC_LoadSystemLibrary_Entry(libName);

#if defined(AIXPPC)
	/* CMVC 137341:
	 * dlopen() searches for libraries using the LIBPATH envvar as it was when the process
	 * was launched.  This causes multiple issues such as:
	 *  - finding 32 bit binaries for libomrsig.so instead of the 64 bit binary needed and vice versa
	 *  - finding compressed reference binaries instead of non-compressed ref binaries
	 *
	 * calling loadAndInit(libname, 0 -> no flags, NULL -> use the currently defined LIBPATH) allows
	 * us to load the library with the current libpath instead of the one at process creation
	 * time. We can then call dlopen() as per normal and the just loaded library will be found.
	 * */
	loadAndInit((char *)libName, L_RTLD_LOCAL, NULL);
#endif /* defined(AIXPPC) */
	dllHandle = dlopen((char *)libName, RTLD_LAZY);
	if (NULL != dllHandle) {
		Trc_SC_LoadSystemLibrary_Exit(dllHandle);
		return dllHandle;
	}

#endif /* defined(J9UNIX) || defined(J9ZOS390) */

	/* We are here means we failed to load library. Throw java.lang.UnsatisfiedLinkError */
	Trc_SC_LoadSystemLibrary_LoadFailed(libName);

	if (NULL != BFUjavaVM) {
		JNIEnv *env = NULL;
		JavaVM *vm = (JavaVM *)BFUjavaVM;
		(*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_2);
		if (NULL != env) {
			char errMsg[512];
			jio_snprintf(errMsg, sizeof(errMsg), "Failed to load library \"%s\"", libName);
			errMsg[sizeof(errMsg) - 1] = '\0';
			throwNewUnsatisfiedLinkError(env, errMsg);
		}
	}
	Trc_SC_LoadSystemLibrary_Exit(NULL);

	return NULL;
}

/**
 * Prior to jdk11:
 *   void * JNICALL JVM_LoadLibrary(char *libName)
 *
 * Beginning in jdk11:
 *   void * JNICALL JVM_LoadLibrary(char *libName, jboolean throwOnFailure)
 *
 * Attempts to load the shared library specified by libName.
 * If successful, returns the file handle, otherwise returns NULL or throws
 * UnsatisfiedLinkError if throwOnFailure.
 *
 * @param libName a null terminated string containing the libName.
 *   For Windows platform, this incoming libName is encoded as J9STR_CODE_WINDEFAULTACP,
 *   and is required to be converted to J9STR_CODE_MUTF8 for internal usages.
 *
 * @return the shared library's handle if successful, throws java/lang/UnsatisfiedLinkError on failure
 *
 * DLL: jvm
 *
 * NOTE this is required by jdk15+ jdk.internal.loader.NativeLibraries.load().
 * It is only invoked by jdk.internal.loader.BootLoader.loadLibrary().
 */
void * JNICALL
#if JAVA_SPEC_VERSION < 11
JVM_LoadLibrary(const char *libName)
#else /* JAVA_SPEC_VERSION < 11 */
JVM_LoadLibrary(const char *libName, jboolean throwOnFailure)
#endif /* JAVA_SPEC_VERSION < 11 */
{
	void *result = NULL;
	J9JavaVM *javaVM = (J9JavaVM *)BFUjavaVM;
	PORT_ACCESS_FROM_JAVAVM(javaVM);

#if defined(WIN32)
#if JAVA_SPEC_VERSION >= 17
	BOOLEAN attemptedLoad = FALSE;
#endif /* JAVA_SPEC_VERSION >= 17 */
	char *libNameConverted = NULL;
	UDATA libNameLen = strlen(libName);
	UDATA libNameLenConverted = j9str_convert(J9STR_CODE_WINDEFAULTACP, J9STR_CODE_MUTF8, libName, libNameLen, NULL, 0);
	if (libNameLenConverted > 0) {
		libNameLenConverted += 1; /* an extra byte for null */
		libNameConverted = j9mem_allocate_memory(libNameLenConverted, OMRMEM_CATEGORY_VM);
		if (NULL != libNameConverted) {
			libNameLenConverted = j9str_convert(J9STR_CODE_WINDEFAULTACP, J9STR_CODE_MUTF8, libName, libNameLen, libNameConverted, libNameLenConverted);
			if (libNameLenConverted > 0) {
				/* j9str_convert null-terminated the string */
				libName = libNameConverted;
			}
		}
	}
	if (libName == libNameConverted) {
#if JAVA_SPEC_VERSION >= 17
		attemptedLoad = TRUE;
#endif /* JAVA_SPEC_VERSION >= 17 */
#endif /* defined(WIN32) */
		Trc_SC_LoadLibrary_Entry(libName);
		{
			UDATA handle = 0;
			UDATA flags = J9_ARE_ANY_BITS_SET(javaVM->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_LAZY_SYMBOL_RESOLUTION) ? J9PORT_SLOPEN_LAZY : 0;

#if defined(J9VM_ZOS_3164_INTEROPERABILITY)
			if (J9_ARE_ALL_BITS_SET(javaVM->extendedRuntimeFlags2, J9_EXTENDED_RUNTIME2_3164_INTEROPERABILITY)) {
				flags |= OMRPORT_SLOPEN_ATTEMPT_31BIT_OPEN;
			}
#endif /* defined(J9VM_ZOS_3164_INTEROPERABILITY) */

			UDATA slOpenResult = j9sl_open_shared_library((char *)libName, &handle, flags);
			Trc_SC_LoadLibrary_OpenShared(libName);

/* jdk17+ calls JVM_LoadLibrary with decorated library names. If the following is done
 * then it overwrites the real error with a failure to load "liblib<name>.so.so".
 */
#if JAVA_SPEC_VERSION < 17
			if (0 != slOpenResult) {
				slOpenResult = j9sl_open_shared_library((char *)libName, &handle, flags | J9PORT_SLOPEN_DECORATE);
				Trc_SC_LoadLibrary_OpenShared_Decorate(libName);
			}
#endif /* JAVA_SPEC_VERSION < 17 */
			if (0 == slOpenResult) {
				result = (void *)handle;
			}
		}
#if defined(WIN32)
	}
	if (NULL != libNameConverted) {
		j9mem_free_memory(libNameConverted);
	}
#endif /* defined(WIN32) */

#if JAVA_SPEC_VERSION >= 17
	if ((NULL == result) && throwOnFailure) {
		JNIEnv *env = NULL;
		JavaVM *vm = (JavaVM *)javaVM;
		(*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_2);
		if (NULL != env) {
			char errBuf[512];
			char *errMsg = errBuf;
			int bufSize = sizeof(errBuf);
			const char *portMsg =
#if defined(WIN32)
				!attemptedLoad ? "" :
#endif /* defined(WIN32) */
				j9error_last_error_message();
			const char *space = ('\0' == *portMsg) ? "" : " ";
			bufSize = jio_snprintf(errMsg, bufSize, "Failed to load library (\"%s\")%s%s", libName, space, portMsg);
			errBuf[sizeof(errBuf) - 1] = '\0';
			bufSize += 1;
			if (bufSize > sizeof(errBuf)) {
				errMsg = (char *)j9mem_allocate_memory(bufSize, OMRMEM_CATEGORY_VM);
				if (NULL != errMsg) {
					jio_snprintf(errMsg, bufSize, "Failed to load library (\"%s\")%s%s", libName, space, portMsg);
				} else {
					errMsg = errBuf;
				}
			}
			throwNewUnsatisfiedLinkError(env, errMsg);
			if (errBuf != errMsg) {
				j9mem_free_memory(errMsg);
			}
		}
	}
#endif /* JAVA_SPEC_VERSION >= 17 */

	Trc_SC_LoadLibrary_Exit(result);

	return result;
}

/* NOTE this is required by JDK15+ jdk.internal.loader.NativeLibraries.unload().
 */
#if JAVA_SPEC_VERSION >= 15
void JNICALL JVM_UnloadLibrary(void *handle)
#else /* JAVA_SPEC_VERSION >= 15 */
jobject JNICALL JVM_UnloadLibrary(jint arg0)
#endif /* JAVA_SPEC_VERSION >= 15 */
{
#if JAVA_SPEC_VERSION >= 15
	Trc_SC_UnloadLibrary_Entry(handle);

#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT) && (JAVA_SPEC_VERSION >= 17)
	{
		UDATA doSwitching = ((UDATA)handle) & J9_NATIVE_LIBRARY_SWITCH_MASK;
		handle = (void *)(((UDATA)handle) ^ doSwitching);
	}
#endif /* defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT) && (JAVA_SPEC_VERSION >= 17) */

#if defined(WIN32)
	FreeLibrary((HMODULE)handle);
#elif defined(J9ZOS390) /* defined(WIN32) */
	PORT_ACCESS_FROM_JAVAVM(BFUjavaVM);
	/* Call j9sl_close_shared_library to handle potential 31-bit interoperability handles. */
	j9sl_close_shared_library((UDATA)handle);
#elif defined(J9UNIX) /* defined(J9ZOS390) */
	dlclose(handle);
#else /* defined(J9UNIX) */
#error "Please implement jvm.c:JVM_UnloadLibrary(void *handle)"
#endif /* defined(WIN32) */
	Trc_SC_UnloadLibrary_Exit();
#else /* JAVA_SPEC_VERSION >= 15 */
	assert(!"JVM_UnloadLibrary() stubbed!");
	return NULL;
#endif /* JAVA_SPEC_VERSION >= 15 */
}

#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT) && (JAVA_SPEC_VERSION >= 17)
/**
 * Validates the JNI library for offload.
 * If successful, returns a library handle which may be modified from the handle passed in,
 * otherwise returns NULL.
 *
 * @param libName a null terminated string containing the library name.
 * @param handle the handle of a JNI library providing the libName.
 * @param isStatic indicates if the library is a static JNI library.
 *
 * @return the shared library's handle if successful, which may be modified, NULL if out of memory.
 *
 * DLL: jvm
 */
void * JNICALL
JVM_ValidateJNILibrary(const char *libName, void *handle, jboolean isStatic)
{
	void *result = NULL;
	UDATA doSwitching = 0;
	PORT_ACCESS_FROM_JAVAVM(BFUjavaVM);

	Trc_SC_ValidateJNILibrary_Entry(libName, handle, isStatic);

	doSwitching = validateLibrary(BFUjavaVM, libName, (UDATA)handle, isStatic);
	result = (void *)(((UDATA)handle) | doSwitching);
	/* Ensure there are no switching bits out of range of the mask. */
	Assert_SC_true((UDATA)handle == (((UDATA)result) & ~(UDATA)J9_NATIVE_LIBRARY_SWITCH_MASK));

	Trc_SC_ValidateJNILibrary_Exit(result);
	return result;
}
#endif /* defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT) && (JAVA_SPEC_VERSION >= 17) */


/**
 *  Returns a pointer to a function specified by the string
 *  functionName within a given library specified by handle.
 *
 *  @param handle the DLL's handle.
 *  @param functionName null terminated function name.
 *
 *  @returns a pointer to the given function.
 *
 *	DLL: jvm
 */

/* NOTE THIS IS NOT REQUIRED FOR 1.4 */

void* JNICALL
JVM_FindLibraryEntry(void* handle, const char *functionName)
{
	void *result = NULL;
#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT) && (JAVA_SPEC_VERSION >= 17)
	UDATA doSwitching = ((UDATA)handle) & J9_NATIVE_LIBRARY_SWITCH_MASK;
	BOOLEAN decorate = FALSE;
#endif /* defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT) && (JAVA_SPEC_VERSION >= 17) */

	Trc_SC_FindLibraryEntry_Entry(handle, functionName);

#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT) && (JAVA_SPEC_VERSION >= 17)
	handle = (void *)(((UDATA)handle) ^ doSwitching);
#define JNI_PREFIX "Java_"
	decorate = (0 != doSwitching) && (0 == strncmp(JNI_PREFIX, functionName, LITERAL_STRLEN(JNI_PREFIX)));
#undef JNI_PREFIX
#endif /* defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT) && (JAVA_SPEC_VERSION >= 17) */

#if defined(WIN32)
	result = GetProcAddress ((HINSTANCE)handle, (LPCSTR)functionName);
#elif defined(J9ZOS390) /* defined(WIN32) */
	PORT_ACCESS_FROM_JAVAVM(BFUjavaVM);
	/* Call j9sl_lookup_name to handle potential 31-bit interoperability targets. */
	j9sl_lookup_name((UDATA)handle, (char *)functionName, (void *)&result, "");
#elif defined(J9UNIX) /* defined(J9ZOS390) */
	result = (void *)dlsym((void *)handle, (char *)functionName);
#else /* defined(J9UNIX) */
#error "Please implement jvm.c:JVM_FindLibraryEntry(void* handle, const char *functionName)"
#endif /* defined(WIN32) */

#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT) && (JAVA_SPEC_VERSION >= 17)
	if ((NULL != result) && decorate) {
		UDATA newResult = ((UDATA)result) | doSwitching;
		/* Ensure there are no switching bits out of range of the mask. */
		Assert_SC_true(((UDATA)result) == (newResult & ~(UDATA)J9_NATIVE_LIBRARY_SWITCH_MASK));
		result = (void *)newResult;
	}
#endif /* defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT) && (JAVA_SPEC_VERSION >= 17) */

	Trc_SC_FindLibraryEntry_Exit(result);

	return result;
}


/**
 * JVM_SetLength
 */
jint JNICALL
JVM_SetLength(jint fd, jlong length)
{
	jint result;

	Trc_SC_SetLength_Entry(fd, length);

	if (fd == -1) {
		Trc_SC_SetLength_bad_descriptor();
		return -1;
	}

#if defined(WIN32_IBMC)
	printf("_JVM_SetLength@12 called but not yet implemented. Exiting.");
	exit(43);
#elif defined(WIN32) /* defined(WIN32_IBMC) */
	result = _chsize(fd, (long)length);
#elif defined(J9UNIX) && !defined(J9ZTPF) && !defined(OSX) /* defined(WIN32_IBMC) */
	result = ftruncate64(fd, length);
#elif defined(J9ZOS390) || defined(J9ZTPF) || defined(OSX) /* defined(WIN32_IBMC) */
	/* ftruncate64 is unsupported on OSX. */
	result = ftruncate(fd, length);
#else /* defined(WIN32_IBMC) */
#error "Please provide an implementation of jvm.c:JVM_SetLength(jint fd, jlong length)"
#endif /* defined(WIN32_IBMC) */

	Trc_SC_SetLength_Exit(result);

	return result;
}


/**
* JVM_Write
*/
jint JNICALL
JVM_Write(jint descriptor, const char* buffer, jint length)
{
	PORT_ACCESS_FROM_JAVAVM(BFUjavaVM);
	jint result = 0;

	Trc_SC_Write_Entry(descriptor, buffer, length);

	if (descriptor == -1) {
		Trc_SC_Write_bad_descriptor();
		return JVM_IO_ERR;
	}

#ifndef J9ZOS390
	/* This code SHOULD be for Windows only, but is safe everywhere except z/OS
	 * (see CMVC 99667 for the fun details of z/OS and POSIX File-handles):
	 * Map stdout, stderr to the port library as using the
	 * C library causes CR/LF translation and CR LF turns into CR CR LF.
	 */
	if ( (descriptor == 1) || (descriptor == 2) ) {
		IDATA retval = j9file_write(descriptor, (char *)buffer, length);
		if(retval<0) {
			result = -1;  /* callers seem to expect -1 on failure */
		} else {
			result = (jint)retval;
			Assert_SC_true(retval == (IDATA)result);
		}
	} else
#endif
	{
		/* CMVC 178203 - Restart system calls interrupted by EINTR */
		do {
			result = write(descriptor, buffer, length);
		} while ((-1 == result) && (EINTR == errno));
	}

	Trc_SC_Write_Exit(result);

	return result;
}


/**
 * JVM_Close
 */
jint JNICALL
JVM_Close(jint descriptor)
{
	jint result = 0;

	Trc_SC_Close_Entry(descriptor);

	if (descriptor == -1) {
		Trc_SC_Close_bad_descriptor();
		return JVM_IO_ERR;
	}

	/* Ignore close of stdin, when
	 * JVM_Read/Write mapped to use the port library
	 * api/java_io/FileInputStream/index.html#CtorFD
	 * also ignore close of stdout and stderr */

	if (descriptor >= 0 && descriptor <= 2) {
		Trc_SC_Close_std_descriptor();
		return 0;
	}

	result = close(descriptor);

	Trc_SC_Close_Exit(result);

	return result;
}


/**
 * JVM_Available
 */
jint JNICALL
JVM_Available(jint descriptor, jlong* bytes)
{
	jlong curr = 0;
	jlong end = 0;

	/* On OSX, stat64 and fstat64 are deprecated.
	 * Thus, stat and fstat are used on OSX.
	 */
#if defined(J9UNIX) && !defined(J9ZTPF) && !defined(OSX)
	struct stat64 tempStat;
#endif /* defined(J9UNIX) && !defined(J9ZTPF) && !defined(OSX) */
#if defined(J9ZOS390) || defined(J9ZTPF) || defined(OSX)
	struct stat tempStat;
#endif /* defined(J9ZOS390) || defined(J9ZTPF) || defined(OSX) */
#if defined(LINUX)
	loff_t longResult = 0;
#endif

	Trc_SC_Available_Entry(descriptor, bytes);

	if (descriptor == -1) {
		Trc_SC_Available_bad_descriptor();
		*bytes = 0;
		return JNI_FALSE;
	}

#if defined(WIN32) && !defined(__IBMC__)
	curr = _lseeki64(descriptor, 0, SEEK_CUR);
	if (curr==-1L) {
		if (descriptor == 0) {
			/* Failed on stdin, check input queue. */
			DWORD result = 0;
			if (GetNumberOfConsoleInputEvents(GetStdHandle(STD_INPUT_HANDLE), &result)) {
				*bytes = result;
			} else {
				*bytes = 0;
			}
			Trc_SC_Available_Exit(1, *bytes);
			return 1;
		}
		Trc_SC_Available_lseek_failed(descriptor);
		return 0;		/* Returning zero causes JNU_ThrowIOExceptionWithLastError() */
	}
	end = _lseeki64(descriptor, 0, SEEK_END);
	_lseeki64(descriptor, curr, SEEK_SET);
#else
		if (J9FSTAT(descriptor, &tempStat) == -1) {
			Trc_SC_Available_fstat_failed(descriptor, errno);
			*bytes = 0;
			return 0;
		}
		else if (S_ISFIFO(tempStat.st_mode) || S_ISSOCK(tempStat.st_mode) || S_ISCHR(tempStat.st_mode)) {
			/*
				arg3 is the third argument to ioctl.  The type of this argument is dependent upon the information
				being requested.  FIONREAD specifies that the argument be a pointer to an int
			*/
			int arg3FIONREAD = 0;

#if defined(J9ZOS390)
			/* CMVC 100066:  FIONREAD on z/OS only works for sockets so we need to use fstat, instead */
			if (!S_ISSOCK(tempStat.st_mode)) {
				/* we already performed the stat, so just return the st_size */
				*bytes = tempStat.st_size;
				Trc_SC_Available_Exit(1, *bytes);
				return 1;
			}
#endif
			if (ioctl(descriptor, FIONREAD, &(arg3FIONREAD)) == -1) {
				if (descriptor == 0) {
					/* Failed on stdin, just return st_size. */
					*bytes = tempStat.st_size;
					Trc_SC_Available_Exit(1, *bytes);
					return 1;
				}
#if (defined(LINUX) && !defined(J9ZTPF)) || defined(AIXPPC) || defined(J9ZOS390)
				else {
					struct pollfd pollOne;
					int  ret = 0;
					pollOne.fd = descriptor;
					pollOne.events = POLLRDNORM | POLLRDBAND | POLLPRI;
					pollOne.revents = 0;
					if (-1 != poll(&pollOne, 1, 0)) {
						/* poll succeeded (-1 is failure) */
						if(0 != (pollOne.events & pollOne.revents)) {
							/* if the one descriptor we were looking at returns a modified revents
									which matches a read operation, return at least one byte readable */
							*bytes = 1;
							Trc_SC_Available_Exit(1, *bytes);
							return 1;
						} else {
							/* none of the events are ready so this is 0 bytes readable */
							*bytes = 0;
							Trc_SC_Available_Exit(1, *bytes);
							return 1;
						}
					} else {
						/* poll failed so use a poll failure trace point and return failure */
						Trc_SC_Available_poll_failed(descriptor, errno);
						*bytes = 0;
						return 0;
					}
				}
#endif
				Trc_SC_Available_ioctl_failed(descriptor, errno);
				*bytes = 0;
				return 0;
			}

			*bytes = (jlong) arg3FIONREAD;
			Trc_SC_Available_Exit(1, *bytes);
			return 1;
		}
#if defined(LINUX) && !defined(J9VM_ENV_DATA64)
#if __GLIBC_PREREQ(2,4)
	/* glibc 2.4 (sles 10) and on provide lseek64() */
	curr = lseek64(descriptor, 0, SEEK_CUR);
#else
	/* CMVC 104382:  Linux lseek uses native word size off_t so we need to use the 64-bit _llseek on 32-bit Linux.  AIX and z/OS use 64-bit API implicitly when we define _LARGE_FILES. */
	curr = _llseek(descriptor, 0, 0, &longResult, SEEK_CUR);
	if (0 == curr) {
		/* the call was successful so set the result to what we read */
		curr = (jlong) longResult;
	}
#endif
#else	/* defined(LINUX) && !defined(J9VM_ENV_DATA64) */
	curr = lseek(descriptor, 0, SEEK_CUR);
#endif /*!defined (LINUX) || defined(J9VM_ENV_DATA64) */
	if (curr==-1L) {
		if (descriptor == 0) {
			/* Failed on stdin, just return 0. */
			*bytes = 0;
			Trc_SC_Available_Exit(1, *bytes);
			return 1;
		}
		Trc_SC_Available_lseek_failed(descriptor);
		return 0;		/* Returning zero causes JNU_ThrowIOExceptionWithLastError() */
	}

	end = tempStat.st_size;     /* size from previous fstat                                      */
#endif

	*bytes = (end-curr);

	Trc_SC_Available_Exit(1, *bytes);

	return 1;
}



/**
 * JVM_Lseek
 */
jlong JNICALL
JVM_Lseek(jint descriptor, jlong bytesToSeek, jint origin)
{
	jlong result = 0;
#if defined (LINUX)
	loff_t longResult = 0;
#endif

	Trc_SC_Lseek_Entry(descriptor, bytesToSeek, origin);

	if (descriptor == -1) {
		Trc_SC_Lseek_bad_descriptor();
		return JVM_IO_ERR;
	}

#if defined(WIN32)
#ifdef __IBMC__
	result = lseek(descriptor, (long) bytesToSeek, origin);
#else
	result = _lseeki64(descriptor, bytesToSeek, origin);
#endif
#elif defined(J9UNIX) || defined(J9ZOS390) /* defined(WIN32) */
#if defined(LINUX) && !defined(J9VM_ENV_DATA64)

#if __GLIBC_PREREQ(2,4)
	/* glibc 2.4 (sles 10) and on provide lseek64() */
	result = lseek64(descriptor, bytesToSeek, origin);
#else
	/* CMVC 104382:  Linux lseek uses native word size off_t so we need to use the 64-bit _llseek on 32-bit Linux.  AIX and z/OS use 64-bit API implicitly when we define _LARGE_FILES. */
	result = _llseek(descriptor, (unsigned long) ((bytesToSeek >> 32) & 0xFFFFFFFF), (unsigned long) (bytesToSeek & 0xFFFFFFFF), &longResult, origin);
	if (0 == result) {
		/* the call was successful so set the result to what we read */
		result = (jlong) longResult;
	}
#endif

#else /* defined(LINUX) && !defined(J9VM_ENV_DATA64) */
	result = lseek(descriptor, (off_t) bytesToSeek, origin);
#endif /* !defined(LINUX) ||defined(J9VM_ENV_DATA64) */
#else /* defined(WIN32) */
#error No JVM_Lseek provided
#endif /* defined(WIN32) */

	Trc_SC_Lseek_Exit(result);

	return result;
}


/**
 * JVM_Read
 */
jint JNICALL
JVM_Read(jint descriptor, char *buffer, jint bytesToRead)
{
	PORT_ACCESS_FROM_JAVAVM(BFUjavaVM);
	jint result = 0;

	Trc_SC_Read_Entry(descriptor, buffer, bytesToRead);

	if (descriptor == -1) {
		Trc_SC_Read_bad_descriptor();
		return -1;
	}

#ifndef J9ZOS390
	/* This code SHOULD be for Windows only, but is safe everywhere except z/OS
	 * (see CMVC 99667 for the fun details of z/OS and POSIX File-handles):
	 * Map stdin to the port library, so we avoid any CR/LF translation.
	 * See JVM_WRITE
	 */
	if (descriptor == 0) {
		IDATA charsRead = j9tty_get_chars(buffer, bytesToRead);
		result = (jint)charsRead;
		Assert_SC_true(charsRead == (IDATA)result);
	} else
#endif
	{
		/* CMVC 178203 - Restart system calls interrupted by EINTR */
		do {
			result = read(descriptor, buffer, bytesToRead);
		} while ((-1 == result) && (EINTR == errno));
	}

	Trc_SC_Read_Exit(result, errno);

	return result;
}


/**
 * JVM_Open method
 */
jint JNICALL
JVM_Open(const char* filename, jint flags, jint mode)
{
	int errorVal = 0;
	jint returnVal = 0;

	/* On OSX, stat64 and fstat64 are deprecated.
	 * Thus, stat and fstat are used on OSX.
	 */
#if defined(J9UNIX) && !defined(J9ZTPF) && !defined(OSX)
	struct stat64 tempStat;
	int doUnlink = 0;
#endif /* defined(J9UNIX) && !defined(J9ZTPF) && !defined(OSX) */
#if defined(J9ZOS390) || defined(J9ZTPF) || defined(OSX)
	struct stat tempStat;
	int doUnlink = 0;
#endif /* defined(J9ZOS390) || defined(J9ZTPF) || defined(OSX) */

	Trc_SC_Open_Entry(filename, flags, mode);

#define JVM_EEXIST -100

#ifdef WIN32
#ifdef __IBMC__
#define EXTRA_OPEN_FLAGS O_NOINHERIT | O_BINARY
#else
#define EXTRA_OPEN_FLAGS _O_NOINHERIT | _O_BINARY
#endif
#endif

#if defined(J9UNIX) || defined(J9ZOS390)
#if defined(OSX) || defined(J9ZTPF)
#define EXTRA_OPEN_FLAGS 0
#else
#define EXTRA_OPEN_FLAGS O_LARGEFILE
#endif /* defined(OSX) || defined(J9ZTPF) */

#ifndef O_DSYNC
#define O_DSYNC O_SYNC
#endif

	doUnlink = (flags & O_TEMPORARY);
#if !defined(J9ZTPF)
	flags &= (O_CREAT | O_APPEND | O_RDONLY | O_RDWR | O_TRUNC | O_WRONLY | O_EXCL | O_NOCTTY | O_NONBLOCK | O_NDELAY | O_SYNC | O_DSYNC);
#else /* !defined(J9ZTPF) */
	flags &= (O_CREAT | O_APPEND | O_RDONLY | O_RDWR | O_TRUNC | O_WRONLY | O_EXCL | O_NOCTTY | O_NONBLOCK | O_SYNC | O_DSYNC);
#endif /* !defined(J9ZTPF) */
#endif /* defined(J9UNIX) || defined(J9ZOS390) */

	/* For some reason, although JVM_NativePath is called on the filenames, some of them seem to
		get mangled between JVM_NativePath being called and JVM_open being called */
	filename = JVM_NativePath((char *)filename);

#if defined(J9UNIX) || defined(J9ZOS390)
	do {
		errorVal = 0;
#endif /* defined(J9UNIX) || defined(J9ZOS390) */

#ifdef J9OS_I5
	returnVal = Xj9Open_JDK6((char *)filename, (flags | EXTRA_OPEN_FLAGS), mode);
#else
	returnVal = open(filename, (flags | EXTRA_OPEN_FLAGS), mode);
#endif
	if (-1 == returnVal) {
		errorVal = errno;
	}

#if defined(J9UNIX) || defined(J9ZOS390)
	/* Unix open() call does not reject directories, so extra checks required */
	if ((returnVal>=0) && (J9FSTAT(returnVal, &tempStat)==-1)) {
		Trc_SC_Open_fstat64(filename);
		close(returnVal);
		return -1;
	}
	if ((returnVal>=0) && S_ISDIR(tempStat.st_mode)) {
		char buf[1];

		Trc_SC_Open_isDirectory(filename);

		/* force errno to be EISDIR in case JVM_GetLastErrorString is called */
		errno = EISDIR;

		close(returnVal);
		return -1;
	}

	/* On unices, open() may return EAGAIN or EINTR, and should then be re-invoked */
	}
	while ((-1 == returnVal) && ((EAGAIN == errorVal) || (EINTR == errorVal)));

	/* Unix does not have an O_TEMPORARY flag. Unlink if Sovereign O_TEMPORARY flag passed in. */
	if ((returnVal>=0) && doUnlink)
		unlink(filename);
#endif /* defined(J9UNIX) || defined(J9ZOS390) */

	if (returnVal<0) {
		Trc_SC_Open_error(filename, errorVal);
	} else {
		Trc_SC_Open_Exit(filename, returnVal);
	}

	if (returnVal>=0)
		return returnVal;
	else if (EEXIST == errorVal)
		return JVM_EEXIST;
	else
		return -1;
}


/**
 * JVM_Sync
 */
jint JNICALL
JVM_Sync(jint descriptor)
{
	jint result;

	Trc_SC_Sync_Entry(descriptor);

	if (descriptor == -1) {
		Trc_SC_Sync_bad_descriptor();
		return -1;
	}

#if defined(WIN32)
#ifdef WIN32_IBMC
	printf("_JVM_Sync@4 called but not yet implemented. Exiting.\n");
	exit(44);
#else
	result = _commit(descriptor);
#endif
#elif defined(J9UNIX) || defined(J9ZOS390) /* defined(WIN32) */
	result = fsync(descriptor);
#else /* defined(WIN32) */
#error No JVM_Sync implementation
#endif /* defined(WIN32) */

	Trc_SC_Sync_Exit(result);

	return result;
}


/**
* Change a pathname into platform specific format and do some canonicalization such as
* removing redundant separators.  Claims to modify the pathname in-place.
* arg1 = char* = pathname
* rtn = char* = ?
*/
char* JNICALL
JVM_NativePath(char* path)
{
	char * pathIndex;
	size_t length = strlen(path);

	Trc_SC_NativePath_Entry(path);

	if (jclSeparator == '/') {
		Trc_SC_NativePath_Exit(path);
		return path; /* Do not do POSIX platforms */
	}

	/* Convert all separators to the same type */
	pathIndex = path;
	while (*pathIndex != '\0') {
		if ((*pathIndex == '\\' || *pathIndex == '/') && (*pathIndex != jclSeparator))
			*pathIndex = jclSeparator;
		pathIndex++;
	}

	/* Remove duplicate initial separators */
	pathIndex = path;
	while ((*pathIndex != '\0') && (*pathIndex == jclSeparator)) {
		pathIndex++;
	}
	if ((pathIndex > path) && (length > (size_t)(pathIndex - path)) && (*(pathIndex + 1) == ':')) {
		/* For Example '////c:\*' */
		size_t newlen = length - (pathIndex - path);
		memmove(path, pathIndex, newlen);
		path[newlen] = '\0';
	} else {
		if ((pathIndex - path > 3) && (length > (size_t)(pathIndex - path))) {
			/* For Example '////serverName\*' */
			size_t newlen = length - (pathIndex - path) + 2;
			memmove(path, pathIndex - 2, newlen);
			path[newlen] = '\0';
		}
	}

	Trc_SC_NativePath_Exit(path);
	return path;
}

/**
 * Check if a signal is used for shutdown. SIGHUP,
 * SIGINT and SIGTERM are characterized as shutdown signals.
 * Terminator.setup() registers handlers for these signals
 * during startup.
 *
 * @param sigNum Integer value of the signal
 *
 * @returns TRUE if the signal is used for shutdown
 *          FALSE if the signal is not used for shutdown
 */
static BOOLEAN
isSignalUsedForShutdown(jint sigNum)
{
	return
#if defined(SIGHUP)
		(SIGHUP == sigNum) ||
#endif /* defined(SIGHUP) */
#if defined(SIGINT)
		(SIGINT == sigNum) ||
#endif /* defined(SIGINT) */
#if defined(SIGTERM)
		(SIGTERM == sigNum) ||
#endif /* defined(SIGTERM) */
		FALSE;
}

/**
 * Check if a signal is reserved by the VM. This function is invoked from
 * JVM_*Signal functions. This function only includes signals which
 * are listed in jvm.c::signalMap. If the signal is not listed in
 * jvm.c::signalMap, then a sun.misc.Signal instance can't be created
 * for a signal value. The native JVM_*Signal functions can't be invoked
 * in the absence of a sun.misc.Signal instance.
 *
 * Synchronous signals are handled by the VM via omrsig_protect: SIGFPE,
 * SIGILL, SIGSEGV, SIGBUS and SIGTRAP. On Windows, SIGBREAK is reserved
 * by the VM to trigger dumps. On Unix/Linux platforms, SIGQUIT is
 * reserved by the VM to trigger dumps. SIGABRT is reserved by the JVM
 * for abnormal termination. SIGCHLD is reserved for internal control.
 * On zOS, SIGUSR1 is reserved by the JVM. SIGRECONFIG is reserved to
 * detect any change in the number of CPUs, processing capacity, or
 * physical memory.
 *
 * @param sigNum Integer value of the signal
 *
 * @returns TRUE if the signal is reserved by the VM
 *          FALSE if the signal is not reserved by the VM
 */
static BOOLEAN
isSignalReservedByJVM(jint sigNum)
{
	return
#if defined(SIGFPE)
		(SIGFPE == sigNum) ||
#endif /* defined(SIGFPE) */
#if defined(SIGILL)
		(SIGILL == sigNum) ||
#endif /* defined(SIGILL) */
#if defined(SIGSEGV)
		(SIGSEGV == sigNum) ||
#endif /* defined(SIGSEGV) */
#if defined(SIGBUS)
		(SIGBUS == sigNum) ||
#endif /* defined(SIGBUS) */
#if defined(SIGTRAP)
		(SIGTRAP == sigNum) ||
#endif /* defined(SIGTRAP) */
#if defined(SIGQUIT)
		(SIGQUIT == sigNum) ||
#endif /* defined(SIGQUIT) */
#if defined(SIGBREAK)
		(SIGBREAK == sigNum) ||
#endif /* defined(SIGBREAK) */
		FALSE;
}

/**
 * Raise a signal to the calling process or thread.
 *
 * isSignalReservedByJVM function lists the signals reserved by the JVM.
 * Do not raise a signal if it is reserved by the JVM and not registered
 * via JVM_RegisterSignal.
 *
 * isSignalUsedForShutdown lists all signals characterized as shutdown signals.
 * Do not raise a shutdown signal if the -Xrs or the -Xrs:async cmdnline option
 * is specified. If a shutdown signal is ignored by the OS, do not raise that
 * shutdown signal. Only raise a shutdown signal if the previous two conditions
 * are false.
 *
 * @param sigNum Integer value of the signal to be sent to the
 *               calling process or thread
 *
 * @returns JNI_TRUE if the signal is successfully raised/sent
 *          JNI_FALSE if the signal is not sent
 */
jboolean JNICALL
JVM_RaiseSignal(jint sigNum)
{
	jboolean rc = JNI_FALSE;
	J9JavaVM *javaVM = (J9JavaVM *)BFUjavaVM;
	BOOLEAN isShutdownSignal = isSignalUsedForShutdown(sigNum);
	BOOLEAN isSignalIgnored = FALSE;
	int32_t isSignalIgnoredError = 0;
	uint32_t portlibSignalFlag = 0;

	PORT_ACCESS_FROM_JAVAVM(javaVM);

	Trc_SC_RaiseSignal_Entry(sigNum);

	portlibSignalFlag = j9sig_map_os_signal_to_portlib_signal(sigNum);
	if (0 != portlibSignalFlag) {
		isSignalIgnoredError = j9sig_is_signal_ignored(portlibSignalFlag, &isSignalIgnored);
	}

	if (isSignalReservedByJVM(sigNum)) {
		/* Don't raise a signal if it is reserved by the JVM, and not
		 * registered via JVM_RegisterSignal.
		 */
	} else if (J9_ARE_ALL_BITS_SET(javaVM->sigFlags, J9_SIG_XRS_ASYNC) && isShutdownSignal) {
		/* Ignore shutdown signals if -Xrs or -Xrs:async is specified.
		 * If -Xrs:sync is specified, then raise shutdown signals.
		 */
	} else if (isShutdownSignal && ((0 == isSignalIgnoredError) && isSignalIgnored)) {
		/* Ignore shutdown signal if it is ignored by the OS. */
	} else {
		raise(sigNum);
		rc = JNI_TRUE;
	}

	Trc_SC_RaiseSignal_Exit(rc);

	return rc;
}

/**
 * Register a Java SignalHandler for a signal.
 *
 * isSignalReservedByJVM function lists the signals reserved by the JVM.
 * This function shouldn't override handlers for signals that are reserved
 * by the JVM. A user is allowed to register a native handler for the
 * reserved signals if -Xrs cmdline option is specified. This function
 * shouldn't register a Java SignalHandler for the reserved signals.
 *
 * isSignalUsedForShutdown lists all signals characterized as shutdown signals.
 * This function shouldn't register a handler for shutdown signals if
 * the -Xrs or the -Xrs:async cmdnline option is specified. If a shutdown
 * signal is ignored by the OS, this function shouldn't register a handler
 * for that shutdown signal. Only register a handler for shutdown signals if
 * the previous two conditions are false.
 *
 * If handler has the special value of J9_PRE_DEFINED_HANDLER_CHECK (2),
 * then the predefinedHandlerWrapper is registered with asynchSignalReporterThread
 * in OMR. mainASynchSignalHandler notifies asynchSignalReporterThread whenever a
 * signal is received. If the old OS handler is a main signal handler, then a
 * Java signal handler was previously registered with the signal. In this case,
 * J9_USE_OLD_JAVA_SIGNAL_HANDLER must be returned. sun.misc.Signal.handle(...) or
 * jdk.internal.misc.Signal.handle(...) will return the old Java signal handler if
 * JVM_RegisterSignal returns J9_USE_OLD_JAVA_SIGNAL_HANDLER. Otherwise, an instance
 * of NativeHandler is returned with oldHandler's address stored in
 * NativeHandler.handler.
 *
 * Java 8 - NativeHandler is sun.misc.NativeSignalHandler
 * Java 9 - NativeHandler is jdk.internal.misc.Signal.NativeHandler
 *
 * @param sigNum Integer value of the signal to be sent to the
 *                  calling process or thread
 * @param handler New handler to be associated to the signal
 *
 * @returns address of the old signal handler on success
 *          J9_SIG_ERR (-1) in case of error
 */
void* JNICALL
JVM_RegisterSignal(jint sigNum, void *handler)
{
	void *oldHandler = (void *)J9_SIG_ERR;
	J9JavaVM *javaVM = (J9JavaVM *)BFUjavaVM;
	J9InternalVMFunctions *vmFuncs = javaVM->internalVMFunctions;
	J9VMThread *currentThread = vmFuncs->currentVMThread(javaVM);
	BOOLEAN isShutdownSignal = isSignalUsedForShutdown(sigNum);
	BOOLEAN isSignalIgnored = FALSE;
	int32_t isSignalIgnoredError = 0;
	uint32_t portlibSignalFlag = 0;

	PORT_ACCESS_FROM_JAVAVM(javaVM);

	Trc_SC_RegisterSignal_Entry(currentThread, sigNum, handler);

	portlibSignalFlag = j9sig_map_os_signal_to_portlib_signal(sigNum);
	if (0 != portlibSignalFlag) {
		isSignalIgnoredError = j9sig_is_signal_ignored(portlibSignalFlag, &isSignalIgnored);
	}

	if (isSignalReservedByJVM(sigNum)) {
		/* Return error if signal is reserved by the JVM. oldHandler is initialized to
		 * J9_SIG_ERR.
		 */
		goto exit;
	} else if (J9_ARE_ANY_BITS_SET(javaVM->sigFlags, J9_SIG_XRS_ASYNC) && isShutdownSignal) {
		/* Don't register a handler for shutdown signals if -Xrs or -Xrs:async is specified.
		 * If -Xrs:sync is specified, then register a handler for shutdown signals. oldHandler
		 * is initialized to J9_SIG_ERR.
		 */
		goto exit;
	} else if (isShutdownSignal && ((0 == isSignalIgnoredError) && isSignalIgnored)) {
		/* Ignore shutdown signal if it is ignored by the OS. */
		oldHandler = (void *)J9_SIG_IGNORED;
		goto exit;
	} else {
		/* Register the signal. */
		IDATA isHandlerRegistered = 0;
		if ((void *)J9_PRE_DEFINED_HANDLER_CHECK == handler) {
			isHandlerRegistered = vmFuncs->registerPredefinedHandler(javaVM, sigNum, &oldHandler);
		} else {
			isHandlerRegistered = vmFuncs->registerOSHandler(javaVM, sigNum, handler, &oldHandler);
		}
		if (0 != isHandlerRegistered) {
			Trc_SC_RegisterSignal_FailedToRegisterHandler(currentThread, sigNum, handler, oldHandler);
		}
	}

	/* If oldHandler is a main handler, then a Java signal handler was previously registered with
	 * the signal. sun.misc.Signal.handle(...) or jdk.internal.misc.Signal.handle(...) will return
	 * the old Java signal handler if JVM_RegisterSignal returns J9_USE_OLD_JAVA_SIGNAL_HANDLER.
	 * Otherwise, an instance of NativeHandler is returned with the oldHandler's address stored in
	 * NativeHandler.handler. In Java 8, NativeHandler.handle() will invoke JVM_RegisterSignal using
	 * NativeHandler.handler, which represents the address of the native signal handler function. In
	 * Java 9, NativeHandler.handle() will throw UnsupportedOperationException.
	 */
	if (j9sig_is_main_signal_handler(oldHandler)) {
		oldHandler = (void *)J9_USE_OLD_JAVA_SIGNAL_HANDLER;
	}

exit:
	Trc_SC_RegisterSignal_Exit(currentThread, oldHandler);
	return oldHandler;
}


/**
 * Return the integer value of the signal given the name of the
 * signal.
 *
 * @param  sigName Name of the signal
 *
 * @returns Integer value of the signal on success
 *          J9_SIG_ERR (-1) on failure
 */
jint JNICALL
JVM_FindSignal(const char *sigName)
{
	const J9SignalMapping *mapping = NULL;
	jint signalValue = J9_SIG_ERR;
	BOOLEAN nameHasSigPrefix = FALSE;
	const char *fullSigName = sigName;
#if !defined(WIN32)
	char nameWithSIGPrefix[J9_SIGNAME_BUFFER_LENGTH] = {0};
#endif /* !defined(WIN32) */

	Trc_SC_FindSignal_Entry(sigName);

	if (NULL != sigName) {
		size_t sigPrefixLength = sizeof(J9_SIG_PREFIX) - 1;

#if !defined(WIN32)
		if (0 != strncmp(sigName, J9_SIG_PREFIX, sigPrefixLength)) {
			/* nameWithSIGPrefix is a char buffer of length J9_SIGNAME_BUFFER_LENGTH.
			 * We are concatenating SIG + sigName, and storing the new string in
			 * nameWithSIGPrefix. We want to make sure that the concatenated string
			 * fits inside nameWithSIGPrefix. We also know that all known signal names
			 * have less than J9_SIGNAME_BUFFER_LENGTH chars. If the concatenated
			 * string doesn't fit inside nameWithSIGPrefix, then we can consider signal
			 * name to be unknown. In sigNameLength, +1 is for the NULL terminator.
			 */
			size_t sigNameLength = sigPrefixLength + strlen(sigName) + 1;
			if (sigNameLength <= J9_SIGNAME_BUFFER_LENGTH) {
				strcpy(nameWithSIGPrefix, J9_SIG_PREFIX);
				strcat(nameWithSIGPrefix, sigName);
				fullSigName = nameWithSIGPrefix;
			} else {
				goto exit;
			}
		}
#endif /* !defined(WIN32) */

		for (mapping = signalMap; NULL != mapping->signalName; mapping++) {
			if (0 == strcmp(fullSigName, mapping->signalName)) {
				signalValue = mapping->signalValue;
				break;
			}
		}
	}

#if !defined(WIN32)
exit:
#endif /* !defined(WIN32) */
	Trc_SC_FindSignal_Exit(signalValue);
	return signalValue;
}




/**
 * Gets last O/S or C error
 */
jint JNICALL
JVM_GetLastErrorString(char* buffer, jint length)
{
	int savedErrno = errno;
#ifdef WIN32
	DWORD errorCode = GetLastError();
#endif
	jint retVal = 0;

	Trc_SC_GetLastErrorString_Entry(buffer, length);

	memset(buffer, 0, length);

#ifdef WIN32
	if (errorCode) {
		retVal = formatErrorMessage(errorCode, buffer, length);
	} else
#endif
	if (savedErrno) {
		PORT_ACCESS_FROM_JAVAVM(BFUjavaVM);
		I_32 sigRC;
		UDATA fnRC;

		sigRC = j9sig_protect(protectedStrerror, (void *) (IDATA) savedErrno, strerrorSignalHandler, NULL, J9PORT_SIG_FLAG_SIGALLSYNC | J9PORT_SIG_FLAG_MAY_RETURN, &fnRC);
		if (sigRC == 0) {
			strncat(buffer, (char *) fnRC, (length-1));
			/* casting to jint is safe since we know that the result is <= length */
			retVal = (jint)strlen(buffer);
		}
	}

	Trc_SC_GetLastErrorString_Exit(retVal, buffer);

	return retVal;
}



/**
 * jobject JVM_CurrentClassLoader(JNIEnv *)
 */
jobject JNICALL
JVM_CurrentClassLoader(JNIEnv *env)
{
	jobject result;

	Trc_SC_CurrentClassLoader_Entry(env);

	result = (*env)->CallStaticObjectMethod(env, jlClass, currentClassLoaderMID);

	/* CMVC 95169: ensure that the result is a well defined error value if an exception occurred */
	if ((*env)->ExceptionCheck(env)) {
		result = NULL;
	}

	Trc_SC_CurrentClassLoader_Exit(env, result);

	return result;
}


/**
 * jclass JVM_CurrentLoadedClass(JNIEnv *)
 */
jclass JNICALL
JVM_CurrentLoadedClass(JNIEnv *env)
{
	jobject result;

	Trc_SC_CurrentLoadedClass_Entry(env);

	result = (*env)->CallStaticObjectMethod(env, jlClass, currentLoadedClassMID);

	/* CMVC 95169: ensure that the result is a well defined error value if an exception occurred */
	if ((*env)->ExceptionCheck(env)) {
		result = NULL;
	}

	Trc_SC_CurrentLoadedClass_Exit(env, result);

	return result;
}




/**
 * jint JVM_ClassLoaderDepth(JNIEnv *)
 */
jint JNICALL
JVM_ClassLoaderDepth(JNIEnv *env)
{
	jint result;

	Trc_SC_ClassLoaderDepth_Entry(env);

	result = (*env)->CallStaticIntMethod(env, jlClass, classLoaderDepthMID);

	/* CMVC 95169: ensure that the result is a well defined error value if an exception occurred */
	if ((*env)->ExceptionCheck(env)) {
		result = -1;
	}

	Trc_SC_ClassLoaderDepth_Exit(env, result);

	return result;
}


/**
 * jint JVM_ClassDepth(JNIEnv *, jstring)
 */
jint JNICALL
JVM_ClassDepth(JNIEnv *env, jstring name)
{
	jint result;

	Trc_SC_ClassDepth_Entry(env, name);

	result = (*env)->CallStaticIntMethod(env, jlClass, classDepthMID, name);

	/* CMVC 95169: ensure that the result is a well defined error value if an exception occurred */
	if ((*env)->ExceptionCheck(env)) {
		result = -1;
	}

	Trc_SC_ClassDepth_Exit(env, result);

	return result;
}


/**
 * Method stub for method not yet implemented.
 */
void JNICALL
JVM_TraceInstructions(jboolean on)
{
	Trc_SC_TraceInstructions(on);
}


/**
 * Method stub for method not yet implemented.
 */
void JNICALL
JVM_TraceMethodCalls(jboolean on)
{
	Trc_SC_TraceMethodCalls(on);
}






/**
 * Destroy the monitor
 *
 * @param mon pointer of the monitor
 */
void JNICALL
JVM_RawMonitorDestroy(void* mon)
{
	Trc_SC_RawMonitorDestroy_Entry(mon);

	f_monitorDestroy((omrthread_monitor_t) mon);

	Trc_SC_RawMonitorDestroy_Exit();
}


/**
 * Monitor enter
 *
 * @param mon pointer of the monitor
 * @return 0
 */
jint JNICALL
JVM_RawMonitorEnter(void* mon)
{
	Trc_SC_RawMonitorEnter_Entry(mon);

	f_monitorEnter((omrthread_monitor_t)mon);

	Trc_SC_RawMonitorEnter_Exit();

	return 0;
}


/**
 * Monitor exit
 *
 * @param mon pointer of the monitor
 */
void JNICALL
JVM_RawMonitorExit(void* mon)
{
	Trc_SC_RawMonitorExit_Entry(mon);

	f_monitorExit((omrthread_monitor_t)mon);

	Trc_SC_RawMonitorExit_Exit();
}


/**
 * Create a new monitor
 *
 * @return pointer of the monitor
 */
void* JNICALL
JVM_RawMonitorCreate(void)
{
	omrthread_monitor_t newMonitor;

	Trc_SC_RawMonitorCreate_Entry();

	if (f_monitorInit(&newMonitor, 0, "JVM_RawMonitor")) {
		Trc_SC_RawMonitorCreate_Error();
		printf("error initializing raw monitor\n");
		exit(1);
	}

	Trc_SC_RawMonitorCreate_Exit(newMonitor);
	return (void *) newMonitor;
}


/**
 * JVM_MonitorWait
 */
void JNICALL JVM_MonitorWait(JNIEnv *env, jobject anObject, jlong timeout) {
	Trc_SC_MonitorWait_Entry(env, anObject, timeout);

	(*env)->CallVoidMethod(env, anObject, waitMID, timeout);

	Trc_SC_MonitorWait_Exit(env);
}


/**
 * JVM_MonitorNotify
 */
void JNICALL JVM_MonitorNotify(JNIEnv *env, jobject anObject) {
	Trc_SC_MonitorNotify_Entry(env, anObject);

	(*env)->CallVoidMethod(env, anObject, notifyMID);

	Trc_SC_MonitorNotify_Exit(env);
}


/**
 * JVM_MonitorNotifyAll
 */
void JNICALL JVM_MonitorNotifyAll(JNIEnv *env, jobject anObject) {
	Trc_SC_MonitorNotifyAll_Entry(env, anObject);

	(*env)->CallVoidMethod(env, anObject, notifyAllMID);

	Trc_SC_MonitorNotifyAll_Exit(env);

}


/**
 * JVM_Accept
 */
jint JNICALL
JVM_Accept(jint descriptor, struct sockaddr* address, int* length)
{
	jint retVal;

	Trc_SC_Accept_Entry(descriptor, address, length);

#if defined(AIXPPC)
	{
		int returnVal=0;
		fd_set fdset;
		struct timeval tval;
		socklen_t socklen = (socklen_t)*length;

		tval.tv_sec = 1;
		tval.tv_usec = 0;

		do {
			FD_ZERO(&fdset);
			FD_SET((u_int)descriptor, &fdset);

			returnVal = select(descriptor+1, &fdset, 0, 0, &tval);
		} while(returnVal == 0);

		do {
			retVal = accept(descriptor, address, &socklen);
		} while ((-1 == retVal) && (EINTR == errno));

		*length = (int)socklen;
	}
#elif defined (WIN32)
	{
		SOCKET socketResult = accept(descriptor, address, length);
		retVal = (jint)socketResult;
		Assert_SC_true(socketResult == (SOCKET)retVal);
	}
#else
	{
		socklen_t socklen = (socklen_t)*length;
		do {
			retVal = accept(descriptor, address, &socklen);
		} while ((-1 == retVal) && (EINTR == errno));
		*length = (int)socklen;
	}
#endif

	Trc_SC_Accept_Exit(retVal, *length);

	return retVal;
}


/**
 * JVM_Connect
 */
jint JNICALL
JVM_Connect(jint descriptor, const struct sockaddr*address, int length)
{
	jint retVal;

	Trc_SC_Connect_Entry(descriptor, address, length);

#if defined (WIN32)
	retVal = connect(descriptor, address, length);
#else /* defined (WIN32) */
	do {
		retVal = connect(descriptor, address, length);
	} while ((-1 == retVal) && (EINTR == errno));
#endif /* defined (WIN32) */

	Trc_SC_Connect_Exit(retVal);

	return retVal;
}


/**
 * JVM_CurrentTimeMillis
 */
jlong JNICALL
JVM_CurrentTimeMillis(JNIEnv *env, jint unused1)
{
	jlong result;

	if (env != NULL) {
		PORT_ACCESS_FROM_ENV(env);

		Trc_SC_CurrentTimeMillis_Entry(env, unused1);

		result = (jlong) j9time_current_time_millis();

		Trc_SC_CurrentTimeMillis_Exit(env, result);
	} else {
		PORT_ACCESS_FROM_JAVAVM(BFUjavaVM);

		result = (jlong) j9time_current_time_millis();
	}

	return result;
}



/**
 * Initialized in port library startup.
 */
jint JNICALL
JVM_InitializeSocketLibrary(void)
{
	Trc_SC_InitializeSocketLibrary();
	return 0;
}


/**
 * JVM_Listen
 */
jint JNICALL JVM_Listen(jint descriptor, jint count) {
	jint retVal;

	Trc_SC_Listen_Entry(descriptor, count);

	retVal = listen(descriptor, count);

	Trc_SC_Listen_Exit(retVal);

	return retVal;
}


/**
 * JVM_Recv
 */
jint JNICALL
JVM_Recv(jint descriptor, char* buffer, jint length, jint flags)
{
	jint retVal;

#ifdef AIXPPC
	int returnVal=0;
	fd_set fdset;
	struct timeval tval;
#endif

	Trc_SC_Recv_Entry(descriptor, buffer, length, flags);

#ifdef AIXPPC
	tval.tv_sec = 1;
	tval.tv_usec = 0;

	do {
		FD_ZERO(&fdset);
		FD_SET((u_int)descriptor, &fdset);

		returnVal = select(descriptor+1, &fdset, 0, 0, &tval);
	} while(returnVal == 0);
#endif

#ifdef WIN32
	retVal = recv(descriptor, buffer, (int)length, flags);
#else
	do {
		retVal = recv(descriptor, buffer, (size_t)length, flags);
	} while ((-1 == retVal) && (EINTR == errno));
#endif

	Trc_SC_Recv_Exit(retVal);

	return retVal;
}


/**
 * JVM_RecvFrom
 */
jint JNICALL
JVM_RecvFrom(jint descriptor, char* buffer, jint length, jint flags, struct sockaddr* fromAddr, int* fromLength)
{
	jint retVal;

	Trc_SC_RecvFrom_Entry(descriptor, buffer, length, flags, fromAddr, fromLength);

#if defined (WIN32)
	retVal = recvfrom(descriptor, buffer, length, flags, fromAddr, fromLength);
#else
	{
		socklen_t address_len = (socklen_t)*fromLength;
		do {
			retVal = recvfrom(descriptor, buffer, (size_t)length, flags, fromAddr, &address_len);
		} while ((-1 == retVal) && (EINTR == errno));
		*fromLength = (int)address_len;
	}
#endif

	Trc_SC_RecvFrom_Exit(retVal, *fromLength);

	return retVal;
}


/**
 * JVM_Send
 */
jint JNICALL
JVM_Send(jint descriptor, const char* buffer, jint numBytes, jint flags)
{
	jint retVal;

	Trc_SC_Send_Entry(descriptor, buffer, numBytes, flags);

#if defined (WIN32)
	retVal = send(descriptor, buffer, numBytes, flags);
#else /* defined (WIN32) */
	do {
		retVal = send(descriptor, buffer, numBytes, flags);
	} while ((-1 == retVal) && (EINTR == errno));
#endif /* defined (WIN32) */

	Trc_SC_Send_Exit(retVal);

	return retVal;
}


/**
 * JVM_SendTo
 */
jint JNICALL JVM_SendTo(jint descriptor, const char* buffer, jint length, jint flags, const struct sockaddr* toAddr, int toLength) {
	jint retVal;

	Trc_SC_SendTo_Entry(descriptor, buffer, length, flags, toAddr, toLength);

#if defined (WIN32)
	retVal = sendto(descriptor, buffer, length, flags, toAddr, toLength);
#else /* defined (WIN32) */
	do {
		retVal = sendto(descriptor, buffer, length, flags, toAddr, toLength);
	} while ((-1 == retVal) && (EINTR == errno));
#endif /* defined (WIN32) */

	Trc_SC_SendTo_Exit(retVal);

	return retVal;
}


/**
 * JVM_Socket
 */
jint JNICALL
JVM_Socket(jint domain, jint type, jint protocol)
{
	jint result;

	Trc_SC_Socket_Entry(domain, type, protocol);

#ifdef WIN32
	{
		SOCKET socketResult = socket(domain, type, protocol);
		SetHandleInformation((HANDLE)socketResult, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
		result = (jint)socketResult;
		Assert_SC_true(socketResult == (SOCKET)result);
	}
#else
	result = socket(domain, type, protocol);
#endif

	Trc_SC_Socket_Exit(result);

	return result;
}


/**
 * JVM_SocketAvailable
 * Note: Java 9 and beyond don't use this JVM method.
 *
 * @param descriptor socket file descriptor
 * @param result the number of bytes that can be read without blocking
 *
 * @return result of this JVM method, 0 for failure, 1 (or non-zero value) for success
 */
jint JNICALL
JVM_SocketAvailable(jint descriptor, jint* result)
{
	jint retVal = 0;

	Trc_SC_SocketAvailable_Entry(descriptor, result);

#ifdef WIN32
	/* Windows JCL native doesn't invoke this JVM method */
	Assert_SC_unreachable();
#endif
#if defined(J9UNIX) || defined(J9ZOS390)
	if (0 <= descriptor) {
		do {
			retVal = ioctl(descriptor, FIONREAD, result);
		} while ((-1 == retVal) && (EINTR == errno));

		if (0 <= retVal) {
			/* ioctl succeeded, return 1 to indicate that this JVM method succeeds */
			retVal = 1;
		} else {
			/* ioctl failed, return 0 to indicate that this JVM method fails */
			retVal = 0;
		}
	}
#endif /* defined(J9UNIX) || defined(J9ZOS390) */

	Trc_SC_SocketAvailable_Exit(retVal, *result);

	return retVal;
}


/**
 * JVM_SocketClose
 */
jint JNICALL
JVM_SocketClose(jint descriptor)
{
	jint retVal;

	Trc_SC_SocketClose_Entry(descriptor);

	if (descriptor<=0) {
		Trc_SC_SocketClose_bad_descriptor();
		return 1;
	}

#if defined(WIN32)
	(void)shutdown(descriptor, SD_SEND);
	(void)closesocket(descriptor);
	retVal = 1; /* Always return TRUE */
#else
	do {
		retVal = close(descriptor);
	} while ((-1 == retVal) && (EINTR == errno));
#endif

	Trc_SC_SocketClose_Exit(retVal);

	return retVal;
}


/**
 * JVM_Timeout
 */
jint JNICALL
JVM_Timeout(jint descriptor, jint timeout)
{
	jint result = 0;
	struct timeval tval;
#ifdef WIN32
	struct fd_set fdset;
#endif

#if defined(J9UNIX) || defined(J9ZOS390)
	jint returnVal = 0;
	jint crazyCntr = 10;
	fd_set fdset;
#endif /* defined(J9UNIX) || defined(J9ZOS390) */

	Trc_SC_Timeout_Entry(descriptor, timeout);

	tval.tv_sec = timeout / 1000;
	tval.tv_usec = (timeout % 1000) * 1000;
	FD_ZERO(&fdset);
	FD_SET((u_int)descriptor, &fdset);
#if defined(WIN32)
	result = select(0, &fdset, 0, 0, &tval);
#elif defined(J9ZTPF) /* defined(WIN32) */
	if (-1 == timeout)  {
		result = select(0, &fdset, 0, 0, NULL);
	} else  {
		result = select(0, &fdset, 0, 0, &tval);
	}
#elif defined(J9UNIX) || defined(J9ZOS390) /* defined(WIN32) */
	do {
		crazyCntr--;
		returnVal = select(descriptor+1, &fdset, 0, 0, &tval);
		if (returnVal==1 && !FD_ISSET((u_int)descriptor, &fdset)) {
			result = 0;
			break;
		}
		if (!(returnVal<0 && errno==EINTR)) {
			result = returnVal;
			break;
		}
	} while (crazyCntr);
#endif /* defined(WIN32) */

	Trc_SC_Timeout_Exit(result);

	return result;
}


/**
 * @return Is 8 byte compare and swap directly supported?
 *
 * @note In 1.4, this indicates whether or not JVM_FieldCX8 is supported.
 * @note In 1.5, this indicates whether a fast implementation of sun.misc.Unsafe.compareAndSwapLong() is supported.
 *
 * @note even though this may answer false on 32-bit platforms, JVM_CX8Field and
 * \compareAndSwapLong are supported, albeit in an inefficient manner.
 */
jboolean JNICALL
JVM_SupportsCX8(void)
{
	Trc_SC_SupportsCX8();

	return JNI_TRUE;
}


/**
 * Perform an atomic compare and swap on a volatile long field.
 *
 * @arg[in] env the JNI environment
 * @arg[in] obj the jobject to which the instance field belongs, or the jclass to which the static field belongs
 * @arg[in] field the field to perform the compare and swap on
 * @arg[in] oldval if the current value in field is not oldval, it is unchanged
 * @arg[in] newval if the current value in field is oldval, newval is atomically stored into the field
 *
 * @return JNI_TRUE if newval was stored into the field, or JNI_FALSE if the fields value was not oldval
 *
 * @note The field must be volatile.
 * @note If obj is null this function produces undefined behaviour.
 *
 * @note This function is used in 1.4 if JVM_SupportsCX8 returns true.
 * @note In 1.5 it is unused.
 */
jboolean JNICALL
JVM_CX8Field(JNIEnv* env, jobject obj, jfieldID field, jlong oldval, jlong newval)
{
	jboolean result = JNI_FALSE;

	Trc_SC_CX8Field_Entry(env, obj, field, oldval, newval);
	exit(230);

	return result;
}


/**
 * Method stub for method not yet implemented. Required for 1.4 support.
 */
void JNICALL
JVM_RegisterUnsafeMethods(JNIEnv* env, jclass unsafeClz)
{

	Trc_SC_RegisterUnsafeMethods(env);

}


jint JNICALL
JVM_ActiveProcessorCount(void)
{
	PORT_ACCESS_FROM_JAVAVM(BFUjavaVM);
	jint num;

	Trc_SC_ActiveProcessorCount_Entry();
	/*
	 * This is used by Runtime.availableProcessors().
	 * Runtime.availableProcessors() by specification returns a number greater or equal to 1.
	 * RTC 112959: [was 209402] Liberty JAX-RS Default Executor poor performance.  Match reference implementation behaviour
	 * to return the bound CPUs rather than physical CPUs.
	 *
	 * This implementation should be kept consistent with jvmtiGetAvailableProcessors
	 */
	num = (jint)j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_TARGET);
	if (num < 1) {
		num = 1;
	}

	Trc_SC_ActiveProcessorCount_Exit(num);

	return num;
}

J9Class* java_lang_Class_vmRef(JNIEnv* env, jobject clazz);

/**
 * JVM_GetClassName / JVM_InitClassName
 *
 * The name was changed from JVM_GetClassName to JVM_InitClassName
 * in interface version 6.
 */
jstring JNICALL
#if JAVA_SPEC_VERSION < 11
JVM_GetClassName
#else /* JAVA_SPEC_VERSION < 11 */
JVM_InitClassName
#endif /* JAVA_SPEC_VERSION < 11 */
(JNIEnv *env, jclass theClass)
{
	J9JavaVM* vm = ((J9VMThread*)env)->javaVM;
	jstring result;

	Trc_SC_GetClassName_Entry(env, theClass);

#ifdef J9VM_IVE_RAW_BUILD /* J9VM_IVE_RAW_BUILD is not enabled by default */
	{
		J9Class* ramClass = java_lang_Class_vmRef(env, theClass);
		J9ROMClass* romClass = ramClass->romClass;
		PORT_ACCESS_FROM_JAVAVM(vm);

		if (J9ROMCLASS_IS_ARRAY(romClass)) {
			J9ArrayClass* arrayClass = (J9ArrayClass*) ramClass;
			J9ArrayClass* elementClass = (J9ArrayClass*)arrayClass->leafComponentType;
			UDATA arity = arrayClass->arity;
			UDATA nameLength, prefixSkip;
			J9UTF8* nameUTF;
			char* name;
			UDATA finalLength;

			if (J9ROMCLASS_IS_PRIMITIVE_TYPE(elementClass->romClass)) {
				nameUTF = J9ROMCLASS_CLASSNAME(elementClass->arrayClass->romClass);
				arity -= 1;
				nameLength = arity; /* The name will have a [ in it already */
				prefixSkip = arity;
			} else {
				nameUTF = J9ROMCLASS_CLASSNAME(elementClass->romClass);
				nameLength = arity + 2; /* The semi colon and the L */
				prefixSkip = arity + 1;
			}

			finalLength = nameLength + J9UTF8_LENGTH(nameUTF) + 1;
			name = (char*)j9mem_allocate_memory(nameLength + J9UTF8_LENGTH(nameUTF) + 1, OMRMEM_CATEGORY_VM);
			if (NULL != name) {
				memset(name,'[', nameLength);
				memcpy(name+nameLength, J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(nameUTF));
				name[J9UTF8_LENGTH(nameUTF)] = 0;
			}
			return NULL;
		} else {
			J9UTF8* nameUTF = J9ROMCLASS_CLASSNAME(ramClass->romClass);
			jobject result = NULL;

			char* name = (char*)j9mem_allocate_memory(J9UTF8_LENGTH(nameUTF) + 1, OMRMEM_CATEGORY_VM);
			if (NULL != name) {
				memcpy(name, J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(nameUTF));
				name[J9UTF8_LENGTH(nameUTF)] = 0;
			}

			result = (*env)->NewStringUTF(env, name);
			j9mem_free_memory(name);
#if JAVA_SPEC_VERSION >= 11
			// JVM_InitClassName is expected to also cache the result in the 'name' field
			(*env)->SetObjectField(env, theClass, classNameFID, result);
			if ((*env)->ExceptionCheck(env)) {
				result = NULL;
			}
#endif /* JAVA_SPEC_VERSION >= 11 */
			return result;
		}
	}
#endif /* J9VM_IVE_RAW_BUILD */

	result = (*env)->CallObjectMethod(env, theClass, getNameMID);

	/* CMVC 95169: ensure that the result is a well defined error value if an exception occurred */
	if ((*env)->ExceptionCheck(env)) {
		result = NULL;
	}

	Trc_SC_GetClassName_Exit(env, result);

	return result;
}


#if JAVA_SPEC_VERSION < 17
/**
 * Return the JVM_INTERFACE_VERSION. This function should not lock, gc or throw exception.
 * @return JVM_INTERFACE_VERSION, JDK8 - 4, JDK11+ - 6.
 */
jint JNICALL
JVM_GetInterfaceVersion(void)
{
	jint result = 4;	/* JDK8 */

	Trc_SC_GetInterfaceVersion_Entry();
	if (J2SE_CURRENT_VERSION >= J2SE_V11) {
		result = 6;
	}
	Trc_SC_GetInterfaceVersion_Exit(result);

	return result;
}
#endif /* JAVA_SPEC_VERSION < 17 */


/* jclass parameter 2 is apparently not used */

void JNICALL
JVM_Sleep(JNIEnv* env, jclass thread, jlong timeout)
{
	Trc_SC_Sleep_Entry(env, thread, timeout);

#if JAVA_SPEC_VERSION < 21
	(*env)->CallStaticVoidMethod(env, jlThread, sleepMID, timeout);
#else /* JAVA_SPEC_VERSION < 21 */
	{
		jlong millis = timeout / 1000000;
		jint nanos = (jint)(timeout % 1000000);
		(*env)->CallStaticVoidMethod(env, jlThread, sleepNanosMID, millis, nanos);
	}
#endif /* JAVA_SPEC_VERSION < 21 */

	Trc_SC_Sleep_Exit(env);
}



jint JNICALL
JVM_UcsOpen(const jchar* filename, jint flags, jint mode)
{
#ifdef WIN32
	WCHAR *prefixStr;
	DWORD prefixLen ;

	DWORD fullPathLen, rc;
	WCHAR* longFilename;
	DWORD newFlags, disposition, attributes;
	HANDLE hFile;
	jint returnVal;
	int isUNC = FALSE;
	int isDosDevices = FALSE;

	if (filename==NULL) {
		Trc_SC_UcsOpen_nullName();
		return -1;
	}

	Trc_SC_UcsOpen_Entry(filename, flags, mode);

	if (filename[0] == L'\\' && filename[1] == L'\\') {
		/* found a UNC path */
		if (filename[2] == L'?') {
			prefixStr = L"";
			prefixLen = 0;
		} else if (filename[2] == L'.' && filename[3] == L'\\') {
			isDosDevices = TRUE;
			prefixStr = L"";
			prefixLen = 0;
		} else {
			isUNC = TRUE;
			prefixStr = L"\\\\?\\UNC";
			prefixLen = (sizeof(L"\\\\?\\UNC") / sizeof(WCHAR)) - 1;
		}
	} else {
		prefixStr = L"\\\\?\\";
		prefixLen = (sizeof(L"\\\\?\\") / sizeof(WCHAR)) - 1;
	}

	/* Query size of full path name and allocate space accordingly */
	fullPathLen = GetFullPathNameW(filename, 0, NULL, NULL);
	/*[CMVC 74127] Workaround for 1 character filenames on Windows 2000 */
	if (filename[0] == L'\0' || filename[1] == L'\0') fullPathLen += 3;
	/*[CMVC 77501] Workaround for "\\.\" - reported length is off by 4 characters */
	if (isDosDevices) fullPathLen += 4;
	longFilename = malloc(sizeof(WCHAR) * (prefixLen+fullPathLen));

	/* Prefix "\\?\" to allow for very long filenames */
	wcscpy(longFilename, prefixStr);

	/* Finally append full path name */
	if (isUNC) prefixLen--;
	rc = GetFullPathNameW(filename, fullPathLen, &longFilename[prefixLen], NULL);
	if (!rc || rc >= fullPathLen) {
		Trc_SC_UcsOpen_GetFullPathNameW(rc);
		return -1;
	}

	if (isUNC) longFilename[prefixLen] = L'C';

	if (!flags || (flags & O_RDONLY) || (flags == O_TEMPORARY))
		newFlags = GENERIC_READ;
	else if (flags & O_WRONLY)
		newFlags = GENERIC_WRITE;
	else if (flags & O_RDWR)
		newFlags = (GENERIC_READ | GENERIC_WRITE);

	if (flags & O_TRUNC)
		disposition = CREATE_ALWAYS;
	else if (flags & O_CREAT)
		disposition = OPEN_ALWAYS;
	else
		disposition = OPEN_EXISTING;

	if (flags & (O_SYNC | O_DSYNC))
		attributes = FILE_FLAG_WRITE_THROUGH;
	else
		attributes = FILE_ATTRIBUTE_NORMAL;

	if (flags & O_TEMPORARY)
		attributes |= FILE_FLAG_DELETE_ON_CLOSE;

	hFile = CreateFileW(longFilename, newFlags, mode, NULL, disposition, attributes, NULL);
	returnVal = _open_osfhandle((UDATA)hFile, flags);

	if (returnVal<0) {
		Trc_SC_UcsOpen_error(returnVal);
	} else {
		Trc_SC_UcsOpen_Exit(returnVal);
	}

	free(longFilename);

	if (returnVal>=0)
		return returnVal;
	else if (errno==EEXIST)
		return JVM_EEXIST;
	else
		return -1;
#else
	printf("JVM_UcsOpen is only supported on Win32 platforms\n");
	return -1;
#endif
}






/**
 * jclass JVM_ConstantPoolGetClassAt(JNIEnv *, jobject, jobject, jint)
 */
jclass JNICALL
JVM_ConstantPoolGetClassAt(JNIEnv *env, jobject anObject, jobject constantPool, jint index)
{
	Trc_SC_ConstantPoolGetClassAt(env);
	exit(207);
}


/**
 * jclass JVM_ConstantPoolGetClassAtIfLoaded(JNIEnv *, jobject, jobject, jint)
 */
jclass JNICALL
JVM_ConstantPoolGetClassAtIfLoaded(JNIEnv *env, jobject anObject, jobject constantPool, jint index)
{
	Trc_SC_ConstantPoolGetClassAtIfLoaded(env);
	exit(208);
}


/**
 * jdouble JVM_ConstantPoolGetDoubleAt(JNIEnv *, jobject, jobject, jint)
 */
jdouble JNICALL
JVM_ConstantPoolGetDoubleAt(JNIEnv *env, jobject anObject, jobject constantPool, jint index)
{
	Trc_SC_ConstantPoolGetDoubleAt(env);
	exit(217);
}


/**
 * jobject JVM_ConstantPoolGetFieldAt(JNIEnv *, jobject, jobject, jint)
 */
jobject JNICALL
JVM_ConstantPoolGetFieldAt(JNIEnv *env, jobject anObject, jobject constantPool, jint index)
{
	Trc_SC_ConstantPoolGetFieldAt(env);
	exit(211);
}


/**
 * jobject JVM_ConstantPoolGetFieldAtIfLoaded(JNIEnv *, jobject, jobject, jint)
 */
jobject JNICALL
JVM_ConstantPoolGetFieldAtIfLoaded(JNIEnv *env, jobject anObject, jobject constantPool, jint index)
{
	Trc_SC_ConstantPoolGetFieldAtIfLoaded(env);
	exit(212);
}


/**
 * jfloat JVM_ConstantPoolGetFloatAt(JNIEnv *, jobject, jobject, jint)
 */
jfloat JNICALL
JVM_ConstantPoolGetFloatAt(JNIEnv *env, jobject anObject, jobject constantPool, jint index)
{
	Trc_SC_ConstantPoolGetFloatAt(env);
	exit(216);
}


/**
 * jint JVM_ConstantPoolGetIntAt(JNIEnv *, jobject, jobject, jint)
 */
jint JNICALL
JVM_ConstantPoolGetIntAt(JNIEnv *env, jobject anObject, jobject constantPool, jint index)
{
	Trc_SC_ConstantPoolGetIntAt(env);
	exit(214);
}


/**
 * jlong JVM_ConstantPoolGetLongAt(JNIEnv *, jobject, jobject, jint)
 */
jlong JNICALL
JVM_ConstantPoolGetLongAt(JNIEnv *env, jobject anObject, jobject constantPool, jint index)
{
	Trc_SC_ConstantPoolGetLongAt(env);
	exit(215);
}


/**
 * jobjectArray JVM_ConstantPoolGetMemberRefInfoAt(JNIEnv *, jobject, jobject, jint)
 */
jobjectArray JNICALL
JVM_ConstantPoolGetMemberRefInfoAt(JNIEnv *env, jobject anObject, jobject constantPool, jint index)
{
	Trc_SC_ConstantPoolGetMemberRefInfoAt(env);
	exit(213);
}


/**
 * jobject JVM_ConstantPoolGetMethodAt(JNIEnv *, jobject, jobject, jint)
 */
jobject JNICALL
JVM_ConstantPoolGetMethodAt(JNIEnv *env, jobject anObject, jobject constantPool, jint index)
{
	Trc_SC_ConstantPoolGetMethodAt(env);
	exit(209);
}


/**
 * jobject JVM_ConstantPoolGetMethodAtIfLoaded(JNIEnv *, jobject, jobject, jint)
 */
jobject JNICALL
JVM_ConstantPoolGetMethodAtIfLoaded(JNIEnv *env, jobject anObject, jobject constantPool, jint index)
{
	Trc_SC_ConstantPoolGetMethodAtIfLoaded(env);
	exit(210);
}


/**
 * jint JVM_ConstantPoolGetSize(JNIEnv *, jobject, jobject)
 */
jint JNICALL
JVM_ConstantPoolGetSize(JNIEnv *env, jobject anObject, jobject constantPool)
{
	Trc_SC_ConstantPoolGetSize(env);
	exit(206);
}


/**
 * jstring JVM_ConstantPoolGetStringAt(JNIEnv *, jobject, jobject, jint)
 */
jstring JNICALL
JVM_ConstantPoolGetStringAt(JNIEnv *env, jobject anObject, jobject constantPool, jint index)
{
	Trc_SC_ConstantPoolGetStringAt(env);
	exit(218);
}


/**
 * jstring JVM_ConstantPoolGetUTF8At(JNIEnv *, jstring)
 */
jstring JNICALL
JVM_ConstantPoolGetUTF8At(JNIEnv *env, jobject anObject, jobject constantPool, jint index)
{
	Trc_SC_ConstantPoolGetUTF8At(env);
	exit(219);
}


/**
 * jclass JVM_DefineClassWithSource(JNIEnv *, const char *, jobject, const jbyte *, jsize, jobject, const char *)
 */
jclass JNICALL
JVM_DefineClassWithSource(JNIEnv *env, const char * className, jobject classLoader, const jbyte * classArray, jsize length, jobject domain, const char * source)
{
	J9VMThread* currentThread = (J9VMThread*) env;
	J9JavaVM*  vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	J9ClassLoader* vmLoader;
	j9object_t loaderObject;
	jstring classNameString = (*env)->NewStringUTF(env,className);

	vmFuncs->internalEnterVMFromJNI(currentThread);

	loaderObject = J9_JNI_UNWRAP_REFERENCE(classLoader);
	vmLoader = J9VMJAVALANGCLASSLOADER_VMREF(currentThread, loaderObject);
	if (NULL == vmLoader) {
		vmLoader = vmFuncs->internalAllocateClassLoader(vm, loaderObject);
		if (NULL == vmLoader) {
			vmFuncs->internalExitVMToJNI(currentThread);
			return NULL;
		}
	}
	vmFuncs->internalExitVMToJNI(currentThread);

	return jvmDefineClassHelper(env, classLoader, classNameString, (jbyte*)classArray, 0, length, domain, 0);
}


/**
 * jobjectArray JVM_DumpThreads(JNIEnv *, jclass, jobjectArray)
 */
jobjectArray JNICALL
JVM_DumpThreads(JNIEnv *env, jclass thread, jobjectArray threadArray)
{
	Trc_SC_DumpThreads(env);
	exit(224);
}


/**
 * jobjectArray JVM_GetAllThreads(JNIEnv *, jclass)
 */
jobjectArray JNICALL
JVM_GetAllThreads(JNIEnv *env, jclass aClass)
{
	Trc_SC_GetAllThreads(env);
	exit(223);
}


/**
 * jbyteArray JVM_GetClassAnnotations(JNIEnv *, jclass)
 */
jbyteArray JNICALL
JVM_GetClassAnnotations(JNIEnv *env, jclass target)
{
	Trc_SC_GetClassAnnotations(env);
	exit(221);
}


/**
 * jobject JVM_GetClassConstantPool(JNIEnv *, jclass)
 */
jobject JNICALL
JVM_GetClassConstantPool(JNIEnv *env, jclass target)
{
	Trc_SC_GetClassConstantPool(env);
	exit(205);
}


/**
 * jstring JVM_GetClassSignature(JNIEnv *, jclass)
 */
jstring JNICALL
JVM_GetClassSignature(JNIEnv *env, jclass target)
{
	Trc_SC_GetClassSignature(env);
	exit(220);
}


/**
 * jobjectArray JVM_GetEnclosingMethodInfo(JNIEnv *, jclass)
 */
jobjectArray JNICALL
JVM_GetEnclosingMethodInfo(JNIEnv *env, jclass theClass)
{
	Trc_SC_GetEnclosingMethodInfo(env);
	exit(204);
}

static jint JNICALL
managementVMIVersion(JNIEnv *env)
{
	return JNI_VERSION_1_8;
}

typedef struct ManagementVMI {
	void * unused1;
	void * unused2;
	jint (JNICALL *getManagementVMIVersion)(JNIEnv *env);
} ManagementVMI;

static ManagementVMI globalManagementVMI = {NULL, NULL, &managementVMIVersion};

/**
 * void* JVM_GetManagement(jint)
 */
void* JNICALL
JVM_GetManagement(jint version)
{
	Trc_SC_GetManagement();
	return &globalManagementVMI;
}


/**
 * jlong JVM_NanoTime(JNIEnv *, jclass)
 */
jlong JNICALL
JVM_NanoTime(JNIEnv *env, jclass aClass)
{
	jlong ticks, freq;

	PORT_ACCESS_FROM_JAVAVM(BFUjavaVM);

	Trc_SC_NanoTime(env);

	ticks = j9time_hires_clock();
	freq = j9time_hires_frequency();

	/* freq is "ticks per s" */
	if ( freq == 1000000000L ) {
		return ticks;
	} else if ( freq < 1000000000L ) {
		return ticks * (1000000000L / freq);
	} else {
		return ticks / (freq / 1000000000L);
	}
}



struct J9PortLibrary*
JNICALL JVM_GetPortLibrary(void)
{
	return &j9portLibrary;
}


jint JNICALL
JVM_ExpandFdTable(jint fd)
{
	return 0;
}



void JNICALL
JVM_ZipHook(JNIEnv *env, const char* filename, jint newState)
{
#ifdef J9VM_OPT_ZIP_SUPPORT
	VMI_ACCESS_FROM_ENV(env);
	J9JavaVM *vm = (J9JavaVM*)((J9VMThread*)env)->javaVM;
	J9HookInterface ** hook = (*VMI)->GetZipFunctions(VMI)->zip_getZipHookInterface(VMI);

	if (hook != NULL) {
		UDATA state;

		switch (newState) {
		case JVM_ZIP_HOOK_STATE_OPEN :
			state = J9ZIP_STATE_OPEN;
			break;
		case JVM_ZIP_HOOK_STATE_CLOSED :
			state = J9ZIP_STATE_CLOSED;
			break;
		case JVM_ZIP_HOOK_STATE_RESET :
			state = J9ZIP_STATE_RESET;
			break;
		default :
			state = 0;
		}
		/* Can't use hook trigger macros as can't include vmzipcachehook_internal.h */
		if (state) {
			struct J9VMZipLoadEvent eventData;

			eventData.portlib = vm->portLibrary;
			eventData.userData = vm;
			eventData.zipfile = NULL;
			eventData.newState = state;
			eventData.cpPath = (U_8*)filename;
			eventData.returnCode = 0;

			(*hook)->J9HookDispatch(hook, J9HOOK_VM_ZIP_LOAD, &eventData);
		}
	}
#endif /* J9VM_OPT_ZIP_SUPPORT */
}


void * JNICALL
JVM_RawAllocate(size_t size, const char * callsite)
{
	return j9portLibrary.omrPortLibrary.mem_allocate_memory(&j9portLibrary.omrPortLibrary, (UDATA) size, (char *) ((callsite == NULL) ? J9_GET_CALLSITE() : callsite), J9MEM_CATEGORY_SUN_JCL);
}

void * JNICALL
JVM_RawRealloc(void * ptr, size_t size, const char * callsite)
{
	return j9portLibrary.omrPortLibrary.mem_reallocate_memory(&j9portLibrary.omrPortLibrary, ptr, (UDATA) size, callsite, J9MEM_CATEGORY_SUN_JCL);
}

void * JNICALL
JVM_RawCalloc(size_t nmemb, size_t size, const char * callsite)
{
	size_t byteSize = nmemb * size;
	void * mem = JVM_RawAllocate(byteSize, callsite);

	if (mem != NULL) {
		memset(mem, 0, byteSize);
	}

	return mem;
}

void * JNICALL
JVM_RawAllocateInCategory(size_t size, const char * callsite, jint category)
{
	return j9portLibrary.omrPortLibrary.mem_allocate_memory(&j9portLibrary.omrPortLibrary, (UDATA) size, (char *) ((callsite == NULL) ? J9_GET_CALLSITE() : callsite), category);
}

void * JNICALL
JVM_RawReallocInCategory(void * ptr, size_t size, const char * callsite, jint category)
{
	return j9portLibrary.omrPortLibrary.mem_reallocate_memory(&j9portLibrary.omrPortLibrary, ptr, (UDATA) size, callsite, category);
}

void * JNICALL
JVM_RawCallocInCategory(size_t nmemb, size_t size, const char * callsite, jint category)
{
	size_t byteSize = nmemb * size;
	void * mem = JVM_RawAllocateInCategory(byteSize, callsite, category);

	if (mem != NULL) {
		memset(mem, 0, byteSize);
	}

	return mem;
}

void JNICALL
JVM_RawFree(void * ptr)
{
	j9portLibrary.omrPortLibrary.mem_free_memory(&j9portLibrary.omrPortLibrary, ptr);
}


/**
 * JVM_IsNaN
 */
jboolean JNICALL
JVM_IsNaN(jdouble dbl)
{
	Trc_SC_IsNaN(*(jlong*)&dbl);
	return IS_NAN_DBL(dbl);
}


/*
 * Called before class library initialization to allow the JVM interface layer to
 * perform any necessary initialization.
 *
 * @note Java 7 and beyond uses J9HOOK_VM_INITIALIZE_REQUIRED_CLASSES_DONE instead
 * 		however, this function must be retained in order to satisfy the redirector.
 */
jint JNICALL
JVM_Startup(JavaVM* vm, JNIEnv* env)
{
	return JNI_OK;
}

#if defined(J9ZOS390)
/**
 * @return TRUE if we were able to set omrthread_global("thread_weight") with
 * a thread weight value. FALSE otherwise.
 */
static BOOLEAN
setZOSThrWeight(void)
{
	BOOLEAN success = FALSE;
	const UDATA jvmZOSTW = checkZOSThrWeightEnvVar();

	if (ZOS_THR_WEIGHT_NOT_FOUND != jvmZOSTW) {
		omrthread_t thandle = NULL;
		/* Attach so that we can use omrthread_global() below */
		IDATA trc = f_threadAttachEx(&thandle, J9THREAD_ATTR_DEFAULT);

		if (0 == trc) {
			UDATA *gtw = f_threadGlobal("thread_weight");

			if (NULL != gtw) {
				if (ZOS_THR_WEIGHT_HEAVY == jvmZOSTW) {
					*gtw = (UDATA)"heavy";
				} else {
					*gtw = (UDATA)"medium";
				}
				success = TRUE;
			}

			f_threadDetach(thandle);
		}
	} else {
		success = TRUE;
	}

	return success;
}

/**
 * @return ZOS_THR_WEIGHT_HEAVY or ZOS_THR_WEIGHT_MEDIUM if the customer is
 * explicitly requesting heavy or medium weight threads via the
 * JAVA_THREAD_MODEL env var. Otherwise, it returns
 * ZOS_THR_WEIGHT_NOT_FOUND.
 */
static UDATA
checkZOSThrWeightEnvVar(void)
{
	UDATA retVal = ZOS_THR_WEIGHT_NOT_FOUND;
#pragma convlit(suspend)
#undef getenv
	const char * const val = getenv("JAVA_THREAD_MODEL");

	if (NULL != val) {
		/*
		 * If the customer did not request heavy weight, assume medium.
		 * Note that the goal here is not to properly parse the env
		 * var. This is done in threadParseArguments() and it will flag
		 * if the customer attempts to pass a bad value in the env var.
		 */
		if (0 == strcmp(val, "HEAVY")) {
#pragma convlit(resume)
			retVal = ZOS_THR_WEIGHT_HEAVY;
		} else {
			retVal = ZOS_THR_WEIGHT_MEDIUM;
		}
	}

	return retVal;
}
#endif /* defined(J9ZOS390) */

void JNICALL
JVM_BeforeHalt()
{
	/* To be implemented via https://github.com/eclipse-openj9/openj9/issues/1459 */
}

#if defined(J9VM_ZOS_3164_INTEROPERABILITY) && (JAVA_SPEC_VERSION >= 17)
/*
 * Utility function used by NativeLibraries to invoke JNI_OnLoad or JNI_OnUnload
 * functions in 31-bit native interoperability targets. This requires mapping to
 * the corresponding 31-bit JavaVM object handle, along with invoking CEL4RO31
 * (via FFI) to the corresponding target function.
 *
 * @param vm The JavaVM pointer first parameter for JNI_OnXLoad function
 * @param handle The target function pointer to invoke - should be a 31-bit interop target
 * @param isOnLoad TRUE if invoking JNI_OnLoad, FALSE if invoking JNI_OnUnload
 * @param reserved The reserved second parameter for JNI_OnXLoad function
 *
 * @return the return value for JNI_OnLoad, or 0 for JNI_OnUnload
 */
jint JNICALL
JVM_Invoke31BitJNI_OnXLoad(JavaVM *vm, void *handle, jboolean isOnLoad, void *reserved)
{
	J9JavaVM *javaVM = (J9JavaVM *)vm;
	jint result = isOnLoad ? JNI_VERSION_1_1 : 0;

	Trc_SC_Invoke31BitJNI_OnXLoad_Entry(vm, handle, isOnLoad, reserved);

	if (J9_IS_31BIT_INTEROP_TARGET(handle)) {
		result = javaVM->internalVMFunctions->invoke31BitJNI_OnXLoad(javaVM, handle, isOnLoad, reserved);
	}

	Trc_SC_Invoke31BitJNI_OnXLoad_Exit(result);

	return result;
}
#endif /* defined(J9VM_ZOS_3164_INTEROPERABILITY) && (JAVA_SPEC_VERSION >= 17) */
