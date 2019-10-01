This project contains an infrastructure and tests for exercising the shared classes command line options.

The infrastructure is all in 'tests.sharedclasses' and the most important class there is 
TestUtils.  It provides a lot of infrastructure that subclasses can use.  The subclasses
are in tests.sharedclasses.options and each of those is a single test, with a main method.
They can all be invoked standalone.  However, they are also pulled together into a Junit
testcase in 'tests.shared.options.junit'.  The TestOptionsBase class pulls together all
the tests but is abstract - the further subtypes of TestOptionsBase are the real testcases
but all they do is provide alternative setUp/tearDown methods that will run executed around
each of the testcases in TestOptionsBase.  The testsuite AllTests pulls together the 
variations of running the testcases.  For example, TestOptionsDefault runs all the tests
letting the cache directory default whilst TestOptionsCacheDir runs all the tests whilst
setting the cacheDir explicitly to a destination.

When writing a testcase main method, the runXXX methods inherited from the superclass TestUtils
will invoke a command, the output of the command is collected and can be examined directly or
checked via helper checkOutputXXX methods.

TestUtils currently distinguishes Windows from 'every other platform' - other distinctions may
be required if running on further platforms.  The distinction is made due to differences in 
how the control files manifest on windows and unix platforms.

Some of the shared memory tests are also likely to be linux only.

The testcode directory contains some simple classes that can be invoked by the commands that are
spawned - if the testcode is changed, the testcode.jar file at the top level should be updated by
running this command whilst in the testcode subdirectory:
	jar -cvMf ../src/testcode.jar *
The testcode jar is kept in the src tree so it can be treated as a java resource and will be
found by a classloader lookup - meaning the user wont have to supply the path to it.
	
===
The tests can be launched as JUnit tests and the five possible combinations are included as launch profiles here.
You must edit the config.properties to choose the JVM that needs testing before you launch them.

There is a lightweight cmdlinetester script that invokes JUnit and that is what is used on nescafe.