package com.ibm.j9.offload.tests.jniservice;

/*******************************************************************************
 * Copyright (c) 2008, 2012 IBM Corp. and others
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

public class JValue {
	private boolean z;
	private byte b;
	private char c;
	private short s;
	private int i;
	private long j;
	private float f;
	private double d;
	private TestNatives l;
	
	public JValue() {
		z = false;
		b = 0;
		c = 0;
		s = 0;
		i = 0;
		j = 0L;
		f = 0.0f;
		d = 0.0d;
		l = null;
	}
	
	public JValue(boolean z, byte b, char c, short s, int i, long j, float f, double d, TestNatives l) {
		this.z = z;
		this.b = b;
		this.c = c;
		this.s = s;
		this.i = i;
		this.j = j;
		this.f = f;
		this.d = d;
		this.l = l;
	}
	
	public void setZ(boolean z) {
		this.z = z;
	}
	
	public void setB(byte b) {
		this.b = b;
	}
	
	public void setC(char c) {
		this.c = c;
	}
	
	public void setS(short s) {
		this.s = s;
	}
	
	public void setI(int i) {
		this.i = i;
	}
	
	public void setJ(long j) {
		this.j = j;
	}
	
	public void setF(float f) {
		this.f = f;
	}
	
	public void setD(double d) {
		this.d = d;
	}
	
	public void setL(TestNatives l) {
		this.l = l;
	}	
	
	public boolean getZ() {
		return z;
	}
	
	public byte getB() {
		return b;
	}
	
	public char getC() {
		return c;
	}
	
	public short getS() {
		return s;
	}
	
	public int getI() {
		return i;
	}
	
	public long getJ() {	
		return j;
	}
	
	public float getF() {
		return f;
	}
	
	public double getD() {
		return d;
	}
	
	public TestNatives getL() {
		return l;
	}	
}
