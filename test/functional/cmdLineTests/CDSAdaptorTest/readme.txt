This test suite is the place holder for adding CDS Adaptor tests.
Command line tests present in cdsadaptortest.xml use combination of java classes and OSGi bundles to achieve their purpose.
Main purpose of java class is to start OSGi framework and install required bundles. 
But they may not be limited to this task only.

Few guidelines/assumptions when writing/updating tests placed in this project:

1) Java class are placed in com.ibm.cdsadaptortest package.

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
	|--org.eclipse.osgi_<version>.jar: this is Equinox jar file, an OSGi framework implementation.
	|
	|--Test1Bundles: this directory contains bundles required by java class Test1.
	|
	|--Test2Bundles: this directory contains bundles required by java class Test2.
	
5) If a new version of Equinox jar or CDS Adaptor is checked in, their version number in cdsadaptortest.xml needs to be updated as well.
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
