<!--
Copyright (c) 2023, 2023 IBM Corp. and others

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

# MicroJIT Regression Tests

## Quick Start

Export your JAVA_HOME and MJIT_JAVA_BINARY environment variables and run `make all` to compile classes in `./build` directory. The `./run.sh` command will run all the tests in the `src/tests` directory.

```bash
export JAVA_HOME=<path/to/bootjdk8>;
export CONF=release;
export MJIT_JAVA_BINARY="<path/to>/openj9-openjdk-jdk8/build/linux-x86_64-normal-server-${CONF}/images/j2sdk-image/bin/java";
make all;
./run.sh;
```

> Note: `CONF` can be one of release, slowdebug or fastdebug. See `./configure` help in `<path/to>/openj9-openjdk-jdk8/`.

---

## Usage

### Table of Contents

- [Running Individual Tests](#running-individual-tests)
- [Running with GDB](#running-with-gdb)
- [Project Structure](#project-structure)
- [JUnit Classes](#junit-classes)
- [test-list.txt Format](#test-list.txt-format)
- [Helpful Tips](#helpful-tips)

### Running Individual Tests

Tests can be run individually by passing the method signature to `run.sh`:

```bash
./run.sh <method-name>
./run.sh <class-name>.<method-name>
./run.sh <method-name1> <method-name2> ...
./run.sh <class-name>
```

#### Example

```bash
./run.sh add
```

The above command will run all tests with the add signature.

#### Example

```bash
./run.sh IntTests.add
```

The above command will run only the IntTests.add test. You can also provide a list of tests to perform.

#### Example

```bash
./run.sh add sub mul
```

Passing the class name will run all tests in the given file:

#### Example

```bash
./run.sh IntTests
```

This feature also works for debugging with gdb.

### Running with GDB

The `run.sh` script also works with gdb as follows:

```bash
export MJIT_DEBUG_ENABLED=true;
./run.sh;
```

It will prompt you for each test to ask if you'd like to launch gdb:

```bash
Using JVM:
openjdk version "1.8.0_342-internal"
OpenJDK Runtime Environment (build 1.8.0_342-internal-test_2022_06_02_09_41-b00)
Eclipse OpenJ9 VM (build microjit-rebased-0de87ac1b, JRE 1.8.0 Linux amd64-64-Bit Compressed References 20220602_000000 (JIT enabled, AOT enabled)
OpenJ9   - 0de87ac1b
OMR      - 5432ae76a
JCL      - 1a5e46c542 based on jdk8u342-b01)

Running tests (n=30000,threshold=3)...

Running using gdb BranchTests.gotoTest<I>I Y/n [n]: ... skipping
Running using gdb ConversionTests.i2l<I>J Y/n [n]:y
GNU gdb (Ubuntu 10.2-0ubuntu1~18.04~2) 10.2
Copyright (C) 2021 Free Software Foundation, Inc.

[... snip]

Reading symbols from /workspace/openj9-openjdk-jdk8/build/linux-x86_64-normal-server-release/images/j2sdk-image/bin/java...
(No debugging symbols found in /workspace/openj9-openjdk-jdk8/build/linux-x86_64-normal-server-release/images/j2sdk-image/bin/java)
(gdb)
```

> Note: You must run through the program once before setting any breakpoints, as the debug symbols are dynamically loaded.

> To quit from `run.sh`, press Ctrl+C.

### Project Structure

```bash
├── build
│   ├── <class-name>.class
│   ├── ...
├── .do
│   ├── <class-name>
│   │   └── <JUnit-tag>
│   │       ├── cmd
│   │       ├── debug.sh
│   │       ├── limit
│   │       ├── mjit.log.454115.12822.20200709.134022.454115
│   │       ├── result_env
│   │       ├── run.err
│   │       ├── run.log
│   │       └── test.sh
│   └── pid
│   │   ├── currently_alive_test
├── lib
│   └── junit-platform-console-standalone-1.6.0.jar
├── makefile
├── README.md
├── run.sh
├── run-tests.sh
├── src
│   ├── Helper.java
│   ├── lib
│   |   ├── <class-name>.class
│   |   ├── <class-name>.java
│   |   ├── ...
│   └── tests
│       ├── <class-name>.java
│       ├── ...
└── test-list.txt
```

### JUnit Classes

To implement new test cases, you must add a new Java method to the `src/tests` directory. The method can be added to an existing `.java` file, or a new file can be created. When creating new files, you must import the following JUnit libraries:

#### Example Java Test Class

```java
import static org.junit.jupiter.api.Assertions.assertEquals;

import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.Tag;
import org.junit.jupiter.api.Order;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.CsvSource;

public class <class-name> {

    @Tag("<JUnit-tag>")
    @Order(1)
    @ParameterizedTest(name = "#{index} - <method-name>({0},{1}) = {2}")
    @CsvSource({
        /*
         * Each line is a JUnit test parameter and will be executed
         * as indicated in @ParameterizedTest. Each line has its
         * own unique index
         */
        "<{0}>, <{1}>, <{2}>",
        // Example
        "1, 1, 2"
    })
    public void <test-name>(int arg1, int arg2, int expected){
        int result = 0;
        for(int i = 0; i < Helper.invocations(); i++) {
            result = <method-name>(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static int <method-name>(int x, int y){
        // method-body
    }

}
```

#### JUnit Tags

JUnit tags must be of the form `methodName<argumentTypes>returnType`
where:

- `methodName` must match the method you want to compile with MicroJIT.
- `<argumentTypes>` must match the types of variables you are passing.
- `returnType` must match the type of return value.

where `Types` are:

| Type              | Meaning                                     |
|:-----------------:|:-------------------------------------------:|
| **B**             | <pre lang="java">`byte`</pre>               |
| **C**             | <pre lang="java">`char`</pre>               |
| **D**             | <pre lang="java">`double`</pre>             |
| **F**             | <pre lang="java">`float`</pre>              |
| **I**             | <pre lang="java">`int`</pre>                |
| **J**             | <pre lang="java">`long`</pre>               |
| **L ClassName ;** | reference -> an instance of class ClassName |
| **S**             | <pre lang="java">`short`</pre>              |
| **Z**             | <pre lang="java">`boolean`</pre>            |
| **[**             | reference -> one array dimension            |

Add the method JUNIT tags and class names to the test-list.txt.

<h3 id="test-list.txt-format">test-list.txt Format</h3>

```txt
# This is a comment
<class-name>.<JUnit-tag>

# This test has one dependency
<class-name>.<JUnit-tag>: [ <class-name>.<JUnit-tag> ]

# This test has multiple dependencies
<class-name>.<JUnit-tag>: [ <class-name>.<JUnit-tag>, <class-name>.<JUnit-tag> ]
```

> Note: Dependency array is only relevant if your test is dependent on another method, i.e., if you need to first warm up another method. You can also make a test dependent on itself, if you'd like to run the test multiple times on one VM session.

#### Example

```txt
LongTests.xor<JJ>J
GetPutStaticTests.add<>I: [ GetPutStaticTests.setInts<II>V ]
```

and their matching JUnit tags:

```Java
public class LongTests {
    // [... snip]
    @Tag("xor<JJ>J")
    @Order(1)
    // [... snip]
    public static long xor(long x, long y);
    // [... snip]
}
```

```Java
public class GetPutStaticTests {
    // [... snip]
    @Tag("add<>I")
    @Order(2)
    // [... snip]
    public static int add();
    // [... snip]
    @Tag("setInts<II>V")
    @Order(1)
    // [... snip]
    public static void setInts(int arg1, int arg2);
}
```

In this example, `setInts` must be warmed up before `add` for the GetPutStaticTests because of its dependency, so it gets `@Order(1)` and `add` gets `@Order(2)`.

### Helpful Tips

After running a test, the log files will appear in the .do file directory. If a test fails, it can be helpful to check the log files to find out what happened.

#### `✔`

  > Your test passed!

#### `✗ : Not compiled by MicroJIT`

  > The mjit.log file is a good place to start for these failures. It will show the disassembly for the method to be compiled which can be helpful, to know if you are testing the correct bytecodes, and to make sure that there are no unsupported bytecodes being used by the method.

#### `✗ : JUnit or Java failure`

  > The run.log file will contain the JUnit test results. If there is a javacore dump file, that can be helpful too, as you will be able to see the state of all of the registers at the time the JVM crashed.

---

## Adding an extra layer to testing: run-tests.sh to compile using Testarossa or MicroJIT

```bash
./run-tests.sh <compiler> <signature>
```

where `<compiler>` is a mandatory parameter to signify which compiler to use and may take values as T/TR/M/MJIT in any case (will be ultimately converted to uppercase).
The tr.log file will be generated for TRJIT and mjit.log for MicroJIT and `<signature>` is an optional parameter which provides class and method names to the `run.sh` command, if specified.

Hence, `./run-tests.sh <compiler> <signature>` will be executed as:
`./run.sh <signature>` using the compiler specified in `<compiler>` parameter.
