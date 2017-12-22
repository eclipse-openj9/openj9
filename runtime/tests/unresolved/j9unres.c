/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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

/*
 * j9unres.c
 *
 * This file is used to generate a shared library specifically containing unresolved references.
 * This library is used to test lazy loading of shared libraries.
 *
 *  Created on: Jun 12, 2012
 *      Author: bdillman
 */

/* Define a function which won't resolve, used for testing.
 * An unresolved variable won't work, as all variables are resolved regardless of RTLD_LAZY flag.
 */
extern int
dontResolveThis(void);

/* Define a function with a reference to the unresolved function. */
int
functionWithUnresolvedRef(void) {
	return dontResolveThis();
}

