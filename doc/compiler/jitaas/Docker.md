<!--
Copyright (c) 2018, 2019 IBM Corp. and others

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

# Guidelines on the JITaaS Dockerfiles

## A Quick Start Demo on Building JITaaS for JDK8 in Ubuntu 18.04 on x86_64
1. Retrieve OpenJ9 Repo
   ```
   git clone git@github.com:eclipse/openj9.git
   OR
   git clone https://github.com/eclipse/openj9.git

   # checkout jitaas branch
   cd openj9
   git checkout jitaas
   ```
2. Build `openj9` build env image. Make sure `jitaas` branch is checked out
   ```
   cd openj9/buildenv/docker/jdk8/x86_64/ubuntu18
   docker build -f Dockerfile -t=openj9 .
   ```
3. Build JITaaS
   ```
   cd openj9/buildenv/docker/jdk8/x86_64/ubuntu18/jitaas/build
   docker build -f Dockerfile -t=openj9-jitaas .
   ```

## Details on the JITaaS Dockerfiles
JITaaS Dockerfiles are located under: `openj9/buildenv/docker/jdk<version>/<platform>/ubuntu<version>/jitaas`
- `build/Dockerfile`
   - Sets up the build environment for JITaaS
   - Pulls the source code from OpenJDK, OpenJ9, and OMR repos. By default, `jitaas` branch is used for OpenJ9 and OMR.
   - Runs make commands to build JITaaS
- `buildenv/Dockerfile`
    - Similar to `jitaas/build/Dockerfile`. The difference is that it does not build JITaaS. It only pulls the source code and sets up the environment for JITaaS build
- `run/Dockerfile`
   - Starts up a JITaaS server
- `test/Dockerfile`
   - Sets up the OpenJ9 test environment for JITaaS

## How to use the JITaaS Dockerfiles
### [build/Dockerfile](https://github.com/eclipse/openj9/blob/jitaas/buildenv/docker/jdk8/x86_64/ubuntu18/jitaas/build/Dockerfile)
- **Prerequisite**: obtain `openj9` image first
   ```
   docker build -f \
   openj9/buildenv/docker/jdk<version>/<platform>/ubuntu<version>/Dockerfile \
   -t=openj9 .
   ```
- <a name="openj9-jitaas-build"></a>Build `openj9-jitaas-build` image using `build/Dockerfile`
   ```
  docker build -f \ 
  buildenv/docker/jdk<version>/<platform>/ubuntu<version>/jitaas/build/Dockerfile \ 
  --build-arg openj9_repo=<your-openj9-repo> \ 
  --build-arg openj9_branch=<your-openj9-branch> \ 
  --build-arg omr_repo=<your-omr-repo> \ 
  --build-arg omr_branch=<your-omr-branch> \
  -t=openj9-jitaas-build .
  ```
  Or without specifying repos and using the default latest `jitaas` branch for OpenJ9 and OMR
  ```
  docker build -f \ 
  buildenv/docker/jdk<version>/<platform>/ubuntu<version>/jitaas/build/Dockerfile \ 
  -t=openj9-jitaas-build .
  ```

### [run/Dockerfile](https://github.com/eclipse/openj9/blob/jitaas/buildenv/docker/jdk8/x86_64/ubuntu18/jitaas/run/Dockerfile)
- **Prerequisite**: [`openj9-jitaas-build`](#openj9-jitaas-build) image
- Build `openj9-jitaas-run-server` using `run/Dockerfile`
   ```
   docker build -f \
   buildenv/docker/jdk<version>/<platform>/ubuntu<version>/jitaas/run/Dockerfile \
   -t=openj9-jitaas-run-server .
   ```
- <a name="openj9-jitaas-run-server"></a>Use the image to start up a JITaaS server:
   ```
   docker run -it -d openj9-jitaas-run-server
   ```
- Find out its IPAddress
   ```
   docker inspect <openj9-jitaas-run-server-container-id> | grep "IPAddress"
   ```
- Look at jitaas verbose logs
   ```
   docker logs -f <openj9-jitaas-run-server-container-id>
   ```

### [test/Dockerfile](https://github.com/eclipse/openj9/blob/jitaas/buildenv/docker/jdk8/x86_64/ubuntu18/jitaas/test/Dockerfile)
- **Prerequisite**:
   - [`openj9-jitaas-build`](#openj9-jitaas-build) image
   - A running [`openj9-jitaas-run-server`](#openj9-jitaas-run-server) container and its IPAddress
- Build `openj9-jitaas-test` using `test/Dockerfile`:
   ```
   docker build -f \
   buildenv/docker/jdk<version>/<platform>/ubuntu<version>/jitaas/test/Dockerfile \
   -t=openj9-jitaas-test .
   ```
- Use the image for testing:
   ```
   docker run -it openj9-jitaas-test
   // once you're inside the container
   make _sanity EXTRA_OPTIONS=" -XX:JITaaSClient:server=<IPAddress> " 
   // make sure to put spaces before and after " -XX:JITaaSClient:server=<IPAddress> "

   // Rerun failed tests
   make _failed EXTRA_OPTIONS=" -XX:JITaaSClient:server=<IPAddress> "

   // Run individual test case
   make _<test_name> EXTRA_OPTIONS=" -XX:JITaaSClient:server=<IPAddress> " 
   ```

### [buildenv/Dockerfile](https://github.com/eclipse/openj9/blob/jitaas/buildenv/docker/jdk8/x86_64/ubuntu18/jitaas/buildenv/Dockerfile)
- Build `openj9-jitaas-buildenv` using `buildenv/Dockerfile`:
   ```
   docker build -f \
   buildenv/docker/jdk<version>/<platform>/ubuntu<version>/jitaas/buildenv/Dockerfile \
   -t=openj9-jitaas-buildenv .
   ```
- Use the image
   ```
   docker run -it openj9-jitaas-buildenv
   // then you can build your jitaas binaries inside this container
   ```
