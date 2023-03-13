<!--
Copyright IBM Corp. and others 2016

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
[2] https://openjdk.org/legal/assembly-exception.html

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
-->

# Quick Start

Below is an example to run all sanity.functional tests against a JDK8
Linux x86-64 cmprssptrs OpenJ9 SDK:

```
    cd openj9/test
    git clone https://github.com/adoptium/TKG.git
    cd TKG
    export TEST_JDK_HOME=/my/openj9/jdk
    make compile               // downloads test related material/libs
                               // and compiles test material
    make _sanity.functional.regular    // generates makefiles and runs tests
```

Please read [OpenJ9 Test User Guide](./docs/OpenJ9TestUserGuide.md) for
details and other examples.  For example, if you wish to test a JDK11 version,
you would export JDK_VERSION=11 (default JDK_VERSION=8).

## Prerequisites:
Please read [Prerequisites.md](./docs/Prerequisites.md) for details on
what tools should be installed on your test machine to run tests.

# FAQ

While the [OpenJ9 Test User Guide](./docs/OpenJ9TestUserGuide.md) gives
a more complete list of OpenJ9 test use cases, there are some
frequently asked questions or common use cases by OpenJ9 developers
listed below.

## 1) How to compile tests in selected directories?

By default, `make compile` compiles all tests. This is the safest way
to ensure all the test code needed has been compiled. However, there is a
way to shortcut the compilation process to reduce compilation time. If
`BUILD_LIST` is set, `make compile` will only compile the folder names
that match within `BUILD_LIST`.

```
    export BUILD_LIST=functional/TestUtilities,functional/Java8andUp
    make compile
```

## 2) How to add a test?

### Add FV (functional) test
Adding a testNG test as an example:
- adding a single test class to an existing directory
    - update testng.xml to add the test class to a existing <test> or
    create a new <test>
    - If the new <test> is created in testng.xml, playlist.xml should
    be updated to add the new <test> based on [playlist.xsd](./TKG/playlist.xsd)
    Supported test groups are `functional|system|openjdk|external|perf|jck`.
    It is required to provide one group per test in playlist.xml.
- adding additional new test methods for new Java10 functionality
    - test should be automatically picked up

### Add external test
Please refer to the [video and tutorial](https://blog.adoptopenjdk.net/2018/02/adding-third-party-application-tests-adoptopenjdk)
that describes how to add container-based 3rd party application tests
(run inside of Docker images). These tests are added and run in the
automated test builds at the Adoptium project.

## 3) How to disable a test?

- In playlist.xml, to disable a test target, add

 ```
    <disables>
        <disable>
            <comment>issue url or issue comment url</comment>
        <disable>
    </disables>
 ```

inside the `<test>` element that you want to disable.

- auto exclusion
Instead of having to manually create a PR to disable test targets, they can now be automatically disabled via Github workflow (see autoTestPR.yml). In the issue that describes the test failure, add a comment with the following format:

```auto exclude test <testName>```

- more granular exclusion for testNG test

add a line to `TestConfig/resources/excludes/latest_exclude_$(JDK_VERSION).txt`
 file with issue number and specific specs to disable
```
    org.openj9.test.java.lang.management.TestOperatingSystemMXBean 123 linux_x86-64
```

Please read [Configure environment](./docs/OpenJ9TestUserGuide.md#5-exclude-tests) for details and examples.

## 4) How to execute a different group of tests?

Test can be run with different levels, groups and types or combination of two
(i.e., level.group, level.type, group.type) or three (i.e., level.group.type)

Supported levels are `sanity|extended`

Supported groups  are `functional|system|openjdk|external|perf|jck`

Supported types  are `regular|native`

```
    make _sanity
    make _functional
    make _extended.perf
    make _sanity.native
    make _extended.functional.native
```

## 5) How to execute a list of tests?

A list of Tests can be executed through the `_testList` target followed by parameter `TESTLIST`. User can specify comma separated list of test names in `TESTLIST`. Note:  level, group, type or combinations of above (e.g., functional, sanity, sanity.native) are not supported in the TESTLIST.

```
make _testList TESTLIST=jit_jitt,jit_recognizedMethod,testSCCMLTests2_1
```

## 6) How to execute disabled tests?

If a test is disabled using `<disabled>` tag in playlist.xml, it can be executed through specifying the test target or adding `disabled` in front of regular target.

```
    make _disabled.testA    // testA has <disabled> tag in playlist.xml
    make _disabled.sanity.functional
    make _disabled.extended
```

Disabled tests and reasons can also be printed through adding `echo.disabled` in front of regular target.

```
    make _echo.disabled.testA
    make _echo.disabled.sanity.functional
    make _echo.disabled.extended
```

## 7) How to execute a directory of tests?

Only the tests in `BUILD_LIST` will be executed.

```
    export BUILD_LIST=functional/JIT_Test
    make compile
    make _sanity
```

## 8) How to run an individual JCK?

Please read [How-to Run customized JCK test targets](https://github.com/adoptium/aqa-tests/blob/master/jck/README.md) for details.

## 9) How to run the test with different `JDK_VERSION` and `JDK_IMPL`?

User can run tests against different jdk version and/or jdk
implementation. While the default values of these variables match a
typical use case for OpenJ9 developers, there are also many cases
where developers need to verify features for a specific version or
compare behaviour against a particular implementation.

There is no extra step needed.
By default, AUTO_DETECT is turned on, and the test framework will
auto detect SPEC, JDK_IMPL, and JDK_VERSION. Please read [Configure environment](./docs/OpenJ9TestUserGuide.md#1-configure-environment) for
details and examples.

## 10) How to interpret test results?
- test results summary

At the end of each run, test results summary will be printed:

```
    TEST TARGETS SUMMARY
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    DISABLED test targets:
	    cmdLineTester_pltest_tty_extended_0
	    cmdLineTester_pltest_numcpus_notBound_0
        ...
    PASSED test targets:
        cmdLineTester_javaAssertions_0
        cmdLineTester_LazyClassLoading_0
        ...
    FAILED test targets:
        TestAttachAPIEnabling_SE80_0
        TestFileLocking_SE80_0

    TOTAL: 91   EXECUTED: 84   PASSED: 82   FAILED: 2   DISABLED: 7   SKIPPED: 0
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
```

You can find the failed test output in console output.

- TAP result

A simple standardized TAP output is produced at the end of a test run,
to provide developers with a convenient summary of the test results.
It is also necessary as the tests used to verify OpenJ9 use a variety
of test output formats. This summary is a way to standardize the output
which allows CI tools to present results in a common way.

- `SKIPPED` tests

If a test is skipped, it means that this test cannot be run on this
platform due to jvm options, platform requirements and/or test
capabilities.

## 11) How to rerun failed tests?

`failed.mk` will be generated if there is any failed test target.
We can rerun failed tests as following:

```
    make _failed
```

`failed.mk` will be over-written each test run. If you want to
'save it', you can make a copy of the generated `failed.mk` file.
