# Copyright (c) 2019, 2020 IBM Corp. and others
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
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0

# To use this docker file:
# 1.Depends on your OS platform, build the base openj9 docker
#   image from the Dockerfile in buildenv/docker/jdk<version>/
#   `docker build -f buildenv/docker/jdk<version>/<platform>/ubuntu<version>/Dockerfile -t=openj9 .`
#
# 2.Build the jitserver docker image from buildenv/docker/jitserver/.../build/ to fetch dependencies, source code and build jitserver
#   `docker build -f \
#    buildenv/docker/jitserver/jdk<version>/<platform>/ubuntu<version>/build/Dockerfile \
#    --build-arg openj9_repo=<your-openj9-repo> \
#    --build-arg openj9_branch=<your-openj9-branch> \
#    --build-arg omr_repo=<your-omr-repo> \
#    --build-arg omr_branch=<your-omr-branch> \
#    -t=openj9-jitserver-build .`
#

FROM openj9:latest

ARG openj9_repo=https://github.com/eclipse/openj9.git
ARG omr_repo=https://github.com/eclipse/openj9-omr.git

RUN apt-get update \
 && apt-get install -qq -y --no-install-recommends \
    automake \
    curl \
    libssl-dev \
    libtool \
    build-essential \
    autoconf \
    pkg-config \
    libfontconfig1-dev \
 && rm -rf /var/lib/apt/lists/*

RUN apt-get update \
 && apt-get install software-properties-common -y

RUN apt-get update \
 && add-apt-repository ppa:ubuntu-toolchain-r/test -y

RUN apt-get update \
 && apt-get install -qq -y --no-install-recommends gcc-7 g++-7 \
 && rm -rf /var/lib/apt/lists/*

# JITServer needs to be built with a newer GCC
RUN ln -sf /usr/bin/g++ /usr/bin/c++ \
 && ln -sf /usr/bin/g++-7 /usr/bin/g++ \
 && ln -sf /usr/bin/gcc /usr/bin/cc \
 && ln -sf /usr/bin/gcc-7 /usr/bin/gcc

# Install protobuf 3.7.1
WORKDIR /
RUN wget https://github.com/protocolbuffers/protobuf/releases/download/v3.7.1/protobuf-cpp-3.7.1.tar.gz \
 && tar -xvzf protobuf-cpp-3.7.1.tar.gz \
 && cd protobuf-3.7.1 \
 && ./configure --disable-shared --with-pic && make && make install && ldconfig \
 && rm -rf /protobuf-3.7.1 && rm -rf /protobuf-cpp-3.7.1.tar.gz

# Grab sources from openj9-openjdk-jdk8 and JITServer repos
# Then builds JITServer
WORKDIR /root
RUN git clone https://github.com/ibmruntimes/openj9-openjdk-jdk8 \
 && cd openj9-openjdk-jdk8 \
 && bash ./get_source.sh -openj9-repo=$openj9_repo -omr-repo=$omr_repo \
 && bash ./configure --with-freemarker-jar=/root/freemarker.jar --enable-jitserver\
 && make all

# Clean up
RUN mv /root/openj9-openjdk-jdk8/build/linux-s390x-normal-server-release/images/j2sdk-image /root/ \
 && mv /root/openj9-openjdk-jdk8/openj9/test /root/ \
 && rm -rf /root/openj9-openjdk-jdk8 \
 && rm -rf /root/bootjdk8 \
 && rm -rf /root/freemarker.jar

WORKDIR /root
