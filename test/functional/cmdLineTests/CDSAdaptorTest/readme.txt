*******************************************************************************
*  Copyright (c) 2001, 2020 IBM Corp. and others
*
*  This program and the accompanying materials are made available under
*  the terms of the Eclipse Public License 2.0 which accompanies this
*  distribution and is available at https://www.eclipse.org/legal/epl-2.0/
*  or the Apache License, Version 2.0 which accompanies this distribution and
*  is available at https://www.apache.org/licenses/LICENSE-2.0.
*
*  This Source Code may also be made available under the following
*  Secondary Licenses when the conditions for such availability set
*  forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
*  General Public License, version 2 with the GNU Classpath
*  Exception [1] and GNU General Public License, version 2 with the
*  OpenJDK Assembly Exception [2].
*
*  [1] https://www.gnu.org/software/classpath/license.html
*  [2] http://openjdk.java.net/legal/assembly-exception.html
*
*  SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
*******************************************************************************

This test suite is the place holder for adding CDS Adaptor tests.
Command line tests present in cdsadaptortest.xml use combination of java classes and OSGi bundles to achieve their purpose.
Main purpose of java class is to start OSGi framework and install required bundles. 
But they may not be limited to this task only.

Few guidelines/assumptions when writing/updating tests placed in this project:

1) Java class are placed in org.openj9.test.cdsadaptortest package.

2) OSGi bundles required by the java class are created outside this project and
   only the bundle jar (including the source code) is committed to this project.
   
3) As a convention, OSGi bundles required by a java class are to be placed in a directory corresponding to the class name.
   For example if java class name is CDSTest.java, then its bundles should be put in resources/CDSTestBundles directory.
   You may also add a readme.txt describing the working of your test in the same directory.

4) We are using Equinox as the OSGi framework. It is present as org.eclipse.osgi_<version>.jar under "resources" directory.
   CDS Adaptor bundle is present in "resources/FrameworkBundles" directory.

   Structure of "resources" directory:
   
	resources
	|
	|--FrameworkBundles: this directory contains bundles required by the framework. eg CDS Adaptor bundle.
	|
	|--org.eclipse.osgi_<version>.jar: this is Equinox jar file, an OSGi framework implementation, and it contains cds adaptor.
	|
	|--TestBundle: this directory contains bundle source code required by java class TestBundle.
	|
	|--WeavinghookTest: this directory contains bundle source code required by java class WeavinghookTest.
	
5) If a new version of Equinox jar is checked in, their version number in cdsadaptortest.xml needs to be updated as well.
   Failing to do this will cause tests to fail.
   Equinox builds are available at: http://download.eclipse.org/equinox/
   
6) If existing OSGi bundles are to be modified, the corresponding bundle jar file needs to be checked out and 
   imported in the eclipse workspace as follows:
   - Go to File->Import->Plug-in Development->Plug-ins and Fragments. This opens up "Import Plug-ins and Fragments" form.
   - Under "Import From" browse the "Directory" where bundle jar is present.
   - Under "Plug-ins and Fragments to Import" select "Select from all plug-in and fragments found at the specified location".  
   - Under "Import As", select "Projects with source folders".
   - Click Next.
   - Under "Plug-ins and Fragments found", select the bundles you wish to import.
   - Click Finish.
   - A new project should appear in your workspace for each bundle imported.
	
   Now, the OSGi bundle can be modified as required.
 
   To create jar file for your bundle, follow these steps:
   - Right click the bundle project and select Export->Plug-in Development->Deployable plug-ins and Fragments.
   - In the Options tab, check "Export Source" and select "Include source in exported plug-ins" from the drop-down list.
   - You may set the "Qualifier replacement" as you wish. If not given, today's date will be used.
