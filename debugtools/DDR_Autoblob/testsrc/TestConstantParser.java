import java.io.File;
import java.io.IOException;

import com.ibm.j9ddr.autoblob.constants.ConstantParser;
import com.ibm.j9ddr.autoblob.constants.ICVisitor;

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
public class TestConstantParser
{

	/**
	 * @param args
	 */
	public static void main(String[] args) throws IOException
	{
		for (String filename : args)
		{
			System.out.println("Trying " + filename);
			
			ConstantParser parser = new ConstantParser();
			
			parser.visitCFile(new File(filename), new ICVisitor(){

				public void visitComment(String comment, int lineNumber)
				{
					System.out.println("START COMMENT, LINE " + lineNumber);
					System.out.println(comment);
					System.out.println("END COMMENT");
				}

				public void visitNumericConstant(String name, int lineNumber)
				{
					System.out.println("CONSTANT NAME: " + name + " LINE: " + lineNumber);
				}

				public void visitNoValueConstant(String name, boolean defined)
				{
					
				}});
		}
	}

}
