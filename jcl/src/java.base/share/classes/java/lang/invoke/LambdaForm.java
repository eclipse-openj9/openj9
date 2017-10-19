/*[INCLUDE-IF Sidecar18-SE-OpenJ9]*/

/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

package java.lang.invoke;

import java.lang.reflect.Method;

/*
 * Stub class to compile OpenJDK j.l.i.MethodHandleImpl
 */

class LambdaForm {

	LambdaForm(String str, int num1, Name[] names, int num2) {
		OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	LambdaForm(String str, int num, Name[] names) {
		OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	LambdaForm(String str, int num, Name[] names, boolean flag) {
		OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	/*[IF Java18.3]*/
	LambdaForm(int a, Name[] b, int c) {
		OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	LambdaForm(int a, Name[] b, int c, Kind d) {
		OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	LambdaForm(int a, Name[] b, int c, boolean d, MethodHandle e) {
		OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	LambdaForm(int a, Name[] b, int c, boolean d, MethodHandle e, Kind f) {
		OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	LambdaForm(int a, Name[] b) {
		OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	LambdaForm(int a, Name[] b, Kind c) {
		OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	LambdaForm(int a, Name[] b, boolean c) {
		OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	LambdaForm(int a, Name[] b, boolean c, Kind d) {
		OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	LambdaForm(Name[] a, Name[] b, Name c) {
		OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	LambdaForm(Name[] a, Name[] b, Name c, boolean d) {
		OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	/*[ENDIF]*/
	
	static class NamedFunction {
		NamedFunction(MethodHandle mh) {
			OpenJDKCompileStub.OpenJDKCompileStubThrowError();
		}
		NamedFunction(MethodType mt) {
			OpenJDKCompileStub.OpenJDKCompileStubThrowError();
		}
		NamedFunction(MemberName mn, MethodHandle mh) {
			OpenJDKCompileStub.OpenJDKCompileStubThrowError();
		}
		NamedFunction(Method m) {
			OpenJDKCompileStub.OpenJDKCompileStubThrowError();
		}
		/*[IF Java18.3]*/
		NamedFunction(MethodHandle a, MethodHandleImpl.Intrinsic b){
			OpenJDKCompileStub.OpenJDKCompileStubThrowError();
		}
		NamedFunction(MemberName a, MethodHandle b, MethodHandleImpl.Intrinsic c){
			OpenJDKCompileStub.OpenJDKCompileStubThrowError();
		}
		/*[ENDIF]*/
		
		MethodHandle resolvedHandle() {
			throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
		}

		/*[IF Sidecar18-SE-OpenJ9&!Sidecar19-SE-OpenJ9]*/
		MethodHandle resolve() {
			throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
		}
		/*[ENDIF]*/
	}
	
	enum BasicType {
		L_TYPE;
		
		static BasicType basicType(Class<?> cls) {
			throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
		}
	}
	/*[IF Java18.3]*/
	enum Kind {
		CONVERT,
		SPREAD,
		COLLECT,
		GUARD,
		GUARD_WITH_CATCH,
		LOOP,
		TRY_FINALLY
	}
	/*[ENDIF]*/
	
	@interface Hidden{
	}
	
	static final class Name {
		Name(MethodHandle mh, Object... objs) {
			OpenJDKCompileStub.OpenJDKCompileStubThrowError();
		}
		Name(MethodType mt, Object... objs) {
			OpenJDKCompileStub.OpenJDKCompileStubThrowError();
		}
		Name(NamedFunction nf,Object... objs) {
			OpenJDKCompileStub.OpenJDKCompileStubThrowError();
		}
		
		Name withConstraint(Object obj) {
			throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
		}
	}
	
	static LambdaForm.Name[] arguments(int num, MethodType mt) {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}

	static NamedFunction constantZero(BasicType bt) {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	
	void compileToBytecode() {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	
	LambdaFormEditor editor() {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
}
