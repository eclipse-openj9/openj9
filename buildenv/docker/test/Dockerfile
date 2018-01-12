# Copyright (c) 2017, 2017 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception

# To use this docker file:
# 1.Depends on your OS platform, build the base openj9 docker
#   image from the Dockerfile in buildenv/docker/jdk9/
#   `docker build -f buildenv/docker/jdk9/<platform>/ubuntu/Dockerfile -t=openj9 .`
#
# 2.Build the Docker image from buildenv/docker/test/ for testing
#   `docker build -t=openj9-buildandtest .`
#   If your previous openj9 base Docker image is in a different
#   name, use this to build the testing Docker image
#   `docker build --build-arg IMAGE_NAME=<image_name> --build-arg IMAGE_VERSION=<image_version> -t openj9-buildandtest .`
#
# 3.Run this Docker image for your scenario
#   a) Scenario 1: I want to run OpenJ9 tests against a downloaded
#      OpenJ9 binary (OpenJ9 SDK). Or I just want to test my new
#      test code against a pre-built OpenJ9 SDK.
#
#   Run this Docker cmd:
#   `docker run -it -v <path_to_openj9_sdk_roots>:/java -v <path_to_openj9_test_dir>:/test openj9-buildandtest`
#
#   b) Scenario 2: I want to build the OpenJ9 and also test it.
#
#   Follow both README in these two links
#   https://www.eclipse.org/openj9/oj9_build.html
#   https://github.com/eclipse/openj9/blob/master/test/README.md
#
#   Highly recommend mounting folders to Docker container rather
#   than cloning them within Docker container. e.g.
#   `docker run -it -v <path_to_openj9-openjdk-jdk9_dir>:/openj9-openjdk-jdk9`
#   In this case, the built OpenJ9 SDK can be easily ported to localhost machine.
#

ARG IMAGE_NAME=openj9
ARG IMAGE_VERSION=latest

FROM ${IMAGE_NAME}:${IMAGE_VERSION}

# Install required executable dependencies for testing
RUN apt-get update \
    && apt-get install -qq -y --no-install-recommends \
    ant \
    ant-contrib \
    build-essential \
    perl \
    vim \
  && rm -rf /var/lib/apt/lists/*

# Install Docker module to run test framework
RUN echo yes | cpan install JSON Text::CSV

# Create links for c++,g++,cc,gcc
RUN ln -sf g++ /usr/bin/c++ \
  && ln -sf g++-4.8 /usr/bin/g++ \
  && ln -sf gcc /usr/bin/cc \
  && ln -sf gcc-4.8 /usr/bin/gcc

# These two folders are used for Scenario 1 talked above.
# In the `docker run` command, user also need mount <JDK_ROOT>
# and openj9/test dir to the Docker image by using cmd:
#   `docker run -it -v <path_to_openj9_sdk_roots>:/java -v <path_to_openj9_test_dir>:/test openj9-buildandtest`
VOLUME ["/test","/java"]

