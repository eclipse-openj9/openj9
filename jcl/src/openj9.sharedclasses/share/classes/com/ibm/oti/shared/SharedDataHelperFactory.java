/*[INCLUDE-IF SharedClasses]*/
/*
 * Copyright IBM Corp. and others 1998
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
package com.ibm.oti.shared;

/**
 * SharedDataHelperFactory provides an interface used to create SharedDataHelpers.
 *
 * @see SharedDataHelper
 */
public interface SharedDataHelperFactory {

	/**
	 * Return a SharedDataHelper for a given ClassLoader.
	 *
	 * <p>Creates a new SharedDataHelper if one cannot be found, otherwise
	 * if a SharedDataHelper already exists for the ClassLoader, the
	 * existing Helper is returned.
	 *
	 * <p>Returns null if a SecurityManager is installed and there is no
	 * SharedClassPermission for the ClassLoader specified.
	 *
	 * @see SharedDataHelper
	 *
	 * @param owner the ClassLoader which owns the SharedDataHelper
	 *
	 * @return SharedDataHelper a new or existing SharedDataHelper
	 */
	public SharedDataHelper getDataHelper(ClassLoader owner);
}
