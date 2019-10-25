/*[INCLUDE-IF Sidecar18-SE-OpenJ9]*/

/*******************************************************************************
 * Copyright (c) 2017, 2020 IBM Corp. and others
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
/*[IF Java11]*/

package java.lang.invoke;

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

/*[IF Java14]*/
	static long objectFieldOffset(MemberName memberName) {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}

	static long staticFieldOffset(MemberName memberName) {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}

	static Object staticFieldBase(MemberName memberName) {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
/*[ENDIF] Java14 */

	class Constants {
		static final int MN_IS_METHOD			= 0x10000;
		static final int MN_IS_CONSTRUCTOR		= 0x20000;
		static final int MN_IS_FIELD			= 0x40000;
		static final int MN_IS_TYPE				= 0x80000;
		static final int MN_CALLER_SENSITIVE	= 0x100000;
		static final int MN_REFERENCE_KIND_SHIFT= 24;
		static final int MN_REFERENCE_KIND_MASK	= 0xF;
		static final int MN_SEARCH_SUPERCLASSES	= 0x100000;
		static final int MN_SEARCH_INTERFACES	= 0x200000;
		static final byte REF_NONE				= 0;
		static final byte REF_getField			= 1;
		static final byte REF_getStatic			= 2;
		static final byte REF_putField			= 3;
		static final byte REF_putStatic			= 4;
		static final byte REF_invokeVirtual		= 5;
		static final byte REF_invokeStatic		= 6;
		static final byte REF_invokeSpecial		= 7;
		static final byte REF_newInvokeSpecial	= 8;
		static final byte REF_invokeInterface	= 9;
		static final byte REF_LIMIT				= 10;
	}
}

/*[ENDIF] Java11 */
