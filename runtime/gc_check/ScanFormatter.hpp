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

#if !defined(SCANFORMATTER_HPP_)
#define SCANFORMATTER_HPP_

#include "j9.h"
#include "j9cfg.h"

#include "Base.hpp"

#define NUMBER_ELEMENTS_DISPLAYED_PER_LINE 8

class GC_ScanFormatter : public MM_Base
{
private:
	J9PortLibrary *_portLibrary;
	
	UDATA _currentCount;
	bool _displayedData;

public:
	void section(const char *type, void *pointer);
	void section(const char *type);
	void endSection();
	void entry(void *pointer);
	void end(const char *type, void *pointer);
	void end(const char *type);

	GC_ScanFormatter(J9PortLibrary *portLibrary, const char *title, void *pointer) :
		MM_Base(),
		_portLibrary(portLibrary),
		_currentCount(0),
		_displayedData(false)
	{
		PORT_ACCESS_FROM_PORT(_portLibrary);
		j9tty_printf(PORTLIB, "<gc check: Start scan %s (%p)>\n", title, pointer);
	}
	
	GC_ScanFormatter(J9PortLibrary *portLibrary, const char *title) :
		MM_Base(),
		_portLibrary(portLibrary),
		_currentCount(0),
		_displayedData(false)
	{
		PORT_ACCESS_FROM_PORT(_portLibrary);
		j9tty_printf(PORTLIB, "<gc check: Start scan %s>\n", title);
	}
};

#endif /* SCANFORMATTER_HPP_ */
