import java.io.File;
import java.util.Collections;
import java.util.Comparator;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;

import com.ibm.dtfj.image.Image;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImageFactory;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.j9ddr.view.dtfj.image.J9DDRImageFactory;

/*******************************************************************************
 * Copyright (c) 2010, 2014 IBM Corp. and others
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

/**
 * @author andhall
 *
 */
public class DumpImageMemoryRanges
{

	/**
	 * @param args
	 */
	public static void main(String[] args) throws Exception
	{
		String fileName = args[0];
		boolean useJExtract = false;
		
		if (args.length > 1) {
			if (args[1].toLowerCase().trim().equals("jextract")) {
				useJExtract = true;
			}
		}
		
		ImageFactory factory = null;
		if (useJExtract) {
			try {
				Class<?> jxfactoryclass = Class.forName("com.ibm.dtfj.image.j9.ImageFactory");
				factory =  (ImageFactory)jxfactoryclass.newInstance();
			} catch (Exception e) {
				System.out.println("Could not create a jextract based implementation of ImageFactory");
				e.printStackTrace();
				System.exit(1);
			}
		} else {
			factory = new J9DDRImageFactory();
		}
		
		Image img = factory.getImage(new File(fileName));
		
		Iterator<?> addressSpaceIt = img.getAddressSpaces();
		
		while (addressSpaceIt.hasNext()) {
			Object addressSpaceObj = addressSpaceIt.next();
			
			if (addressSpaceObj instanceof ImageAddressSpace) {
				ImageAddressSpace as = (ImageAddressSpace)addressSpaceObj;
				
				System.err.println("Address space " + as);
				
				List<ImageSection> imageSections = new LinkedList<ImageSection>();
				
				Iterator<?> isIt = as.getImageSections();
				
				while (isIt.hasNext()) {
					Object isObj = isIt.next();
					
					if (isObj instanceof ImageSection) {
						ImageSection thisIs = (ImageSection)isObj;
						
						imageSections.add(thisIs);
					} else {
						System.err.println("Weird image section object: " + isObj + ", class = " + isObj.getClass().getName());
					}
				}
				
				Collections.sort(imageSections, new Comparator<ImageSection>(){

					public int compare(ImageSection object1,
							ImageSection object2)
					{
						int baseResult = Long.signum(object1.getBaseAddress().getAddress() - object2.getBaseAddress().getAddress());
						
						if (baseResult == 0) {
							return Long.signum(object1.getSize() - object2.getSize());
						} else {
							return baseResult;
						}
					}});
				
				for (ImageSection thisIs : imageSections) {
					System.err.println("0x" + Long.toHexString(thisIs.getBaseAddress().getAddress()) + " - " + "0x" + Long.toHexString(thisIs.getBaseAddress().getAddress() + thisIs.getSize() - 1));
				}
			} else {
				System.err.println("Weird address space object: " + addressSpaceObj + " class = " + addressSpaceObj.getClass().getName());
			}
		}
	}

}
