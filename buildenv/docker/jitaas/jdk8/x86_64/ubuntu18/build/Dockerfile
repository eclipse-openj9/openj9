# Copyright (c) 2017, 2018 IBM Corp. and others
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
# 2.Build the jitaas docker image from buildenv/docker/jitaas/.../build/ to fetch dependencies, source code and build jitaas
#   `docker build -f \
#    buildenv/docker/jitaas/jdk<version>/<platform>/ubuntu<version>/build/Dockerfile \
#    --build-arg openj9_repo=<your-openj9-repo> \
#    --build-arg openj9_branch=<your-openj9-branch> \
#    --build-arg omr_repo=<your-omr-repo> \ 
#    --build-arg omr_branch=<your-omr-branch> \
#    -t=openj9-jitaas-build .`
#

FROM openj9:latest

ARG openj9_repo=git@github.ibm.com:runtimes/Jaas_tr.open.git
ARG openj9_branch=milestone_8
ARG omr_repo=git@github.ibm.com:runtimes/Jaas_omr.git
ARG omr_branch=milestone_8

RUN apt-get update \
 && apt-get install -qq -y --no-install-recommends \
    automake \
    curl \
    g++-6 \
    gcc-6 \
    libssl-dev \
    libtool \
    build-essential \ 
    autoconf \
    pkg-config \
 && rm -rf /var/lib/apt/lists/*

# JITaaS needs to be built with a newer GCC
RUN ln -sf /usr/bin/g++ /usr/bin/c++ \
 && ln -sf /usr/bin/g++-6 /usr/bin/g++ \
 && ln -sf /usr/bin/gcc /usr/bin/cc \
 && ln -sf /usr/bin/gcc-6 /usr/bin/gcc
 
RUN git clone -b $(curl -L https://grpc.io/release) https://github.com/grpc/grpc \
 && cd grpc \
 && git submodule update --init \
 && make \
 && make install \
 && cd /root/grpc/third_party/protobuf \
 && make install \
 && rm -rf /root/grpc

RUN mkdir -p /root/.ssh
COPY key /root/.ssh/

# Grab sources from openj9-openjdk-jdk8 and JITaaS repos
# Then builds JITaaS
WORKDIR /root
RUN git clone https://github.com/ibmruntimes/openj9-openjdk-jdk8 \
 && cd openj9-openjdk-jdk8 \
 && bash ./get_source.sh -openj9-repo=$openj9_repo -openj9-branch=$openj9_branch -omr-repo=$omr_repo -omr-branch=$omr_branch \
 && bash ./configure --with-freemarker-jar=/root/freemarker.jar \
 && export UMA_SUPPRESS_WARNINGS_AS_ERRORS=1 \
 && cd openj9/runtime/compiler \
 && make -f compiler.mk -C . proto \
 && cd ../../.. \
 && make all

# Clean up
RUN mv /root/openj9-openjdk-jdk8/build/linux-x86_64-normal-server-release/images/j2sdk-image /root/ \
 && mv /root/openj9-openjdk-jdk8/openj9/test /root/ \
 && rm -rf /root/openj9-openjdk-jdk8 \
 && rm -rf /root/bootjdk8 \
 && rm -rf /root/freemarker.jar

WORKDIR /root
