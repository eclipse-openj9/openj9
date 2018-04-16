/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
 * Source Control Header
 * 
 * $Author: fengj $ 
 * $Date: 2012/11/23 21:12:03 $
 * $Revision: 1.2 $
 * $Name:  $
 * $Log: WithoutDebugInfo.java,v $
 * Revision 1.2  2012/11/23 21:12:03  fengj
 * JAZZ 60879 : : fix copyright notices in HEAD for Java 8 beta 2012
 *
 * Revision 1.1  2012/03/21 14:33:07  lanxia
 * Jazz 54452:Jason Feng & Ronald Servant: move cmdline_options_testresources from javasvt to ottcvs1
 *
 * Revision 1.1  2007-11-06 16:51:00  vsebe
 * Created for new build system implementation (Hudson). Project contains old cmdline_options_tester/testresources module.
 *
 * Revision 1.1  2006/02/16 00:44:03  rajeev_rattehall
 * Initial check in
 *
 * 
 */

/*
 * Created on Oct 14, 2004
 *
 */

import java.io.IOException;

public class WithoutDebugInfo {
	public static void main( String[] args ) throws Exception {
		switch (Integer.parseInt( args[0] )) {
			case 1:
			case 2:
				System.out.println("OK");
				break;
			case 3:
			case 4:
				// Runtime exception
				Object nullObj = null;
				System.out.println( nullObj.toString() );
				break;
			case 5:
			case 6:
				// Declared exception
				throw new IOException();
			case 7:
			case 8:
				// Print a stack trace, but then exit normally
				new Exception().printStackTrace();
				break;
			case 9:
			case 10:
				// Stacktrace contains some unknown code, some known code
				WithDebugInfo.main( new String[] { "3" } );
				break;
			case 11:
			case 12:
				// Stacktrace contains some unknown code, some known code
				WithDebugInfo.main( new String[] { "5" } );
				break;
			case 13:
			case 14:
				// Stacktrace contains some unknown code, some known code
				WithDebugInfo.main( new String[] { "7" } );
				break;
		}
	}
}
