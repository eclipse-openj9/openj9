# Copyright (c) 2018, 2018 IBM Corp. and others
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

FROM centos:centos6.9

# Install required OS tools

ENV USER="jenkins"

RUN yum -y install \
    JSON \
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
    gettext \
    glibc \
    glibc-common \
    glibc-devel \
    gmp-devel \
    java-1.7.0-openjdk-devel \
    java-1.8.0-openjdk-devel \
    lbzip2 \
    libdwarf \
    libX11-devel \
    libXext-devel \
    libXrender-devel \
    libXt-devel \
    libXtst-devel \
    make \
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
    xz \
    zip \
    https://$(rpm -E '%{?centos:centos}%{!?centos:rhel}%{rhel}').iuscommunity.org/ius-release.rpm \
    libdwarf-devel \
  && rm -rf /var/lib/apt/lists/*

# Install autoconf version 2.69
RUN cd /usr/src/ \
  && wget https://fossies.org/linux/misc/autoconf-2.69.tar.gz \
  && tar -xzf autoconf-2.69.tar.gz \
  && rm -f autoconf-2.69.tar.gz \
  && cd autoconf-2.69 \
  && ./configure \
  && make \
  && make install

# Dependency required by test framework
RUN /usr/bin/perl -MCPAN -e'install Text::CSV' \
  && /usr/bin/perl -MCPAN -e'install JSON'

# install GCC-4.8
RUN cd /etc/yum.repos.d \
  && wget http://people.centos.org/tru/devtools-2/devtools-2.repo -O /etc/yum.repos.d/devtools-2.repo \
  && yum -y install devtoolset-2-gcc devtoolset-2-binutils \
  && yum -y install devtoolset-2-gcc-c++ devtoolset-2-gcc-gfortran \
  && scl enable devtoolset-2 bash \
  && mv /opt/rh/devtoolset-2/root/usr/bin/gcc /opt/rh/devtoolset-2/root/usr/bin/gcc-4.8 \
  && mv /opt/rh/devtoolset-2/root/usr/bin/g++ /opt/rh/devtoolset-2/root/usr/bin/g++-4.8 \
  && ln -s /opt/rh/devtoolset-2/root/usr/bin/gcc-4.8 /usr/bin/gcc \
  && ln -s /opt/rh/devtoolset-2/root/usr/bin/g++-4.8 /usr/bin/g++ \
  && ln -s /opt/rh/devtoolset-2/root/usr/bin/gcc-4.8 /usr/bin/cc

#Building and setting up git version 2.5.3
RUN yum -y install curl-devel expat-devel gettext-devel openssl-devel zlib-devel perl-ExtUtils-MakeMaker \
  && cd /usr/src \
  && wget https://www.kernel.org/pub/software/scm/git/git-2.5.3.tar.gz \
  && tar xzf git-2.5.3.tar.gz \
  && rm git-2.5.3.tar.gz \
  && cd git-2.5.3 \
  && make prefix=/usr/local/git all \
  && make prefix=/usr/local/git install \
  && ln -s /usr/local/git/bin/git /usr/bin/git

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
  && /usr/src/cuda_7.5.18_linux.run --silent --toolkit --override

# add user home/jenkins
RUN useradd -ms /bin/bash ${USER} \
  && mkdir /home/${USER}/.ssh/
COPY authorized_keys /home/${USER}/.ssh/authorized_keys
COPY known_hosts /home/${USER}/.ssh/known_hosts
RUN chown -R ${USER}:${USER} /home/${USER} \
  && chmod 644 /home/${USER}/.ssh/authorized_keys

# set up sshd config
RUN mkdir /var/run/sshd \
  && sed -i 's/#PermitRootLogin/PermitRootLogin/' /etc/ssh/sshd_config \
  && sed -i 's/#RSAAuthentication.*/RSAAuthentication yes/' /etc/ssh/sshd_config \
  && sed -i 's/#PubkeyAuthentication.*/PubkeyAuthentication yes/' /etc/ssh/sshd_config

# SSH login fix. Otherwise user is kicked off after login
RUN sed 's@session\s*required\s*pam_loginuid.so@session optional pam_loginuid.so@g' -i /etc/pam.d/sshd

# Setup boot JDK for building Java 8
RUN ln -s java-1.7.0 /usr/lib/jvm/java-7-openjdk-amd64

# Setup boot JDK for building Java 9
RUN mkdir /usr/lib/jvm/adoptojdk-java-80 \
  && cd /usr/lib/jvm/adoptojdk-java-80 \
  && wget -O bootjdk8.tar.gz "https://api.adoptopenjdk.net/v2/binary/releases/openjdk8?openjdk_impl=openj9&os=linux&arch=x64&release=latest&type=jdk" \
  && tar -xzf bootjdk8.tar.gz \
  && rm -f bootjdk8.tar.gz \
  && mv $(ls | grep -i jdk) bootjdk8 \
  && mv bootjdk8/* /usr/lib/jvm/adoptojdk-java-80

# Setup boot JDK for building Java 10
RUN mkdir /usr/lib/jvm/adoptojdk-java-90 \
  && cd /usr/lib/jvm/adoptojdk-java-90 \
  && wget -O bootjdk9.tar.gz "https://api.adoptopenjdk.net/v2/binary/releases/openjdk9?openjdk_impl=openj9&os=linux&arch=x64&release=latest&type=jdk" \
  && tar -xzf bootjdk9.tar.gz \
  && rm -f bootjdk9.tar.gz \
  && mv $(ls | grep -i jdk) bootjdk9 \
  && mv bootjdk9/* /usr/lib/jvm/adoptojdk-java-90

# Setup boot JDK for building Java 11
RUN mkdir /usr/lib/jvm/adoptojdk-java-10 \
  && cd /usr/lib/jvm/adoptojdk-java-10 \
  && wget -O bootjdk10.tar.gz "https://api.adoptopenjdk.net/v2/binary/releases/openjdk10?openjdk_impl=openj9&os=linux&arch=x64&release=latest&type=jdk" \
  && tar -xzf bootjdk10.tar.gz \
  && rm -f bootjdk10.tar.gz \
  && mv $(ls | grep -i jdk) bootjdk10 \
  && mv bootjdk10/* /usr/lib/jvm/adoptojdk-java-10

# Install Freemaker for building OpenJ9. Used in bash ./configure --with-freemarker-jar=<path-to-freemaker-jar>
RUN cd /home/${USER} \
  && wget https://sourceforge.net/projects/freemarker/files/freemarker/2.3.8/freemarker-2.3.8.tar.gz/download -O freemarker.tgz \
  && tar -xzf freemarker.tgz freemarker-2.3.8/lib/freemarker.jar --strip=2 \
  && rm -f freemarker.tgz

# Install Curl version 7.24.0.
RUN cd /usr/src \
  && wget http://curl.haxx.se/download/curl-7.24.0.tar.bz2 \
  && tar xfj curl-7.24.0.tar.bz2 \
  && rm -f curl-7.24.0.tar.bz2 \
  && cd curl-7.24.0 \
  && ./configure --prefix=/usr \
  && make \
  && make install 

# Setup a reference repository cache for faster clones in the container
RUN mkdir /home/${USER}/openjdk_cache \
  && cd /home/${USER}/openjdk_cache \
  && git init --bare \
  && git remote add jdk8 https://github.com/ibmruntimes/openj9-openjdk-jdk8.git \
  && git remote add jdk9 https://github.com/ibmruntimes/openj9-openjdk-jdk9.git \
  && git remote add jdk10 https://github.com/ibmruntimes/openj9-openjdk-jdk10.git \
  && git remote add jdk11 https://github.com/ibmruntimes/openj9-openjdk-jdk11.git \
  && git remote add jdk https://github.com/ibmruntimes/openj9-openjdk-jdk.git \
  && git remote add openj9 https://github.com/eclipse/openj9.git \
  && git remote add omr https://github.com/eclipse/openj9-omr.git \
  && git fetch --all

# Expose SSH port and run SSHD
EXPOSE 22

# Enable SSH daemon program to provide secure encrypted communications
RUN mkdir -p /var/run/sshd \
  && service sshd start \
  && service sshd stop

CMD ["/usr/sbin/sshd","-D"]
