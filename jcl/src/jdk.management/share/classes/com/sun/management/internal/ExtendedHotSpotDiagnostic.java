/*[INCLUDE-IF JAVA_SPEC_VERSION >= 21]*/
/*
 * Copyright IBM Corp. and others 2023
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

package com.sun.management.internal;

import java.io.IOException;
import java.util.List;
import java.util.Collections;
import com.sun.management.VMOption;
import com.sun.management.HotSpotDiagnosticMXBean;
import com.sun.management.internal.HotSpotDiagnostic;

/**
 * Runtime type for {@link com.sun.management.HotSpotDiagnosticMXBean}.
 */
public class ExtendedHotSpotDiagnostic extends HotSpotDiagnostic implements HotSpotDiagnosticMXBean {

	private static final HotSpotDiagnosticMXBean instance = new ExtendedHotSpotDiagnostic();

	/**
	 * Singleton accessor method.
	 *
	 * @return the <code>HotSpotDiagnostic</code> singleton.
	 */
	public static HotSpotDiagnosticMXBean getInstance() {
		return instance;
	}

	/**
	 * Constructor intentionally private to prevent instantiation by others.
	 */
	private ExtendedHotSpotDiagnostic() {
		super();
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public void dumpHeap(String outputFile, boolean live) throws IOException {
		throw new UnsupportedOperationException();
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public List<VMOption> getDiagnosticOptions() {
		return Collections.emptyList();
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public VMOption getVMOption(String name) {
		if (name == null) {
			throw new NullPointerException("A non-null name is required");
		}
		throw new IllegalArgumentException("OpenJ9 doesn't support the API i.e. the VM option might exist, but OpenJ9 can't retrieve it");
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public void setVMOption(String name, String value) {
		if (name == null) {
			throw new NullPointerException("A non-null name is required");
		}
		if (value == null) {
			throw new NullPointerException("A non-null value is required");
		}
		throw new IllegalArgumentException("OpenJ9 doesn't support the API i.e. the VM option might exist, but OpenJ9 can't set it");
	}
}
