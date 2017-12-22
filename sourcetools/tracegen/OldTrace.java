/*******************************************************************************
 * Copyright (c) 2004, 2014 IBM Corp. and others
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

/* This class contains snippets of code from the old tracegen, which are not directly ported to
 * the new tracegen due to their complexity or the fact that they will be phased out shortly */
public class OldTrace{

	private static String subParm;
	private static boolean processingTracepoint = true;
	private static StringBuffer formatTemplate;
	private static boolean j9build;

	private static String template;
	private static boolean isWindows;
	private static boolean isLinux;
	private static boolean platform64bit = false; /* I don't think this was even used in the old tracegen */

	private static int parmType[] = new int[100];
	private static int parmCount = 0;

	static {
		String os = System.getProperty("os.name").toLowerCase();
		isWindows = (os.indexOf("windows") != -1);
		isLinux = (os.indexOf("linux") != -1);
	}

	public static String getFormatStringTypes(String formatString){
		j9build = TraceGen.j9buildenv;
		
		parmCount = 0;
		subParm = "\"" + formatString + "\"";
		int rc = processTemplate();
		if( rc == 0 ) {
			return template;
		} else {
			throw new IllegalArgumentException();
		}
	}

	static int tdfSyntaxError = -99;

    /*
     *  Trace data type identifiers
     */

    public static final int TRACE_DATA_TYPE_END         = 0;
    public static final int TRACE_DATA_TYPE_CHAR        = 1;
    public static final int TRACE_DATA_TYPE_SHORT       = 2;
    public static final int TRACE_DATA_TYPE_INT32       = 4;
    public static final int TRACE_DATA_TYPE_INT64       = 8;
    public static final int TRACE_DATA_TYPE_STRING      = 255;

    // The above are defined for compatibility

    public static final int TRACE_DATA_TYPE_FLOAT       = 5;
    public static final int TRACE_DATA_TYPE_POINTER     = 6;
    public static final int TRACE_DATA_TYPE_DOUBLE      = 7;
    public static final int TRACE_DATA_TYPE_LONG_DOUBLE = 9;
    public static final int TRACE_DATA_TYPE_PRECISION   = 10;

    /*
     *  Miscellaneous constants
     */

    public static final byte TYPE_EXPLICIT    = 64;

    /**************************************************************************
     * name        - processTemplate
     * description - Processes the Template keyword
     * parameters  - None
     * returns     - Return code
     *************************************************************************/

    static int processTemplate()
    {
        int  i = 0;
        int  rc = 0;
        int  start;
        int  modifier;
        int  modifierSize;
        boolean precision;
        String newModifier = "";

        /*
         *  The following bitmasks indicate the allowed state transitions when
         *  processing a '%' type specifier. The states are noted in comments
         *  at the point they become active. This seemingly complex technique
         *  is required because we need to catch syntax errors now, rather than
         *  at trace formatting time.
         */
        byte allowFromStates[] =
        {
            (byte)0xBE,  /* State 0 allows entry from states 0, 2 - 6  */
            (byte)0x80,  /* State 1 allows entry from states 0         */
            (byte)0xE0,  /* State 2 allows entry from states 1,2       */
            (byte)0xF0,  /* State 3 allows entry from states 1,2,3     */
            (byte)0xF0,  /* State 4 allows entry from states 1,2,3     */
            (byte)0x0c,  /* State 5 allows entry from states 4,5       */
            (byte)0xF4,  /* State 6 allows entry from states 1,2,3,5   */
            (byte)0xF6,  /* State 7 allows entry from states 1,2,3,5,6 */
            (byte)0x41   /* State 8 allows entry from states 1,7       */
        };

        int state;
        /*
         *  State number to bit lookup table.
         */
        byte stateBits[] = {(byte)0x80, (byte)0x40, (byte)0x20, (byte)0x10,
                            (byte)0x08, (byte)0x04, (byte)0x02, (byte)0x01};
        StringBuffer str;
        StringBuffer incStr;


        str = new StringBuffer();
        str.append('\"');
        incStr = new StringBuffer();

        if (processingTracepoint) {
            if ((subParm.length() > 1) &&
                (subParm.charAt(0) == '\"')) {
                formatTemplate = new StringBuffer(subParm);
                while (((i = formatTemplate.toString().indexOf('%', i)) != -1) &&
                       (rc == 0)) {
                    TraceGen.dp("Found a possible format specifier");
                    modifier = -1;
                    modifierSize = 0;
                    parmType[parmCount] = 0;
                    precision = false;
                    /*
                     *  Enter state 0 (Parsing specifier)
                     */
                    state = 0;
                    start = i;
                    while ((++i < formatTemplate.length()) &&
                           ((stateBits[state] & allowFromStates[0]) != 0) &&
                           (rc == 0)) {
                        TraceGen.dp("Parsing started for format specifier");
                        switch (formatTemplate.charAt(i)) {
                        /*
                         *  Enter state 1 (Found another %)
                         */
                        case '%':
                            if ((stateBits[state] & allowFromStates[1]) != 0) {
                                state = 1;
                            } else {
                                rc = tdfSyntaxError;
                            }
                            break;
                            /*
                             *  Enter state 2 (Found a flag)
                             */
                        case '+':
                        case '-':
                        case ' ':
                        case '#':
                            if ((stateBits[state] & allowFromStates[2]) == 0) {
                                rc = tdfSyntaxError;
                            }
                            state = 2;
                            break;
                        case '0': 
                        	/* 0 can be a width, a precision, or a request for left padding with 0. */ 
                        	if (formatTemplate.length() > i) {
                        		/* For a valid string, there must always be a follow on type specifier, so this
                        		 * code runs in all valid cases
                        		 */
                        		char peekAhead = formatTemplate.charAt(i+1);
                        		if(peekAhead == '.') { 
									/* This 0 is part of a width specifier if the next char is a . */
									if ((stateBits[state] & allowFromStates[3]) == 0) {
		                                rc = tdfSyntaxError;
									}
									state = 3;
								} else if (Character.isDigit(peekAhead)) {
									/* A digit follows, could be in the middle of a big width/precision number, or could be 
									 * that this 0 is for 0 padding and the next number is the beginning of a width specifier. 
									 * Would need to completely rewrite with a tokeniser to be certain, so naively take
									 * 0 padding case for now. %.1024s for example may not work with this assumption. 
									 */ 
		                            if ((stateBits[state] & allowFromStates[2]) == 0) {
		                                rc = tdfSyntaxError;
		                            }
		                            state = 2;
								} else if (state == 5) {
									/* We are in the precision specifier, so this can't be 0 padding */
									if ((stateBits[state] & allowFromStates[5]) == 0) {
		                                rc = tdfSyntaxError;
									}
									state = 5;
								} else {
									/* This 0 will be interpreted as 0 padding, reached in instances were peakAhead is a type specifier */
		                            if ((stateBits[state] & allowFromStates[2]) == 0) {
		                                rc = tdfSyntaxError;
		                            }
								}
                        	} else {
                        		rc = tdfSyntaxError;
                        	}
                        	break;
                            
                            /*
                             *  Enter state 3 or 5 (Found a width or precision)
                             */
                        case '*':
                        case '1':
                        case '2':
                        case '3':
                        case '4':
                        case '5':
                        case '6':
                        case '7':
                        case '8':
                        case '9':
                            if ((stateBits[state] & allowFromStates[3]) != 0) {
                                state = 3;
                            } else if ((stateBits[state] & allowFromStates[5]) != 0) {
                                state = 5;
                                if (formatTemplate.charAt(i) == '*') {
                                    precision = true;
                                }
                            } else {
                                rc = tdfSyntaxError;
                            }
                            break;

                        /*
                         *  Enter state 4  (Found a period)
                         */
                        case '.':
                            if ((stateBits[state] & allowFromStates[4]) == 0) {
                                rc = tdfSyntaxError;
                            }
                            state = 5;
                            break;
                            /*
                             *  Enter state 6 (Found a size specifier)
                             */
                        case 'h':
                        case 'L':
                        case 'z':
                        case 'P':   // The P size specifier indicates an integer that can contain a pointer
                            if ((stateBits[state] & allowFromStates[6]) == 0) {
                                rc = tdfSyntaxError;
                            } else {
                                modifier = i;
                                state = 6;
                            }
                            break;
                        case 'l':
                            if ((stateBits[state] & allowFromStates[6]) == 0) {
                                rc = tdfSyntaxError;
                            } else {
                                modifier = i;
                                state = 6;
                                // Check for a long long
                                if (formatTemplate.length() > i + 1) {
                                	switch(formatTemplate.charAt(i + 1))  {
                                	
                                	case 'l' :
                                		i++;
                                		break;
                                	/* %ld, %li, %lx, %lX and %lu are not valid modifiers.  */	
                                	case 'd' :
                                	case 'i' :
                                	case 'x' :
                                	case 'X' :
                                	case 'u' :
                                    	if (true == TraceGen.treatWarningAsError) {
                                    		System.err.println("ERROR : %l is not valid modifier. (parameter " + parmCount + " in mutated format string " + formatTemplate + ")");
                                    		return tdfSyntaxError;
                                    	} else {
                                    		System.out.println("WARNING : Modifier %l is not supported and will be ignored.  (parameter " + parmCount + " in mutated format string " + formatTemplate + ")");
                                    	}
                                	}
                                }
                            }
                            break;
                            /*
                             *  Enter state 7 (Found a type)
                             */
                        case 'c':
                        case 'd':
                        case 'e':
                        case 'E':
                        case 'f':
                        case 'g':
                        case 'G':
                        case 'i':
                        case 'o':
                        case 'p':
                        case 'x':
                        case 'X':
                        case 'u':
                        case 'J':
                            if (precision) {
                                rc = tdfSyntaxError;
                                break;
                            }
                            // Fall through
                        case 's':
                            if ((stateBits[state] & allowFromStates[7]) == 0) {
                                rc = tdfSyntaxError;
                            }

                            switch (formatTemplate.charAt(i)) {
                            case 'c':
                                str.append("\\" + Integer.toOctalString(TRACE_DATA_TYPE_CHAR));
                                incStr.append(Integer.toHexString(TRACE_DATA_TYPE_CHAR));
                                break;
                            case 'X':
                                formatTemplate.replace(i, i + 1, "x");          // Make it consistent
                                // Fall through
                            case 'x':
                                if (start > 1 &&
                                    (formatTemplate.charAt(start - 2) != '0' ||
                                     (formatTemplate.charAt(start - 1) != 'x' &&
                                      formatTemplate.charAt(start - 1) != 'X'))) {
                                    formatTemplate.insert(start, "0x");
                                    i += 2;
                                    if (modifier != -1) {
                                        modifier += 2;
                                    }
                                }
                                // Fall through
                            case 'd':
                            case 'i':
                            case 'o':
                            case 'u':
                                if (modifier == -1) {
                                    str.append("\\" + Integer.toOctalString(TRACE_DATA_TYPE_INT32));
                                    incStr.append(Integer.toHexString(TRACE_DATA_TYPE_INT32));
                                    break;
                                }
                                switch (formatTemplate.charAt(modifier)) {

                                case 'h':
                                    str.append("\\" + Integer.toOctalString(TRACE_DATA_TYPE_SHORT));
                                    incStr.append(Integer.toHexString(TRACE_DATA_TYPE_SHORT));
                                    break;

                                case 'l':
                                case 'L':
                                    modifierSize = i - modifier;              // Cater for  L, l, or ll
                                    str.append("\\" + Integer.toOctalString(TRACE_DATA_TYPE_INT64));
                                    incStr.append(Integer.toHexString(TRACE_DATA_TYPE_INT64));
                                    newModifier = (isWindows) ? "I64" : "ll"; // Windows uses I64, the rest use ll
                                    break;
                                case 'z':
                                case 'P':
                                    modifierSize = 1;
                                    str.append("\\" + Integer.toOctalString(TRACE_DATA_TYPE_POINTER));
                                    incStr.append(Integer.toHexString(TRACE_DATA_TYPE_POINTER));
                                    if (j9build) {
                                        newModifier = "z";
                                    } else {
                                        if (platform64bit) {
                                            newModifier = (isWindows) ? "I64" : "ll"; // Windows uses I64, the rest use ll
                                        } else {
                                            newModifier = "";
                                        }
                                    }
                                    break;

                                default:
                                    flagTdfSyntaxError("Invalid length modifier and type.");
                                    rc = tdfSyntaxError;
                                }
                                break;
                            case 'J':                                   // Java object reference.
                                parmType[parmCount] = 1;                // Flag the parameter as a special case
                                if (j9build) {
                                   formatTemplate.replace(i, i + 1, "zx"); // Convert it to a pointer specifier
                                } else {
                                   formatTemplate.replace(i, i + 1, "p");  // Convert it to a pointer specifier
                                }
                                   // Fall through
                            case 'p':
                                str.append("\\" + Integer.toOctalString(TRACE_DATA_TYPE_POINTER));
                                incStr.append(Integer.toHexString(TRACE_DATA_TYPE_POINTER));
                                if (j9build) {
                                    formatTemplate.delete(i, i + 1);
                                    formatTemplate.insert(i, "zx");
                                }
                                if (start > 1 &&
                                    formatTemplate.charAt(start - 2) == '0' &&
                                    (formatTemplate.charAt(start - 1) == 'x' ||
                                     formatTemplate.charAt(start - 1) == 'X')) {
                                    if (!j9build & isLinux) {
                                        formatTemplate.delete(start - 2, start);
                                        i -= 2;
                                    }
                                } else {
                                    if (j9build || !isLinux) {
                                        formatTemplate.insert(start, "0x");
                                        i += 2;
                                    }
                                }
                                break;
                            case 'e':
                            case 'E':
                            case 'f':
                            case 'g':
                            case 'G':
                           	 /* CMVC 164940 All %f tracepoints are internally promoted to double.
                           	  * Affects:
                           	  *  TraceFormat  com/ibm/jvm/format/Message.java
                           	  *  TraceFormat  com/ibm/jvm/trace/format/api/Message.java
                           	  *  runtime    ute/ut_trace.c
                           	  *  TraceGen     OldTrace.java 
                           	  */

                                if (modifier == -1) {
                                    str.append("\\" + Integer.toOctalString(TRACE_DATA_TYPE_DOUBLE));
                                    incStr.append(Integer.toHexString(TRACE_DATA_TYPE_DOUBLE));
                                } else if (formatTemplate.charAt(modifier) == 'L') {
                                    str.append("\\" + Integer.toOctalString(TRACE_DATA_TYPE_LONG_DOUBLE));
                                    incStr.append(Integer.toHexString(TRACE_DATA_TYPE_LONG_DOUBLE));
                                } else {
                                    flagTdfSyntaxError("Invalid length modifier and type.");
                                    rc = tdfSyntaxError;
                                }
                                break;
                            case 's':

                                if (precision) {
                                    str.append("\\" + Integer.toOctalString(TRACE_DATA_TYPE_PRECISION));
                                    incStr.append(Integer.toHexString(TRACE_DATA_TYPE_PRECISION));
                                    parmCount++;
                                }
                                str.append("\\" + Integer.toOctalString(TRACE_DATA_TYPE_STRING));
                                incStr.append(Integer.toHexString(TRACE_DATA_TYPE_STRING));
                                break;
                            }
                            /*
                             *  Modifier size will be non-zero if it has to be changed
                             */
                            if (modifierSize != 0) {
                                formatTemplate.replace(modifier, modifier + modifierSize, newModifier);
                                i += (newModifier.length() - modifierSize);
                            }
                            parmCount++;
                            state = 7;
                            break;
                        default:
                            rc = tdfSyntaxError;
                        }
                        TraceGen.dp("State " + state + " entered for " + formatTemplate.charAt(i));
                    }
                    if ((rc != 0) || ((stateBits[state] & allowFromStates[8]) == 0)) {
                        flagTdfSyntaxError("Invalid template specifier");
                        rc = tdfSyntaxError;
                    } else {
                        state = 1;
                        TraceGen.dp("State reset to 1. Specifiers so far: " + str + "\"");
                    }
                }
                str.append('\"');
                template = new String(str);
                incStr.append("00");
            } else {
                flagTdfSyntaxError("Quoted string string expected for Template");
                rc = tdfSyntaxError;
            }
        } else {
            flagTdfSyntaxError("Keyword not valid here");
            rc = tdfSyntaxError;
        }
        return rc;
    }

    static void flagTdfSyntaxError(String error){
    	System.err.println("error in tdf file: \n\tparameter "+parmCount+" in mutated format string "+formatTemplate+" is incorrect\n\t") ;
    	System.err.println(error);
    }

}
