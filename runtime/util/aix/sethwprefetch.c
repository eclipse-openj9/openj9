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

#include "j9.h"
#include "util_api.h"
#include <sys/machine.h>
#include "ut_j9util.h"
#include "errno.h"

IDATA
setHWPrefetch(UDATA value)
{

/*	DPFD_DEFAULT 0
	DPFD_NONE 1
	DPFD_SHALLOWEST 2
	DPFD_SHALLOW 3
	DPFD_MEDIUM 4
	DPFD_DEEP 5
	DPFD_DEEPER 6
	DPFD_DEEPEST 7
	DSCR_SSE 8*/

	int rc;
	long long dscr;

	dscr = (long long) value;
	rc = dscr_ctl(DSCR_WRITE, &dscr, sizeof(dscr));

	if (-1 == rc) {
		Trc_Util_setHWPrefetch_Failed(errno, strerror(errno));
	}

	Trc_Util_setHWPrefetch(rc, dscr);

	return rc;
}
