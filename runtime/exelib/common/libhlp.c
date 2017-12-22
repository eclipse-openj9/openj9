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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "jni.h"
#include "j9user.h"
#include "j9port.h"
#include "j9socket.h"
#include "cfr.h"
#include "j9.h"
#include "j9protos.h"
#include "rommeth.h"
#include "j9version.h"

#include "libhlp.h"

#include "j9exelibnls.h"
#include "exelib_internal.h"

#define J9_PATH_SLASH DIR_SEPARATOR

static IDATA convertString (JNIEnv* env, J9PortLibrary *j9portLibrary, jclass utilClass, jmethodID utilMid, char* chars, jstring* str);


#define ROUND_TO_1K( x )  (((x + 1023) / 1024) * 1024)

static char versionString[255];

I_32 main_appendToClassPath( J9PortLibrary *portLib, U_16 sep, J9StringBuffer **classPath, char *toAppend) {

	/* append a separator, first */
	if ((NULL != *classPath)
		&& ((*classPath)->data[strlen((const char*)(*classPath)->data)] != sep)
	) {
		char separator[2];
	 	separator[0] = (char)sep;
		separator[1] = '\0';
		*classPath = strBufferCat(portLib, *classPath, separator);
		if (*classPath == NULL) return -1;
	}

	*classPath = strBufferCat(portLib, *classPath, toAppend);
	if (*classPath == NULL) return -1;

	return 0;
}




IDATA main_initializeBootLibraryPath(J9PortLibrary * portLib, J9StringBuffer **finalBootLibraryPath, char *argv0)
{
	*finalBootLibraryPath = NULL;
	{
		char *p = NULL;
		char *bootLibraryPath = NULL;
		PORT_ACCESS_FROM_PORT(portLib);

		if (j9sysinfo_get_executable_name (argv0, &bootLibraryPath)) {
			return -1;
		}

		p = strrchr(bootLibraryPath, J9_PATH_SLASH);
		if (p) {
			p[1] = '\0';
			*finalBootLibraryPath = strBufferCat(portLib, NULL, bootLibraryPath);
		} 
		/* Do /not/ delete executable name string; system-owned. */
	}
	return 0;
}



/* Allocates and retrieves initial value of the classPath. */

I_32 main_initializeClassPath( J9PortLibrary *portLib, J9StringBuffer** classPath)
{
	PORT_ACCESS_FROM_PORT( portLib );
	IDATA rc;
	char* envvars = "CLASSPATH\0classpath\0";
	char* envvar;

	for (envvar = envvars; *envvar; envvar += strlen(envvar) + 1) {
		rc = j9sysinfo_get_env(envvar, NULL, 0);
		if (rc > 0) {
			char * cpData = NULL;
			*classPath = strBufferEnsure(portLib, *classPath, rc);
			if (*classPath == NULL) return -1; 
			cpData = (*classPath)->data + strlen((const char*)(*classPath)->data);
			j9sysinfo_get_env(envvar, cpData, rc);
			(*classPath)->remaining -= rc;
			break;
		}
	}

	return 0;
}


IDATA main_initializeJavaHome(J9PortLibrary * portLib, J9StringBuffer **finalJavaHome, int argc, char **argv)
{
	char *javaHome = NULL;
	char *javaHomeModifiablePart = NULL;
	char *p;
	IDATA retval = -1;
	IDATA rc;
	UDATA isUpper = TRUE;
	char* envvars = "JAVA_HOME\0java_home\0";
	char* envvar;

	PORT_ACCESS_FROM_PORT(portLib);

#ifdef DEBUG
	j9tty_printf(portLib, "initializeJavaHome called\n");
#endif

	for (envvar = envvars; *envvar; envvar += strlen(envvar) + 1) {
		rc = j9sysinfo_get_env(envvar, NULL, 0);
		if (rc > 0) {
			char* javaHomeData = NULL;
			*finalJavaHome = strBufferEnsure(PORTLIB, *finalJavaHome, rc);
			if (NULL == *finalJavaHome) {
				return -1;
			}
			javaHomeData = (*finalJavaHome)->data + strlen((const char*)(*finalJavaHome)->data);
			j9sysinfo_get_env(envvar, javaHomeData , rc);
			(*finalJavaHome)->remaining -= rc;
			return 0;
		}
	}
		
	/* Compute the proper value for the var. */

	if ((argc < 1) || !argv) return -1;

	retval = j9sysinfo_get_executable_name(argv[0], &javaHome);
	if (retval) {
		*finalJavaHome = strBufferCat(PORTLIB, *finalJavaHome, "..");
		return 0;
	}

	javaHomeModifiablePart = javaHome;
#if defined(WIN32) || defined(OS2)
	/* Make sure we don't modify a drive specifier in a pathname. */
	if ((strlen(javaHome) > 2) && (javaHome[1] == ':')) {
		javaHomeModifiablePart = javaHome + 2;
		if (javaHome[2] == J9_PATH_SLASH)
			javaHomeModifiablePart++;
	}
#endif
#if defined(WIN32) || defined(OS2)
	/* Make sure we don't modify the root of a UNC pathname. */
	if ((strlen(javaHome) > 2) && (javaHome[0] == J9_PATH_SLASH) && (javaHome[1] == J9_PATH_SLASH)) {
		javaHomeModifiablePart = javaHome + 2;
		/* skip over the machine name */
		while (*javaHomeModifiablePart && (*javaHomeModifiablePart != J9_PATH_SLASH)) {
			javaHomeModifiablePart++;
		}
		if (*javaHomeModifiablePart)
			javaHomeModifiablePart++;
		/* skip over the share name */
		while (*javaHomeModifiablePart && (*javaHomeModifiablePart != J9_PATH_SLASH)) {
			javaHomeModifiablePart++;
		}
	}
#endif
	if ((javaHomeModifiablePart == javaHome) && javaHome[0] == J9_PATH_SLASH) {
		/* make sure we don't modify a root slash. */
		javaHomeModifiablePart++;
	}

	/* Note: if sysinfo_get_executable_name claims we were invoked from a root directory, */
	/* then this code will return that root directory for java.home also. */
	p = strrchr(javaHomeModifiablePart, J9_PATH_SLASH);
	if (!p) {
		javaHomeModifiablePart[0] = '\0';	/* chop off whole thing! */
	} else {
		p[0] = '\0';		/* chop off trailing slash and executable name. */
		p = strrchr(javaHomeModifiablePart, J9_PATH_SLASH);
		if (!p) {
			javaHomeModifiablePart[0] = '\0';	/* chop off the rest */
		} else {
			p[0] = '\0';	/* chop off trailing slash and deepest subdirectory. */
		}
	}

	*finalJavaHome = strBufferCat(PORTLIB, *finalJavaHome, javaHome);
	/* Do /not/ delete executable name string; system-owned. */
	return 0;
}



IDATA main_initializeJavaLibraryPath(J9PortLibrary * portLib, J9StringBuffer **finalJavaLibraryPath, char *argv0)
{
#ifdef WIN32
#define ENV_PATH "PATH"
#else
#if defined(AIXPPC)
#define ENV_PATH "LIBPATH"
#else
#define ENV_PATH "LD_LIBRARY_PATH"
#endif
#endif

	J9StringBuffer *javaLibraryPath = NULL;
	char *exeName = NULL;
	IDATA rc = -1;
	char *p = NULL;
	char *envResult = NULL;
	IDATA envSize;
#define ENV_BUFFER_SIZE 80
	char envBuffer[ENV_BUFFER_SIZE]; 
	char sep[2];
	PORT_ACCESS_FROM_PORT(portLib);

	sep[0] = (char)j9sysinfo_get_classpathSeparator();
	sep[1] = '\0';

	if (j9sysinfo_get_executable_name(argv0, &exeName)) {
		goto done;
	}
	p = strrchr(exeName, J9_PATH_SLASH);
	if (NULL != p) {
		p[1] = '\0';
	} else {
		/* Reset to NULL; do /not/ delete (system-owned string). */
		exeName = NULL;
	}

	envSize = j9sysinfo_get_env( ENV_PATH, NULL, 0 );
	if( envSize > 0 ) {
		if( envSize >= ENV_BUFFER_SIZE ) {
			envResult = j9mem_allocate_memory( envSize + 1 , OMRMEM_CATEGORY_VM);
			if (!envResult) goto done;
			j9sysinfo_get_env( ENV_PATH, envResult, envSize );			
		} else {
			envSize = -1;	/* make it -1 so we don't free the buffer */
			j9sysinfo_get_env( ENV_PATH, envBuffer, ENV_BUFFER_SIZE );	
			envResult = envBuffer;
		}
	} else {
		envResult = NULL;
	}


	/* Add one to each length to account for the separator character.  Add 2 at the end for the "." and NULL terminator */

	if (exeName) {
		javaLibraryPath = strBufferCat(portLib, javaLibraryPath, exeName);
		javaLibraryPath = strBufferCat(portLib, javaLibraryPath, sep);
	}
	javaLibraryPath = strBufferCat(portLib, javaLibraryPath, ".");
	if (envResult) {
		javaLibraryPath = strBufferCat(portLib, javaLibraryPath, sep);
		javaLibraryPath = strBufferCat(portLib, javaLibraryPath, envResult);
		if( envSize != -1 ) {
			j9mem_free_memory( envResult );
		}
	}

	rc = 0;

done:
	/* Do /not/ delete executable name string; system-owned. */
	*finalJavaLibraryPath = javaLibraryPath;
	return rc;
}



int main_runJavaMain(JNIEnv * env, char *mainClassName, int nameIsUTF, int java_argc, char **java_argv, J9PortLibrary * j9portLibrary)
{
	int i, rc = 0;
	jclass cls;
	jmethodID mid, utilMid;
	jarray args;
	jclass utilClass, stringClass;
	char *slashifiedClassName, *dots, *slashes;
	const char *utfClassName;
	jboolean isCopy;
	jstring str;
	jclass globalCls;
	jarray globalArgs;

	PORT_ACCESS_FROM_PORT(j9portLibrary);

	slashifiedClassName = j9mem_allocate_memory(strlen(mainClassName) + 1, OMRMEM_CATEGORY_VM);
	if (slashifiedClassName == NULL) {
		/* J9NLS_EXELIB_INTERNAL_VM_ERR_OUT_OF_MEMORY=Internal VM error: Out of memory\n */
		j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_EXELIB_INTERNAL_VM_ERR_OUT_OF_MEMORY);
		rc = 2;
		goto done;
	}
	for (slashes = slashifiedClassName, dots = mainClassName; *dots; dots++, slashes++) {
		*slashes = (*dots == '.' ? '/' : *dots);
	}
	*slashes = '\0';

	/* These classes must have already been found as part of bootstrap */
	stringClass = (*env)->FindClass(env, "java/lang/String");
	if (!stringClass) {
		/* J9NLS_EXELIB_INTERNAL_VM_ERR_FAILED_TO_FIND_JLS=Internal VM error: Failed to find class java/lang/String\n */
		j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_EXELIB_INTERNAL_VM_ERR_FAILED_TO_FIND_JLS );
		rc = 5;
		goto done;
	}
	utilClass = (*env)->FindClass(env, "com/ibm/oti/util/Util");
	if (NULL == utilClass) {
		/* J9NLS_EXELIB_INTERNAL_VM_ERR_FAILED_CREATE_JLS_FOR_CLASSNAME=Internal VM error: Failed to create java/lang/String for class name %s\n */
		j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_EXELIB_INTERNAL_VM_ERR_FAILED_CREATE_JLS_FOR_CLASSNAME, mainClassName);
		rc = 13;
		goto done;
	}
	utilMid = ((*env)->GetStaticMethodID(env, utilClass, "toString", "([BII)Ljava/lang/String;"));
	if (NULL == utilMid) {
		/* J9NLS_EXELIB_INTERNAL_VM_ERR_FAILED_CREATE_JLS_FOR_CLASSNAME=Internal VM error: Failed to create java/lang/String for class name %s\n */
		j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_EXELIB_INTERNAL_VM_ERR_FAILED_CREATE_JLS_FOR_CLASSNAME, mainClassName);
		rc = 14;
		goto done;
	}

	if(nameIsUTF) {
		cls = (*env)->FindClass(env, slashifiedClassName);
		j9mem_free_memory(slashifiedClassName);
	} else {
		IDATA rcConvert = convertString(env, j9portLibrary, utilClass, utilMid, slashifiedClassName, &str);
		j9mem_free_memory(slashifiedClassName);

		if(rcConvert == 1) {
			/* J9NLS_EXELIB_INTERNAL_VM_ERR_FAILED_CREATE_BA=Internal VM error: Failed to create byte array for class name %s\n */
			j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_EXELIB_INTERNAL_VM_ERR_FAILED_CREATE_BA, mainClassName);
			rc = 10;
			goto done;
		}
		if(rcConvert == 2) {
			/* J9NLS_EXELIB_INTERNAL_VM_ERR_FAILED_CREATE_JLS_FOR_CLASSNAME=Internal VM error: Failed to create java/lang/String for class name %s\n */
			j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_EXELIB_INTERNAL_VM_ERR_FAILED_CREATE_JLS_FOR_CLASSNAME, mainClassName);
			rc = 11;
			goto done;
		}
		utfClassName = (const char *) (*env)->GetStringUTFChars(env, str, &isCopy); 
		if(utfClassName == NULL) {
			/* J9NLS_EXELIB_INTERNAL_VM_ERR_OUT_OF_MEMORY_CONVERTING=Internal VM error: Out of memory converting string to UTF Chars for class name %s\n */
			j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_EXELIB_INTERNAL_VM_ERR_OUT_OF_MEMORY_CONVERTING, mainClassName);
			rc = 12;
			goto done;
		}
		cls = (*env)->FindClass(env, utfClassName);
		(*env)->ReleaseStringUTFChars(env, str, utfClassName);
		(*env)->DeleteLocalRef(env, str);
	}

	if (!cls) {
		rc = 3;
		goto done;
	}

	/* Create the String array before getting the methodID to get better performance from HOOK_ABOUT_TO_RUN_MAIN */
	args = (*env)->NewObjectArray(env, java_argc, stringClass, NULL);
	if (!args) {
		/* J9NLS_EXELIB_INTERNAL_VM_ERR_FAILED_CREATE_ARG_ARRAY=Internal VM error: Failed to create argument array\n */
		j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_EXELIB_INTERNAL_VM_ERR_FAILED_CREATE_ARG_ARRAY);
		rc = 6;
		goto done;
	}
	for (i = 0; i < java_argc; ++i) {
		IDATA rcConvert = convertString(env, j9portLibrary, utilClass, utilMid, java_argv[i], &str);
		if(rcConvert == 1) {
			/* J9NLS_EXELIB_INTERNAL_VM_ERR_FAILED_CREATE_BYTE_ARRAY=Internal VM error: Failed to create byte array for argument %s\n */
			j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_EXELIB_INTERNAL_VM_ERR_FAILED_CREATE_BYTE_ARRAY, java_argv[i]);
			rc = 7;
			goto done;
		}
		if(rcConvert == 2) {
			/* J9NLS_EXELIB_INTERNAL_VM_ERR_FAILED_CREATE_JLS_FOR_ARG=Internal VM error: Failed to create java/lang/String for argument %s\n*/
			j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_EXELIB_INTERNAL_VM_ERR_FAILED_CREATE_JLS_FOR_ARG,  java_argv[i]);
			rc = 8;
			goto done;
		}

		(*env)->SetObjectArrayElement(env, args, i, str);
		if ((*env)->ExceptionCheck(env)) {
			/* J9NLS_EXELIB_INTERNAL_VM_ERR_FAILED_SET_ARRAY_ELEM=Internal VM error: Failed to set array element for %s\n */
			j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_EXELIB_INTERNAL_VM_ERR_FAILED_SET_ARRAY_ELEM,
									  java_argv[i]);
			rc = 9;
			goto done;
		}
		(*env)->DeleteLocalRef(env, str);
	}

	mid = (*env)->GetStaticMethodID(env, cls, "main", "([Ljava/lang/String;)V");
	if (!mid) {
		/* Currently, GetStaticMethodID does not throw an exception when the method is not found */
		/* J9NLS_EXELIB_CLASS_DOES_NOT_IMPL_MAIN=Class %s does not implement main()\n */
		j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_EXELIB_CLASS_DOES_NOT_IMPL_MAIN, mainClassName);
		rc = 4;
		goto done;
	}
	/* For compliance purposes on CDC we must apply the spec more strictly than the JDK seems to */
	/* Verify that the main method we found is public, static, and void -- as described in 2.17.1 of the spec */
	/* WARNING: this is non-portable J9 specific code */
	if( ((J9VMThread *)env)->javaVM->runtimeFlags & J9RuntimeFlagVerify ) {
		J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD( ((J9JNIMethodID *)mid)->method );
		if( (romMethod->modifiers & (CFR_ACC_STATIC | CFR_ACC_PUBLIC)) != (CFR_ACC_STATIC | CFR_ACC_PUBLIC)  ) {
			/* J9NLS_EXELIB_MUST_BE_PSV=The method main must be declared public, static and void.\n */
			j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_EXELIB_MUST_BE_PSV);
			rc = 4;
			goto done;
		}
	}	/* end of non-portable J9 specific code */

	globalCls = (jclass) (*env)->NewGlobalRef(env, cls);
	if (globalCls) {
		(*env)->DeleteLocalRef(env, cls);
		cls = globalCls;
	}		
	globalArgs = (jarray) (*env)->NewGlobalRef(env, args);
	if (globalArgs) {
		(*env)->DeleteLocalRef(env, args);
		args = globalArgs;
	}		
	(*env)->DeleteLocalRef(env, stringClass);
	(*env)->CallStaticVoidMethod(env, cls, mid, args);

  done:
	if ((*env)->ExceptionCheck(env)) {
		if (rc == 0)
			rc = 100;
	}

	(*env)->ExceptionDescribe(env);
	return rc;
}



/* Compute and return a string which is the vm version */

char * main_vmVersionString(void)
{
	U_32 minorVersion;
	char *native = "";
	char *versionStringPtr = GLOBAL_DATA(versionString);
	minorVersion = EsVersionMinor;

	if ((minorVersion % 10) == 0)
		minorVersion /= 10;
	sprintf (versionStringPtr, "%d.%d%s,%s %s", 
		EsVersionMajor, minorVersion, EsExtraVersionString, native, EsBuildVersionString);
	return versionStringPtr;
}




static IDATA
convertString(JNIEnv* env, J9PortLibrary *j9portLibrary, jclass utilClass, jmethodID utilMid, char* chars, jstring* str)
{
		jsize strLength;
		jarray bytearray;
		jstring string;

		strLength = (jsize) strlen(chars);
		bytearray = (*env)->NewByteArray(env, strLength);
		if (((*env)->ExceptionCheck(env))) {
			return 1;
		}

		(*env)->SetByteArrayRegion(env, bytearray, (UDATA)0, strLength, (jbyte*)chars);

		string = (*env)->CallStaticObjectMethod(env, utilClass, utilMid, bytearray, (jint)0, strLength);
		(*env)->DeleteLocalRef(env, bytearray);

		if (!string) {
			return 2;
		} else {
			*str = string;
			return 0;
		}
}


/* Compute and return a string which is the nitty-gritty for the vm */

char * vmDetailString( J9PortLibrary *portLib, char *detailString, UDATA detailStringLength )
{
	const char *ostype, *osversion, *osarch;
	PORT_ACCESS_FROM_PORT( portLib );

	ostype = j9sysinfo_get_OS_type();
	osversion = j9sysinfo_get_OS_version();
	osarch = j9sysinfo_get_CPU_architecture();

	j9str_printf (PORTLIB, detailString, detailStringLength, "%s (%s %s %s)", EsBuildVersionString, ostype ? ostype : "unknown", osversion ? osversion : "unknown", osarch ? osarch : "unknown");
	return detailString;
}


/* Try to initialize the NLS catalog; otherwise use defaults   */
void main_setNLSCatalog(J9PortLibrary * portLib, char **argv)
{
	char *exename = NULL;

	PORT_ACCESS_FROM_PORT(portLib);

	/* Locate 'lib' directory which contains java.properties file */
	if ( j9sysinfo_get_executable_name( argv[0], &exename ) == 0 ) {
		/* For Java 8, java.properties file is present at same location as 'exename',
		 * but for Java 9, java.properties is under 'lib' directory.
		 * Since we have no way to determine if we are running for Java 8 or Java 9,
		 * pass both the locations to j9nls_set_catalog() and let it search for the file.
		 */
		char *nlsPaths[] = { exename, NULL };
		char *exe = NULL;
		char *vmName = NULL;
		UDATA exeNameLen = strlen(exename);
#ifdef WIN32
		UDATA j9libLen = exeNameLen + strlen(DIR_SEPARATOR_STR) * 2 + strlen("lib") + 1;
		char *j9lib = j9mem_allocate_memory(j9libLen, OMRMEM_CATEGORY_VM);

		if (NULL != j9lib) {
			char *bin = NULL;

			strcpy(j9lib, exename);

			/* Remove exe name, VM name ('default' or 'compressedrefs') and 'bin', then add 'lib' */
			exe = strrchr(j9lib, DIR_SEPARATOR);
			if (NULL != exe) {
				*exe = '\0';
			}
			vmName = strrchr(j9lib, DIR_SEPARATOR);
			if (NULL != vmName) {
				*vmName = '\0';
			}
			bin = strrchr(j9lib, DIR_SEPARATOR);
			if (NULL != bin) {
				*bin = '\0';
			}

			/* j9nls_set_catalog ignores everything after the last slash. Append a slash to j9lib. */
			strcat(j9lib, DIR_SEPARATOR_STR);
			strcat(j9lib, "lib");
			strcat(j9lib, DIR_SEPARATOR_STR);
#else
		char *j9lib = j9mem_allocate_memory(exeNameLen + 1, OMRMEM_CATEGORY_VM);

		if (NULL != j9lib) {
			strcpy(j9lib, exename);

			/* Remove exe name and VM name ('default' or 'compressedrefs') */
			exe = strrchr(j9lib, DIR_SEPARATOR);
			if (NULL != exe) {
				*exe = '\0';
			}
			vmName = strrchr(j9lib, DIR_SEPARATOR);
			if (NULL != vmName) {
				*vmName = '\0';
			}
			/* leave the platform name, it gets removed by j9nls_set_catalog as it ignores everything after the last slash. */
#endif /* WIN32 */
			nlsPaths[1] = j9lib;
		}
		j9nls_set_catalog( (const char**)nlsPaths, 2, "java", "properties" );
		if (NULL != j9lib) {
			j9mem_free_memory(j9lib);
		}
	}
	/* Do /not/ delete executable name string; system-owned. */
}

/*
 * Returns a non-zero value if an error occurred, returns zero on success.
 * If a default option file is located, optionFileName is an allocated buffer (must be freed by 
 * callee) that contains the fully qualified filename.
 * If a default option file cannot be found, optionFileName will point at NULL. 
 *     
 */
I_32 main_findDefaultOptionsFile( J9PortLibrary *portLib, char *argv0, char **optionFileName) {
	char *exeName;
	char *eptr;
	char *ptr;
	char *defaultOptionFileName;
	IDATA file;
	PORT_ACCESS_FROM_PORT(portLib);
	

	if( j9sysinfo_get_executable_name(argv0, &exeName)) {
		return -1;
	}
	defaultOptionFileName = j9mem_allocate_memory(strlen(exeName) + 6, OMRMEM_CATEGORY_VM);	/* +6 == ".j9vm" + zero termination */
	if(NULL == defaultOptionFileName ) {
		return -1;
	}
	/* First look for <path>/.<exename> */
	strcpy( defaultOptionFileName, exeName );
#if defined(WIN32)
	/* On Windows we have to strip the .exe extension */
	eptr = strrchr( exeName, '.');
	if( eptr ) {
		*eptr = '\0';
	}
#endif
	eptr = strrchr( exeName, DIR_SEPARATOR);
	ptr = strrchr( defaultOptionFileName, DIR_SEPARATOR);
	if( ptr ) {	/* we can assume if ptr is valid, so is eptr */
		ptr[1] = '.';
		ptr[2] = '\0';
		strcat( defaultOptionFileName, &eptr[1] );
		file = j9file_open( defaultOptionFileName, EsOpenRead, 0);
		if( file != -1 ) {
			j9file_close(file);
			*optionFileName = defaultOptionFileName;
			/* Do /not/ delete executable name string; system-owned. */
			return 0;
		}
	} 
	/* Do /not/ delete executable name string; system-owned. */
	
	ptr = strrchr( defaultOptionFileName, DIR_SEPARATOR);
	if( ptr ) {
		/* if we found a separator - truncate just after it */
		ptr++;
		*ptr = '\0';
		strcat( defaultOptionFileName, ".j9vm" );
		file = j9file_open( defaultOptionFileName, EsOpenRead, 0);
		if( file != -1 ) {
			j9file_close(file);
			*optionFileName = defaultOptionFileName;
			return 0;
		}
	} 
	/* No failure, but no file found */
	*optionFileName = NULL;
	j9mem_free_memory( defaultOptionFileName );
	return 0;
}
