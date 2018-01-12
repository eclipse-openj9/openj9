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

#include "iohelp.h"
#include "jclglob.h"
#include "jclprots.h"


/**
  * This will convert all separators to the proper platform separator
  * and remove duplicates on non POSIX platforms.
  */
void ioh_convertToPlatform(char *path)
{
	char * pathIndex;
	int length = (int)strlen(path);

	/* Convert all separators to the same type */
	pathIndex = path;
	while (*pathIndex != '\0') {
		if ((*pathIndex == '\\' || *pathIndex == '/') && (*pathIndex != jclSeparator))
			*pathIndex = jclSeparator;
		pathIndex++;
	}

	/* Remove duplicate separators */
	if (jclSeparator == '/') return; /* Do not do POSIX platforms */

	/* Remove duplicate initial separators */
	pathIndex = path;
	while ((*pathIndex != '\0') && (*pathIndex == jclSeparator)) {
		pathIndex++;
	}
	if ((pathIndex > path) && (length > (pathIndex - path)) && (*(pathIndex + 1) == ':')) {
		/* For Example '////c:/_*' ('_' added to silence compiler warning) */
		int newlen = (int)(length - (pathIndex - path));
		memmove(path, pathIndex, newlen);
		path[newlen] = '\0';
	} else {
		if ((pathIndex - path > 3) && (length > (pathIndex - path))) {
			/* For Example '////serverName/_*' ('_' added to silence compiler warning) */
			int newlen = (int)(length - (pathIndex - path) + 2);
			memmove(path, pathIndex - 2, newlen);
			path[newlen] = '\0';
		}
	}
	/* This will have to handle extra \'s but currently doesn't */
	
}
