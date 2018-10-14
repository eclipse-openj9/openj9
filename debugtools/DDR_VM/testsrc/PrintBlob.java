/*******************************************************************************
 * Copyright (c) 2010, 2018 IBM Corp. and others
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

import java.io.File;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

import javax.imageio.stream.FileImageInputStream;
import javax.imageio.stream.ImageInputStream;

import com.ibm.j9ddr.StructureReader;
import com.ibm.j9ddr.StructureReader.StructureDescriptor;

/**
 * @author andhall
 *
 */
public class PrintBlob
{

	/**
	 * @param args
	 */
	public static void main(String[] args) throws Exception
	{
		if (args.length != 1) {
			System.err.println("Expected 1 argument. Got " + args.length);
			System.exit(1);
		}

		File blobFile = new File(args[0]);
		
		ImageInputStream iis = new FileImageInputStream(blobFile);
		iis.setByteOrder(ByteOrder.LITTLE_ENDIAN);

		StructureReader structReader = new StructureReader(iis);
		
		List<StructureDescriptor> structures = new ArrayList<StructureDescriptor>(structReader.getStructures());
		
		Collections.sort(structures, new Comparator<StructureDescriptor>(){

			public int compare(StructureDescriptor o1, StructureDescriptor o2)
			{
				return o1.getName().compareTo(o2.getName());
			}});
		
		for (StructureDescriptor thisStruct : structures) 
		{
			String superClass = thisStruct.getSuperName();
			if (superClass != null && superClass.length() > 0) {
				System.out.println("Structure: " + thisStruct.getName() + " extends " + superClass);
			} else {
				System.out.println("Structure: " + thisStruct.getName());
			}
			
			
//			List<FieldDescriptor> fields = new ArrayList<FieldDescriptor>(thisStruct.getFields());
//			
//			Collections.sort(fields, new Comparator<FieldDescriptor>(){
//
//				public int compare(FieldDescriptor o1, FieldDescriptor o2)
//				{
//					return o1.getName().compareTo(o2.getName());
//				}});
//			
//			System.out.println("\tFields");
//			
//			for (FieldDescriptor thisField : fields) {
//				System.out.println("\t\t" + thisField.getName() + " - " + thisField.getType());
//			}
//			
//			List<ConstantDescriptor> constants = new ArrayList<ConstantDescriptor>(thisStruct.getConstants());
//			
//			Collections.sort(constants, new Comparator<ConstantDescriptor>(){
//
//				public int compare(ConstantDescriptor o1, ConstantDescriptor o2)
//				{
//					return o1.getName().compareTo(o2.getName());
//				}});
//			
//			System.out.println("\tConstants:");
//			for (ConstantDescriptor thisConstant : constants) {
//				System.out.println("\t\t" + thisConstant.getName());
//			}
		}
	}

}
