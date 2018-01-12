/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2006, 2017 IBM Corp. and others
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
package com.ibm.dtfj.corereaders.zos.dumpreader;

/**
 * This interface is used when searching a dump or an address space to report back when
 * a match is found.
 */
public interface SearchListener {

    /**
     * This method determines the pattern to be searched for. This is called before the
     * search begins.
     * @return the pattern of bytes to search for
     */
    public byte[] getPattern();

    /**
     * Called to report a match has been found. The listener must return true to continue
     * the search.
     * @param space the {@link AddressSpace} in which the match was found
     * @param address the address where the match was found
     * @return true if the search is to continue
     */
    public boolean foundMatch(AddressSpace space, long address);
}
