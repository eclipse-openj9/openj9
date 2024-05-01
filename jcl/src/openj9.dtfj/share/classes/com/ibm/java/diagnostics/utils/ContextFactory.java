/*[INCLUDE-IF Sidecar18-SE]*/
/*
 * Copyright IBM Corp. and others 2011
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
package com.ibm.java.diagnostics.utils;

import com.ibm.dtfj.image.Image;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImageProcess;
import com.ibm.dtfj.java.JavaRuntime;

/**
 * Factory for creating different types of contexts.
 *
 * @author adam
 */
public class ContextFactory {

	private ContextFactory() {
		/* no instances needed */
	}

	/**
	 * Create a DTFJ context.
	 *
	 * @param major DTFJ API major version to be supported
	 * @param minor DTFJ minor version to be supported
	 * @param image the source image for the context
	 * @param space address space for this context (cannot be null)
	 * @param process process in this address space
	 * @param runtime Java runtime for this context (may be null)
	 * @return the context
	 */
	public static IDTFJContext getContext(final int major, final int minor, final Image image, final ImageAddressSpace space, final ImageProcess process, final JavaRuntime runtime) {
		DTFJContext ctx = new DTFJContext(major, minor, image, space, process, runtime);
		ctx.refresh();
		return ctx;
	}

	/**
	 * Create a stub DTFJ context which just contains the global commands.
	 *
	 * @param major DTFJ API major version to be supported
	 * @param minor DTFJ minor version to be supported
	 * @return the context
	 */
	public static IDTFJContext getEmptyContext(final int major, final int minor) {
		IDTFJContext ctx = new EmptyDTFJContext(major, minor, null, null, null, null);
		ctx.refresh();
		return ctx;
	}
}
