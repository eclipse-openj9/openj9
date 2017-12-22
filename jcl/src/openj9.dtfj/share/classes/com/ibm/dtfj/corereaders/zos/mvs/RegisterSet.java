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
package com.ibm.dtfj.corereaders.zos.mvs;

import java.util.logging.Logger;

/**
 * This class represents a set of registers for a failed thread.
 */
public class RegisterSet {

    long[] registers = new long[16];
    long psw;
    String whereFound;
    /** Logger */
    private static Logger log = Logger.getLogger(RegisterSet.class.getName());

    /**
     * Return an array of the register values. XXX I haven't considered 64-bit machines
     * yet - do they use the same number of registers? The values contain what you'd expect,
     * eg getRegisters()[0] contains GPR0 and so on.
     */
    public long[] getRegisters() {
        throw new Error("tbc");
    }

    /**
     * Get the value of the specified register.
     */
    public long getRegister(int index) {
        return registers[index];
    }
	
    /**
     * Get the value of the specified register for use as an address.
     */
    public long getRegisterAsAddress(int index) {
		int addressMode = (int)(this.getPSW() >>> 31) & 3;
		switch (addressMode) {
		case 0:
			return registers[index] & 0xffffff;
		case 1:
			return registers[index] & 0x7fffffff;
		case 2:
			assert false;			
		case 3:
		}
		return registers[index];
    }

    /**
     * Sets the specified register.
     * @param index the register whose value is to be set
     * @param value the value to set it to
     */
    public void setRegister(int index, long value) {
        registers[index] = value;
        log.fine("set register " + index + " to 0x" + hex(value));
    }

    /**
     * Returns the PSW. XXX How big is the PSW on a 64-bit machine?
     */
    public long getPSW() {
        assert psw != 0;
        return psw;
    }

    /**
     * Sets the PSW.
     */
    public void setPSW(long psw) {
        this.psw = psw;
        log.fine("set psw to 0x" + hex(psw));		
    }

    /**
     * Sets the whereFound string.
     */
    public void setWhereFound(String whereFound) {
        this.whereFound = whereFound;
    }

    /**
     * Returns a string indicating where the registers were found. This is mainly for
     * debugging purposes.
     */
    public String whereFound() {
        return whereFound;
    }

    private static String hex(int i) {
        return Integer.toHexString(i);
    }

    private static String hex(long i) {
        return Long.toHexString(i);
    }
}
