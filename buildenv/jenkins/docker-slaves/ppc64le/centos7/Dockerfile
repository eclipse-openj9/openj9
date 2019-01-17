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

FROM ppc64le/centos:centos7

# Install required OS tools

ENV USER="jenkins"

RUN yum -y update \
  && yum -y install \
    alsa-lib-devel \
    bind-utils \
    bison \
    bzip2 \
    ca-certificates \
    cups-devel \
    cmake \
    cpio \
    elfutils-libelf-devel \
    file-devel \
    file-libs \
    flex \
    fontconfig \
    fontconfig-devel \
    freetype-devel \
    gcc-4.8.5 \
    gcc-c++-4.8.5 \
    gettext \
    glibc \
    glibc-common \
    glibc-devel \
    gmp-devel \
    lbzip2 \
    libdwarf \
    libstdc++-static \
    libX11-devel \
    libXext-devel \
    libXi-devel \
    libXrandr-devel \
    libXrender-devel \
    libXt-devel \
    libXtst-devel \
    mesa-libGL-devel \
    mpfr-devel \
    numactl-devel \
    ntp \
    openssl-devel \
    openssh-server \
    openssh-clients \
    perl-CPAN \
    perl-DBI \
    perl-devel \
    perl-GD \
    perl-libwww-perl \
    perl-Time-HiRes \
    unzip \
    vim \
    wget\
    xorg-x11-server-Xvfb \
    xz \
    zip \
    libdwarf-devel \
  && yum clean all

# Install autoconf version 2.69
RUN cd /usr/src/ \
  && wget https://fossies.org/linux/misc/autoconf-2.69.tar.gz \
  && tar -xzf autoconf-2.69.tar.gz \
  && rm -f autoconf-2.69.tar.gz \
  && cd autoconf-2.69 \
  && ./configure --build=$(rpm --eval %{_host}) \
  && make \
  && make install \
  && cd .. \
  && rm -rf /usr/src/autoconf-2.69

# Dependency required by test framework
RUN wget -O - http://cpanmin.us | perl - --self-upgrade \
  && cpanm Text::CSV \
  && cpanm JSON

# Setup links to GCC-4.8.5
RUN rm /usr/bin/c++ /usr/bin/cc \
  && ln -s g++ /usr/bin/c++ \
  && ln -s gcc /usr/bin/cc

# Install GCC-7.3.1
RUN cd /usr/local \
  && wget -O gcc-7.tar.xz "https://ci.adoptopenjdk.net/userContent/gcc/gcc730+ccache.ppc64le.tar.xz" \
  && tar -xJf gcc-7.tar.xz --strip-components=1 \
  && rm -rf gcc-7.tar.xz

#Building and setting up git version 2.5.3
RUN yum -y update \
  && yum -y install \
    curl-devel \
    expat-devel \
    gettext-devel \
    openssl-devel \
    zlib-devel \
    perl-ExtUtils-MakeMaker \
  && yum clean all \
  && cd /usr/src \
  && wget https://www.kernel.org/pub/software/scm/git/git-2.5.3.tar.gz \
  && tar xzf git-2.5.3.tar.gz \
  && rm git-2.5.3.tar.gz \
  && cd git-2.5.3 \
  && make prefix=/usr/local all \
  && make prefix=/usr/local install \
  && cd .. \
  && rm -rf /usr/src/git-2.5.3

# Install ant version 1.10.5.
RUN wget http://www.us.apache.org/dist/ant/binaries/apache-ant-1.10.5-bin.tar.gz \
  && tar xvfz apache-ant-1.10.5-bin.tar.gz -C /opt \
  && rm apache-ant-1.10.5-bin.tar.gz \
  && ln -s /opt/apache-ant-1.10.5 /opt/ant \
  && ln -s /opt/ant/bin/ant /usr/bin/ant \
  && cd /opt/ant/lib \
  && wget https://sourceforge.net/projects/ant-contrib/files/ant-contrib/1.0b3/ant-contrib-1.0b3-bin.tar.gz \
  && tar xfvz ant-contrib-1.0b3-bin.tar.gz -C .\
  && rm ant-contrib-1.0b3-bin.tar.gz \
  && cd ant-contrib \
  && mv ant-contrib-1.0b3.jar .. \
  && cd .. \
  && rm -rf ant-contrib

# Install Cuda version 7.5.18
RUN cd /usr/src/ \
  && wget http://developer.download.nvidia.com/compute/cuda/7.5/Prod/local_installers/cuda_7.5.18_linux.run \
  && chmod +x cuda_7.5.18_linux.run \
  && /usr/src/cuda_7.5.18_linux.run --silent --toolkit --override \
  && rm -f /usr/src/cuda_7.5.18_linux.run

# add user home/jenkins
RUN useradd -ms /bin/bash ${USER} \
  && mkdir /home/${USER}/.ssh/
COPY authorized_keys /home/${USER}/.ssh/authorized_keys
COPY known_hosts /home/${USER}/.ssh/known_hosts
RUN chown -R ${USER}:${USER} /home/${USER} \
  && chmod 644 /home/${USER}/.ssh/authorized_keys \
  && chmod 644 /home/${USER}/.ssh/known_hosts

# Install make 4.1 so can compile on java 8
RUN cd /tmp \
  && wget -O make-4.1.tar.gz "http://ftp.gnu.org/gnu/make/make-4.1.tar.gz" \
  && tar xvf make-4.1.tar.gz \
  && rm -f make-4.1.tar.gz \
  && cd make-4.1/ \
  && ./configure \
  && make \
  && make install \
  && rm -f /usr/local/bin/gmake \
  && ln -s /usr/local/bin/make /usr/local/bin/gmake \
  && cd .. \
  && rm -rf /tmp/make-4.1

# Setup boot JDK for building Java 8
RUN mkdir -p /usr/lib/jvm/adoptojdk-java-80 \
  && cd /usr/lib/jvm/adoptojdk-java-80 \
  && wget -O bootjdk8.tar.gz "https://api.adoptopenjdk.net/v2/binary/nightly/openjdk8?openjdk_impl=openj9&os=linux&arch=ppc64le&release=latest&type=jdk" \
  && tar -xzf bootjdk8.tar.gz \
  && rm -f bootjdk8.tar.gz \
  && mv $(ls | grep -i jdk8) bootjdk8 \
  && mv bootjdk8/* /usr/lib/jvm/adoptojdk-java-80 \
  && rm -rf bootjdk8

# Allow jenkins to find JDK8
RUN ln -s /usr/lib/jvm/adoptojdk-java-80/bin/java /usr/bin/java

# Setup boot JDK for building Java 11
RUN mkdir -p /usr/lib/jvm/adoptojdk-java-11 \
  && cd /usr/lib/jvm/adoptojdk-java-11 \
  && wget -O bootjdk11.tar.gz "https://github.com/AdoptOpenJDK/openjdk11-binaries/releases/download/jdk11u-2018-11-26-07-41/OpenJDK11U-jdk_ppc64le_linux_openj9_2018-11-26-07-41.tar.gz" \
  && tar -xzf bootjdk11.tar.gz \
  && rm -f bootjdk11.tar.gz \
  && mv $(ls | grep -i jdk-11) bootjdk11 \
  && mv bootjdk11/* /usr/lib/jvm/adoptojdk-java-11 \
  && rm -rf bootjdk11

# Install Freemaker for building OpenJ9. Used in bash ./configure --with-freemarker-jar=<path-to-freemaker-jar>
RUN cd /home/${USER} \
  && wget https://sourceforge.net/projects/freemarker/files/freemarker/2.3.8/freemarker-2.3.8.tar.gz/download -O freemarker.tgz \
  && tar -xzf freemarker.tgz freemarker-2.3.8/lib/freemarker.jar --strip=2 \
  && rm -f freemarker.tgz

# Install Curl version 7.29.0.
RUN cd /usr/src \
  && wget http://curl.haxx.se/download/curl-7.29.0.tar.bz2 \
  && tar xfj curl-7.29.0.tar.bz2 \
  && rm -f curl-7.29.0.tar.bz2 \
  && cd curl-7.29.0 \
  && ./configure --prefix=/usr/local --build=$(rpm --eval %{_host}) \
  && make \
  && make install \
  && cd .. \
  && rm -rf /usr/src/curl-7.29.0

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

# set up sshd config
RUN mkdir /var/run/sshd \
  && sed -i 's/#PermitRootLogin/PermitRootLogin/' /etc/ssh/sshd_config \
  && sed -i 's/#RSAAuthentication.*/RSAAuthentication yes/' /etc/ssh/sshd_config \
  && sed -i 's/#PubkeyAuthentication.*/PubkeyAuthentication yes/' /etc/ssh/sshd_config

# SSH login fix. Otherwise user is kicked off after login
RUN sed 's@session\s*required\s*pam_loginuid.so@session optional pam_loginuid.so@g' -i /etc/pam.d/sshd

# Generate ssh host keys
RUN /usr/bin/ssh-keygen -A

# Expose SSH port and run SSHD
EXPOSE 22
