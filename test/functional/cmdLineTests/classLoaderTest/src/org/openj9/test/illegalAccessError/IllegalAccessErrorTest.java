/*
 * Copyright IBM Corp. and others 2025
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

package org.openj9.test.illegalAccessError;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.io.InputStream;
import java.io.IOException;
import java.net.URL;
import java.net.URLClassLoader;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import org.openj9.test.util.VersionCheck;

public class IllegalAccessErrorTest {

	public static void main(String[] args) throws Exception {
		boolean result;
		URLClassLoader ucl = new URLClassLoader(new URL[] {});
		Method method = ClassLoader.class.getDeclaredMethod("defineClass", String.class, byte[].class, int.class, int.class);
		method.setAccessible(true);
		try (InputStream in = ucl.getResourceAsStream("org/openj9/test/illegalAccessError/ExtendsDefaultVisibility.class")) {
			byte[] bytes = new byte[in.available()];
			in.read(bytes);
			method.invoke(ucl, "org.openj9.test.illegalAccessError.ExtendsDefaultVisibility", bytes, 0, bytes.length);
			System.out.println("defineClass for ExtendsDefaultVisibility should throw IllegalAccessError but didn't, test failed");
			result = false;
		} catch (InvocationTargetException e) {
			String causeString = e.getCause().toString();
			Pattern pattern = Pattern.compile(
					"java.lang.IllegalAccessError:"
						+ " class org/openj9/test/illegalAccessError/ExtendsDefaultVisibility"
						+ " \\(in unnamed module 0x0{1,16} from loader java/net/URLClassLoader\\)"
						+ " cannot access its superclass"
						+ " org/openj9/test/illegalAccessError/DefaultVisibility"
						+ " \\(in unnamed module 0x0{1,16} from loader jdk/internal/loader/ClassLoaders\\$AppClassLoader\\)");
			Matcher matcher = pattern.matcher(causeString);
			System.out.println("causeString = " + causeString);
			if (((VersionCheck.major() == 8) && causeString.contains(
					"java.lang.IllegalAccessError:"
						+ " class org/openj9/test/illegalAccessError/ExtendsDefaultVisibility"
						+ " (from loader java/net/URLClassLoader) cannot access its superclass"
						+ " org/openj9/test/illegalAccessError/DefaultVisibility"
						+ " (from loader sun/misc/Launcher$AppClassLoader)"))
				|| ((VersionCheck.major() >= 11) && matcher.find())
			) {
				result = true;
				System.out.println("defineClass for ExtendsDefaultVisibility threw IllegalAccessError with the expected exception message");
			} else {
				e.printStackTrace();
				result = false;
				System.out.println("defineClass for ExtendsDefaultVisibility threw an InvocationTargetException with unexpected exception cause, test failed");
			}
		} catch (SecurityException | IllegalAccessException | IOException e) {
			e.printStackTrace();
			result = false;
			System.out.println("defineClass for ExtendsDefaultVisibility threw an unexpected exception, test failed");
		}

		try (InputStream in = ucl.getResourceAsStream("org/openj9/test/illegalAccessError/ImplementDefaultVisibilityInterface.class")) {
			byte[] bytes = new byte[in.available()];
			in.read(bytes);
			method.invoke(ucl, "org.openj9.test.illegalAccessError.ImplementDefaultVisibilityInterface", bytes, 0, bytes.length);
			System.out.println("defineClass for ImplementDefaultVisibilityInterface should throw IllegalAccessError but didn't, test failed");
			result = false;
		} catch (InvocationTargetException e) {
			String causeString = e.getCause().toString();
			Pattern pattern = Pattern.compile(
					"java.lang.IllegalAccessError:"
						+ " class org/openj9/test/illegalAccessError/ImplementDefaultVisibilityInterface"
						+ " \\(in unnamed module 0x0{1,16} from loader java/net/URLClassLoader\\)"
						+ " cannot access its superinterface org/openj9/test/illegalAccessError/DefaultVisibilityInterface"
						+ " \\(in unnamed module 0x0{1,16} from loader jdk/internal/loader/ClassLoaders\\$AppClassLoader\\)");
			Matcher matcher = pattern.matcher(causeString);
			System.out.println("causeString = " + causeString);
			if (((VersionCheck.major() == 8) && causeString.contains(
					"java.lang.IllegalAccessError:"
						+ " class org/openj9/test/illegalAccessError/ImplementDefaultVisibilityInterface"
						+ " (from loader java/net/URLClassLoader) cannot access its superinterface"
						+ " org/openj9/test/illegalAccessError/DefaultVisibilityInterface"
						+ " (from loader sun/misc/Launcher$AppClassLoader)"))
				|| ((VersionCheck.major() >= 11) && matcher.find())
			) {
				result = true;
				System.out.println("defineClass for ImplementDefaultVisibilityInterface threw IllegalAccessError with the expected exception message");
			} else {
				e.printStackTrace();
				result = false;
				System.out.println("defineClass for ImplementDefaultVisibilityInterface threw an InvocationTargetException with unexpected exception cause, test failed");
			}
		} catch (SecurityException | IllegalAccessException | IOException e) {
			e.printStackTrace();
			result = false;
			System.out.println("defineClass for ImplementDefaultVisibilityInterface threw an unexpected exception, test failed");
		}

		try (InputStream in = ucl.getResourceAsStream("org/openj9/test/illegalAccessError/ExtendsDefaultVisibilityInterface.class")) {
			byte[] bytes = new byte[in.available()];
			in.read(bytes);
			method.invoke(ucl, "org.openj9.test.illegalAccessError.ExtendsDefaultVisibilityInterface", bytes, 0, bytes.length);
			System.out.println("defineClass for ExtendsDefaultVisibilityInterface should throw IllegalAccessError but didn't, test failed");
			result = false;
		} catch (InvocationTargetException e) {
			String causeString = e.getCause().toString();
			Pattern pattern = Pattern.compile(
					"java.lang.IllegalAccessError:"
						+ " interface org/openj9/test/illegalAccessError/ExtendsDefaultVisibilityInterface"
						+ " \\(in unnamed module 0x0{1,16} from loader java/net/URLClassLoader\\)"
						+ " cannot access its superinterface org/openj9/test/illegalAccessError/DefaultVisibilityInterface"
						+ " \\(in unnamed module 0x0{1,16} from loader jdk/internal/loader/ClassLoaders\\$AppClassLoader\\)");
			Matcher matcher = pattern.matcher(causeString);
			System.out.println("causeString = " + causeString);
			if (((VersionCheck.major() == 8) && causeString.contains(
					"java.lang.IllegalAccessError:"
						+ " interface org/openj9/test/illegalAccessError/ExtendsDefaultVisibilityInterface"
						+ " (from loader java/net/URLClassLoader) cannot access its superinterface"
						+ " org/openj9/test/illegalAccessError/DefaultVisibilityInterface"
						+ " (from loader sun/misc/Launcher$AppClassLoader)"))
				|| ((VersionCheck.major() >= 11) && matcher.find())
			) {
				result = true;
				System.out.println("defineClass for ExtendsDefaultVisibilityInterface threw IllegalAccessError with the expected exception message");
			} else {
				e.printStackTrace();
				result = false;
				System.out.println("defineClass for ExtendsDefaultVisibilityInterface threw an InvocationTargetException with unexpected exception cause, test failed");
			}
		} catch (SecurityException | IllegalAccessException | IOException e) {
			e.printStackTrace();
			result = false;
			System.out.println("defineClass for ExtendsDefaultVisibilityInterface threw an unexpected exception, test failed");
		}

		if (result) {
			System.out.println("IllegalAccessErrorTest PASSED");
		} else {
			System.out.println("IllegalAccessErrorTest FAILED");
		}
	}
}
