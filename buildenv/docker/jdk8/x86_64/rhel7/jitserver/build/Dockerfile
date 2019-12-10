# Copyright (c) 2019, 2019 IBM Corp. and others
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

FROM registry.access.redhat.com/rhel7:7.6 AS jitserver_build
# To use this docker file:
#
# 1.Execute the following command to generate the jitserver build docker image
#   `docker build -f \
#    buildenv/docker/jitserver/jdk8/x86_64/rhel7/jitserver/build/Dockerfile \
#    -t=openj9-jitserver-build .`
#

ARG openj9_repo=https://github.com/eclipse/openj9.git
ARG omr_repo=https://github.com/eclipse/openj9-omr.git

RUN yum-config-manager --enable rhel-7-server-optional-rpms 
RUN yum install -y autoconf \
    automake \
    ca-certificates \
    cmake \
    cpio \ 
    file \
    alsa-lib-devel \
    cups-devel \
    libdwarf-devel \
    elfutils-libelf-devel \
    fontconfig-devel \
    freetype-devel \
    numactl-devel \
    libtool \
    libX11-devel \
    libXext-devel \
    libXrender-devel \
    libXt-devel \
    libXtst-devel \
    java-1.8.0-openjdk-devel \
    make \
    pkgconfig \
    coreutils \
    openssh \
    unzip \
    wget \
    zip 

RUN yum-config-manager --enable rhel-server-rhscl-7-rpms
RUN yum install -y rh-git29 \
                devtoolset-7-gcc \
                devtoolset-7-gcc-c++ \
                devtoolset-7-libstdc++-devel \
                devtoolset-7-make
RUN yum-config-manager --add-repo https://www.nasm.us/nasm.repo
RUN yum install -y nasm

SHELL ["/usr/bin/scl", "enable", "devtoolset-7", "rh-git29"]
#RUN scl enable rh-git29 bash
#RUN scl enable devtoolset-7 bash

# Download and setup freemarker.jar to /root/freemarker.jar
RUN cd /root \
  && wget https://sourceforge.net/projects/freemarker/files/freemarker/2.3.8/freemarker-2.3.8.tar.gz/download -O freemarker.tgz \
  && tar -xzf freemarker.tgz freemarker-2.3.8/lib/freemarker.jar --strip=2 \
  && rm -f freemarker.tgz

# Install protobuf
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
 && bash ./configure --with-freemarker-jar=/root/freemarker.jar --enable-jitserver \
 && make clean \
 && make all

FROM registry.access.redhat.com/rhel7:7.6
COPY --from=jitserver_build /root/openj9-openjdk-jdk8/build/linux-x86_64-normal-server-release/images/j2sdk-image /root/j2sdk-image
WORKDIR /root
