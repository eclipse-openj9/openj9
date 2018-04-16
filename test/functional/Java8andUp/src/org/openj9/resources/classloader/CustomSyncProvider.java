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
package org.openj9.resources.classloader;

import java.io.Serializable;

import javax.sql.RowSetReader;
import javax.sql.RowSetWriter;
import javax.sql.rowset.spi.SyncProvider;
import javax.sql.rowset.spi.SyncProviderException;

/* CustomSyncProvider is loaded by the Application ClassLoader.
 * CustomSyncProvider is used to verify that JVM_LatestUserDefinedLoader returns
 * a valid Application ClassLoader in Java 9+.
 */
public class CustomSyncProvider extends SyncProvider implements Serializable {
	private static final long serialVersionUID = 1L;

	@Override
	public String getProviderID() {
		return null;
	}

	@Override
	public RowSetReader getRowSetReader() {
		return null;
	}

	@Override
	public RowSetWriter getRowSetWriter() {
		return null;
	}

	@Override
	public int getProviderGrade() {
		return 0;
	}

	@Override
	public void setDataSourceLock(int datasource_lock) throws SyncProviderException {
		/* Empty */
	}

	@Override
	public int getDataSourceLock() throws SyncProviderException {
		return 0;
	}

	@Override
	public int supportsUpdatableView() {
		return 0;
	}

	@Override
	public String getVersion() {
		return null;
	}

	@Override
	public String getVendor() {
		return null;
	}
}