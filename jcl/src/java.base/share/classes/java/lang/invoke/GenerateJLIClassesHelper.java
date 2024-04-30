/*[INCLUDE-IF (JAVA_SPEC_VERSION >= 9) & !OPENJDK_METHODHANDLES]*/
/*
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package java.lang.invoke;

import java.util.Map;
import java.util.stream.Stream;

/*
 * Stub class to compile OpenJDK java.lang.invoke.MethodHandleImpl.
 */
class GenerateJLIClassesHelper {
	static byte[] generateDirectMethodHandleHolderClassBytes(String str, MethodType[] mts, int[] nums) {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}

	static byte[] generateDelegatingMethodHandleHolderClassBytes(String str, MethodType[] mets) {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}

	static Map.Entry<String, byte[]> generateConcreteBMHClassBytes(String str) {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}

	static byte[] generateBasicFormsClassBytes(String str) {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}

	static byte[] generateInvokersHolderClassBytes(String str, MethodType[] mts) {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}

	/*[IF JAVA_SPEC_VERSION >= 11]*/
	static byte[] generateInvokersHolderClassBytes(String str, MethodType[] invokerMts, MethodType[] callMts) {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 11*/

	/*[IF JAVA_SPEC_VERSION >= 16]*/
	static Map<String, byte[]> generateHolderClasses(Stream<String> traces) {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 16*/
}
