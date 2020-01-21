# Copyright (c) 2017, 2020 IBM Corp. and others
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
#   `docker build -f buildenv/docker/jdk<version>/<platform>/ubuntu/Dockerfile -t=openj9 .`
#
# 2.Build the jitserver docker image from buildenv/docker/jitserver/build/ to fetch dependencies, source code and build jitserver
#   `docker build -f \
#    buildenv/docker/jitserver/jdk<version>/<platform>/ubuntu<version>/build/Dockerfile \
#    --build-arg openj9_repo=<your-openj9-repo> \
#    --build-arg openj9_branch=<your-openj9-branch> \
#    --build-arg omr_repo=<your-omr-repo> \ 
#    --build-arg omr_branch=<your-omr-branch> \
#    -t=openj9-jitserver-build .`
#
# 3.Build the jitserver test docker image from buildenv/docker/jitserver/.../run/ to set up environment for running openj9 tests
#   `docker build -f \
#   buildenv/docker/jitserver/jdk<version>/<platform>/ubuntu<version>/test/Dockerfile \
#   -t=openj9-jitserver-test .`
#

FROM openj9-jitserver-build:latest

RUN apt-get update \
 && apt-get install -qq -y --no-install-recommends \
    ant \
    ant-contrib \
    perl \
    vim \
    libxml-parser-perl \
 && rm -rf /var/lib/apt/lists/*

ENV JAVA_VERSION=SE80
ENV JAVA_BIN=/root/j2sdk-image/jre/bin
ENV SPEC=linux_x86-64_cmprssptrs
ENV JAVA_HOME=/root/j2sdk-image

# Builds tests
RUN cd /root/test \
 && git clone https://github.com/AdoptOpenJDK/TKG.git \
 && cd TKG \
 && make compile

WORKDIR /root/test/TKG
