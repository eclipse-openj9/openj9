/*[INCLUDE-IF SharedClasses]*/
package com.ibm.oti.shared;

/*******************************************************************************
 * Copyright (c) 1998, 2016 IBM Corp. and others
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

import java.nio.ByteBuffer;

import com.ibm.oti.util.Msg;

/**
 * Implementation of SharedDataHelper. 
 * <p>
 * @see SharedDataHelper
 * @see SharedAbstractHelper
 */
final class SharedDataHelperImpl extends SharedAbstractHelper implements SharedDataHelper {
	/* Not public - should only be created by factory */
	SharedDataHelperImpl(ClassLoader loader, int id, boolean canFind, boolean canStore) {
		initialize(loader, id, canFind, canStore);
	}

	private native ByteBuffer findSharedDataImpl(int loaderId, String token);

	private native ByteBuffer storeSharedDataImpl(ClassLoader loader, int loaderId, String token, ByteBuffer data);

	@Override
	public ByteBuffer findSharedData(String token) {
		ClassLoader loader = getClassLoader();
		if (loader == null) {
			/*[MSG "K059f", "ClassLoader has been garbage collected. Returning null."]*/
			printVerboseInfo(Msg.getString("K059f")); //$NON-NLS-1$
			return null;
		}
		if (!canFind) {
			return null;
		}
		if (!checkReadPermission(loader)) {
			/*[MSG "K05b5", "Read permission denied. Returning null."]*/
			printVerboseError(Msg.getString("K05b5")); //$NON-NLS-1$
			return null;
		}
		if (token==null) {
			/*[MSG "K05b6", "Cannot call findSharedData with null token. Returning null."]*/
			printVerboseError(Msg.getString("K05b6")); //$NON-NLS-1$
			return null;
		}
		return findSharedDataImpl(this.id, token);
	}

	@Override
	public ByteBuffer storeSharedData(String token, ByteBuffer data) {
		ClassLoader loader = getClassLoader();
		if (loader == null) {
			/*[MSG "K059f", "ClassLoader has been garbage collected. Returning null."]*/
			printVerboseInfo(Msg.getString("K059f")); //$NON-NLS-1$
			return null;
		}
		if (!canStore) {
			return null;
		}
		if (!checkWritePermission(loader)) {
			/*[MSG "K05b7", "Write permission denied. Returning null."]*/
			printVerboseError(Msg.getString("K05b7")); //$NON-NLS-1$
			return null;
		}
		if (token==null) {
			/*[MSG "K05b8", "Cannot call storeSharedData with null token. Returning null."]*/
			printVerboseError(Msg.getString("K05b8")); //$NON-NLS-1$
			return null;
		}
		if (data!=null && !data.isDirect()) {
			/*[MSG "K05b9", "Cannot call storeSharedData with a non-direct ByteBuffer. Returning null."]*/
			printVerboseError(Msg.getString("K05b9")); //$NON-NLS-1$
			return null;
		}
		return storeSharedDataImpl(loader, this.id, token, data);
	}

	@Override
	String getHelperType() {
		return "SharedDataHelper"; //$NON-NLS-1$
	}
}
