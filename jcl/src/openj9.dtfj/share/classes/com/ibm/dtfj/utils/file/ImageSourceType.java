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
package com.ibm.dtfj.utils.file;

/**
 * Enum which represents the type of source that an image has been derived from
 * i.e. core file, javacore or PHD.
 *
 * @author adam
 */
public enum ImageSourceType {

	CORE("com.ibm.j9ddr.view.dtfj.image.J9DDRImageFactory"), //$NON-NLS-1$
	JAVACORE("com.ibm.dtfj.image.javacore.JCImageFactory"), //$NON-NLS-1$
	PHD("com.ibm.dtfj.phd.PHDImageFactory"), //$NON-NLS-1$
	META;

	private final String[] factoryNames;

	ImageSourceType(String... factoryNames) {
		this.factoryNames = factoryNames;
	}

	public String[] getFactoryNames() {
		return factoryNames;
	}

}
