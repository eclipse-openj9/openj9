/*[INCLUDE-IF Sidecar18-SE-OpenJ9 & !OPENJDK_METHODHANDLES]*/
/*******************************************************************************
 * Copyright IBM Corp. and others 2017
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
/*[IF JAVA_SPEC_VERSION >= 11]*/
package java.lang.invoke;

/*[IF JAVA_SPEC_VERSION >= 15]*/
import jdk.internal.access.JavaLangAccess;
import jdk.internal.access.SharedSecrets;
/*[ENDIF] JAVA_SPEC_VERSION >= 15 */

class MethodHandleNatives {
	static LinkageError mapLookupExceptionToError(ReflectiveOperationException roe) {
		String exMsg = roe.getMessage();
		LinkageError linkageErr;
		if (roe instanceof IllegalAccessException) {
			linkageErr = new IllegalAccessError(exMsg);
		} else if (roe instanceof NoSuchFieldException) {
			linkageErr = new NoSuchFieldError(exMsg);
		} else if (roe instanceof NoSuchMethodException) {
			linkageErr = new NoSuchMethodError(exMsg);
		} else {
			linkageErr = new IncompatibleClassChangeError(exMsg);
		}
		Throwable th = roe.getCause();
		linkageErr.initCause(th == null ? roe : th);
		return linkageErr;
	}

	/*[IF JAVA_SPEC_VERSION >= 14]*/
	static long objectFieldOffset(MemberName memberName) {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}

	static long staticFieldOffset(MemberName memberName) {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}

	static Object staticFieldBase(MemberName memberName) {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 14 */

	/*[IF JAVA_SPEC_VERSION >= 15]*/
	static boolean refKindIsMethod(byte kind) {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	
	static boolean refKindIsField(byte kind) {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	
	static boolean refKindIsConstructor(byte kind) {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	
	private static final JavaLangAccess JLA = SharedSecrets.getJavaLangAccess();

	/**
	 * Returns the classData stored in the class.
	 * 
	 * @param the class from where to retrieve the classData.
	 * 
	 * @return the classData (Object).
	 */
	static Object classData(Class<?> c) {
		return JLA.classData(c);
	}

	native static void checkClassBytes(byte[] bytes);
	/*[ENDIF] JAVA_SPEC_VERSION >= 15 */
}
/*[ENDIF] JAVA_SPEC_VERSION >= 11 */
