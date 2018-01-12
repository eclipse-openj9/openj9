/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
#ifndef j9shsem_h
#define j9shsem_h

/* @ddr_namespace: default */
int32_t j9shsem_startup(struct J9PortLibrary *portLibrary);
void j9shsem_close (struct J9PortLibrary *portLibrary, struct j9shsem_handle **handle);
intptr_t j9shsem_post(struct J9PortLibrary *portLibrary, struct j9shsem_handle* handle, uintptr_t semset, uintptr_t flag);
void j9shsem_shutdown(struct J9PortLibrary *portLibrary);
intptr_t j9shsem_destroy (struct J9PortLibrary *portLibrary, struct j9shsem_handle **handle);
intptr_t j9shsem_setVal(struct J9PortLibrary *portLibrary, struct j9shsem_handle* handle, uintptr_t semset, intptr_t value);
intptr_t j9shsem_open (struct J9PortLibrary *portLibrary, struct j9shsem_handle **handle, const struct J9PortShSemParameters *params);
intptr_t j9shsem_getVal(struct J9PortLibrary *portLibrary, struct j9shsem_handle* handle, uintptr_t semset);
intptr_t j9shsem_wait(struct J9PortLibrary *portLibrary, struct j9shsem_handle* handle, uintptr_t semset, uintptr_t flag);
int32_t j9shsem_params_init(struct J9PortLibrary *portLibrary, struct J9PortShSemParameters *params);

#endif     /* j9shsem_h */


