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
# Make sure you are in the directory containing the Dockerfile, authorized_keys file, and known_hosts file
# Then run:
#   docker build -t openj9 -f Dockerfile .
#   docker run -it openj9

FROM ubuntu:18.04

# Install required OS tools

ENV USER="jenkins"

RUN apt-get update \
  && apt-get install -qq -y --no-install-recommends \
    software-properties-common \
  && add-apt-repository ppa:ubuntu-toolchain-r/test \
  && apt-get update \
  && apt-get install -qq -y --no-install-recommends \
    ant \
    ant-contrib \
    autoconf \
    build-essential \
    curl \
    libexpat1-dev `# This is being used for the xml parser` \
    g++-7 \
    gcc-7 \
    gdb \
    git \
    openssh-client \
    openssh-server \
    perl \
    ssh \
    wget \
    xvfb \
  && rm -rf /var/lib/apt/lists/*

# Install Docker module to run test framework
RUN echo yes | cpan install JSON Text::CSV XML::Parser

# Add user home/USER and copy authorized_keys and known_hosts
RUN useradd -ms /bin/bash ${USER} \
  && mkdir /home/${USER}/.ssh/
COPY authorized_keys /home/${USER}/.ssh/authorized_keys
COPY known_hosts /home/${USER}/.ssh/known_hosts
RUN chown -R ${USER}:${USER} /home/${USER} \
  && chmod 644 /home/${USER}/.ssh/authorized_keys \
  && chmod 644 /home/${USER}/.ssh/known_hosts \
  && chmod 700 /home/${USER}/.ssh

# Set up sshd config
RUN mkdir /var/run/sshd \
  && sed -i 's/#PermitRootLogin/PermitRootLogin/' /etc/ssh/sshd_config \
  && sed -i 's/#RSAAuthentication.*/RSAAuthentication yes/' /etc/ssh/sshd_config \
  && sed -i 's/#PubkeyAuthentication.*/PubkeyAuthentication yes/' /etc/ssh/sshd_config

# SSH login fix. Otherwise user is kicked off after login
RUN sed 's@session\s*required\s*pam_loginuid.so@session optional pam_loginuid.so@g' -i /etc/pam.d/sshd

# Install OpenSSL v1.1.1b
# Required for JITaaS & Crypto functional testing
RUN cd /tmp \
  && wget https://github.com/openssl/openssl/archive/OpenSSL_1_1_1b.tar.gz \
  && tar -xzf OpenSSL_1_1_1b.tar.gz \
  && rm -f OpenSSL_1_1_1b.tar.gz \
  && cd /tmp/openssl-OpenSSL_1_1_1b \
  && ./config --prefix=/usr/local/openssl-1.1.1b --openssldir=/usr/local/openssl-1.1.1b \
  && make \
  && make install \
  && cd .. \
  && rm -rf openssl-OpenSSL_1_1_1b \
  && echo "/usr/local/openssl-1.1.1b/lib" > /etc/ld.so.conf.d/openssl-1.1.1b.conf \
  && echo "PATH=/usr/local/openssl-1.1.1b/bin:$PATH" > /etc/environment

# Install Protobuf v3.5.1
# Required for JITaaS
RUN cd /tmp \
  && wget https://github.com/protocolbuffers/protobuf/releases/download/v3.5.1/protobuf-cpp-3.5.1.tar.gz \
  && tar -xzf protobuf-cpp-3.5.1.tar.gz \
  && rm -f protobuf-cpp-3.5.1.tar.gz \
  && cd /tmp/protobuf-3.5.1 \
  && ./configure \
  && make \
  && make install \
  && cd .. \
  && rm -rf protobuf-3.5.1

# Run ldconfig to create necessary links and cache to shared libraries
RUN echo "/usr/local/lib" > /etc/ld.so.conf.d/usr-local.conf \
  && echo "/usr/local/lib64" >> /etc/ld.so.conf.d/usr-local.conf \
  && ldconfig

# Expose SSH port and run SSH
EXPOSE 22
