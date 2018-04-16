/*******************************************************************************
 * Copyright (c) 2013, 2018 IBM Corp. and others
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
import java.util.ArrayList;
import java.util.List;


/**
 * <br/>The Tokenizer class converts a command string into a command array with arguments split up in it.
 * <br/>In this case, all parts of the command line would be scanned for purpose of splitting.
 * <br/>
 * <br/>The main operations includes:
 * <br/>1) A well-quoted classpath is treated as a single argument (e.g. "/FOOPATH/.../foo.jar" or "/FOOPATH/"foo1.jar:"/FOOPATH/"foo2.jar)
 * <br/>2) White spaces between arguments of command are ignored when scanning the command string.
 * <br/>3) Arguments without white spaces involved would be filled into the command array eventually without further processing.
 * <br/>
 * <br/>Error condition:
 * <br/>If open/close double-quotes are missing in the command line,  an error message prompting tester with the error
 * <br/>would be thrown out as an exception to the test framework.
 * <br/>
 * <br/>NOTE:
 * <br/>1) Only double-quotes(") are accepted in Tokenizer and single-quotes(') cannot be used as delimiters.
 * <br/>1) Non-quoted and quoted portions of an argument would be concatenated as long as they are part of the same argument.
 * <br/>1) A quote(") itself would not be included in any argument of the command array.
 * <br/>2) An well-quoted empty argument (e.g. "/FOOPATH/" expanding to "") is possible for tester but would be ignored eventually in the command array.
 * <br/>3) No support of escapes or nested quoted strings.
 * <br/>4) Escape characters (e.g. \b, \n, \t) in the classpath would be interpreted into two individual characters if detected.
 *
 * @author Cheng Jin and Francis Bogsanyi
 * @version 1.0
 */
class Tokenizer {

	private int index = 0;
    private final String buffer;

    /**
     * Accepts a command string
     * @param b a command string passed in for splitting
     */
    private Tokenizer(String b) {
        buffer = b;
    }

    /**
     * Ignore the white spaces between arguments in the command string.
     */
    private void skipWhitespace() {
        while ((index < buffer.length()) && Character.isWhitespace(buffer.charAt(index))) {
            index++;
        }
    }

    /**
     * Extract double-quoted classpaths from the command string to form a single argument if spotted.
     */
    private String token() throws Exception {
        String tok = "";
    	skipWhitespace();

        /**
         * The external while loop begins once a non-white-space character is encountered for the first time;
         * otherwise it will ends up returning an empty argument to be ignored in forming the command array.
         *
         * Three cases are taken into account in the while loop:
         * 1) Strings in double-quotes (a argument starting with double-quotes(")) are completely extracted from the command string,
         *    whether white spaces in the argument exist or not. Double-quotes themselves are discarded after extraction when scanning forward.
         * 2) Normal arguments (no double-quotes occur in the argument) are split up and extracted with white spaces between arguments.
         * 3) Following the original order of the command line, non-quoted portions of an argument would be extracted in case 2)
         *    and concatenated to quoted portions of the argument extracted in case 1) within the external while loop.
         */
        while ((index < buffer.length()) && !Character.isWhitespace(buffer.charAt(index))) {

        	/**
        	 * Case 1: Expect a double-quoted string once a double-quotes is detected
        	 */
            if (buffer.charAt(index) == '"') {
                index++;
                int i = index;

                /**
                 * search for another double-quotes since double-quotes appear in pairs.
                 */
                while ((i < buffer.length()) && (buffer.charAt(i) != '"')) {
                    i++;
                }

                /**
                 * Throw out an exception if either of the double-quotes pair is missing
                 */
                if (i == buffer.length()) {
                	throw new Exception("TEST CMD FORMAT ERROR: Quote is missing in the path of .jar file: " + buffer);
                }

                tok += buffer.substring(index, i);

                /**
                 * In quoted case, it will move forward by one character coz the current character is known as a double-quote(")
                 * or the end of command string.
                 */
                index = i + 1;
            } else {

            	/**
            	 * Case 2: normal arguments or non-quoted portions of an argument are scanned
            	 */
                int i = index;

                while ((i < buffer.length()) && (buffer.charAt(i) != '"') && !Character.isWhitespace(buffer.charAt(i))) {
                    i++;
                }

                tok += buffer.substring(index, i);

                /**
                 * In non-quoted case, the current character will be judged later by the external while loop
                 * as well as the if-statement in Case 1 if required. Whether it is a white space, a double-quote(")
                 * or the end of command string has not yet be decided at this point.
                 * Thus, the current character has to be counted in for the later judgment and can't be skipper over.
                 */
                index = i;
            }
        }

        return tok;
    }

    /**
     * Convert a command string to a command array with all arguments split in it.
     * @param buffer a command string passed in for splitting
     * @return command a command array after splitting-up
     */
    public static String[] tokenize(String buffer) throws Exception {

    	List<String> result = new ArrayList<String>();
        Tokenizer t = new Tokenizer(buffer);
        while (t.index < buffer.length()) {
            String token = t.token();
            if (!token.isEmpty()) {
                result.add(token);
            }
        }

        String[] command = new String[result.size()];
        result.toArray(command);

        return command;
    }
}
