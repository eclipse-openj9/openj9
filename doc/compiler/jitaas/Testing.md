<!--
Copyright (c) 2018, 2018 IBM Corp. and others

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

There are currently 2 supported test procedures. Select the one that best suits your neeeds.

- [Testing locally](#local). Choose this if you have build & test dependencies set up.
- [Testing in Docker](#docker). Choose this if you don't want to deal with build & test dependency setups.

## LOCAL

These are the steps to run the tests on your machine.

1. Compile a debug build of openj9 from scratch. To do this pass the flag `--with-debug-level=slowdebug` to `configure`.

2. Install [prerequisites](https://github.com/eclipse/openj9/blob/master/test/docs/Prerequisites.md)

   Example installation can be found [here](https://github.com/eclipse/openj9/blob/master/buildenv/docker/test/Dockerfile#L57-L68).
3. Compile  the tests (only need to compile once):
   ```
   cd $OPENJ9_DIR/openj9/test/TestConfig
   export JAVA_VERSION=SE80
   export JAVA_BIN=/your/sdk/jre/bin
   export SPEC=linux_x86-64_cmprssptrs
   export EXTRA_OPTIONS=" -XX:JITaaSClient " # spaces at the start and end are important!
   make -f run_configure.mk
   make compile
   ```

   NOTE: to compile and run tests, ant-contrib.jar is needed, and on some systems just installing ant will not install ant-contrib.
   On Ubuntu, the easiest way to get it is to run:
   ```
   sudo apt-get install ant-contrib
   ```

4. Run the tests! First start a server, then do
   ```
   make _sanity
   ```
   NOTE: It's important to put spaces before and after `-XX:JITaaSClient`, otherwise
   some tests will not run properly.
   
5. To rerun the failed tests, run
   ```
   make _failed
   ```
   which tests will be rerun can be modified in `failedtargets.mk`.

   or, if you want to run an individual test, run
   ```
   make <name_of_the_test>
   ```

For more advanced testing features, refer to [this guide](https://github.com/eclipse/openj9/blob/master/test/docs/OpenJ9TestUserGuide.md).

## DOCKER

Follow the [Docker.md](Docker.md) here for building then testing JITaaS in Docker.
