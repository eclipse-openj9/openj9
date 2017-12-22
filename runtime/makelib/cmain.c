/*******************************************************************************
 * Copyright (c) 1998, 2014 IBM Corp. and others
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

#if defined(WIN32)
/* windows.h defined UDATA.  Ignore its definition */
#define UDATA UDATA_win32_
#include <windows.h>
#undef UDATA	/* this is safe because our UDATA is a typedef, not a macro */
#endif
#include <stdlib.h>
#include <string.h>

#include "j9comp.h"
#include "j9port.h"
#include "libhlp.h"

#if defined(J9ZOS390)
#include "atoe.h"
#endif

#define VMOPT_XUSE_CEEHDLR "-XCEEHDLR"

UDATA signalProtectedMain(struct J9PortLibrary* portLibrary, void *arg);
static UDATA genericSignalHandler(struct J9PortLibrary* portLibrary, U_32 gpType, void* gpInfo, void* userData);

#if defined(WIN32)
int
translated_main(int argc, char ** argv, char ** envp)
#else
int
main(int argc, char ** argv, char ** envp)
#endif
{
	J9PortLibrary j9portLibrary;
	J9PortLibraryVersion portLibraryVersion;
	struct j9cmdlineOptions options;
	int rc = 257;
	UDATA result;
	
	options.shutdownPortLib = FALSE;

#if defined(J9ZOS390)
	iconv_init();
	{  /* translate argv strings to ascii */
		int i;
		for (i = 0; i < argc; i++) {
			argv[i] = e2a_string(argv[i]);
		}
	}
#endif

	if (0 == omrthread_init_library()) {
		omrthread_t attachedThread = NULL;
		if (0 == omrthread_attach_ex(&attachedThread, J9THREAD_ATTR_DEFAULT)) {
			/* Use portlibrary version which we compiled against, and have allocated space
			 * for on the stack.  This version may be different from the one in the linked DLL.
			 */
			J9PORT_SET_VERSION(&portLibraryVersion, J9PORT_CAPABILITY_MASK);
			if (0 == j9port_init_library(&j9portLibrary, &portLibraryVersion, sizeof(J9PortLibrary))) {
				options.argc = argc;
				options.argv = argv;
				options.envp = envp;
				options.portLibrary = &j9portLibrary;
				options.shutdownPortLib = TRUE;

				{
					/* signal options needs to be set before sig_protect is called, parse and set them now */
					int i;
					U_32 sigOptions = 0;

					for (i = 0; i < argc ; i++) {
						if (0 == strcmp(VMOPT_XUSE_CEEHDLR, argv[i])) {
							sigOptions |= J9PORT_SIG_OPTIONS_ZOS_USE_CEEHDLR;
						}
					}

					j9portLibrary.omrPortLibrary.sig_set_options(&j9portLibrary.omrPortLibrary, sigOptions);
				}

				if (j9portLibrary.omrPortLibrary.sig_protect((OMRPortLibrary*)&j9portLibrary,
					(omrsig_protected_fn)signalProtectedMain, &options,
					(omrsig_handler_fn)genericSignalHandler, NULL,
					J9PORT_SIG_FLAG_SIGALLSYNC,
					&result) == 0
				) {
					rc = (int)result;
				}

				/* CMVC 130066 : as a result of failing to destroy the VM, the VM may still be
				 * holding onto (and using) port library resources.
				 */
				if (options.shutdownPortLib) {
					j9portLibrary.port_shutdown_library(&j9portLibrary);
					omrthread_detach(attachedThread);
					omrthread_shutdown_library();
				}
			}
		}
	}

	return rc;
}

#if defined(WIN32)
int
wmain(int argc, wchar_t ** argv, wchar_t ** envp)
{
	char **translated_argv = NULL;
	char **translated_envp = NULL;
	char* cursor;
	int i, length, envc;
	int rc;

	/* Translate argv to UTF-8 */
	length = argc;	/* 1 null terminator per string */
	for(i = 0; i < argc; i++) {
		length += (int)(wcslen(argv[i]) * 3);
	}
	translated_argv = (char**)malloc(length + ((argc + 1) * sizeof(char*))); /* + array entries */
	cursor = (char*)&translated_argv[argc + 1];
	for(i = 0; i < argc; i++) {
		int utf8Length;

		translated_argv[i] = cursor;
		if(0 == (utf8Length = WideCharToMultiByte(OS_ENCODING_CODE_PAGE, OS_ENCODING_WC_FLAGS, argv[i], -1, cursor, length, NULL, NULL))) {
			return -1;
		}
		cursor += utf8Length;
		*cursor++ = '\0';
		length -= utf8Length;
	}
	translated_argv[argc] = NULL;	/* NULL terminated the new argv */

	/* Translate argv to UTF-8 */
	if(envp) {
		envc = 0;
		while(NULL != envp[envc]) {
			envc++;
		}
		length = envc;	/* 1 null terminator per string */
		for(i = 0; i < envc; i++) {
			length += (int)(wcslen(envp[i]) * 3);
		}
		translated_envp = (char**)malloc(length + ((envc + 1) * sizeof(char*))); /* + array entries */
		cursor = (char*)&translated_envp[envc + 1];
		for(i = 0; i < envc; i++) {
			int utf8Length;
			translated_envp[i] = cursor;
			if(0 == (utf8Length = WideCharToMultiByte(OS_ENCODING_CODE_PAGE, OS_ENCODING_WC_FLAGS, envp[i], -1, cursor, length, NULL, NULL))) {
				return -1;
			}
			cursor += utf8Length;
			*cursor++ = '\0';
			length -= utf8Length;
		}
		translated_envp[envc] = NULL;	/* NULL terminated the new envp */
	}

	rc = translated_main(argc, translated_argv, translated_envp);

	/* Free the translated strings */
	free(translated_argv);
	free(translated_envp);

	return rc;
}
#endif

static UDATA
genericSignalHandler(struct J9PortLibrary* portLibrary, U_32 gpType, void* gpInfo, void* userData)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	U_32 category;

	j9tty_printf(PORTLIB, "\nAn unhandled error (%d) has occurred.\n", gpType);
	for (category=0 ; category<J9PORT_SIG_NUM_CATEGORIES ; category++) {
		U_32 infoCount = j9sig_info_count(gpInfo, category) ;
		U_32 infoKind, index;
		void *value;
		const char* name;

		for (index=0 ; index < infoCount ; index++) {
			infoKind = j9sig_info(gpInfo, category, index, &name, &value);

			switch (infoKind) {
				case J9PORT_SIG_VALUE_32:
					j9tty_printf(PORTLIB, "%s=%08.8x\n", name, *(U_32 *)value);
					break;
				case J9PORT_SIG_VALUE_64:
				case J9PORT_SIG_VALUE_FLOAT_64:
					j9tty_printf(PORTLIB, "%s=%016.16llx\n", name, *(U_64 *)value);
					break;
				case J9PORT_SIG_VALUE_STRING:
					j9tty_printf(PORTLIB, "%s=%s\n", name, (const char *)value);
					break;
				case J9PORT_SIG_VALUE_ADDRESS:
					j9tty_printf(PORTLIB, "%s=%p\n", name, *(void**)value);
					break;
			}
		}
	}

	abort();

	/* UNREACHABLE */
	return 0;
}
