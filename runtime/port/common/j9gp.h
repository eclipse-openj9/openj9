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
#ifndef j9gp_h
#define j9gp_h

void j9gp_register_handler(struct J9PortLibrary *portLibrary, handler_fn fn, void *aUserData );
int32_t j9gp_startup(struct J9PortLibrary *portLibrary);
void j9gp_shutdown(struct J9PortLibrary *portLibrary);
uintptr_t j9gp_protect(struct J9PortLibrary *portLibrary, protected_fn fn, void *arg );
uint32_t j9gp_info_count(struct J9PortLibrary *portLibrary, void *info, uint32_t category);
uint32_t j9gp_info(struct J9PortLibrary *portLibrary, void *info, uint32_t category, int32_t index, const char **name, void **value);


#endif     /* j9gp_h */
 

