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

This file explains the working of org.openj9.test.cdsadaptortest.CDSAdaptorOrphanTest.java
This test verifies that a class modified by a weaving hook is stored as ORPHAN in shared class cache.

Usage: 
Only describes the arguments accepted by this program. For complete command, please refer cdsadaptortest.xml.
	CDSAdaptorOrphanTest <arguments> 
		Following arguments are mandatory:
			-frameworkBundleLocation <location>   - location to install bundles required by the framework 
			-testBundleLocation <location>        - location to install bundles required by the test 
		Optional arguments: 
			-ignoreWeavingHookBundle              - if present, do not install bundle that performs weaving
 
Operation:
It requires two bundles for its operation:

1) org.openj9.test.testbundle: This is a simple bundle that consists of two classes:
	org.openj9.test.testbundle.SomeMessageV1: Loaded by the bundle when it starts up.
									  It has a method printMessage() that prints a message M1.
									  This method is called by the bundle when it starts up.
	org.openj9.test.testbundle.SomeMessageV2: Identical to org.openj9.test.testbundle.SomeMessageV2 except that its printMessage() method prints a message M2.
									  Its class bytes are used by weaving hook to replace class bytes of org.openj9.test.testbundle.SomeMessageV1.
	
2) org.openj9.test.weavinghooktest: This is the weaving hook that transforms class bytes of org.openj9.test.testbundle.SomeMessageV1 to org.openj9.test.testbundle.SomeMessageV2.

As mentioned in "Usage" section, org.openj9.test.cdsadaptortest.CDSAdaptorOrphanTest accepts an argument "-ignoreWeavingHookBundle". 
If this argument is specified org.openj9.test.weavinghooktest bundle is not installed. By default, both the bundles mentioned above are installed.

If "-ignoreWeavingHookBundle" is not specified, weaving hook replaces class bytes of org.openj9.test.testbundle.SomeMessageV1 with org.openj9.test.testbundle.SomeMessageV2.
Hence the output message is M2 printed by printMessage() org.openj9.test.testbundle.SomeMessageV2 class.

If "-ignoreWeavingHookBundle" is specified, weaving hook is not installed.
Hence the output message is M1 printed by printMessage() org.openj9.test.testbundle.SomeMessageV1 class.

How to run:
This test is available in the builds at jvmtest/VM/cdsadaptortest/cdsadaptortest.jar.
Unzip the cdsadaptortest.jar and run following command:
	
	java -Xshareclasses -cp .:./org.eclipse.osgi_3.16.100.v20200904-1304.jar org.openj9.test.cdsadaptortest.CDSAdaptorOrphanTest -frameworkBundleLocation ./FrameworkBundles -testBundleLocation ./CDSAdaptorOrphanTestBundles
	
Version of org.eclipse.osgi and org.openj9.test.cds bundles in the above command may not be correct. 
Please use the same version as present in cdsadaptortest.jar.

Tests 1-a to 1-e in cdsadaptortest.xml use org.openj9.test.cdsadaptortest.CDSAdaptorOrphanTest. 
Please check these tests in the job output for more information.

Eclipse version:
Eclipse version used to create the two bundles org.openj9.test.testbundle and org.openj9.test.weavinghooktest is:
Eclipse Juno M5
Version: 4.2.0
Build id: I20120127-1145