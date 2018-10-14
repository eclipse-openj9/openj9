/*******************************************************************************
 * Copyright (c) 2004, 2018 IBM Corp. and others
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

/* class to represent a tracepoint */
import java.util.*;
import java.io.*;

public class TracePoint{
	public final static int TYPE_UNKNOWN = -1;
	public final static int TYPE_EVENT = 0;
	public final static int TYPE_EXCEPTION = 1;
	public final static int TYPE_ENTRY = 2;
	public final static int TYPE_ENTRY_EXCEPTION = 3;
	public final static int TYPE_EXIT = 4;
	public final static int TYPE_EXIT_EXCEPTION = 5;
	public final static int TYPE_MEM = 6;
	public final static int TYPE_MEM_EXCEPTION = 7;
	public final static int TYPE_DEBUG = 8;
	public final static int TYPE_DEBUG_EXCEPTION = 9;
	public final static int TYPE_PERF = 10;
	public final static int TYPE_PERF_EXCEPTION = 11;
	public final static int TYPE_ASSERTION = 12;
		
	public final String type;
	public final String name;
	public final int overhead;
	public final int level;
	public final String[] groups;
	public final boolean isexplicit;
	public final boolean isobsolete;
	public final String formatString;
	public final String assertion;
	public final boolean isprivate;
	public final boolean isprefix;
	public final boolean issuffix;
	public final boolean hasnoenv;
	public final boolean istest;
	public final boolean isexception;
	private final TraceComponent component;

        /* this temporarily isn't final until the sov tdf files are fixed */
	public int tpid;
	public final int typeNumber;

	public final int numberofparameters;
	

	/* Current port library limit */
	public static final int MAX_TRACE_PARAMETERS = 16;

	private static class ParseError extends Exception {
		private static final long serialVersionUID = 1L;
		public ParseError(String message) {
			super(message);
		}
		public ParseError(String expected, StreamTokenizer tokenizer) {
			this("Expected " + expected + ", found: " + tokenizer);
		}
	}
	
	private TracePoint(String type, String name, int overhead, int level,
			  String[] groups, boolean isexplicit, boolean isobsolete,
			  String formatString, String assertion, boolean isprivate, boolean isprefix,
			  boolean issuffix, boolean hasnoenv, boolean istest,
			  TraceComponent component, int tracepointid, int numberofparameters, boolean isexception, int typeNum){
		this.type = type;
		this.name = name;
		this.overhead = overhead;
		this.level = level;
		this.groups = groups;
		this.isexplicit = isexplicit;
		this.isobsolete = isobsolete;
		this.formatString = formatString;
		this.assertion = assertion;
		this.isprivate = isprivate;
		this.isprefix = isprefix;
		this.issuffix = issuffix;
		this.hasnoenv = hasnoenv;
		this.istest = istest;
		
		this.component = component;
		this.tpid = tracepointid;                

		this.numberofparameters = numberofparameters;

		this.isexception = isexception;
		this.typeNumber = typeNum;
	}

        public TracePoint( TracePoint tp, int tpid ){
            this( tp.type, tp.name, tp.overhead, tp.level, tp.groups, tp.isexplicit, tp.isobsolete,
                  tp.formatString, tp.assertion, tp.isprivate, tp.isprefix, tp.issuffix, tp.hasnoenv, tp.istest,
                  tp.component, tpid, tp.numberofparameters, tp.isexception, tp.typeNumber  );

            return;
        }

        public int getLevel(){
            return level;
        }

        public String getType(){
            return type;
        }

        public String[] getGroups(){
            return groups;
        }

        public int getTPID(){
            return tpid;
        }

	public static TracePoint parseTracePoint(String tracepoint, TraceComponent component, int tpid){
		TracePoint tp = null;
		String type = null;
		String name = null;
		int overhead = 9;
		int level = 1;
		String[] groups = null;
		boolean isexplicit = false;
		boolean isobsolete = false;
		String formatString = null;
		String assertion = null;
		boolean isprivate = false;
		boolean isprefix = false;
		boolean issuffix = false;
		boolean hasnoenv = false;
		boolean istest = false;
		boolean isexception = false;
		int typeNum = TYPE_UNKNOWN;

		int numberofparameters = 0;

		StreamTokenizer tokenizer = new StreamTokenizer(new StringReader(tracepoint));

		/* underscores are permitted in tracepoint names */
		tokenizer.wordChars('_', '_');
		
		/* commas are permitted in group lists */
		tokenizer.wordChars(',', ',');
		
		try {
			/* read the first token */
			if (tokenizer.nextToken() == StreamTokenizer.TT_WORD) {
				String token = tokenizer.sval;
				if (token.equalsIgnoreCase("TraceEvent")) {
					typeNum = TYPE_EVENT;
					type = "Event";
				} else if ( token.equalsIgnoreCase("TraceException") ){
					typeNum = TYPE_EXCEPTION;
					type = "Exception";
				} else if ( token.equalsIgnoreCase("TraceEntry") ){
					typeNum = TYPE_ENTRY;
					type = "Entry";
				} else if ( token.equalsIgnoreCase("TraceEntry-Exception") ){
					typeNum = TYPE_ENTRY_EXCEPTION;
					type = "Entry-Exception";
				} else if ( token.equalsIgnoreCase("TraceExit") ){
					typeNum = TYPE_EXIT;
					type = "Exit";
				} else if ( token.equalsIgnoreCase("TraceExit-Exception") ){
					typeNum = TYPE_EXIT_EXCEPTION;
					type = "Exit-Exception";
				} else if ( token.equalsIgnoreCase("TraceMem") ){
					typeNum = TYPE_MEM;
					type = "Mem";
				} else if ( token.equalsIgnoreCase("TraceMemException") ){
					typeNum = TYPE_MEM_EXCEPTION;
					type = "MemException";
				} else if ( token.equalsIgnoreCase("TraceDebug") ){
					typeNum = TYPE_DEBUG;
					type = "Debug";
				} else if ( token.equalsIgnoreCase("TraceDebug-Exception") ){
					typeNum = TYPE_DEBUG_EXCEPTION;
					type = "Debug-Exception";
				} else if ( token.equalsIgnoreCase("TracePerf") ){
					typeNum = TYPE_PERF;
					type = "Perf";
				} else if ( token.equalsIgnoreCase("TracePerf-Exception") ){
					typeNum = TYPE_PERF_EXCEPTION;
					type = "Perf-Exception";
				} else if ( token.equalsIgnoreCase("TraceAssert") ) {
					typeNum = TYPE_ASSERTION;
					type = "Assertion";
				}
			}
			if (typeNum == TYPE_UNKNOWN) {
				throw new ParseError("tracepoint type", tokenizer);
			}
			
			name = parseStringValue("tracepoint name", tokenizer);
			
			/* now parse all of the optional fields */
			HashSet seenTokens = new HashSet();
			while (tokenizer.nextToken() != StreamTokenizer.TT_EOF) {
				if (tokenizer.ttype == StreamTokenizer.TT_WORD) {
					String token = tokenizer.sval;
					if (seenTokens.contains(token.toLowerCase())) {
						throw new ParseError(token + " must only appear once in any tracepoint");
					} 
					seenTokens.add(token.toLowerCase());
			
					if (token.equalsIgnoreCase("Overhead")) {
						overhead = parseIntValue("overhead value", tokenizer);
					} else if (token.equalsIgnoreCase("Level")) {
						level = parseIntValue("level value", tokenizer);					
					} else if (token.equalsIgnoreCase("Group")) {
						String group = parseStringValue("group name", tokenizer);
						groups = group.split(",");
					} else if (token.equalsIgnoreCase("Template")) {
						formatString = parseQuotedStringValue("format string", tokenizer);
					} else if (token.equalsIgnoreCase("Assert")) {
						assertion = parseQuotedStringValue("assertion expression", tokenizer);
					} else if (token.equalsIgnoreCase("NoEnv")){
						hasnoenv = true;
					} else if (token.equalsIgnoreCase("Private")){
						isprivate = true;
					} else if (token.equalsIgnoreCase("Exception")){
						isexception = true;
					} else if (token.equalsIgnoreCase("Prefix")){
						isprefix = true;
					} else if (token.equalsIgnoreCase("Suffix")){
						issuffix = true;
					} else if (token.equalsIgnoreCase("Test")){
						istest = true;
					} else if (token.equalsIgnoreCase("Explicit")){
						isexplicit = true;
					} else if (token.equalsIgnoreCase("Obsolete")){
						isobsolete = true;
					} else if (token.equalsIgnoreCase("Entry")) {
						/* ignored for now. Just consume it */
						parseStringValue("tracepoint name", tokenizer);
					} else {
						throw new ParseError("TDF keyword", tokenizer);
					}
				}
			}
			
			if (typeNum == TYPE_ASSERTION) {
				if (assertion == null) {
					throw new ParseError("No Assert specified");
				} else if (formatString != null) {
					throw new ParseError("Template must not be used with TraceAssert");
				} else {
					formatString="** ASSERTION FAILED ** at %s:%d: %s";
					numberofparameters = parseAssertion(assertion);
				}
			} else {
				if (assertion != null) {
					throw new ParseError("Assert must only be used with TraceAssert");
				} else 	if (formatString != null) {
					numberofparameters = parseFormatString(formatString);
					if (numberofparameters > MAX_TRACE_PARAMETERS){
						throw new ParseError("Tracepoint has too many parameters ("+numberofparameters+">"+MAX_TRACE_PARAMETERS+")");
					}
				}
			}
			
		} catch (IOException e) {
			System.err.println(e.getMessage());
			return null;
		} catch (ParseError e) {
			System.err.println("An error occurred parsing tracepoint (" + component + "." + tpid + (name == null ? "" : ":" + name) + ") -- " + e.getMessage());
			return null;
		}		

		if ( TraceGen.classlibbuildenv ) {
			name = "Tr_" + name;
		}
				
		TraceGen.dp( "TracePoint parsed: TracePoint(" + type + ", " + name + ", " + overhead + ", " + level + ", " + groups + ", " + isexplicit + ", " + isobsolete + ", [" +
				    formatString + "], " + isprivate + ", " + isprefix + ", " + issuffix + ", " + hasnoenv + ", " +
				    istest + ", " + component + ", " + tpid + ", " + numberofparameters + ", " + isexception + " " + typeNum + ")");
		
		tp = new TracePoint(type, name, overhead, level, groups, isexplicit, isobsolete,
				    formatString, assertion, isprivate, isprefix, issuffix, hasnoenv,
				    istest, component, tpid, numberofparameters, isexception, typeNum);
		
		return tp;
	}
	
	private static int parseFormatString(String formatString) {
		int numberofparameters = 0;
		
		/* A % at the end of the string is not a valid specifier, and would break the lookahead. */
		int maxIndex = formatString.length() - 1;
		for ( int i = 0; i < maxIndex ; i++ ) {
		    if (formatString.charAt(i) == '%') {
		        if ( formatString.charAt(i+1) == '%' ) {
		            /* an escaped percent sign - move on */
		            i++;
		        } else {
		        	/* found a parameter */
		        	numberofparameters++;
		        	 
		        	/* Keep scanning forward ignoring characters that can appear 
		        	 * in a specifier. If we encounter a '*' then a width or
		        	 * precision specifier will also be specified in the argument list.
		        	 */
		        	while (++i <= maxIndex) {
		        		char c = formatString.charAt(i);
		        		if (c == ' ' || c == '.' || c == '+' 
		        			|| c == '-' || c == '#' || (c >= '0' && c <= '9')) {
		        			/* These characters are all expected inside a format specifier */
		        		} else if (c == '*') {
		        			numberofparameters++;
		        		} else {
		        			/* Found end of specifier */
		        			break;
		        		}
		        	}
		        }
		    }
		}
		return numberofparameters;
	}

	private static int parseIntValue(String expected, StreamTokenizer tokenizer) throws ParseError, IOException {
		/* consume the = */
		if (tokenizer.nextToken() != '=') {
			throw new ParseError("'='", tokenizer);
		}
		/* read the value */
		if (tokenizer.nextToken() == StreamTokenizer.TT_NUMBER) {
			return (int)tokenizer.nval;
		} else {
			throw new ParseError(expected, tokenizer);
		}
	}
	
	private static String parseStringValue(String expected, StreamTokenizer tokenizer) throws ParseError, IOException {
		/* consume the = */
		if (tokenizer.nextToken() != '=') {
			throw new ParseError("'='", tokenizer);
		}
		/* read the value */
		if (tokenizer.nextToken() == StreamTokenizer.TT_WORD) {
			return tokenizer.sval;
		} else {
			throw new ParseError(expected, tokenizer);
		}
	}
	
	private static String parseQuotedStringValue(String expected, StreamTokenizer tokenizer) throws ParseError, IOException {
		/* consume the = */
		if (tokenizer.nextToken() != '=') {
			throw new ParseError("'='", tokenizer);
		}
		/* read the value */
		if (tokenizer.nextToken() != '"') {
			throw new ParseError(expected, tokenizer);
		}
		
		/* The StreamTokenizer processes escape sequences. We don't want this, so undo them */
		String string = tokenizer.sval;
		StringBuffer result = new StringBuffer();
		for (int i = 0; i < string.length(); i++) {
			char c = string.charAt(i);
			switch (c) {
			case '\n':
				result.append("\\n");
				break;
//			case '\t':
//				result.append("\\t");
//				break;
			case '\b':
				result.append("\\b");
				break;
			case '\0':
				result.append("\\0");
				break;
			case '\r':
				result.append("\\r");
				break;
			case '\\':
				result.append("\\\\");
				break;
//			case '\'':
//				result.append("\\\'");
//				break;			
			case '\"':
				result.append("\\\"");
				break;
			default:
				result.append(c);
			}
		}
		return result.toString();
	}
	
	/*
	 * Parse the assertion expression counting the number of arguments.
	 * This is a somewhat naive algorithm.
	 * Any element of the expression of the form P[1-9][0-9]* is considered
	 * to be an argument. The argument with the highest numeric value is
	 * the maximum argument.
	 * 
	 * Thus parseAssertion("P1 + P2"); will retturn 2.
	 * parseAssertion("P999"); will return 999. 
	 */
	private static int parseAssertion(String assertion) throws IOException {
		int result = 0;		
		StreamTokenizer tokenizer = new StreamTokenizer(new StringReader(assertion));

		/* underscores are permitted in C identifiers */
		tokenizer.wordChars('_', '_');
		
		while (tokenizer.nextToken() != StreamTokenizer.TT_EOF) {
			if (tokenizer.ttype == StreamTokenizer.TT_WORD) {
				int argument = 0;
				String word = tokenizer.sval;
				if (word.length() >= 2) {
					if (word.charAt(0) == 'P') {
						char digit = word.charAt(1);
						if (digit >= '1' && digit <= '9') {
							argument = (int)digit - (int)'0';
							for (int i = 2; i < word.length(); i++) {
								digit = word.charAt(i);
								if (digit >= '0' && digit <= '9') {
									argument = (argument * 10) + ((int)digit - (int)'0');
								} else {
									argument = 0;
									break;
								}
							}
						}
					}
				}
				if (argument > result) {
					result = argument;
				}
			}
		}
		
		return result;
	}
	
	public boolean toMacroOnStream(BufferedWriter output) throws IOException{
		StringBuffer workingMacro = new StringBuffer();
		int i = 0;
		String comp = component.getComponentName();
		boolean result = true;

		output.write( "#if UT_TRACE_OVERHEAD >= " + overhead);
		output.newLine();

		if (istest) {
			output.write("#define TrcEnabled_" + name + "  (" + comp + "_UtActive[" + tpid + "] != 0)");
			output.newLine();
		}

		output.write( "#define " );
		output.write( name + "(" );
		if (!hasnoenv){
			workingMacro.append("thr, ");
		}
		/* append number of parameters */
		for(i = 1; i <= numberofparameters ; i++){
			workingMacro.append("P" + i);
			workingMacro.append(", ");
		}
		if ((numberofparameters > 0) || !hasnoenv ) {
			workingMacro.deleteCharAt( workingMacro.length() - 1 ); /* get rid of last space */
			workingMacro.deleteCharAt( workingMacro.length() - 1 ); /* get rid of comma */
		}
		
		output.write(workingMacro.toString());

		output.write( ") do {");
		output.write(" /* tracepoint name: " + comp + "." + tpid + " */");
		output.write(" \\");
		output.newLine();
		
		if (! component.isAuxiliary()) {
			output.write( "\tif ((unsigned char) " + comp + "_UtActive[" + tpid + "] != 0){ \\" );
			output.newLine();
		}
		
		if (typeNumber == TYPE_ASSERTION) {
			result &= generateAssert(output);
		} else {
			result &= generateTrace(output);
		}
		
		if (! component.isAuxiliary()) {
			output.write("} \\");
			output.newLine();
		}
		output.write("\t} while(0)");
		output.newLine();
		output.write("#else");
		output.newLine();
		if (istest) {
			output.write("#define TrcEnabled_" + name + "  (0)");
			output.newLine();
                }
		output.write("#define " );
		output.write( name + "(" );

		workingMacro = new StringBuffer();
		if (!hasnoenv){
			workingMacro.append("thr, ");
		}
		
		for(i = 1; i <= numberofparameters ; i++){
			workingMacro.append("P" + i);
			workingMacro.append(", ");
		}

		if ((numberofparameters > 0) || !hasnoenv) {
			workingMacro.deleteCharAt( workingMacro.length() - 1 ); /* get rid of last space */
			workingMacro.deleteCharAt( workingMacro.length() - 1 ); /* get rid of comma */
		}
		output.write(workingMacro.toString());
		
		output.write(")   /* tracepoint name: " + comp + "." + tpid + " */");
		output.newLine();
		output.write("#endif");
		output.newLine();
		
		return result;
	}

	private boolean generateTrace(BufferedWriter output) throws IOException {
		String comp = component.getComponentName();
		boolean result = true;
		
		output.write( "\t\t" + comp + "_UtModuleInfo.intf->Trace("  );
		if (hasnoenv){
			output.write( "(void *)NULL" );
		} else {
			output.write( "UT_THREAD(thr)" );
		}
		
		if (component.isAuxiliary()) {
			output.write(", &" + comp + "_UtModuleInfo, ((" + tpid + "u << 8)), ");
		} else {
			output.write(", &" + comp + "_UtModuleInfo, ((" + tpid + "u << 8) | " + comp + "_UtActive[" + tpid + "]), ");
		}

		StringBuffer workingMacro = new StringBuffer();
		if (numberofparameters > 0){
			try {
				workingMacro.append( OldTrace.getFormatStringTypes(formatString) );
			} catch (IllegalArgumentException e) {
				output.write("\n#error Error in tdf file, re-run \"gmake -f buildtools.mk tracing\" and fix problems.\n");
				result = false;
			}
			for(int i = 1; i <= numberofparameters ; i++){
				workingMacro.append(", P" + i);
			}
		} else {
		 	workingMacro.append("NULL");
		}
		output.write(workingMacro.toString());
		output.write(");");
		return result;
	}
	
	private boolean generateAssert(BufferedWriter output) throws IOException {
		String comp = component.getComponentName();
		boolean result = true;
		output.write( "\t\tif (");
		output.write(assertion);
		output.write(") { /* assertion satisfied */ } else { \\");
		output.newLine();
		output.write("\t\t\tif (" + comp + "_UtModuleInfo.intf != NULL) { \\"  );
		output.newLine();
		output.write("\t\t\t\t" + comp + "_UtModuleInfo.intf->Trace("  );
		if (hasnoenv){
			output.write( "(void *)NULL" );
		} else {
			output.write( "UT_THREAD(thr)" );
		}
		
		if (component.isAuxiliary()) {
			output.write(", &" + comp + "_UtModuleInfo, (UT_SPECIAL_ASSERTION | (" + tpid + "u << 8), ");
		} else {
			output.write(", &" + comp + "_UtModuleInfo, (UT_SPECIAL_ASSERTION | (" + tpid + "u << 8) | " + comp + "_UtActive[" + tpid + "]), ");
		}
		try {
			String s = OldTrace.getFormatStringTypes(formatString);
			output.write(s);
		} catch (IllegalArgumentException e) {
			output.write("\n#error Error in tdf file, re-run \"gmake -f buildtools.mk tracing\" and fix problems.\n");
			result = false;
		}
		output.write(", __FILE__, __LINE__, UT_STR((" + assertion + "))); \\");
		output.newLine();

		// Flag as unreachable so prevent bogus warnings during static code analysis.
		output.write("\t\t\t\tTrace_Unreachable(); \\");
		output.newLine();
		// Write early assert case for when trace is not initialized.
		// Important - this case does not cause the assert to trigger vm exit.
		// If an assert is hit before trace starts we just print it to stderr.
		output.write("\t\t\t} else { \\");
		output.newLine();
		output.write("\t\t\t\tfprintf(stderr, \"** ASSERTION FAILED ** " + comp + "." + tpid + " at %s:%d " + name + "%s\\n\", __FILE__, __LINE__, UT_STR((" + assertion + "))); \\");
		output.newLine();
		output.write("\t\t\t} \\");
		output.newLine();
		output.write("\t\t}");

		return result;
	}

	public String toDATFileEntry(){
		StringBuffer sb = new StringBuffer();

		char explicitChar;
		StringBuffer trace_format_template = new StringBuffer();

		switch (typeNumber) {
		case TYPE_EXCEPTION:
		case TYPE_ENTRY_EXCEPTION:
		case TYPE_EXIT_EXCEPTION:
		case TYPE_MEM_EXCEPTION:
		case TYPE_DEBUG_EXCEPTION:
		case TYPE_PERF_EXCEPTION:
		case TYPE_ASSERTION:
			trace_format_template.append("*");
			break;
		default:
			trace_format_template.append(" ");
			break;
		}

		switch (typeNumber) {
		case TYPE_ENTRY:
		case TYPE_ENTRY_EXCEPTION:
			trace_format_template.append(">");
			break;
		case TYPE_EXIT:
		case TYPE_EXIT_EXCEPTION:
			trace_format_template.append("<");
			break;
		default:
			trace_format_template.append(" ");
			break;
		}

		trace_format_template.append( formatString );

		if (isexplicit){
			explicitChar = 'Y';
		} else {
			explicitChar = 'N';
		}

		sb.append( component.getComponentName() );
		if (TraceGen.getVersion() >= 5.1){
			sb.append( "." + tpid );
		}
		sb.append(" " + typeNumber + " " + overhead + " " + level + " " + explicitChar + " " + name + " \"" + trace_format_template.toString() + "\"");
		
		return sb.toString();
	}
}
