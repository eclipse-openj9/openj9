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
export BUILD_LIST=functional
make -f run_configure.mk
make compile
make _sanity.regular
```

## Prerequisites
Please read [Prerequisites.md](./Prerequisites.md) for details on what 
tools should be installed on your test machine to run tests.

# Tasks in OpenJ9 Test

### 1. Configure environment

#### Environment variables
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
    
#### Auto detection
  
By default, AUTO_DETECT is turned on. SPEC, JDK_VERSION, and JDK_IMPL do not need to be exported.
If you do not wish to use AUTO_DETECT and export SPEC, JDK_VERSION, and JDK_IMPL manually, you can export AUTO_DETECT to false.
e.g.,
    
```
    export AUTO_DETECT=false
    export SPEC=linux_x86-64_cmprssptrs
    export JDK_VERSION=8
    export JDK_IMPL=openj9
```

Please refer *.spec files in [buildspecs](https://github.com/eclipse/openj9/tree/master/buildspecs)
for possible SPEC values.

#### Dependent libs for tests

Please read [DependentLibs.md](./DependentLibs.md) for details.

### 2. Compile tests

#### Compile and run all tests (found in directories set by BUILD_LIST environment variable)
```
    make test
```

#### Only compile but do not run tests (found in directories set by BUILD_LIST variable)
```
    make compile
```

### 3. Add more tests

#### For new functionality

- If you have added new features to OpenJ9, you will likely need to add new tests. Check out [openj9/test/functional/TestExample/src/org/openj9/test/MyTest.java](https://github.com/eclipse/openj9/blob/master/test/functional/TestExample/src/org/openj9/test/example/MyTest.java) for
the format to use.

- If you have many new test cases to add and special build requirements, then you may want to copy the [TestExample](https://github.com/eclipse/openj9/blob/master/test/functional/TestExample) update the build.xml and playlist.xml files to match your new Test class names. The playlist.xml format is defined in TestConfig/playlist.xsd.

- A test can be tagged with following elements:
      - level:   [sanity|extended|special] (extended default value)
      - group:   [functional|system|openjdk|external|perf|jck] (required 
                 to provide one group per test)
      - type:    [regular|native] (if a test is tagged with native, it means this
                 test needs to run with test image (native test libs); 
                 NATIVE_TEST_LIBS needs to be set for local testing; if Grinder is used, 
                 native test libs download link needs to be provided in addition to SDK 
                 download link in CUSTOMIZED_SDK_URL; for details, please refer to 
                 [How-to-Run-a-Grinder-Build-on-Jenkins](https://github.com/AdoptOpenJDK/openjdk-tests/wiki/How-to-Run-a-Grinder-Build-on-Jenkins); 
                 default to regular)
      - impl:    [openj9|hotspot|ibm] (filter test based on exported JDK_IMPL 
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

### 4. Run tests

#### Run a group of tests (where group can be functional|system|openjdk|external|perf) <br />
make _group <br />
e.g., 
```
    make _functional
```

#### Run a level of tests (where level can be sanity|extended|special) <br />
make _level <br />
e.g., 
```
    make _sanity
```

#### Run a type of test (where type can be regular|native) <br />
make _type <br />
e.g., 
```
    make _native
```

#### Run a level of tests with specified group <br />
make _level.group <br />
e.g., 
```
    make _sanity.functional
```

#### Run a level of tests with specified type <br />
make _level.type <br />
e.g., 
```
    make _sanity.native
```

#### Run a group of tests with specified type <br />
make _group.type <br />
e.g., 
```
    make _functional.native
```

#### Run a specified level, group and type together <br />
make _level.group.type <br />
note that with each '.' in the make target, the breadth of tests narrows (_sanity > _sanity.functional > _sanity.functional.native)
e.g., 
```
    make _sanity.functional.native
```

#### Run a specific individual test target (where test targets are defined in playlist files, see [testExample](https://github.com/eclipse/openj9/blob/master/test/functional/TestExample/playlist.xml#L27)<br />
make _testTargetName_xxx <br />
e.g., the [1st variation in playlist](https://github.com/eclipse/openj9/blob/master/test/functional/TestExample/playlist.xml#L29) is suffixed by _0, 2nd variation by _1, and so forth
```
    make _testExample_0
```
The suffix number refers to the variation in the playlist.xml file

#### Run all variations in the test target <br />
make _testTargetName <br />
e.g., 
```
    make _testExample
```
Above command will run [all possible variations in _testExample](https://github.com/eclipse/openj9/blob/master/test/functional/TestExample/playlist.xml#L28-L30)
target

#### Run a directory of tests <br />
cd path/to/directory; make -f autoGen.mk testTarget <br /> 
or make -C path/to/directory -f autoGen.mk testTarget <br />
e.g., 
```
    cd test/functional/TestExample
    make -f autoGen.mk _sanity
```

#### Run all tests
- compile & run tests
```
        make test
```
- run all tests without recompiling them
```
        make runtest
```

#### Run tests against specific (e.g., hotspot 8) SDK

`<impl>` and `<subset>` elements are used to annotate tests in playlist.xml, so that the tests will be run against the targeted JDK_IMPL and JDK_VERSION (and is determined by the SDK defined in TEST_JDK_HOME variable)

#### Rerun the failed tests from the last run
```
    make _failed
```

#### With a different set of JVM options
There are 3 ways to add options to your test run:

- If you simply want to add an option for a one-time run, you can either override the original options by using JVM_OPTIONS="your options".

- If you want to append options to the set that are already there, use EXTRA_OPTIONS="your extra options". Below example will append to those options already in the make target.
```
    make _jsr292_InDynTest_SE90_0 EXTRA_OPTIONS=-Xint
```

- If you want to change test options, you can update playlist.xml in the corresponding test project.

### 5. Exclude tests

#### Exclude test target in playlist.xml
Add `<disabled>` element, with a link to the issue that describes the reason for disabling test
for example: 
```
    <disabled>[https://github.com/eclipse/openj9/issues/6777](https://github.com/eclipse/openj9/issues/6777)<disabled>
```
inside the `<test>` element that you want to exclude.

If a test is disabled using `<disabled>` tag in playlist.xml, it can be executed by specifying the test target or adding `disabled` in front of its top-level test target.

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

#### Exclude temporarily on all platforms
Depends on the JDK_VERSION, add a line in the test/TestConfig/resources/excludes/latest_exclude_$(JDK_VERSION).txt file. It is the same format that the OpenJDK tests use, name of test, defect number, platforms to exclude.

To exclude on all platforms, use generic-all.  For example:
```
    org.openj9.test.java.lang.management.TestOperatingSystemMXBean:testGetProcessCPULoad 121187 generic-all
```

Note that we additionally added support to exclude individual methods of a
test class, by using :methodName behind the class name (OpenJDK does not
support this currently). In the example, only the testGetProcessCPULoad
method from that class will be excluded (on all platforms/specs).

#### Exclude temporarily on specific platforms or architectures
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

#### Exclude permanently on all or specific platforms/archs

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

### 6. View results

#### Results in the console
OpenJ9 tests written in testNG format take advantage of the testNG logger.

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

#### Results in html files

TestNG tests produce html (and xml) output from the tests are 
created and stored in a test_output_xxxtimestamp folder in the 
TestConfig directory (or from where you ran "make test"). 

The output is organized by tests, each test having its own set of 
output.  If you open the index.html file in a web browser, you will
be able to see which tests passed, failed or were skipped, along 
with other information like execution time and error messages, 
exceptions and logs from the individual test methods.

#### TAP result files

As some of the tests are not testNG or junit format, a simple standardized 
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

### 7. Attach a debugger

#### To a particular test

The command line that is run for each particular test is echo-ed to
the console, so you can easily copy the command that is run.
You can then run the command directly (which is a direct call to the
java executable, adding any additional options, including those to
attach a debugger.

### 8. Move test into different make targets (layers)

#### From extended to sanity (or vice versa)
- For testng tests, change the group annotated at the top of the 
test class from `level.extended` to `level.sanity`

- Change `<level>` from `extended` to `sanity` in playlist.xml

# How-to Reproduce Test Failures
A common scenario is that automated testing finds a failure and a developer is asked to reproduce it.  An openj9 issue is created reporting a failing test.  The issue should contain:
* link(s) to Jenkins job (which contains all of the info you need, if you get to it before the Jenkins job is deleted, which is quickly to save space on Jenkins master)
* test target name (TARGET)
* test group / test directory name (one of the following, functional, system, openjdk, external, perf) (BUILD_LIST)
* test level (one of the following, sanity, extended, special)
* platform(s) the test fails on (Jenkinsfile)
* version the test fails in (JDK_VERSION)
* implementation the test fails against (JDK_IMPL)
* SDK build that was used by the test (in the console output of the Jenkins job, there is java -version info and a link to the SDK used)

A specific example, [Issue 6555](https://github.com/eclipse/openj9/issues/6555) Test_openjdk13_j9_sanity.system_ppc64le_linux TestIBMJlmRemoteMemoryAuth_0 crash 
we get the following info (captured in the name of the issue):
* TARGET = TestIBMJlmRemoteMemoryAuth_0
* BUILD_LIST = system
* JDK_VERSION = 13
* JDK_IMPL = openj9 (its implied if the failure was found in openj9 testing at ci.eclipse.org/openj9)
* Jenkinsfile = openjdk_ppc64le_linux (corresponds to platform to run on)

Since only a link to the Jenkins job was provided in the example issue 6555, we do not have java -version info, we will have to go to the job link to find out the exact SDK build, though it may be sufficient just to rerun the test with the latest nightly build to reproduce.  Given those pieces of information, we have enough to try and rerun this test, either in a Grinder job in Jenkins, or locally on a machine (on the same platform as the test failure).

For more details on launching a Grinder job, you can see these instructions on [how to run a grinder job](https://github.com/AdoptOpenJDK/openjdk-tests/wiki/How-to-Run-a-Grinder-Build-on-Jenkins).  

To try and reproduce the failure locally, please check out this [wiki for guidance on reproducing failures locally](https://github.com/eclipse/openj9/wiki/Reproducing-Test-Failures-Locally) for details.
