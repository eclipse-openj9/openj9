/*******************************************************************************
 * Copyright (c) 2004, 2004 IBM Corp. and others
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
import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
//import java.util.regex.Pattern;

/**
 * Check for presence of text in specified file or stdin
 */
public class SimpleGrep
{
	static String sSCCSid="%Z%%M% %I% %W% %G% %U%";
	
  public static void main(String[] args) throws IOException
  {
    boolean result = false;
    switch (args.length)
    {
      case 2 :
        result = searchForStringInFile(args[0], args[1]);
        break;
      case 3 :
        result = searchForStringInFile(args[0], args[1], args[2]);
        break;
      case 0 :
      default :
        System.err.println("usage: SimpleGrep <string> <filename> [<count>]");
        System.err.println("       search for <string> in <filename>");
        System.err.println("       if <min> is specified, search for at <count> matching lines");
        break;
    }
    printResult(result);
  }
  public static boolean searchForStringInFile(String searchText, String filename, String count) throws IOException, FileNotFoundException
  {
    boolean result;
    //System.out.println("Searching for " + count + " occurences of \"" + searchText + "\" in file \"" + filename + "\"");
    int actualCount = countLinesContainingString(searchText, new FileInputStream(filename));
    int expectedCount = Integer.parseInt(count);
    //System.out.println("Found " + actualCount + " matching line" + (actualCount == 1 ? "." : "s."));
    result = expectedCount == actualCount;
    if (!result)
      displayFile(filename);
    return result;
  }
  public static boolean searchForStringInFile(String searchText, String filename) throws IOException, FileNotFoundException
  {
    boolean result;
    //System.out.println("Searching for \"" + searchText + "\" in file \"" + filename + "\"");
    result = searchForString(searchText, new FileInputStream(filename));
    //System.out.println("Found" + (result ? " " : " no ") + "match.");
    if (!result)
      displayFile(filename);
    return result;
  }
  public static boolean searchForString(String searchString, InputStream in) throws IOException
  {
    // create a buffered reader with a 1Mb buffer
    BufferedReader br = new BufferedReader(new InputStreamReader(in));

    for (String line = br.readLine(); line != null; line = br.readLine())
    {
      if (line.indexOf(searchString) != -1)
      {
        return true;
      }
    }
    br.close();
    return false;
  }

  public static int countLinesContainingString(String searchString, InputStream in) throws IOException
  {
    int count = 0;
    // create a buffered reader with a 1Mb buffer
    BufferedReader br = new BufferedReader(new InputStreamReader(in));

    for (String line = br.readLine(); line != null; line = br.readLine())
    {
      if (line.indexOf(searchString) != -1)
      {
        count++;
      }
    }
    br.close();
    return count;
  }

  public static void displayFile(String filename) throws IOException, FileNotFoundException
  {
    BufferedReader br = new BufferedReader(new FileReader(filename));

    for (String line = br.readLine(); line != null; line = br.readLine())
    {
      System.out.println(filename + ": " + line);
    }
  }
  public static void printResult(boolean result)
  {
    System.out.println("TEST " + (result ? "PASSED" : "FAILED"));
  }
}
