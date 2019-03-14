/*[INCLUDE-IF Sidecar19-SE]*/
/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
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

import com.ibm.oti.vm.J9UnmodifiableClass;

@J9UnmodifiableClass
class VarHandleInternal {
	/*[IF ]*/
	/* Methods with VarHandle send target 
	 * The 'ignored' object parameter is required to work around a javac bug
	 * which will be fixed in b141.
	 */
	/*[ENDIF]*/
	native void get_impl(Object ignored);
	native void set_impl(Object ignored);
	native void getVolatile_impl(Object ignored);
	native void setVolatile_impl(Object ignored);
	native void getOpaque_impl(Object ignored);
	native void setOpaque_impl(Object ignored);
	native void getAcquire_impl(Object ignored);
	native void setRelease_impl(Object ignored);
	native void compareAndSet_impl(Object ignored);
	native void compareAndExchange_impl(Object ignored);
	native void compareAndExchangeAcquire_impl(Object ignored);
	native void compareAndExchangeRelease_impl(Object ignored);
	native void weakCompareAndSet_impl(Object ignored);
	native void weakCompareAndSetAcquire_impl(Object ignored);
	native void weakCompareAndSetRelease_impl(Object ignored);
	native void weakCompareAndSetPlain_impl(Object ignored);
	native void getAndSet_impl(Object ignored);
	native void getAndSetAcquire_impl(Object ignored);
	native void getAndSetRelease_impl(Object ignored);
	native void getAndAdd_impl(Object ignored);
	native void getAndAddAcquire_impl(Object ignored);
	native void getAndAddRelease_impl(Object ignored);
	native void getAndBitwiseAnd_impl(Object ignored);
	native void getAndBitwiseAndAcquire_impl(Object ignored);
	native void getAndBitwiseAndRelease_impl(Object ignored);
	native void getAndBitwiseOr_impl(Object ignored);
	native void getAndBitwiseOrAcquire_impl(Object ignored);
	native void getAndBitwiseOrRelease_impl(Object ignored);
	native void getAndBitwiseXor_impl(Object ignored);
	native void getAndBitwiseXorAcquire_impl(Object ignored);
	native void getAndBitwiseXorRelease_impl(Object ignored);
}
