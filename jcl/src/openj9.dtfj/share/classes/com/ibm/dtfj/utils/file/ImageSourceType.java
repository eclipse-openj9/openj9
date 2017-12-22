/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2011, 2017 IBM Corp. and others
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
package com.ibm.dtfj.utils.file;

/**
 * Enum which represents the type of source that an image has been derived from
 * i.e. core file, javacore or PHD.
 * 
 * @author adam
 *
 */
public enum ImageSourceType {
	CORE {
		private static final String FACTORY_DTFJ = "com.ibm.dtfj.image.j9.DTFJImageFactory";
		private static final String FACTORY_DDR = "com.ibm.j9ddr.view.dtfj.image.J9DDRImageFactory";
		
		@Override
		public String[] getFactoryNames() {
			return new String[]{FACTORY_DDR, FACTORY_DTFJ};		//DDR factory is given precedence
		}
	},
	JAVACORE {
		private static final String FACTORY_JAVACORE = "com.ibm.dtfj.image.javacore.JCImageFactory";
		
		@Override
		public String[] getFactoryNames() {
			return new String[]{FACTORY_JAVACORE};
		}
	},
	PHD {
		private static final String FACTORY_PHD = "com.ibm.dtfj.phd.PHDImageFactory";
		
		@Override
		public String[] getFactoryNames() {
			return new String[]{FACTORY_PHD};
		}
	},
	META {		//meta-data file which is associated with an image source, so has no factories of it's own
		@Override
		public String[] getFactoryNames() {
			return new String[]{};
		}
	};
	
	public abstract String[] getFactoryNames();
}
