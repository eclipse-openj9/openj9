/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.view.dtfj.comparators;

import com.ibm.dtfj.image.ImageModule;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.image.ImageSymbol;
import com.ibm.j9ddr.view.dtfj.test.DTFJComparator;

public class ImageModuleComparator extends DTFJComparator {

	public static final int NAME = 1;
	public static final int PROPERTIES = 2;
	public static final int SECTIONS = 4;
	public static final int SYMBOLS = 8;
	
	// getName()
	// getProperties()
	// getSections()
	// getSymbols()
	public void testEquals(Object ddrObject, Object jextractObject, int members) {
		ImageModule ddrImageModule = (ImageModule) ddrObject;
		ImageModule jextractImageModule = (ImageModule) jextractObject;
		
		// getName()
		if ((NAME & members) != 0) {
			testJavaEquals(ddrImageModule, jextractImageModule, "getName");
		}
		
		// getProperties
		if ((PROPERTIES & members) != 0) { 
			testPropertiesEquals(ddrImageModule, jextractImageModule, "getProperties");
		}
		
		// getSections
		if ((SECTIONS & members) != 0) { 
			new ImageSectionComparator().testComparatorIteratorEquals(ddrImageModule, jextractImageModule, "getSections", ImageSection.class);
		}

		// getSymbols
		if ((SYMBOLS & members) != 0) { 
			new ImageSymbolComparator().testComparatorIteratorEquals(ddrImageModule, jextractImageModule, "getSymbols", ImageSymbol.class);
		}
	}
}
