/*[INCLUDE-IF Sidecar19-SE-OpenJ9]*/

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

import java.util.Map;

/*
 * Stub class to compile OpenJDK j.l.i.MethodHandleImpl
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

/*[IF Java11]*/
        static byte[] generateInvokersHolderClassBytes(String str, MethodType[] invokerMts, MethodType[] callMts) {
                throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
        }
/*[ENDIF]*/
}
