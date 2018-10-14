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
 * @ingroup GC_Trace
 */

#include "TgcExtensions.hpp"

#include <stdarg.h>

#include "GCExtensions.hpp"

MM_TgcExtensions * 
MM_TgcExtensions::newInstance(MM_GCExtensions *extensions)
{
	MM_TgcExtensions * tgcExtensions = (MM_TgcExtensions *) extensions->getForge()->allocate(sizeof(MM_TgcExtensions), MM_AllocationCategory::DIAGNOSTIC, J9_GET_CALLSITE());
	if (NULL != tgcExtensions) {
		memset((void *)tgcExtensions, 0, sizeof(MM_TgcExtensions));
		new(tgcExtensions) MM_TgcExtensions(extensions);
	}
	return tgcExtensions;
}

void
MM_TgcExtensions::tearDown(MM_GCExtensions *extensions)
{
	if (J9PORT_TTY_ERR != _outputFile) {
		PORT_ACCESS_FROM_PORT(getPortLibrary());
		j9file_close(_outputFile);
		_outputFile = J9PORT_TTY_ERR;
	}
}

void
MM_TgcExtensions::kill(MM_GCExtensions *extensions)
{
	tearDown(extensions);
	extensions->getForge()->free(this);
}

MM_TgcExtensions::MM_TgcExtensions(MM_GCExtensions *extensions) 
	: MM_BaseNonVirtual()
	, _portLibrary(((J9JavaVM *)(extensions->getOmrVM()->_language_vm))->portLibrary)
	, _outputFile(J9PORT_TTY_ERR)
{
	_typeId = __FUNCTION__;
}

void
MM_TgcExtensions::vprintf(const char *format, va_list args) 
{
	PORT_ACCESS_FROM_PORT(getPortLibrary());
	j9file_vprintf(_outputFile, format, args);
}

void
MM_TgcExtensions::printf(const char *format, ...) 
{
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
}

bool 
MM_TgcExtensions::setOutputFile(const char* filename)
{
	bool result = true;
	PORT_ACCESS_FROM_PORT(getPortLibrary());
	IDATA fd = j9file_open(filename, EsOpenCreate | EsOpenTruncate | EsOpenWrite, 0666);
	if (-1 == fd) {
		result = false;
	} else {
		_outputFile = fd;
	}
	return result;
}
