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

# Guidelines on the JITaaS Dockerfiles

## build/Dockerfile
- produces `openj9-jitaas-build` image
- **prerequisite**: `openj9` image
  - to obtain `openj9` image do the following:
  - ```
     docker build -f \ 
     buildenv/docker/jdk<version>/<platform>/ubuntu/Dockerfile \
     -t=openj9 .
      ```
- What the image does:
   - sets up the build environment for JITaaS
   - pulls source from openjdk & JITaaS github repos and runs make commands to build JITaaS
(right now pulling from milestone_8 by default)
- to obtain `openj9-jitaas-build` image do the following:
  ```
  docker build -f \ 
  buildenv/docker/jitaas/jdk<version>/<platform>/ubuntu<version>/build/Dockerfile \ 
  --build-arg openj9_repo=<your-openj9-repo> \ 
  --build-arg openj9_branch=<your-openj9-branch> \ 
  --build-arg omr_repo=<your-omr-repo> \ 
  --build-arg omr_branch=<your-omr-branch> \
  -t=openj9-jitaas-build .
  ```
  or without specifying repos and default to milestone_8
  ```
  docker build -f \ 
  buildenv/docker/jitaas/jdk<version>/<platform>/ubuntu<version>/build/Dockerfile \ 
  -t=openj9-jitaas-build .
  ```
- **Temporary Workaround**: IMPORTANT
   - right now since our repos are not open-sourced
   - the docker image will not have permissions to pull down source files from our repos
   - workaround is to pass your .ssh directory into the docker image
   - to do this, you need to first add your ssh key to your github repo, then copy the content of your /root/.ssh to a directory named `key` 
        - `key` needs to be in the directory that you execute the `docker build` command

## run/Dockerfile
- produces `openj9-jitaas-run` image
- what the image does:
   - starts up a JITaaS Server
   - it's used for ICP deployment & testing
- **prerequisite**: `openj9-jitass-build` image
- to build this Dockerfile, do:
   ```
   docker build -f \
   buildenv/docker/jitaas/jdk<version>/<platform>/ubuntu<version>/run/Dockerfile \
   -t=openj9-jitaas-run .
   ```
- to use the image to start up a JITaaS server:
   ```
   docker run -it -d openj9-jitaas-run
   ```
- to find out its IPAddress
   ```
   docker inspect <openj9-jitaas-run-container-id> | grep "IPAddress"
   ```
- to look at jitaas verbose logs
   ```
   docker logs -f <openj9-jitaas-run-container-id>
   ```

## test/Dockerfile
- produces `openj9-jitaas-test` image
- what the image does:
   - sets up environment for openj9 tests for JITaaS
- **prerequisite**: `openj9-jitaas-build` image
- **prerequisite**: a running `openj9-jitaas-run` container and its IPAddress
- to build the image:
   ```
   docker build -f \
   buildenv/docker/jitaas/jdk<version>/<platform>/ubuntu<version>/test/Dockerfile \
   -t=openj9-jitaas-test .
   ```
- to use the image for testing:
   ```
   docker run -it openj9-jitaas-test
   // once you're inside the container
   make _sanity EXTRA_OPTIONS=" -XX:JITaaSClient:server=<IPAddress> " 
   // make sure to put spaces before and after "-XX:JITaaSClient:server=<IPAddress>"

   // to rerun failed tests
   make _failed EXTRA_OPTIONS=" -XX:JITaaSClient:server=<IPAddress> "
   // to run individual testcase
   make _<test_name> EXTRA_OPTIONS=" -XX:JITaaSClient:server=<IPAddress> " 
   ```


## buildenv/Dockerfile
- what the image does:
    - similar to `build/Dockerfile`
    - difference is it does not produce a jitaas build, it only sets up the environment for jitaas build
- produces `openj9-jitaas-buildenv` image
- to obtain the image do the following
   ```
   docker build -f \
   buildenv/docker/jitaas/jdk<version>/<platform>/ubuntu<version>/buildenv/Dockerfile \
   -t=openj9-jitaas-buildenv .
   ```
- to use the image
   ```
   docker run -it openj9-jitaas-buildenv
   // then you can build your jitaas binaries inside this container
   ```
