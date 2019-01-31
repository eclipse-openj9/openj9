# Copyright (c) 2018, 2019 IBM Corp. and others
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
# First copy your public ssh key into a file named authorized_keys next to the Dockerfile
# Then include a known_hosts file next to the Dockerfile, with github as a saved host
# This can be done with "ssh-keyscan github.com >> path_to_dockerfile/known_hosts"
# Make sure you are in the directory contianing the Dockerfile, authorized_keys file, and known_hosts file
# Then run:
#   docker build -t openj9 -f Dockerfile .
#   docker run -it openj9

FROM ubuntu:16.04

# Install required OS tools

ENV USER="jenkins"

RUN apt-get update \
  && apt-get install -qq -y --no-install-recommends \
    software-properties-common \
    python-software-properties \
  && add-apt-repository ppa:ubuntu-toolchain-r/test \
  && apt-get update \
  && apt-get install -qq -y --no-install-recommends \
    ant \
    ant-contrib \
    autoconf \
    build-essential \
    ca-certificates \
    cmake \
    cpio \
    curl \
    file \
    g++-4.8 \
    g++-7 \
    gcc-4.8 \
    gcc-7 \
    gdb \
    git \
    git-core \
    libasound2-dev \
    libcups2-dev \
    libdwarf-dev \
    libelf-dev \
    libfontconfig \
    libfontconfig1-dev \
    libfreetype6-dev \
    libx11-dev \
    libxext-dev \
    libxrandr-dev \
    libxrender-dev \
    libxt-dev \
    libxtst-dev \
    make \
    openjdk-8-jdk \
    openssh-client \
    openssh-server \
    perl \
    pkg-config \
    realpath \
    ssh \
    unzip \
    wget \
    xvfb \
    zip \
  && rm -rf /var/lib/apt/lists/*

# Install Docker module to run test framework
RUN echo yes | cpan install JSON Text::CSV

# Create links for c++,g++,cc,gcc
RUN rm /usr/bin/c++ /usr/bin/g++ /usr/bin/cc /usr/bin/gcc \
  && ln -s g++ /usr/bin/c++ \
  && ln -s g++-4.8 /usr/bin/g++ \
  && ln -s gcc /usr/bin/cc \
  && ln -s gcc-4.8 /usr/bin/gcc

# Add user home/USER and copy authorized_keys and known_hosts
RUN useradd -ms /bin/bash ${USER} \
  && mkdir /home/${USER}/.ssh/
COPY authorized_keys /home/${USER}/.ssh/authorized_keys
COPY known_hosts /home/${USER}/.ssh/known_hosts
RUN chown -R ${USER}:${USER} /home/${USER} \
  && chmod 644 /home/${USER}/.ssh/authorized_keys \
  && chmod 644 /home/${USER}/.ssh/known_hosts \
  && chmod 700 /home/${USER}/.ssh

# Download and setup freemarker.jar to /home/USER/freemarker.jar
RUN cd /home/${USER} \
  && wget https://sourceforge.net/projects/freemarker/files/freemarker/2.3.8/freemarker-2.3.8.tar.gz/download -O freemarker.tgz \
  && tar -xzf freemarker.tgz freemarker-2.3.8/lib/freemarker.jar --strip=2 \
  && rm -f freemarker.tgz

# Download and install boot JDK from AdoptOpenJDK for java 8
RUN mkdir -p /usr/lib/jvm/adoptojdk-java-80 \
  && cd /usr/lib/jvm/adoptojdk-java-80 \
  && wget -O bootjdk8.tar.gz "https://api.adoptopenjdk.net/v2/binary/releases/openjdk8?openjdk_impl=openj9&os=linux&arch=s390x&release=latest&type=jdk" \
  && tar -xzf bootjdk8.tar.gz \
  && rm -f bootjdk8.tar.gz \
  && mv $(ls | grep -i jdk8) bootjdk8 \
  && mv bootjdk8/* /usr/lib/jvm/adoptojdk-java-80 \
  && rm -rf bootjdk8

# Download and install boot JDK from AdoptOpenJDK for java 11 and 12
RUN mkdir -p /usr/lib/jvm/adoptojdk-java-11 \
  && cd /usr/lib/jvm/adoptojdk-java-11 \
  && wget -O bootjdk11.tar.gz "https://api.adoptopenjdk.net/v2/binary/releases/openjdk11?openjdk_impl=openj9&os=linux&arch=s390x&release=latest&type=jdk" \
  && tar -xzf bootjdk11.tar.gz \
  && rm -f bootjdk11.tar.gz \
  && mv $(ls | grep -i jdk-11) bootjdk11 \
  && mv bootjdk11/* /usr/lib/jvm/adoptojdk-java-11 \
  && rm -rf bootjdk11

# Set up sshd config
RUN mkdir /var/run/sshd \
  && sed -i 's/#PermitRootLogin/PermitRootLogin/' /etc/ssh/sshd_config \
  && sed -i 's/#RSAAuthentication.*/RSAAuthentication yes/' /etc/ssh/sshd_config \
  && sed -i 's/#PubkeyAuthentication.*/PubkeyAuthentication yes/' /etc/ssh/sshd_config

# SSH login fix. Otherwise user is kicked off after login
RUN sed 's@session\s*required\s*pam_loginuid.so@session optional pam_loginuid.so@g' -i /etc/pam.d/sshd

# Setup a reference repository cache for faster clones in the container
RUN mkdir /home/${USER}/openjdk_cache \
  && cd /home/${USER}/openjdk_cache \
  && git init --bare \
  && git remote add jdk8 https://github.com/ibmruntimes/openj9-openjdk-jdk8.git \
  && git remote add jdk11 https://github.com/ibmruntimes/openj9-openjdk-jdk11.git \
  && git remote add jdk https://github.com/ibmruntimes/openj9-openjdk-jdk.git \
  && git remote add openj9 https://github.com/eclipse/openj9.git \
  && git remote add omr https://github.com/eclipse/openj9-omr.git \
  && git fetch --all

# Expose SSH port and run SSH
EXPOSE 22
