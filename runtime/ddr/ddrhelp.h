/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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

#ifndef DDRHELP_H_
#define DDRHELP_H_

/*
 * This macro is used to create a reference to a given type
 * guiding the compiler into emitting debug information for
 * it in the object file.
 */
#define DdrDebugLink(prefix, type) \
	extern "C" size_t \
	DdrLinkName(prefix, __LINE__)(type *pointer) \
	{ \
		return sizeof(*pointer); \
	}
/* helper macros to trigger expansion of __LINE__ */
#define DdrLinkName(prefix, line) DdrLinkName_(prefix, line)
#define DdrLinkName_(prefix, line) prefix ## ddr_link_ ## line

#endif /* DDRHELP_H_ */
