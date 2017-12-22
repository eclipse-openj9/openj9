/*******************************************************************************
 * Copyright (c) 2006, 2013 IBM Corp. and others
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

package com.ibm.j9ddr.corereaders.tdump.zebedee.le;

/**
 * This class represents an LE managed function.
 */

public class Function {

    /** The name of the function */
    private String name;

    Function(DsaStackFrame dsa) {
        name = dsa.getEntryName();
    }

    /**
     * Returns the name of the function. This does not include the library name.
     */
    public String getName() {
        return name;
    }

    /**
     * Returns the name of the program unit. This represents for instance the name of the C
     * file in which this function resides.
     */
    public String getProgramUnit() {
        throw new Error("tbc");
    }

    /**
     * Returns the address of the entry point. The entry point of a function is where
     * the executable code begins.
     */
    public long getEntryPoint() {
        throw new Error("tbc");
    }

    /**
     * Returns true if the name is a valid function name (ie no control characters etc).
     */
    public static boolean isValidName(String name) {
        for (int i = name.length() - 1; i >= 0; i--) {
            char c = name.charAt(i);
            if (Character.isISOControl(c)) {
                return false;
            }
        }
        return true;
    }
}
