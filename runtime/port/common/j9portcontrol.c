/*******************************************************************************
 * Copyright (c) 2015, 2015 IBM Corp. and others
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
#include "j9port.h"
#include "portpriv.h"
#include "ut_j9prt.h"

int32_t
j9port_control(struct J9PortLibrary *portLibrary, const char *key, uintptr_t value)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	if (!strcmp(J9PORT_CTLDATA_TRACE_START, key) && value) {
		UtInterface *utIntf = (UtInterface *) value;
		utIntf->module->TraceInit(NULL, &UT_MODULE_INFO);
	}
	if (!strcmp(J9PORT_CTLDATA_TRACE_STOP, key) && value) {
		UtInterface *utIntf = (UtInterface *) value;
		utIntf->module->TraceTerm(NULL, &UT_MODULE_INFO);
	}
	return omrport_control(key, value);
}


