/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
package com.ibm.jvmti.tests.retransformClasses;

import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class rtc001
{
	public static native boolean retransformClass(Class klass);
	
	public boolean setup(String args)
	{
		return true;
	}

	public boolean testUnsafeAccess()
	{
		try {
			Method method = rtc001_testUnsafe.class.getDeclaredMethod("b");
			method.setAccessible(true);
			if (!Boolean.TRUE.equals(method.invoke(new rtc001_testUnsafe()))) {
				return false;
			}
			
			/* JDK12 disallows direct field reflection on java.lang.reflect.Method.methodAccessor 
			 * (refer https://github.com/eclipse/openj9/issues/4658#issuecomment-461913909 for details), 
			 * this is a workaround via reflection of package access method java.lang.reflect.Method.getMethodAccessor().
			 */
			Method getMethodAccessor = method.getClass().getDeclaredMethod("getMethodAccessor");
			getMethodAccessor.setAccessible(true);
			Object methodAccessor = getMethodAccessor.invoke(method);
			
			System.out.println("Retransforming " + methodAccessor.getClass().getCanonicalName());
			if (!retransformClass(methodAccessor.getClass())) {
				return false;
			}
			if (!Boolean.TRUE.equals(method.invoke(new rtc001_testUnsafe()))) {
				return false;
			}
		} catch (SecurityException e) {
			e.printStackTrace();
			return false;
		} catch (IllegalArgumentException e) {
			e.printStackTrace();
			return false;
		} catch (IllegalAccessException e) {
			e.printStackTrace();
			return false;
		} catch (NoSuchMethodException e) {
			e.printStackTrace();
			return false;
		} catch (InvocationTargetException e) {
			e.printStackTrace();
			return false;
		}
		return true;
	}

	public String helpUnsafeAccess()
	{
		return "Test Unsafe access after retransform.";
	}
}
