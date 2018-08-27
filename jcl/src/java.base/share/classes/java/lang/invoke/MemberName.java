/*[INCLUDE-IF Sidecar18-SE-OpenJ9]*/

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

package java.lang.invoke;

/*
 * Stub class to compile OpenJDK j.l.i.MethodHandleImpl
 */

final class MemberName {
	/*[IF Sidecar18-SE-OpenJ9&!Sidecar19-SE-OpenJ9]*/
	static final class Factory {
		public static Factory INSTANCE = null;
	}
	/*[ENDIF]*/
	
	/*[IF Java11]*/
	private MethodHandle mh;

	public MemberName() {
	}

	public MemberName(MethodHandle methodHandle) {
		mh = methodHandle;
	}
	/*[ENDIF] Java11 */

	public boolean isVarargs() {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	
	public boolean isMethodHandleInvoke() {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	
	public String getName() {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	
	public MethodType getInvocationType() {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	
	public Class<?> getReturnType() {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	
	public boolean isNative() {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	
	/*[IF Java10]*/
	public MethodType getMethodType() {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	
	String getMethodDescriptor() {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	public Class<?> getDeclaringClass() {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	/*[IF Java11]*/
	public boolean isFinal() {
		if (mh instanceof FieldHandle) {
			return ((FieldHandle)mh).isFinal();
		}
		/*[MSG "K0675", "Unexpected MethodHandle instance: {0} with {1}"]*/
		throw new InternalError(com.ibm.oti.util.Msg.getString("K0675", mh, mh.getClass())); //$NON-NLS-1$
	}
	/*[ENDIF]*/
	/*[ENDIF]*/
}
