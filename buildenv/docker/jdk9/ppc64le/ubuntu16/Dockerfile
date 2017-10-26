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
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0

# To use this docker file:
#   docker build -t=openj9 .
#   docker run -it openj9

FROM ppc64le/ibmjava:8-sdk

# Install required OS tools
RUN apt-get update \
  && apt-get install -qq -y --no-install-recommends \
    autoconf \
    ca-certificates \
    ccache \
    cpio \
    file \
    g++-4.8 \
    gcc-4.8 \
    git \
    git-core \
    libasound2-dev \
    libcups2-dev \
    libdwarf-dev \
    libelf-dev \
    libfreetype6-dev \
    libnuma-dev \
    libx11-dev \
    libxext-dev \
    libxrender-dev \
    libxt-dev \
    libxtst-dev \
    make \
    pkg-config \
    realpath \
    ssh \
    unzip \
    wget \
    zip \
  && rm -rf /var/lib/apt/lists/*

# Install optional tools
RUN apt-get update \
  && apt-get install -qq -y --no-install-recommends \
    vim \
  && rm -rf /var/lib/apt/lists/*

# Create links for c++,g++,cc,gcc
RUN ln -s g++ /usr/bin/c++ \
  && ln -s g++-4.8 /usr/bin/g++ \
  && ln -s gcc /usr/bin/cc \
  && ln -s gcc-4.8 /usr/bin/gcc

# Download and setup freemarker.jar to /root/freemarker.jar
RUN cd /root \
  && wget https://sourceforge.net/projects/freemarker/files/freemarker/2.3.8/freemarker-2.3.8.tar.gz/download -O freemarker.tgz \
  && tar -xzf freemarker.tgz freemarker-2.3.8/lib/freemarker.jar --strip=2 \
  && rm -f freemarker.tgz

WORKDIR /root
