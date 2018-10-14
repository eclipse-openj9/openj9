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
package j9vm.runner;
import java.util.*;
import java.io.*;
import com.oti.j9.exclude.*;

public class Menu {
	Vector menuItems;
	Vector allClassNames;
	String jarFileName;
	String exeName;
	String bootClassPath;
	String userClassPath;
	String javaVersion;
	
public Menu(String jarFileName, String exeName, String bootClassPath, String userClassPath, String javaVersion)  {
	this.menuItems = new Vector();
	this.allClassNames = new Vector();
	this.jarFileName = jarFileName;
	this.exeName = exeName;
	this.bootClassPath = bootClassPath;
	this.userClassPath = userClassPath;
	this.javaVersion = javaVersion;
}
public void addElement(MenuItem item)  {
	menuItems.addElement(item);
}

public void printMenu()  {
	System.out.println("---- VMTest Menu ----");
	for (int i = 0; i<menuItems.size(); i++)  {
		MenuItem item = (MenuItem)menuItems.get(i);
		String front = Integer.toString(i+1) + ") ";
		while (front.length() < 4)  front = front + " ";
		System.out.print(front);
		System.out.println(item.getDisplayString());
	}
	System.out.println("A)  Run ALL tests");
	System.out.println("Q)  Quit");
}

public void doCommand(String command)  {
	if (command.equalsIgnoreCase("A"))  {
		runTestSet(allClassNames);
		return;
	}
	try  {
		int n = Integer.valueOf(command).intValue();
		if (command.equals(String.valueOf(n)))  {
			n--;
			if ((n >= 0) && (n < menuItems.size()))  {
				runTestSet(((MenuItem)menuItems.get(n)).getClassNames());
			}
		}
	} catch (NumberFormatException e)  {
		/* Nothing */
	}
}

public void doMenu()  {
	byte[] buf = new byte[256];
	boolean fPrintMenu = true;
	for (;;)  {
		if (fPrintMenu)  {
			printMenu(); fPrintMenu = false;	
		}
		System.out.print("Enter command(s): ");
		System.out.flush();
		try  {
			int result = System.in.read(buf, 0, buf.length);
			if (result == 2 && buf[0] == 13) {
				/* User just pressed return */
				fPrintMenu = true;
				continue;
			}
		} catch (IOException e)  {
			return;
		}
		StringTokenizer tokenizer = new StringTokenizer(new String(buf));
		while (tokenizer.hasMoreTokens())  {
			String command = tokenizer.nextToken();
			if (command.equalsIgnoreCase("Q"))  {
				System.exit(0);
			}
			doCommand(command);
		}
	}
}

public static void usage()  {
	System.out.println("Usage: j9 [vmOptions] j9vm.runner.Menu [testOptions]");
	System.out.println("testOptions:");
	System.out.println("  -jar=<jarFileName>    Read tests out of jar file <jarFileName>");
	System.out.println("    (make sure the tests are in the classpath too, of course!)");
	System.out.println("  -exe=<exeFileName>     Override default VM executable name");
	System.out.println("  -version=<javaVersion> Override default java option");
	System.out.println("  -bp=<bootClassPath>    Override default VM bp with -bp:<bootClassPath>");
	System.out.println("  -cp=<userClassPath>    Override default VM bp with -cp:<userClassPath>");
	System.out.println("  -xlist=<excludeList>   Specify an XML exclude list");
	System.out.println("  -xids=<excludeIDS>     Specify tags to choose excluded tests");
	System.out.println("  -test=<test package>   Run all tests in the given package");
	System.out.println("  <n>                    Run menu item <n>");
	System.out.println("  A   or   a             Run all tests");
	System.out.println("If no tests are specified (i.e. no <n> or A options) an interactive");
	System.out.println("menu will be displayed.");
	System.exit(1);
}

public static void main(String[] args) {
	Vector commands = new Vector();
	String exeName = null;
	String javaVersion = null;
	String jarName = null;
	String bootClassPath = null;
	String userClassPath = null;
	String excludeFN = null;
	String excludeIDs = null;
	String packageToTest = null;
	
	/* Parse all arguments beginning with '-' */
	if (args != null)  {
		for (int i = 0; i<args.length; i++)  {
			if (!args[i].startsWith("-"))  {
				commands.addElement(args[i]);
				continue;
			}
			/* TODO: find a cleaner way to do this */
			if (args[i].startsWith("-exe="))  {
				if (exeName != null)  usage();
				exeName = args[i].substring(5);
			} else if (args[i].startsWith("-version="))  {
				if (javaVersion != null)  usage();
				javaVersion = args[i].substring(9);
			} else if (args[i].startsWith("-jar="))  {
				if (jarName != null)  usage();
				jarName = args[i].substring(5);
			} else if (args[i].startsWith("-bp="))  {
				if (bootClassPath != null)  usage();
				bootClassPath = args[i].substring(4);
			} else if (args[i].startsWith("-cp="))  {
				if (userClassPath != null)  usage();
				userClassPath = args[i].substring(4);
			} else if (args[i].startsWith("-xlist="))  {
				if (excludeFN != null)  usage();
				excludeFN = args[i].substring(7);
			} else if (args[i].startsWith("-xids="))  {
				if (excludeIDs != null)  usage();
				excludeIDs = args[i].substring(6);
			} else if (args[i].startsWith("-test=")) {
				packageToTest = args[i].substring(6);
			} else  {
				System.out.println("Unrecognized option: \"" + args[i] + "\"");
				usage();
			}
		}
	}
	
	ExcludeList excludeList = null;
	if ( excludeFN!=null && excludeIDs!=null) {
		excludeList= ExcludeList.readFrom(excludeFN, excludeIDs);
		if (excludeList == null) 
			System.exit(-1);
		excludeList.explain(System.out);
	}

	if (exeName == null)  {
		exeName = System.getProperty("user.dir");
		if (!exeName.endsWith(System.getProperty("file.separator")))  {
			exeName = exeName + System.getProperty("file.separator");
		}
		exeName = exeName + "j9";  /* TODO: can we reach for the .exe name? */
	}
	
	if (javaVersion == null)  {
		/* set default java version to 9 */
		javaVersion = "9";
	}
	
	if (jarName == null)  {
		/* Need to revisit this */
		jarName = System.getProperty("java.class.path");
	}
	if ((bootClassPath == null) && (Integer.parseInt(javaVersion) < 9)) {
		bootClassPath = System.getProperty("sun.boot.class.path");
	}
	if (userClassPath == null)  {
		userClassPath = System.getProperty("java.class.path");
	}
			
	if ((args == null) || (args.length < 1))  {
	}
	HashMap testPackageMap = new HashMap();
	Enumeration enumeration = null;
	try  {
		enumeration = new AllTestsInJar(jarName);
	} catch (IOException e)  {
		System.out.println("test suite: error reading tests from Jar file \"" + jarName + "\"!");
		System.exit(1);
	}
	System.out.println("test suite: read tests from \"" + jarName + "\"");
	while (enumeration.hasMoreElements())  {
		String testClassName = (String)enumeration.nextElement();
		/* Put tests in bins according to their package. */
		String testPackage = testClassName.substring(0, testClassName.lastIndexOf('.'));
		Vector v = (Vector)testPackageMap.get(testPackage);
		if (v == null)  {
			/* First test class from this package. */
			v = new Vector();
			testPackageMap.put(testPackage, v);
		}
		v.addElement(testClassName);
	}

	Iterator iter = testPackageMap.keySet().iterator();
	Menu menu = new Menu(jarName, exeName, bootClassPath, userClassPath, javaVersion);
	while (iter.hasNext())  {
		String testPackage = (String)iter.next();
		Vector v = (Vector)testPackageMap.get(testPackage);
		if (v.size() == 1)  {
			/* Package contains only one test. */	
			String testName = (String)v.get(0);
			
			if (excludeList==null || excludeList.shouldRun(testName)) {
				menu.addElement(new MenuItem(testName, v));
				menu.allClassNames.addElement(testName);
			}
		} else  {
			/* Package contains multiple tests. */
			String testName = testPackage + ".AllTests";		
			if (excludeList==null || excludeList.shouldRun(testName)) {
				menu.addElement(new MenuItem(testName, v));

				for (int i = 0; i<v.size(); i++)  {
					testName = (String)v.get(i);
					if (excludeList==null || excludeList.shouldRun(testName)) {
						testName = " " + testName;
						menu.addElement(new MenuItem(testName, (String)v.get(i)));
						menu.allClassNames.addElement((String)v.get(i));
					}
				}
			}
		}
	}
	
	/* Get the index of 'packageToTest' in menu.menuItems vector
	 * and add it to commands vector.
	 */
	if (packageToTest != null) {
		int index = 0;
		for (index = 0; index < menu.menuItems.size(); index++)
		{
			MenuItem item = (MenuItem)menu.menuItems.get(index);
			String packageName = item.getDisplayString();
			packageName = packageName.substring(0, packageName.lastIndexOf('.'));
			if (packageName.equals(packageToTest)) {
				commands.addElement(String.valueOf(index+1));
				break;
			}
		}
	}
		
	if (commands.size() > 0)  {
		/* Non-interactive mode. */
		for (int i = 0; i<commands.size(); i++)  {
			menu.doCommand((String)commands.get(i));
		}
	} else  {
		/* Interactive mode */
		menu.doMenu();
	}
}

public Runner newRunnerForTest(String className, String jarFileName)  {
	try  {
		Class runnerClass = Class.forName(className + "Runner");
		Class[] paramTypes = new Class[5];
		Object[] paramValues = new Object[5];
		paramTypes[0] = className.getClass(); paramValues[0] = className;
		paramTypes[1] = exeName.getClass(); paramValues[1] = exeName;
		if (bootClassPath == null) {
			paramTypes[2] = String.class;
		} else {
			paramTypes[2] = bootClassPath.getClass();
		}
		paramValues[2] = bootClassPath;
		paramTypes[3] = userClassPath.getClass(); paramValues[3] = userClassPath;
		paramTypes[4] = javaVersion.getClass(); paramValues[4] = javaVersion;
		return (Runner)runnerClass.getConstructor(paramTypes).newInstance(paramValues);
	} catch (Exception e)  {
		return new Runner(className, exeName, bootClassPath, userClassPath, javaVersion);
	}
}

public void runTestSet(Vector classNames) {
	int numPassed = 0;
	int numFailed = 0;
	boolean passed;
	for (int i = 0; i<classNames.size(); i++)  {
		String className = (String)classNames.get(i);
		System.out.println("+++ " + className + ": +++");
		Runner runner = newRunnerForTest(className, jarFileName);
		passed = false;
		try  {
			if (runner.run())  {
				passed = true;
			}
		} catch (Throwable e)  {
			System.out.println("runTestSet() caught exception:");
			e.printStackTrace();
		}
		if (passed)  {
			numPassed++;
			System.out.println("--- Test PASSED ---");
		} else  {
			numFailed++;
			System.out.println("*** Test FAILED *** (" + className + ")");
		}
		System.out.println("");
	}
	System.out.println(numPassed + " passed / " + numFailed + " failed");
	if (numFailed == 0) {
		System.out.println("ALL J9VM TESTS PASSED");
	}else{
		System.exit(-2);
	}
	System.out.println();
}


}
