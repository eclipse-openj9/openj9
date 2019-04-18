<!--
Copyright (c) 2016, 2019 IBM Corp. and others

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

Let's create the following scenario:
For the purpose of this example, we assume you have an OpenJ9 SDK ready
for testing. Below are the specific commands you'd run to compile and
run tests. Details are explained in *Tasks in OpenJ9 Test* section below.

```
cd openj9/test/TestConfig
export TEST_JDK_HOME=<path to JDK directory that you wish to test>
export SPEC=linux_x86-64_cmprssptrs
make -f run_configure.mk
make _sanity
```

## Prerequisites:
Please read [Prerequisites.md](./Prerequisites.md) for details on what 
tools should be installed on your test machine to run tests.

# Tasks in OpenJ9 Test

### 1. Configure environment:

  * environment variables

    required:
    ```
    TEST_JDK_HOME=<path to JDK directory that you wish to test>
    BUILD_LIST=<comma separated projects to be compiled and executed> (default to all projects)
    ```
    optional:
    ```
    SPEC=[linux_x86-64|linux_x86-64_cmprssptrs|...] (platform on which to test, could be auto detected)
    JDK_VERSION=[8|9|10|11|12|Panama|Valhalla] (8 default value, could be auto detected)
    JDK_IMPL=[openj9|ibm|hotspot|sap] (openj9 default value, could be auto detected)
    NATIVE_TEST_LIBS=<path to native test libraries> (default to native-test-libs folder at same level as TEST_JDK_HOME)
    ```
  * auto detection:
  
    By default, AUTO_DETECT is turned on. SPEC, JDK_VERSION, and JDK_IMPL do not need to be exported.
    If you do not wish to use AUTO_DETECT and export SPEC, JDK_VERSION, and JDK_IMPL manually, you can export AUTO_DETECT to false.
    ```
    export AUTO_DETECT=false
    ```
    e.g.,
    ```
    export AUTO_DETECT=false
    export SPEC=linux_x86-64_cmprssptrs
    export JDK_VERSION=8
    export JDK_IMPL=openj9
    ```
Please refer *.spec files in [buildspecs](https://github.com/eclipse/openj9/tree/master/buildspecs)
for possible SPEC values.

  * dependent libs for tests

Please read [DependentLibs.md](./DependentLibs.md) for details.

## 2. Compile tests:

  * compile and run all tests
    ```
    make test
    ```

  * only compile but do not run tests
    ```
    make compile
    ```

### 3. Add more tests:

  * for Java8/Java9 functionality

    - If you have added new features to OpenJ9, you will likely
    need to add new tests. Check out openj9/test/functional/TestExample for
    the format to use.

    - If you have many new test cases to add and special build 
    requirements, then you may want to copy the TestExample project, 
    change the name,replace the test examples with your own code, 
    update the build.xml and playlist.xml files to match your new Test 
    class names. The playlist.xml format is defined in 
    TestConfig/playlist.xsd.

    - A test can be tagged with following elements:
      - level:   [sanity|extended] (extended default value)
      - group:   [functional|system|openjdk|external|perf|jck] (required 
                 to provide one group per test)
      - type:    [regular|native] (if a test is tagged with native, it means this
                 test needs to run with test image (native test libs); 
                 NATIVE_TEST_LIBS needs to be set for local testing; if Grinder is used, 
                 native test libs download link needs to be provided in addition to SDK 
                 download link in CUSTOMIZED_SDK_URL; for details, please refer to 
                 [How-to-Run-a-Grinder-Build-on-Jenkins](https://github.com/AdoptOpenJDK/openjdk-tests/wiki/How-to-Run-a-Grinder-Build-on-Jenkins); 
                 default to regular)
      - impl:    [openj9|hotspot] (filter test based on exported JDK_IMPL 
                 value; a test can be tagged with multiple impls at the 
                 same time; default to all impls)
      - subset:  [8|8+|9|9+|10|10+|11|11+|Panama|Valhalla] (filter test based on 
                 exported JDK_VERSION value; a test can be tagged with 
                 multiple subsets at the same time; if a test tagged with 
                 a number (e.g., 8), it will used to match JDK_VERSION; if
                 a test tagged with a number followed by + sign, any JDK_VERSION
                 after the number will be a match; default to always match)

    - Most OpenJ9 FV tests are written with TestNG. We leverage
    TestNG groups to create test make targets. This means that
    minimally your test source code should belong to either
    level.sanity or level.extended group to be included in main
    OpenJ9 builds.

### 4. Run tests:

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

  * type of tests <br />
    make _type <br />
    e.g., 
    ```
    make _native
    ```

  * level of tests with specified group <br />
    make _level.group <br />
    e.g., 
    ```
    make _sanity.functional
    ```

  * level of tests with specified type <br />
    make _level.type <br />
    e.g., 
    ```
    make _sanity.native
    ```

  * group of tests with specified type <br />
    make _group.type <br />
    e.g., 
    ```
    make _functional.native
    ```

  * specify level, group and type together <br />
    make _level.group.type <br />
    e.g., 
    ```
    make _sanity.functional.native
    ```

  * a specific individual test <br />
    make _testTargetName_xxx <br />
    e.g., 
    ```
    make _generalExtendedTest_0
    ```
The suffix number refers to the variation in the playlist.xml file

  * all variations in the test target <br />
    make _testTargetName <br />
    e.g., 
    ```
    make _cmdLineTester_classesdbgddrext
    ```
Above command will run all possible variations in _generalExtendedTest 
target

  * a directory of tests <br />
    cd path/to/directory; make -f autoGen.mk testTarget <br />
    or make -C path/to/directory -f autoGen.mk testTarget <br />
    e.g., 
    ```
    cd test/functional/TestExample
    make -f autoGen.mk _sanity
    ```

  * all tests
    - compile & run tests
        ```
        make test
        ```
    - run all tests without recompiling them
        ```
        make runtest
        ```

  * against specific (e.g., hotspot 8) SDK

    impl and subset are used to annotate tests in playlist.xml, 
    so that the tests will be run against the targeted JDK_IMPL 
    and JDK_VERSION.

  * rerun the failed tests from the last run
    ```
    make _failed
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

### 5. Exclude tests:

  * exclude test target in playlist.xml

    Add 
    ```
    <disabled>Reason for disabling test, should include issue number<disabled>
    (for example: <disabled>issue #1 test failed due to OOM<disabled>)
    ```
    inside the
    ```
    <test>
    ```
    element that you want to exclude.

    If a test is disabled using `<disabled>` tag in playlist.xml, it can be executed through specifying the test target or adding `disabled` in front of regular target.

    ```    
        make _testA    // testA has <disabled> tag in playlist.xml  
        make _disabled.sanity.functional
        make _disabled.extended
    ```

    Disabled tests and reasons can also be printed through adding `echo.disabled` in front of regular target.

    ```    
        make _echo.disabled.testA
        make _echo.disabled.sanity.functional
        make _echo.disabled.extended
    ```

  * temporarily on all platforms

    Depends on the JDK_VERSION, add a line in the
    test/TestConfig/resources/excludes/latest_exclude_$(JDK_VERSION).txt
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
    latest_exclude_$(JDK_VERSION).txt file, but with specific specs to
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

### 6. View results:

  * in the console

    OpenJ9 tests written in testNG format take advantage of the testNG 
    logger.

    If you want your test to print output, you are required to use the
    testng logger (and not System.out.print statements). In this way,
    we can not only direct that output to console, but also to various
    other clients (WIP).  At the end of a test run, the results are
    summarized to show which tests are passed / failed / disabled / skipped.  This gives
    you a quick view of the test names and numbers in each category
    (passed/failed/disabled/skipped).  If you've piped the output to a file, or
    if you like scrolling up, you can search for and find the specific
    output of the tests that failed (exceptions or any other logging
    that the test produces).

    - `SKIPPED` tests

      If a test is skipped, it means that this test cannot be run on this
      platform due to jvm options, platform requirements and/or test
      capabilities.

    - `DISABLED` tests

      If a test is disabled, it means that this test is disabled using `<disabled>` tag in playlist.xml.

  * in html files

    TestNG tests produce html (and xml) output from the tests are 
    created and stored in a test_output_xxxtimestamp folder in the 
    TestConfig directory (or from where you ran "make test"). 

    The output is organized by tests, each test having its own set of 
    output.  If you open the index.html file in a web browser, you will
    be able to see which tests passed, failed or were skipped, along 
    with other information like execution time and error messages, 
    exceptions and logs from the individual test methods.

  * TAP result files

    As some of the tests are not testNG format, a simple standardized 
    format for test output was needed so that all tests are reported in
    the same way for a complete test summary. Depending on the 
    requirement there are three different diagnostic levels.

		* export DIAGNOSTICLEVEL=failure
			Default level. Log all detailed failure information if test fails
		* export DIAGNOSTICLEVEL=all
			Log all detailed information no matter test failures or succeed
		* export DIAGNOSTICLEVEL=nodetails
			No need to log any detailed information. Top level TAP test result
			summary is enough 

### 7. Attach a debugger:

  * to a particular test

    The command line that is run for each particular test is echo-ed to
    the console, so you can easily copy the command that is run.
    You can then run the command directly (which is a direct call to the
    java executable, adding any additional options, including those to
    attach a debugger.

### 8. Move test into different make targets (layers):

  * from extended to sanity (or vice versa)

    - For testng tests, change the group annotated at the top of the 
    test class from `level.extended` to `level.sanity`

    - Change `<level>` from `extended` to `sanity` in playlist.xml
