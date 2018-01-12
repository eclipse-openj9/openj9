/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
package com.ibm.dtfj.corereaders;

/**
 * NativeThreadContext based on ThreadContext class from Sovereign
 * basically used to help try and generalize stack traversal
 */
public class NativeThreadContext {
	
	long ee;
    long pc;
    long lr;
    long sp;
    long bp;


    public NativeThreadContext () {}

    public NativeThreadContext (int size, long ee, long pc, long lr, long sp, long bp ) {
        long anderFlag = 0x00000000ffffffffL;
        if (size == 64) {
        	anderFlag = 0xffffffffffffffffL;
        }
    	this.ee = ee & anderFlag;
        this.pc = pc & anderFlag;
        this.lr = lr & anderFlag;
        this.sp = sp & anderFlag;
        this.bp = bp & anderFlag;
    }

    public void setEE(long ee) { this.ee = ee; }
    public void setPc(long pc) { this.pc = pc; }
    public void setLr(long lr) { this.lr = lr; }
    public void setSp(long sp) { this.sp = sp; }
    public void setBp(long bp) { this.bp = bp; }

    public long getEE() { return ee; }
    public long getPc() { return pc; }
    public long getSp() { return sp; }
    public long getBp() { return bp; }
    public long getLr() { return lr; }

}
