/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

#include "jvmti_test.h"

/* TODO: make _jvmtiTest_printf store messages if the configured output level prevents
 * them from being printed. The stored messages could then be printed in case of an error */

/**
 * \brief	Private printf
 * \ingroup 	jvmti_test.message
 *
 * @param[in] env	agent environment
 * @param[in] level 	verbosity level
 * @param[in] indentLevel indentation level in number of tabs
 * @param[in] format 	message format
 * @param[in] ... 	message arguments
 * @return
 *
 */
void JNICALL
_jvmtiTest_printf(agentEnv *env, int level, int indentLevel, char * format, ...)
{
	JVMTI_ACCESS_FROM_AGENT(env);

	if (env->outputLevel >= level) {
		va_list args;
		va_start(args, format);
		vprintf(format, args);
		va_end(args);
	}
}

#if defined (WIN32) || defined(WIN64) || defined(J9ZOS390)
void
tprintf(agentEnv *env, int level, char * format, ...)
{
	va_list args;

	if (env->outputLevel >= level) {
		va_start(args, format);
		vprintf(format, args);
		va_end(args);
	}
}

void
iprintf(agentEnv *env, int level, int indentLevel, char * format, ...)
{
	va_list args;

	if (env->outputLevel >= level) {
		va_start(args, format);
		vprintf(format, args);
		va_end(args);
	}
}
#endif

/* Dumps a buffer in hex, prefix (if non NULL) gets prepended to each line */
void JNICALL
_dumpdata(char *prefix, const char *data, const size_t len)
{
	int j, k;
	unsigned long i;
	char line[128], *ptr;


	for (i = 0; i < len;) {
		ptr = line;
		ptr += sprintf(ptr, "  0x%08lx : ", i);
		for (j = 0; j < 16 && i < len;) {
			for (k = 0; k < 4 && i < len; i++, j++, k++) {
				ptr += sprintf(ptr, "%02X ", ((int) data[i]) & 0xff);
			}
			ptr += sprintf(ptr, " ");
		}
		printf("%s > %s\n", prefix ? prefix : "", line);
	}
}

void JNICALL
_dumpdata_hex_ascii(char *prefix, int dataPrintIndex, const char *data, const size_t len)
{
	int j, k;
	unsigned long i;
	char line[128], *ptr;
	char asciiLine[128], *asciiPtr;

	for (i = 0; i < len;) {
		ptr = line;
		asciiPtr = asciiLine;
		ptr += sprintf(ptr, "  0x%08lx : ", dataPrintIndex + i);
		asciiPtr += sprintf(asciiLine, "  [");
		for (j = 0; j < 16 && i < len;) {
			for (k = 0; k < 4 && i < len; i++, j++, k++) {
				ptr += sprintf(ptr, "%02X ", ((int) data[i]) & 0xff);
				asciiPtr += sprintf(asciiPtr, "%c",
						     isprint((char)  (data[i] & 0xff)) ?
						     ((char)  data[i]) & 0xff : '.');
			}
			ptr += sprintf(ptr, " ");
		}
		printf("%s > %s - %s]\n", prefix ? prefix : "", line, asciiLine);
	}
}



