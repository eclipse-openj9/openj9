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
package com.ibm.j9ddr.vm29.types;

public abstract class UScalar extends Scalar {

	UScalar(long data) {
		super(data);
	}
	
	public UScalar(Scalar value)
	{
		super(value);
	}
	
	@Override
	public boolean gt(Scalar parameter) {
		checkComparisonValid(parameter);
		
		//If both values are shorter than 8 bytes, then compare normally, otherwise do the "complicated" compare
		if (this.sizeof() < 8 && parameter.sizeof() < 8) {
			return this.data > parameter.data;
		} else {
			if (this.sizeof() == 8) {
				return new U64(this).gt(parameter);
			} else {
				return new U64(parameter).lt(this);
			}
		}

	}
	
	@Override
	public boolean lt (Scalar parameter) {
		checkComparisonValid(parameter);
		
		//If both values are shorter than 8 bytes, then compare normally, otherwise do the "complicated" compare
		if (this.sizeof() < 8 && parameter.sizeof() < 8) {
			return this.data < parameter.data;
		} else {
			if (this.sizeof() == 8) {
				return new U64(this).lt(parameter);
			} else {
				return new U64(parameter).gt(this);
			}
		}
	}
	
	@Override
	public boolean isSigned()
	{
		return false;
	}
	
	@Override
	public boolean gt(int parameter)
	{
		return gt(new U32(parameter));
	}
	
	@Override
	public boolean gt(long parameter)
	{
		return gt(new U64(parameter));
	}
	
	@Override
	public boolean lt(int parameter)
	{
		return lt(new U32(parameter));
	}
	
	@Override
	public boolean lt(long parameter)
	{
		return lt(new U64(parameter));
	}
}
