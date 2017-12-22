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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

package java.lang.invoke;

/*
 * Stub class to compile OpenJDK j.l.i.MethodHandleImpl
 */

abstract class DelegatingMethodHandle extends MethodHandle {
	
	static final LambdaForm.NamedFunction NF_getTarget = null;
	
	protected DelegatingMethodHandle(MethodHandle mh) {
		this(mh.type(), mh);
		OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	protected DelegatingMethodHandle(MethodType mt, MethodHandle mh) {
		super(mh, mt);
		OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	protected DelegatingMethodHandle(MethodType mt, LambdaForm lf) {
		super(mt, lf);
		OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	static LambdaForm makeReinvokerForm(MethodHandle mh, int num, Object obj, LambdaForm.NamedFunction nf) {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	static LambdaForm makeReinvokerForm(MethodHandle mh, int num, Object obj, String str, boolean flag, LambdaForm.NamedFunction nf1, LambdaForm.NamedFunction nf2) {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	/*[IF Java10]*/
	static LambdaForm makeReinvokerForm(MethodHandle mh, int num, Object obj, boolean flag, LambdaForm.NamedFunction nf1, LambdaForm.NamedFunction nf2) {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	/*[ENDIF]*/
}
