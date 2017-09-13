/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package com.ibm.jvm.format;

import java.util.Vector;
import java.math.BigInteger;

/** 
 * Parses the arguments and make available to rest of the formatter
 *
 * @author Tim Preece
 */
final public class TraceArgs {

    public      static            String               traceFile;
    public      static            String               outputFile;
    public      static            boolean              indent;
    public      static            boolean              summary;
    public      static            boolean              debug;
    public      static            boolean              symbolic;
    public		static			  boolean			   isHelpOnly;
    public      static            boolean              gui;
    public      static            boolean              j9;
    public      static            boolean              verbose;
    public      static            String               userVMIdentifier;
    public      static            BigInteger           timeZoneOffset = BigInteger.ZERO;
    public      static            boolean              is50orNewer;
    public      static            boolean              override;
    public      static            String               datFileDirectory = null;
    
   /** Initialises static variables.
    *
    *  <p>This is called each time the TraceFormatter is started, from the initStatics()
    *     method in TraceFormat.java</p>
    */
    final protected static void initStatics()
    {
        traceFile  = null;
        outputFile = null;
        indent     = false;
        summary    = false;
        debug      = false;
        symbolic   = false;
        gui        = false;
        j9         = false;
        userVMIdentifier = null;
	is50orNewer = false;
	override = false;
    }

    /** parses the command line arguements
     *
     * @param   args      the arguments
     * @throws  UsageException
     */
    public TraceArgs(String[] args) throws UsageException
    {
        if ( args.length > 0 ) {
            int index;

            /* Catch a request for help in argument 0. */
            if ( args[0].equals("-help") ) {
                isHelpOnly=true;
           } else {
            
            traceFile = args[0];
           }

            if ( args.length > 1 ) {

                if ( args[1].startsWith("-") ) {
                    outputFile = args[0] + ".fmt";
                    index       =  1;
                } else {
                    outputFile = args[1];
                    index       =  2;
                }

                while ( index < args.length ) {
                    if ( args[index].equals("-summary") ) {
                        summary=true;
                    } else if ( args[index].startsWith("-entries:") ) {
                        parseComponentParameter(args[index]);
                    } else if ( args[index].startsWith("-thread:") ) {
                        parseThread(args[index]);
                    } else if ( args[index].equals("-indent") ) {
                        indent=true;
                    } else if ( args[index].equals("-symbolic") ) {
                        symbolic=true;
                    } else if ( args[index].equals("-help") ) {
                        isHelpOnly=true;
                    } else if (args[index].equals("-version") ) {
                    	System.out.println("TraceFormat Version " + TraceFormat.traceFormatMajorVersion + "." + TraceFormat.traceFormatMinorVersion);
                    } else if ( args[index].equals("-debug") ) {
                        debug=true;
                        verbose=true;
                    } else if ( args[index].equals("-j9") ) {
                        j9=true;
                    } else if (args[index].equals("-verbose")) {
                    	verbose=true;
                    } else if ( args[index].equalsIgnoreCase("-uservmid") ) {
                        index++;
                        if ( (index >= args.length) || (args[index].startsWith("-"))) {
                            throw new UsageException("no uservmid string provided");
                        }
                        userVMIdentifier = args[index];
                    } else if ( args[index].equalsIgnoreCase("-overridetimezone") ) {
                        index++;
                        if ( index >= args.length ) {
                            throw new UsageException("no time provided with overridetimezone option");
                        }                        
                        try {
                            int offset = Integer.parseInt( args[index] );
                            System.out.println("All formatted tracepoints will be formatted with an offset to the hours field of " + offset + "hours.");
                            timeZoneOffset = new BigInteger( args[index] );
                        } catch ( NumberFormatException nfe ){
                            System.err.println("Cannot format " + args[index] + " into an integer to use as an hour offset in Trace Formatter.");
                            System.err.println( nfe );
                            throw new UsageException("please specify an integer value after -overridetimezone that will be used as an offset for the formatted hour field");
                        }                        
                    } else if ( args[index].startsWith("-50") ){
			/* this offers a manual override to enforce 50 behaviour when the trace file has a version number of
			 * < 5.0. This could be useful if you have a development build that produced a snap but didn;t yet have
			 * the ute.h patch to upgrade the version number in the headers.
			 */
			is50orNewer = true;
		    } else if ( args[index].startsWith("-11") ){
			/* this offers a manual override to enforce 50 behaviour when the trace file has a version number of
			 * < 5.0. This could be useful if you have a development build that produced a snap but didn;t yet have
			 * the ute.h patch to upgrade the version number in the headers.
			 */
			is50orNewer = false;
			override = true;

		    } else if ( args[index].startsWith("-datdir") ){
			index++;
                        if ( (index >= args.length) || (args[index].startsWith("-"))) {
                            throw new UsageException("no dat file directory name provided");
                        }
			datFileDirectory = new String( args[index] );
		    } else {
                        throw new UsageException();
                    }
                    index++;
                }
            } else {
                outputFile = args[0] + ".fmt";
            }
        } else {
            throw new UsageException();
        }
        return;
    }

    private void parseComponentParameter(String comp) throws UsageException
    {
        String compString = comp.substring("-entries:".length());
        parseComponents(compString);
    }

    private void parseComponents(String compString) throws UsageException
    {

        int paren   =  compString.indexOf("(");
        int comma   =  compString.indexOf(",");
        if ( comma != -1 ) {
            if ( comma < paren ) {
                parseComponent(compString.substring(0, comma));
                parseComponents(compString.substring(comma + 1, compString.length()));
            } else {
                int closeParen = compString.indexOf(")");
                parseComponent(compString.substring(0, closeParen+1));
                int nextComma = compString.indexOf(",", closeParen);
                if ( nextComma != -1 ) {
                    parseComponents(compString.substring(nextComma + 1, compString.length()));
                }
            }
        } else {
            parseComponent(compString);
        }
    }

    private void parseComponent(String component) throws UsageException
    {
        int open    =  component.indexOf("(");
        int close   =  component.indexOf(")");
        if ( close < open ) {
            throw new UsageException();
        } else if ( close == open ) { // only possible if both == -1
            Util.putComponent(component);
        } else {
            Vector types   =  new Vector(10);
            String temp    =  component.substring(open + 1, component.length()-1);

            int index;
            while ( (index = temp.indexOf(",")) != -1 ) {
                types.addElement(temp.substring(0, index));
                temp = temp.substring(index+1, temp.length());
            }
            types.addElement(temp.substring(0, temp.length()));
            Util.putComponent(component.substring(0, open), types);
        }
    }

    private void parseThread(String thread) throws UsageException
    {
        String id = null;
       try {
            String   temp  = thread.substring("-thread:".length());
            int      index;
             while ( (index = temp.indexOf(",")) != -1 ) {
            	id = temp.substring(0, index);
                Util.putThreadID(Long.decode(id));
                temp = temp.substring(index+1, temp.length());
            }
            id = temp.substring(0, temp.length());
            Util.putThreadID(Long.decode(id));
        } catch ( NumberFormatException nfe ) {
            throw new UsageException("Bad thread ID: " + id);
        }
    }

    class UsageException extends Exception {
        UsageException(String s)
        {
            super(s);
        }

        UsageException()
        {
            super();
        }
    }

}


