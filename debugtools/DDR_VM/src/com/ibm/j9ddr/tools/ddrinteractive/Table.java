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
package com.ibm.j9ddr.tools.ddrinteractive;

import java.io.PrintStream;
import java.util.ArrayList;

/**
 * Utility class for printing text-based tables.
 */
public class Table {
	
	int numColumns = -1;
	String title = null;
	ArrayList<String[]> rows = new ArrayList<String[]>();
	
	/**
	 * Add a header to the table.
	 * @param row
	 * @return
	 */
	public Table (String title)
	{
		this.title = title;
	}
	
	/**
	 * Add a new row to the table.
	 * @param row
	 * @return
	 */
	public Table row(String... row)
	{
		if (-1 == numColumns) {
			numColumns = row.length;
		} else {
			if (row.length != numColumns) {
				throw new IllegalArgumentException("Invalid row, expected " + numColumns + " elements but got " + row.length);
			}
		}
		rows.add(row);
		return this;
	}
	
	/**
	 * @return Maximum column widths.
	 */
	private int[] computeColumnWidths()
	{
		// Find the max lengths
		int[] widths = new int[numColumns];
		for (String[] row : rows) {
			int column = 0;
			for (String val : row) {
				if (val.length() > widths[column]) {
					widths[column] = val.length();
				}
				column++;
			}
		}
		return widths;
	}
	
	/**
	 * Print the table on the supplied output device.
	 * @param out
	 */
	public void render(PrintStream out) 
	{
		int[] widths = computeColumnWidths();
		title.length();
		
		int rowIndex = 0;
		String colSpace = "  ";

		// Title 
		out.println();
		out.println(title);
		for (int i = 0 ; i < title.length(); i ++) {
			out.append("=");
		}
		out.println();
		
		for (String[] row : rows) {
			int columnIndex = 0;
			
			// Headers
			for (String value : row) {
				int width = widths[columnIndex]; 
				String format = "%-" + width + "s";
				out.append( String.format(format, value) );
				out.append(colSpace);
				columnIndex++;
			}
			out.append("\n");
			
			// Append some lines under the headers
			if (rowIndex == 0) {
				for (int i=0; i < numColumns; i++) {
					int width = widths[i];
					for (int j=0; j < width; j++) {
						out.append("-");
					}
					out.append(colSpace);
				}
				out.append("\n");				
			}
			
			rowIndex++;
		}
		
	}
	
	public static void main(String[] args)
	{
		 new Table("Test")
		 	.row("col1", "col2", "col3")
		 	.row("value-1", "value-2", "value-3")
		 	.row("a longer value", "short", "value-3")
		 	.render(System.out);
	}

}
