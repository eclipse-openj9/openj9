<!--
Copyright (c) 2018, 2020 IBM Corp. and others

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

# Guidelines on the JITServer Dockerfiles

## A Quick Start Demo on Building JITServer for JDK8 in Ubuntu 18.04 on x86_64
1. Retrieve OpenJ9 Repo
   ```
   git clone git@github.com:eclipse/openj9.git
   OR
   git clone https://github.com/eclipse/openj9.git

   # checkout master branch
   cd openj9
   git checkout master
   ```
2. Build `openj9` build env image.
   ```
   cd buildenv/docker
   bash mkdocker.sh --build --dist=ubuntu --tag=openj9 --version=18
   ```
3. Build JITServer
   ```
   cd buildenv/docker/jdk8/x86_64/ubuntu18/jitserver/build
   docker build -f Dockerfile -t=openj9-jitserver-build .
   ```

## Details on the JITServer Dockerfiles
JITServer Dockerfiles are located under: `openj9/buildenv/docker/jdk<version>/<platform>/ubuntu<version>/jitserver`
- `build/Dockerfile`
   - Sets up the build environment for JITServer
   - Pulls the source code from OpenJDK, OpenJ9, and OMR repos.
   - Runs make commands to build JITServer
- `buildenv/Dockerfile`
    - Similar to `jitserver/build/Dockerfile`. The difference is that it does not build JITServer. It only pulls the source code and sets up the environment for JITServer build
- `buildserver/Dockerfile`
   - Starts up a JITServer server
- `test/Dockerfile`
   - Sets up the OpenJ9 test environment for JITServer

## How to use the JITServer Dockerfiles
### [build/Dockerfile](https://github.com/eclipse/openj9/blob/master/buildenv/docker/jdk8/x86_64/ubuntu18/jitserver/build/Dockerfile)
- **Prerequisite**: obtain `openj9` image first
   ```
   docker build -f \
   openj9/buildenv/docker/jdk<version>/<platform>/ubuntu<version>/Dockerfile \
   -t=openj9 .
   ```
- <a name="openj9-jitserver-build"></a>Build `openj9-jitserver-build` image using `build/Dockerfile`
   ```
  docker build -f \
  buildenv/docker/jdk<version>/<platform>/ubuntu<version>/jitserver/build/Dockerfile \
  --build-arg openj9_repo=<your-openj9-repo> \
  --build-arg openj9_branch=<your-openj9-branch> \
  --build-arg omr_repo=<your-omr-repo> \
  --build-arg omr_branch=<your-omr-branch> \
  -t=openj9-jitserver-build .
  ```
  Or without specifying repos and using the default latest branch for OpenJ9 and OMR
  ```
  docker build -f \
  buildenv/docker/jdk<version>/<platform>/ubuntu<version>/jitserver/build/Dockerfile \
  -t=openj9-jitserver-build .
  ```

### [buildserver/Dockerfile](https://github.com/eclipse/openj9/blob/master/buildenv/docker/jdk8/x86_64/ubuntu18/jitserver/buildserver/Dockerfile)
- **Prerequisite**: [`openj9-jitserver-build`](#openj9-jitserver-build) image
- Build `openj9-jitserver-run-server` using `buildserver/Dockerfile`
   ```
   docker build -f \
   buildenv/docker/jdk<version>/<platform>/ubuntu<version>/jitserver/buildserver/Dockerfile \
   -t=openj9-jitserver-run-server .
   ```
- <a name="openj9-jitserver-run-server"></a>Use the image to start up a JITServer server:
   ```
   docker run -it -d openj9-jitserver-run-server
   ```
- Find out its IPAddress
   ```
   docker inspect <openj9-jitserver-run-server-container-id> | grep "IPAddress"
   ```
- Look at jitserver verbose logs
   ```
   docker logs -f <openj9-jitserver-run-server-container-id>
   ```

### [test/Dockerfile](https://github.com/eclipse/openj9/blob/master/buildenv/docker/jdk8/x86_64/ubuntu18/jitserver/test/Dockerfile)
- **Prerequisite**:
   - [`openj9-jitserver-build`](#openj9-jitserver-build) image
   - A running [`openj9-jitserver-run-server`](#openj9-jitserver-run-server) container and its IPAddress
- Build `openj9-jitserver-test` using `test/Dockerfile`:
   ```
   docker build -f \
   buildenv/docker/jdk<version>/<platform>/ubuntu<version>/jitserver/test/Dockerfile \
   -t=openj9-jitserver-test .
   ```
- Use the image for testing:
   ```
   docker run -it openj9-jitserver-test
   // once you're inside the container
   make _sanity EXTRA_OPTIONS=" -XX:+UseJITServer:server=<IPAddress> "
   // make sure to put spaces before and after " -XX:+UseJITServer:server=<IPAddress> "

   // Rerun failed tests
   make _failed EXTRA_OPTIONS=" -XX:+UseJITServer:server=<IPAddress> "

   // Run individual test case
   make _<test_name> EXTRA_OPTIONS=" -XX:+UseJITServer:server=<IPAddress> "
   ```

### [buildenv/Dockerfile](https://github.com/eclipse/openj9/blob/master/buildenv/docker/jdk8/x86_64/ubuntu18/jitserver/buildenv/Dockerfile)
- Build `openj9-jitserver-buildenv` using `buildenv/Dockerfile`:
   ```
   docker build -f \
   buildenv/docker/jdk<version>/<platform>/ubuntu<version>/jitserver/buildenv/Dockerfile \
   -t=openj9-jitserver-buildenv .
   ```
- Use the image
   ```
   docker run -it openj9-jitserver-buildenv
   // then you can build your jitserver binaries inside this container
   ```
