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
package com.ibm.j9ddr.autoblob;

import java.io.File;
import java.io.IOException;
import java.util.Collections;
import java.util.Comparator;
import java.util.LinkedList;
import java.util.List;

import com.ibm.j9ddr.autoblob.config.BlobHeader;
import com.ibm.j9ddr.autoblob.config.Configuration;

/**
 * Reads a config file and prints out the list of headers formatted
 * one-per-line in alphabetical order.
 * 
 * Usage:
 * 
 * java com.ibm.j9ddr.autoblob.SortHeadersInConfig <config file>
 * 
 * @author andhall
 *
 */
public class SortHeadersInConfig
{

	public static final int EXPECTED_ARGS = 1;
	
	/**
	 * @param args
	 * @throws IOException 
	 */
	public static void main(String[] args) throws IOException
	{
		if (args.length < EXPECTED_ARGS) {
			System.err.println("Not enough arguments. Expected " + EXPECTED_ARGS);
			return;
		}

		String filename = args[0];
		File inputFile = new File(filename);
		
		Configuration c = Configuration.loadConfiguration(inputFile, null);
		
		List<BlobHeader> headerList = new LinkedList<BlobHeader>();
		
		headerList.addAll(c.getBlobHeaders());
		
		Collections.sort(headerList, new Comparator<BlobHeader>(){

			public int compare(BlobHeader o1, BlobHeader o2)
			{
				return o1.getName().compareToIgnoreCase(o2.getName());
			}});
		
		System.out.print(Configuration.DDRBLOB_HEADERS_PROPERTY + "=");
		
		boolean first = true;
		
		for (BlobHeader header : headerList) {
			if (! first) {
				System.out.println(",\\");
			}
			
			System.out.print(header.getName());
			
			first = false;
		}
		
		System.out.println();
	}

}
