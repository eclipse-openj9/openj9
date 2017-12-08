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
#   docker build -t=openj9arm .
#   docker run -v </path/openj9-openjdk-jdk9>:/root/openj9-openjdk-jdk9 -it openj9arm

FROM ubuntu:16.04

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
    libelf-dev \
    libfreetype6-dev \
    libnuma-dev \
    libx11-dev \
    libxext-dev \
    libxrender-dev \
    libxt-dev \
    libxtst-dev \
    make \
    openjdk-8-jdk \
    pkg-config \
    qemu \
    realpath \
    ssh \
    unzip \
    vim-tiny \
    wget \
    zip \
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

# get the toolchain
RUN cd /root \
  && wget https://releases.linaro.org/components/toolchain/binaries/4.9-2017.01/arm-linux-gnueabihf/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf.tar.xz \
  && tar xf gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf.tar.xz \
  && rm gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf.tar.xz

# Get the debs for native libraries on the target platform
# These were current releases in stretch as of Nov 2017
# We might need older libraries for older OS releases, and presumably at
# some point we might need to update to newer ones.
# Libraries can be found through https://www.debian.org/distrib/packages
# TODO Mirror site should be configurable.
ADD \
    http://ftp.us.debian.org/debian/pool/main/a/alsa-lib/libasound2_1.1.3-5_armhf.deb                \
    http://ftp.us.debian.org/debian/pool/main/a/alsa-lib/libasound2-dev_1.1.3-5_armhf.deb            \
    http://ftp.us.debian.org/debian/pool/main/c/cups/libcups2-dev_2.2.1-8_armhf.deb                  \
    http://ftp.us.debian.org/debian/pool/main/c/cups/libcupsimage2-dev_2.2.1-8_armhf.deb             \
    http://ftp.us.debian.org/debian/pool/main/d/dwarfutils/libdwarf-dev_20161124-1+deb9u1_armhf.deb  \
    http://ftp.us.debian.org/debian/pool/main/f/freetype/libfreetype6_2.6.3-3.2_armhf.deb            \
    http://ftp.us.debian.org/debian/pool/main/f/freetype/libfreetype6-dev_2.6.3-3.2_armhf.deb        \
    http://ftp.us.debian.org/debian/pool/main/libi/libice/libice-dev_1.0.9-2_armhf.deb               \
    http://ftp.us.debian.org/debian/pool/main/libp/libpng1.6/libpng16-16_1.6.28-1_armhf.deb          \
    http://ftp.us.debian.org/debian/pool/main/libp/libpng1.6/libpng-dev_1.6.28-1_armhf.deb           \
    http://ftp.us.debian.org/debian/pool/main/libs/libsm/libsm-dev_1.2.2-1+b3_armhf.deb              \
    http://ftp.us.debian.org/debian/pool/main/libx/libx11/libx11-6_1.6.4-3_armhf.deb                 \
    http://ftp.us.debian.org/debian/pool/main/libx/libx11/libx11-dev_1.6.4-3_armhf.deb               \
    http://ftp.us.debian.org/debian/pool/main/libx/libxext/libxext6_1.3.3-1+b2_armhf.deb             \
    http://ftp.us.debian.org/debian/pool/main/libx/libxext/libxext-dev_1.3.3-1+b2_armhf.deb          \
    http://ftp.us.debian.org/debian/pool/main/libx/libxi/libxi6_1.7.9-1_armhf.deb                    \
    http://ftp.us.debian.org/debian/pool/main/libx/libxi/libxi-dev_1.7.9-1_armhf.deb                 \
    http://ftp.us.debian.org/debian/pool/main/libx/libxrender/libxrender1_0.9.10-1_armhf.deb         \
    http://ftp.us.debian.org/debian/pool/main/libx/libxrender/libxrender-dev_0.9.10-1_armhf.deb      \
    http://ftp.us.debian.org/debian/pool/main/libx/libxt/libxt-dev_1.1.5-1_armhf.deb                 \
    http://ftp.us.debian.org/debian/pool/main/libx/libxtst/libxtst6_1.2.3-1_armhf.deb                \
    http://ftp.us.debian.org/debian/pool/main/libx/libxtst/libxtst-dev_1.2.3-1_armhf.deb             \
    http://ftp.us.debian.org/debian/pool/main/x/x11proto-core/x11proto-core-dev_7.0.31-1_all.deb     \
    http://ftp.us.debian.org/debian/pool/main/x/x11proto-input/x11proto-input-dev_2.3.2-1_all.deb    \
    http://ftp.us.debian.org/debian/pool/main/x/x11proto-kb/x11proto-kb-dev_1.0.7-1_all.deb          \
    http://ftp.us.debian.org/debian/pool/main/x/x11proto-render/x11proto-render-dev_0.11.1-2_all.deb \
    http://ftp.us.debian.org/debian/pool/main/x/x11proto-xext/x11proto-xext-dev_7.3.0-1_all.deb      \
    http://ftp.us.debian.org/debian/pool/main/z/zlib/zlib1g_1.2.8.dfsg-5_armhf.deb                   \
    /root/debs/

# unpack debs into toolchain libc dir
RUN cd /root/debs \
  && for f in *.deb; do dpkg-deb -x $f .; done \
  && cp -r usr/include/* ../gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/arm-linux-gnueabihf/libc/usr/include/ \
  && cp -r usr/lib/arm-linux-gnueabihf/* ../gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/arm-linux-gnueabihf/libc/usr/lib/ \
  && cp lib/arm-linux-gnueabihf/* ../gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/arm-linux-gnueabihf/libc/lib/ \
  && rm -rf /root/debs

# Env vars set here will be visibile in the running container, so can be
# used to convey configuration information to the build scripts.

# flag to indicate a docker hosted cross compilation targetting arm
ENV OPENJ9_DOCKER_CC_ARM 1

# Directory containing the cross compilation tool chain
ENV OPENJ9_CC_DIR /root/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf

# Prefix for the cross compilation tools (without the trailing '-')
ENV OPENJ9_CC_PREFIX arm-linux-gnueabihf

# Include directory for target
ENV OPENJ9_CC_INCLUDE_DIR ${OJ_CC_DIR}/arm-linux-gnueabihf/libc/usr/include

WORKDIR /root
