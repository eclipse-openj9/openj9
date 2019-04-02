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

## [build/Dockerfile](https://github.com/eclipse/openj9/blob/jitaas/buildenv/docker/jdk8/x86_64/ubuntu18/jitaas/build/Dockerfile)
- **Prerequisite**: obtain `openj9` image first as below:
   ```
   docker build -f \
   openj9/buildenv/docker/jdk<version>/<platform>/ubuntu<version>/Dockerfile \
   -t=openj9 .
   ```
- <a name="openj9-jitaas-build"></a>`openj9-jitaas-build` image
   - Sets up the build environment for JITaaS
   - Pulls source from OpenJDK and OpenJ9 & OMR `jitaas` branch by default and runs make commands to build JITaaS
- Build `openj9-jitaas-build` image as below:
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

## [run/Dockerfile](https://github.com/eclipse/openj9/blob/jitaas/buildenv/docker/jdk8/x86_64/ubuntu18/jitaas/run/Dockerfile)
- **Prerequisite**: [`openj9-jitaas-build` image](#openj9-jitaas-build)
- `openj9-jitaas-run` image starts up a JITaaS Server. Build it as below:
   ```
   docker build -f \
   buildenv/docker/jdk<version>/<platform>/ubuntu<version>/jitaas/run/Dockerfile \
   -t=openj9-jitaas-run .
   ```
- <a name="openj9-jitaas-run"></a>To use the image to start up a JITaaS server:
   ```
   docker run -it -d openj9-jitaas-run
   ```
- To find out its IPAddress
   ```
   docker inspect <openj9-jitaas-run-container-id> | grep "IPAddress"
   ```
- To look at jitaas verbose logs
   ```
   docker logs -f <openj9-jitaas-run-container-id>
   ```

## [test/Dockerfile](https://github.com/eclipse/openj9/blob/jitaas/buildenv/docker/jdk8/x86_64/ubuntu18/jitaas/test/Dockerfile)
- **Prerequisite**:
   - [`openj9-jitaas-build` image](#openj9-jitaas-build)
   - A running [`openj9-jitaas-run` container](#openj9-jitaas-run) (server) and its IPAddress
- `openj9-jitaas-test` image (client) sets up the OpenJ9 test environment for JITaaS. Build it as below:
   ```
   docker build -f \
   buildenv/docker/jdk<version>/<platform>/ubuntu<version>/jitaas/test/Dockerfile \
   -t=openj9-jitaas-test .
   ```
- To use the image for testing:
   ```
   docker run -it openj9-jitaas-test
   // once you're inside the container
   make _sanity EXTRA_OPTIONS=" -XX:JITaaSClient:server=<IPAddress> " 
   // make sure to put spaces before and after " -XX:JITaaSClient:server=<IPAddress> "

   // to rerun failed tests
   make _failed EXTRA_OPTIONS=" -XX:JITaaSClient:server=<IPAddress> "

   // to run individual testcase
   make _<test_name> EXTRA_OPTIONS=" -XX:JITaaSClient:server=<IPAddress> " 
   ```

## [buildenv/Dockerfile](https://github.com/eclipse/openj9/blob/jitaas/buildenv/docker/jdk8/x86_64/ubuntu18/jitaas/buildenv/Dockerfile)
- `openj9-jitaas-buildenv` image:
    - Similar to `openj9-jitaas-build` built from `build/Dockerfile`
    - The difference is that it does not produce a JITaaS build. It sets up only the environment for JITaaS build
- Build `openj9-jitaas-buildenv` image as blow:
   ```
   docker build -f \
   buildenv/docker/jdk<version>/<platform>/ubuntu<version>/jitaas/buildenv/Dockerfile \
   -t=openj9-jitaas-buildenv .
   ```
- To use the image
   ```
   docker run -it openj9-jitaas-buildenv
   // then you can build your jitaas binaries inside this container
   ```
