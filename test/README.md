<!--
Copyright (c) 2016, 2018 IBM Corp. and others

This program and the accompanying materials are made available under
the terms of the Eclipse Public License 2.0 which accompanies this
distribution and is available at https://www.eclipse.org/legal/epl-2.0/
or the Apache License, Version 2.0 which accompanies this distribution and
is available at https://www.apache.org/licenses/LICENSE-2.0.

This Source Code may also be made available under the following
Secondary Licenses when the conditions for such availability set
forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
General Public License, version 2 with the GNU Classpath
Exception [1] and GNU General Public License, version 2 with the
OpenJDK Assembly Exception [2].

[1] https://www.gnu.org/software/classpath/license.html
[2] http://openjdk.java.net/legal/assembly-exception.html

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
-->

# How-to Run Tests
Prerequisites:

  * ant 1.7.1 or above with ant-contrib.jar
  * make 3.81 or above (recommend make 4.1 or above on windows)
  * perl 5.10.1 or above with JSON and Text::CSV module installed
  * wget 1.14 or above

Let's create the following scenario:
For the purpose of this example, we assume you have an OpenJ9 SDK ready
for testing. Below are the specific commands you'd run to compile and
run tests. Details are explained in *Tasks in OpenJ9 Test* section below.

```
cd openj9/test/TestConfig
export JAVA_BIN=<path to JDK bin directory that you wish to test>
export SPEC=linux_x86-64_cmprssptrs
make -f run_configure.mk
make test
```

# Tasks in OpenJ9 Test

1. Configure environment:

  * required environment variables
    ```
    JAVA_BIN=<path to JDK bin directory that you wish to test>
    SPEC=[linux_x86-64|linux_x86-64_cmprssptrs|...] platform_on_which_to_test
    JAVA_VERSION=[SE80|SE90|SE100|Panama|Valhalla] (SE90 default value)
    JAVA_IMPL=[openj9|hotspot] (openj9 default value)
    ```

  * list of dependent jars

    * asm-all-6.0_BETA.jar
    * asmtools-6.0.jar
    * commons-cli-1.2.jar
    * commons-exec-1.1.jar
    * javassist-3.20.0-GA.jar
    * jcommander-1.48.jar
    * junit-4.10.jar
    * testng-6.10.jar

    To get the above dependent jars, please run `make getdependency`.
    getdependency target will get called when running make compile
    or test target.

2. Compile tests:

  * compile and run all tests
    ```
    make test
    ```

  * only compile but do not run tests
    ```
    export BUILD_LIST=comma_separated_projects_to_be_compiled (defaults to all projects)
    make compile
    ```

3. Add more tests:

  * for Java8/Java9 functionality

    - If you have added new features to OpenJ9, you will likely
    need to add new tests. Check out openj9/test/TestExample for
    the format to use.

    - If you have many new test cases to add, then you may want to
    copy the TestExample project, change the name,replace the test
    examples with your own code, update the build.xml and playlist.xml
    files to match your new Test class names. The playlist.xml format 
    is defined in TestConfig/playlist.xsd.

    - A test can be tagged with following elements:
      - level:   [sanity|extended] (extended default value)
      - group:   [functional|openjdk|external|perf|jck|system] (required 
                 to provide one group per test)
      - impl:    [openj9|hotspot] (filter test based on exported JAVA_IMPL 
                 value; a test can be tagged with multiple impls at the 
                 same time; default to all impls)
      - subset:  [SE80|SE90|SE100|Panama|Valhalla] (filter test based on 
                 exported JAVA_VERSION value; a test can be tagged with 
                 multiple subsets at the same time; default to all subsets)

    - Most OpenJ9 FV tests are written with TestNG. We leverage
    TestNG groups to create test make targets. This means that
    minimally your test source code should belong to either
    level.sanity or level.extended group to be included in main
    OpenJ9 builds.

4. Run tests:

  * all tests
    - compile & run tests
        ```
        make test
        ```
    - run all tests without recompiling them
        ```
        make runtest
        ```

  * group of tests <br />
    make _group <br />
    e.g., 
    ```
    make _functional
    ```

  * level of tests <br />
    make _level <br />
    e.g., 
    ```
    make _sanity
    ```

  * level of tests with specified group <br />
    make _level.group <br />
    e.g., 
    ```
    make _sanity.functional
    ```

  * a specific individual test <br />
    make _testTargetName <br />
    e.g., 
    ```
    make _generalExtendedTest_0
    ```

  * a directory of tests <br />
    cd path/to/directory; make -f autoGen.mk testTarget <br />
    or make -C path/to/directory -f autoGen.mk testTarget <br />
    e.g., 
    ```
    cd test/TestExample
    make -f autoGen.mk _sanity
    ```

  * against specific (e.g., hotspot SE80) SDK

    impl and subset are used to annotate tests in playlist.xml, 
    so that the tests will be run against the targeted JAVA_IMPL 
    and JAVA_VERSION.

  * rerun the failed tests from the last run
    ```
    make failed
    ```

  * with a different set of JVM options

    There are 3 ways to add options to your test run:

    - If you simply want to add an option for a one-time run, you can
    either override the original options by using JVM_OPTIONS="your options".

    - If you want to append options to the set that are already there,
    use EXTRA_OPTIONS="your extra options". Below example will append
    to those options already in the make target.
    ```
    make _jsr292_InDynTest_SE90_0 EXTRA_OPTIONS=-Xint
    ```

    - If you want to change test options, you can update playlist.xml
    in the corresponding test project.

5. Exclude tests:

  * exclude test target in playlist.xml

    Add 
    ```
    <disabled>Reason for disabling test, preferably includes issue number<disabled>
    (for example: <disabled>issue #1 test failed due to OOM<disabled>)
    ```
    inside the
    ```
    <test>
    ```
    element that you want to exclude.

  * temporarily on all platforms

    Depends on the JAVA_VERSION, add a line in the
    test/TestConfig/resources/excludes/latest_exclude_$(JAVA_VERSION).txt
    file. It is the same format that the OpenJDK tests use, name of test,
    defect number, platforms to exclude.

    To exclude on all platforms, use generic-all.  For example:
    ```
    org.openj9.test.java.lang.management.TestOperatingSystemMXBean:testGetProcessCPULoad 121187 generic-all
    ```

Note that we additionally added support to exclude individual methods of a
test class, by using :methodName behind the class name (OpenJDK does not
support this currently). In the example, only the testGetProcessCPULoad
method from that class will be excluded (on all platforms/specs).

  * temporarily on specific platforms or architectures

    Same as excluding on all platforms, you add a line to
    latest_exclude_$(JAVA_VERSION).txt file, but with specific specs to
    exclude, for example:
    ```
    org.openj9.test.java.lang.management.TestOperatingSystemMXBean 121187 linux_x86-64
    ```

This example would exclude all test methods of the TestOperatingSystemMXBean
from running on the linux_x86-64 platform.
Note: in OpenJ9 the defect numbers would associate with git issue numbers
(OpenJDK defect numbers associate with their bug tracking system).

  * permanently on all or specific platforms/archs

    For tests that should NEVER run on particular platforms or
    architectures, we should not use the default_exclude.txt file. To
    disable those tests, we annotate the test class to be disabled. To
    exclude MyTest from running on the aix platform, for example:
    ```
    @Test(groups={ "level.sanity", "component.jit", "disabled.os.aix" })
        public class MyTest {
        ...
    ```

We currently support the following exclusion groups:
```
disabled.os.<os> (e.g. disabled.os.aix)
disabled.arch.<arch> (e.g. disabled.arch.ppc)
disabled.bits.<bits> (e.g. disabled.bits.64)
disabled.spec.<spec> (e.g. disabled.spec.linux_x86-64)
```

6. View results:

  * in the console

    OpenJ9 tests take advantage of the testNG logger.
    If you want your test to print output, you are required to use the
    testng logger (and not System.out.print statements). In this way,
    we can not only direct that output to console, but also to various
    other clients (WIP).  At the end of a test run, the results are
    summarized to show which tests passed / failed / skipped.  This gives
    you a quick view of the test names and numbers in each category
    (passed/failed/skipped).  If you've piped the output to a file, or
    if you like scrolling up, you can search for and find the specific
    output of the tests that failed (exceptions or any other logging
    that the test produces).

  * in html files

    Html (and xml) output from the tests are created and stored in a
    test_output_xxxtimestamp folder in the TestConfig directory (or from
    where you ran "make test").  The output is organized by tests, each
    test having its own set of output.  If you open the index.html file
    in a web browser, you will be able to see which tests passed, failed
    or were skipped, along with other information like execution time and
    error messages, exceptions and logs from the individual test methods.

  * TAP result files

    TAP files are created. Depending on the requirement there are three
    different diagnostic levels.
    
		* export DIAGNOSTICLEVEL=failure
			Default level. Log all detailed failure information if test fails
		* export DIAGNOSTICLEVEL=all
			Log all detailed information no matter test failures or succeed
		* export DIAGNOSTICLEVEL=nodetails
			No need to log any detailed information. Top level TAP test result
			summary is enough 
			
  * in the cloud (WIP)
  

7. Attach a debugger:

  * to a particular test

    The command line that is run for each particular test is echo-ed to
    the console, so you can easily copy the command that is run.
    You can then run the command directly (which is a direct call to the
    java executable, adding any additional options, including those to
    attach a debugger.

8. Move test into different make targets (layers):

  * from extended to sanity (or vice versa)

    Change the group annotated at the top of the test class from
    "level.extended" to "level.sanity" and the test will be automatically
    switched from the extended target to the sanity target.
