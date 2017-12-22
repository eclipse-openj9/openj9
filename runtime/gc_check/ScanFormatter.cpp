/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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
 * @ingroup GC_Check
 */

#include "j9.h"
#include "j9cfg.h"

#include "ScanFormatter.hpp"

void
GC_ScanFormatter::section(const char *type, void *pointer)
{
	PORT_ACCESS_FROM_PORT(_portLibrary);

	j9tty_printf(PORTLIB, "  <%s (%p)>\n", type, pointer);
	_currentCount = 0;
}

void
GC_ScanFormatter::section(const char *type)
{
	PORT_ACCESS_FROM_PORT(_portLibrary);

	j9tty_printf(PORTLIB, "  <%s>\n", type);
	_currentCount = 0;
}

void
GC_ScanFormatter::endSection()
{
	PORT_ACCESS_FROM_PORT(_portLibrary);
	
	if ((0 != _currentCount) && _displayedData) {
		j9tty_printf(PORTLIB,">\n");
		_currentCount = 0;
	}
}

void
GC_ScanFormatter::entry(void *pointer)
{
	PORT_ACCESS_FROM_PORT(_portLibrary);

	if (0 == _currentCount) {
		j9tty_printf(PORTLIB, "    <");
		_displayedData = true;
	}
	j9tty_printf(PORTLIB, "%p ", pointer);

	_currentCount += 1;

	if (NUMBER_ELEMENTS_DISPLAYED_PER_LINE == _currentCount) {
		j9tty_printf(PORTLIB, ">\n");
		_currentCount = 0;
	}
}

void
GC_ScanFormatter::end(const char *type, void *pointer)
{
	PORT_ACCESS_FROM_PORT(_portLibrary);
	
	if ((0 != _currentCount) && _displayedData) {
		j9tty_printf(PORTLIB,">\n");
	}
	j9tty_printf(PORTLIB, "<gc check: End scan %s (%p)>\n", type, pointer);
}

void
GC_ScanFormatter::end(const char *type)
{
	PORT_ACCESS_FROM_PORT(_portLibrary);
	
	if ((0 != _currentCount) && _displayedData) {
		j9tty_printf(PORTLIB,">\n");
	}
	j9tty_printf(PORTLIB, "<gc check: End scan %s>\n", type);
}
