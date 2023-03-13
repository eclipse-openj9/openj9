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
/*[IF JAVA_SPEC_VERSION >= 15]*/
package java.lang.invoke;

import java.lang.invoke.MethodHandles.Lookup;
import java.lang.reflect.Member;

class InfoFromMemberName implements MethodHandleInfo {
	InfoFromMemberName(Lookup lookup, MemberName member, byte kind) {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}

	@Override
	public Class<?> getDeclaringClass() {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}

	@Override
	public String getName() {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}

	@Override
	public MethodType getMethodType() {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}

	@Override
	public int getModifiers() {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}

	@Override
	public int getReferenceKind() {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}

	@Override
	public <T extends Member> T reflectAs(Class<T> expected, Lookup lookup) {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
}
/*[ENDIF] JAVA_SPEC_VERSION >= 15 */
