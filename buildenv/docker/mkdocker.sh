#!/bin/bash -

print_license() {
cat <<- EOF
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
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
EOF
}

usage() {
  echo "Usage: $0 [OPTION]..."
  echo "Generate a Dockerfile for building OpenJ9"
  echo ""
  echo "Options:"
  echo "  --help|-h      print this help, then exit"
  echo "  --arch=...     specify the processor architecture (default: host architecture)"
  echo "  --build        build the docker image (overrides '--print')"
  echo "  --cuda         include CUDA header files"
  echo "  --dist=...     specify the Linux distribution (e.g. centos, ubuntu)"
  echo "  --print        write the Dockerfile to stdout (default; overrides '--build')"
  echo "  --tag=...      specify a name for the docker image (may be repeated, default: none)"
  echo "  --user=...     specify the user name (default: 'jenkins')"
  echo "  --version=...  specify the distribution version (e.g. 6.9, 16.04)"
  echo ""
  local arch="$(uname -m)"
  echo "Supported build patterns on this host ($arch):"
if [ $arch = x86_64 ] ; then
  echo "  bash mkdocker.sh --tag=openj9/cent69 --dist=centos --version=6.9 --build"
fi
if [ $arch = x86_64 -o $arch = ppc64le ] ; then
  echo "  bash mkdocker.sh --tag=openj9/cent7  --dist=centos --version=7   --build"
fi
  echo "  bash mkdocker.sh --tag=openj9/ub16   --dist=ubuntu --version=16  --build"
  echo "  bash mkdocker.sh --tag=openj9/ub18   --dist=ubuntu --version=18  --build"
  echo "  bash mkdocker.sh --tag=openj9/ub20   --dist=ubuntu --version=20  --build"
  exit 1
}

# Global configuration variables.
action=print
arch=
cuda=no
dist=unspecified
tags=()
user=jenkins
version=unspecified

# Frequently used commands.
wget_O="wget --progress=dot:mega -O"

parse_options() {
  for arg in "$@" ; do
    case "$arg" in
      -h | --help)
        usage
        ;;
      --arch=*)
        arch="${arg#*=}"
        ;;
      --build)
        action=build
        ;;
      --cuda)
        cuda=9.0
        ;;
      --dist=*)
        dist="${arg#*=}"
        ;;
      --print)
        action=print
        ;;
      --tag=*)
        tags+=("${arg#*=}")
        ;;
      --user=*)
        user="${arg#*=}"
        ;;
      --version=*)
        version="${arg#*=}"
        ;;
      *) # bad option
        echo "Unsupported option: '$arg'" >&2
        usage
        ;;
    esac
  done
}

validate_options() {
  if [ "x$arch" = x ] ; then
    arch="$(uname -m)"
  fi

  case "$arch" in
    ppc64le | s390x | x86_64)
      ;;
    *)
      echo "Unsupported architecture: '$arch'" >&2
      exit 1
      ;;
  esac

  # Validate the distribution and version.
  case "$dist" in
    centos)
      if [ $arch = s390x ] ; then
        echo "CentOS is not supported on $arch" >&2
        exit 1
      fi
      case $version in
        6.9)
          if [ $arch = ppc64le ] ; then
            echo "CentOS version 6.9 is not supported on $arch" >&2
            exit 1
          fi
          ;;
        7)
          ;;
        unspecified)
          echo "Unspecified CentOS version: use '--version' option" >&2
          exit 1
          ;;
        *)
          echo "Unsupported CentOS version: '$version'" >&2
          exit 1
          ;;
      esac
      ;;
    ubuntu)
      case $version in
        16 | 18 | 20)
          version=$version.04
          ;;
        16.04 | 18.04 | 20.04)
          ;;
        unspecified)
          echo "Unspecified Ubuntu version: use '--version' option" >&2
          exit 1
          ;;
        *)
          echo "Unsupported Ubuntu version: '$version'" >&2
          exit 1
          ;;
      esac
      ;;
    unspecified)
      echo "Unspecified distribution: use '--dist' option" >&2
      exit 1
      ;;
    *)
      echo "Unsupported distribution: '$dist'" >&2
      exit 1
      ;;
  esac

  # If CUDA is requested, validate the architecture.
  if [ $cuda != no ] ; then
    case "$arch" in
      ppc64le | x86_64)
        ;;
      *)
        echo "CUDA is not supported on architecture: '$arch'" >&2
        exit 1
        ;;
    esac
  fi
}

build_cmd() {
  local cmd=build
  local file=$1
  for tag in ${tags[@]} ; do
    cmd="$cmd --tag $tag"
  done
  echo $cmd --file $file .
}

preamble() {
  echo ""
  echo "# Generated by: mkdocker.sh $command_line"
  echo ""
  echo "# To use this docker file:"
  echo "# First, copy your public ssh key into authorized_keys."
  echo "# Then, in the directory containing the Dockerfile, authorized_keys"
  echo "# file, and known_hosts file, run:"
  echo "#   docker $(build_cmd Dockerfile)"
  echo "#"
if [ ${#tags[@]} -lt 2 ] ; then
  echo "# To start a container based on the resulting image, run this command:"
else
  echo "# To start a container based on the resulting image, run one of these commands:"
fi
if [ ${#tags[@]} -eq 0 ] ; then
  echo "#   docker run -it <image>"
else
for tag in ${tags[@]} ; do
  echo "#   docker run -it $tag"
done
fi
  echo ""
if [ $cuda != no ] ; then
  echo "FROM nvidia/cuda:${cuda}-devel-ubuntu16.04 AS cuda-dev"
  echo ""
fi
  echo "FROM $dist:$version"
}

install_centos_packages() {
  echo "RUN yum -y update \\"
if [ $version = 6.9 ] ; then
  echo " && yum -y install https://repo.ius.io/ius-release-el6.rpm \\"
fi
  echo " && yum -y install \\"
  echo "    alsa-lib-devel \\"
  echo "    automake \\" # required to update make
  echo "    bind-utils \\"
  echo "    bison \\"
  echo "    bzip2 \\"
  echo "    ca-certificates \\"
  echo "    cpio \\"
  echo "    cups-devel \\"
  echo "    curl-devel \\" # required by git
  echo "    elfutils-libelf-devel \\"
  echo "    expat-devel \\" # required by git and the xml parser
  echo "    file-devel \\"
  echo "    file-libs \\"
  echo "    flex \\"
  echo "    fontconfig \\"
  echo "    fontconfig-devel \\"
  echo "    freetype-devel \\"
  echo "    gettext \\"
  echo "    gettext-devel \\" # required by git
  echo "    glibc \\"
  echo "    glibc-common \\"
  echo "    glibc-devel \\"
  echo "    gmp-devel \\"
  echo "    lbzip2 \\"
  echo "    libdwarf \\"
  echo "    libdwarf-devel \\"
  echo "    libffi-devel \\"
  echo "    libstdc++-static \\"
  echo "    libX11-devel \\"
  echo "    libXext-devel \\"
  echo "    libXi-devel \\"
  echo "    libXrandr-devel \\"
  echo "    libXrender-devel \\"
  echo "    libXt-devel \\"
  echo "    libXtst-devel \\"
  echo "    make \\"
  echo "    mesa-libGL-devel \\"
  echo "    mpfr-devel \\"
  echo "    ntp \\"
  echo "    numactl-devel \\"
  echo "    openssh-clients \\"
  echo "    openssh-server \\"
  echo "    openssl-devel \\" # required by git, etc.
  echo "    perl-CPAN \\"
  echo "    perl-DBI \\"
  echo "    perl-devel \\"
  echo "    perl-ExtUtils-MakeMaker \\" # required by git
  echo "    perl-GD \\"
  echo "    perl-libwww-perl \\"
  echo "    perl-Time-HiRes \\"
  echo "    systemtap-devel \\"
  echo "    texinfo \\" # required to update make
  echo "    unzip \\"
  echo "    vim \\"
  echo "    wget \\"
  echo "    xorg-x11-server-Xvfb \\"
  echo "    xz \\"
  echo "    zip \\"
  echo "    zlib-devel \\" # required by git, python
  echo " && yum clean all"
  echo ""
  local autoconf_version=2.69
  echo "# Install autoconf."
  echo "RUN cd /tmp \\"
  echo " && $wget_O autoconf.tar.gz https://fossies.org/linux/misc/autoconf-$autoconf_version.tar.gz \\"
  echo " && tar -xzf autoconf.tar.gz \\"
  echo " && cd autoconf-$autoconf_version \\"
  echo " && ./configure --build=\$(rpm --eval %{_host}) \\"
  echo " && make \\"
  echo " && make install \\"
  echo " && cd .. \\"
  echo " && rm -rf autoconf.tar.gz autoconf-$autoconf_version"
  echo ""
  echo "# Install gcc-7.5."
  echo "RUN cd /usr/local \\"
  echo " && $wget_O gcc-7.tar.xz 'https://ci.adoptopenjdk.net/userContent/gcc/gcc750+ccache.$arch.tar.xz' \\"
  echo " && tar -xJf gcc-7.tar.xz --strip-components=1 \\"
  echo " && ln -sf ../local/bin/gcc-7.5 /usr/bin/gcc \\"
  echo " && ln -sf ../local/bin/g++-7.5 /usr/bin/g++ \\"
  echo " && ln -sf gcc /usr/bin/cc \\"
  echo " && ln -sf g++ /usr/bin/c++ \\"
  echo " && rm -f gcc-7.tar.xz"
  echo ""
  local ant_version=1.10.5
  echo "# Install ant."
  echo "RUN cd /tmp \\"
  echo " && $wget_O ant.zip https://archive.apache.org/dist/ant/binaries/apache-ant-$ant_version-bin.zip \\"
  echo " && unzip -q ant.zip -d /opt \\"
  echo " && ln -s apache-ant-$ant_version /opt/ant \\"
  echo " && ln -s /opt/ant/bin/ant /usr/bin/ant \\"
  echo " && $wget_O ant-contrib.tar.gz https://sourceforge.net/projects/ant-contrib/files/ant-contrib/1.0b3/ant-contrib-1.0b3-bin.tar.gz \\"
  echo " && tar -xzf ant-contrib.tar.gz \\"
  echo " && mv ant-contrib/ant-contrib-1.0b3.jar /opt/ant/lib \\"
  echo " && rm -rf ant-contrib ant.zip ant-contrib.tar.gz"
  echo ""
  local git_version=2.5.3
  echo "# Install git."
  echo "RUN cd /tmp \\"
  echo " && $wget_O git.tar.gz https://www.kernel.org/pub/software/scm/git/git-$git_version.tar.gz \\"
  echo " && tar -xzf git.tar.gz \\"
  echo " && cd git-$git_version \\"
  echo " && make prefix=/usr/local all \\"
  echo " && make prefix=/usr/local install \\"
  echo " && cd .. \\"
  echo " && rm -rf git.tar.gz git-$git_version"
  # updating make requires automake 1.15
  echo ""
  local automake_version=1.15
  echo "# Update automake."
  echo "RUN cd /tmp \\"
  echo " && $wget_O automake.tar.gz http://ftp.gnu.org/gnu/automake/automake-$automake_version.tar.gz \\"
  echo " && tar -xzf automake.tar.gz \\"
  echo " && cd automake-$automake_version \\"
  echo " && ./configure \\"
  echo " && make \\"
  echo " && make install \\"
  echo " && cd .. \\"
  echo " && rm -rf automake.tar.gz automake-$automake_version"
if [ $arch = x86_64 ] ; then
  echo ""
  local gettext_version=0.20.1
  echo "# Update gettext."
  echo "RUN cd /tmp \\"
  echo " && $wget_O gettext.tar.gz http://ftp.gnu.org/gnu/gettext/gettext-$gettext_version.tar.gz \\"
  echo " && tar -xzf gettext.tar.gz \\"
  echo " && cd gettext-$gettext_version \\"
  echo " && ./autogen.sh --skip-gnulib \\"
  echo " && ./configure --disable-nls \\"
  echo " && make \\"
  echo " && make install \\"
  echo " && cd .. \\"
  echo " && rm -rf gettext.tar.gz gettext-$gettext_version"
fi
  echo ""
if [ $arch = ppc64le ] ; then
  local make_version=4.2
else
  local make_version=4.1
fi
  echo "# Update make."
  echo "RUN cd /tmp \\"
  echo " && $wget_O make.tar.gz https://github.com/mirror/make/archive/$make_version.tar.gz \\"
  echo " && tar -xzf make.tar.gz \\"
  echo " && cd make-$make_version \\"
  echo " && ACLOCAL_PATH=/usr/share/aclocal autoreconf -i \\"
  echo " && ./configure \\"
  echo " && make update \\"
  echo " && make \\"
  echo " && make install \\"
  echo " && ln -s make /usr/local/bin/gmake \\"
  echo " && cd .. \\"
  echo " && rm -rf make.tar.gz make-$make_version"
if [ $arch = x86_64 ] ; then
  echo ""
  local nasm_version=2.13.03
  echo "# Install nasm."
  echo "RUN cd /tmp \\"
  echo " && $wget_O nasm.tar.gz https://www.nasm.us/pub/nasm/releasebuilds/$nasm_version/nasm-$nasm_version.tar.gz \\"
  echo " && tar -xzf nasm.tar.gz \\"
  echo " && cd nasm-$nasm_version \\"
  echo " && ./configure -prefix=/usr/local \\"
  echo " && make install \\"
  echo " && cd .. \\"
  echo " && rm -rf nasm.tar.gz nasm-$nasm_version"
fi
}

install_ubuntu_packages() {
  echo "RUN apt-get update \\"
  echo " && apt-get install -qq -y --no-install-recommends \\"
  echo "    software-properties-common \\"
if [ $version = 16.04 ] ; then
  echo "    python-software-properties \\"
fi
  echo " && add-apt-repository ppa:ubuntu-toolchain-r/test \\"
  echo " && apt-get update \\"
  echo " && apt-get install -qq -y --no-install-recommends \\"
  echo "    ant \\"
  echo "    ant-contrib \\"
  echo "    autoconf \\"
  echo "    build-essential \\"
  echo "    ca-certificates \\"
  echo "    cmake \\"
  echo "    cpio \\"
  echo "    curl \\"
  echo "    file \\"
if [ $arch != s390x ] ; then
  echo "    g++-7 \\"
  echo "    gcc-7 \\"
fi
  echo "    gdb \\"
  echo "    git \\"
  echo "    libasound2-dev \\"
  echo "    libcups2-dev \\"
  echo "    libdwarf-dev \\"
  echo "    libelf-dev \\"
  echo "    libexpat1-dev \\" # required by the xml parser
  echo "    libffi-dev \\"
  echo "    libfontconfig \\"
  echo "    libfontconfig1-dev \\"
  echo "    libfreetype6-dev \\"
if [ $arch != s390x ] ; then
  echo "    libnuma-dev \\"
fi
if [ $arch = s390x ] ; then
  echo "    libmpc3 \\"
fi
  echo "    libssl-dev \\"
  echo "    libx11-dev \\"
  echo "    libxext-dev \\"
  echo "    libxrandr-dev \\"
  echo "    libxrender-dev \\"
  echo "    libxt-dev \\"
  echo "    libxtst-dev \\"
  echo "    make \\"
if [ $arch = x86_64 ] ; then
  echo "    nasm \\"
fi
  echo "    openssh-client \\"
  echo "    openssh-server \\"
  echo "    perl \\"
  echo "    pkg-config \\"
if [ $version = 16.04 ] ; then
  echo "    realpath \\"
fi
  echo "    ssh \\"
  echo "    systemtap-sdt-dev \\"
  echo "    unzip \\"
  echo "    wget \\"
  echo "    xvfb \\"
  echo "    zip \\"
  echo "    zlib1g-dev \\"
  echo " && rm -rf /var/lib/apt/lists/*"
}

install_packages() {
  echo ""
  echo "# Install required OS tools."
  echo ""
if [ $dist = centos ] ; then
  install_centos_packages
else
  install_ubuntu_packages
fi
}

install_compilers() {
if [ $arch = s390x ] ; then
  echo ""
  local gcc_version=7.5
  echo "# Install gcc."
  echo "RUN cd /usr/local \\"
  echo " && $wget_O gcc.tar.xz 'https://ci.adoptopenjdk.net/userContent/gcc/gcc750+ccache.$arch.tar.xz' \\"
  echo " && tar -xJf gcc.tar.xz --strip-components=1 \\"
  echo " && rm -f gcc.tar.xz"
  echo ""
  echo "# Create various symbolic links."
  echo "RUN ln -s lib/$arch-linux-gnu /usr/lib64 \\"
if [ $dist:$version = ubuntu:18.04 ] ; then
  # On s390x perl needs version 4 of mpfr, but we only have version 6.
  echo " && ln -s libmpfr.so.6 /usr/lib64/libmpfr.so.4 \\"
fi
  # /usr/local/include/c++ is a directory that already exists so we create these symbolic links in two steps.
  echo " && ( cd /usr/local/include && for f in \$(ls /usr/include/s390x-linux-gnu/c++) ; do test -e \$f || ln -s /usr/include/s390x-linux-gnu/c++/\$f . ; done ) \\"
  echo " && ln -s \$(ls -d /usr/include/s390x-linux-gnu/* | grep -v '/c++\$') /usr/local/include \\"
  echo " && ln -sf ../local/bin/g++-$gcc_version /usr/bin/g++-7 \\"
  echo " && ln -sf ../local/bin/gcc-$gcc_version /usr/bin/gcc-7"
fi
if [ $dist != centos ] ; then
  echo ""
  echo "ENV CC=gcc-7 CXX=g++-7"
fi
}

install_cmake() {
if [ $dist == centos ] ; then
  echo ""
  local cmake_version=3.11
  local cmake_name=cmake-$cmake_version.4
  echo "# Install cmake."
  echo "RUN cd /tmp \\"
  echo " && $wget_O cmake.tar.gz https://cmake.org/files/v$cmake_version/$cmake_name.tar.gz \\"
  echo " && tar -xzf cmake.tar.gz \\"
  echo " && cd $cmake_name \\"
  echo " && export LDFLAGS='-static-libstdc++' \\"
  echo " && ./configure \\"
  echo " && make \\"
  echo " && make install \\"
  echo " && cd .. \\"
  echo " && rm -rf cmake.tar.gz $cmake_name"
fi
}

prepare_user() {
  # Ensure authorized_keys exists.
  test -f authorized_keys || touch authorized_keys
  # Ensure github.com is a known host.
  ( test -f known_hosts && grep -q '^github.com ' known_hosts ) \
    || ssh-keyscan github.com >> known_hosts
}

create_user() {
  echo ""
  echo "# Add user home and copy authorized_keys and known_hosts."
  echo "RUN useradd -ms /bin/bash $user \\"
  echo " && mkdir /home/$user/.ssh \\"
  echo " && chmod 700 /home/$user/.ssh"
  echo "COPY authorized_keys known_hosts /home/$user/.ssh/"
  echo "RUN chown -R $user:$user /home/$user \\"
  echo " && chmod 600 /home/$user/.ssh/authorized_keys /home/$user/.ssh/known_hosts"
}

install_freemarker() {
  echo ""
  local freemarker_version=2.3.8
  echo "# Download and extract freemarker.jar to /root/freemarker.jar."
  echo "RUN mkdir -p /root \\"
  echo " && cd /root \\"
  echo " && $wget_O freemarker.tar.gz https://sourceforge.net/projects/freemarker/files/freemarker/$freemarker_version/freemarker-$freemarker_version.tar.gz/download \\"
  echo " && tar -xzf freemarker.tar.gz freemarker-$freemarker_version/lib/freemarker.jar --strip=2 \\"
  echo " && rm -f freemarker.tar.gz"
}

bootjdk_dirs() {
  for version in $@ ; do
    if [ $version = 8 ] ; then
      echo /usr/lib/jvm/adoptojdk-java-80
    else
      echo /usr/lib/jvm/adoptojdk-java-$version
    fi
  done
}

bootjdk_url() {
  local version=$1
  if [ $arch = x86_64 ] ; then
    echo https://api.adoptopenjdk.net/v3/binary/latest/$version/ga/linux/x64/jdk/openj9/normal/adoptopenjdk
  else
    echo https://api.adoptopenjdk.net/v3/binary/latest/$version/ga/linux/$arch/jdk/openj9/normal/adoptopenjdk
  fi
}

install_bootjdks() {
  local versions="8 11 14"
  echo ""
  echo "# Download and install boot JDKs from AdoptOpenJDK."
  echo "RUN cd /tmp \\"
for version in $versions ; do
  echo " && $wget_O jdk$version.tar.gz $(bootjdk_url $version) \\"
done
  echo " && mkdir -p" $(bootjdk_dirs $versions) "\\"
for version in $versions ; do
  echo " && tar -xzf jdk$version.tar.gz --directory=$(bootjdk_dirs $version)/ --strip=1 \\"
done
  echo " && rm -f jdk*.tar.gz"
}

install_cuda() {
  echo ""
  echo "# Copy header files necessary to build a VM with CUDA support."
  echo "RUN mkdir -p /usr/local/cuda-${cuda}/nvvm"
  echo "COPY --from=cuda-dev /usr/local/cuda-${cuda}/include      /usr/local/cuda-${cuda}/include"
  echo "COPY --from=cuda-dev /usr/local/cuda-${cuda}/nvvm/include /usr/local/cuda-${cuda}/nvvm/include"
  echo "ENV CUDA_HOME=/usr/local/cuda-${cuda}"
}

install_python() {
  local python_version=3.7.3
  echo ""
  echo "# Install python."
  echo "RUN cd /tmp \\"
  echo " && $wget_O python.tar.xz https://www.python.org/ftp/python/$python_version/Python-$python_version.tar.xz \\"
  echo " && tar -xJf python.tar.xz \\"
  echo " && cd Python-$python_version \\"
  echo " && ./configure --prefix=/usr/local \\"
  echo " && make \\"
  echo " && make install \\"
  echo " && cd .. \\"
  echo " && rm -rf python.tar.xz Python-$python_version"
}

adjust_ldconfig() {
  echo ""
  echo "# Run ldconfig to discover newly installed shared libraries."
  echo "RUN for dir in lib lib64 ; do echo /usr/local/$dir ; done > /etc/ld.so.conf.d/usr-local.conf \\"
  echo " && ldconfig"
}

# called as: add_git_remote remote url
add_git_remote() {
  echo " && git remote add ${1} ${2} \\"
  # Like-named (and conflicting) tags exist in multiple repositories: don't fetch any tags.
  echo " && git config remote.${1}.tagopt --no-tags \\"
}

create_git_cache() {
  local git_cache_dir=/home/$user/openjdk_cache
  # The jdk15 remote is fetched first because that repository was subjected to
  # 'git gc --aggressive --prune=all' before it was first published making it much
  # smaller than some other jdk repositories. There is a large degree of overlap
  # among the jdk repositories so a relatively small number of commits must be
  # fetched from the others.
  echo ""
  echo "# Setup a reference repository cache for faster clones in containers."
  echo "RUN mkdir $git_cache_dir \\"
  echo " && cd $git_cache_dir \\"
  echo " && git init --bare \\"
  add_git_remote jdk8    https://github.com/ibmruntimes/openj9-openjdk-jdk8.git
  add_git_remote jdk11   https://github.com/ibmruntimes/openj9-openjdk-jdk11.git
  add_git_remote jdk15   https://github.com/ibmruntimes/openj9-openjdk-jdk15.git
  add_git_remote jdknext https://github.com/ibmruntimes/openj9-openjdk-jdk.git
  add_git_remote omr     https://github.com/eclipse/openj9-omr.git
  add_git_remote openj9  https://github.com/eclipse/openj9.git
  echo " && echo Fetching repository cache... \\"
  echo " && git fetch jdk15 \\"
  echo " && git fetch --all \\"
  echo " && echo Shrinking repository cache... \\"
  echo " && git gc --aggressive --prune=all"
}

configure_ssh() {
  echo ""
  echo "# Configure sshd."
  echo "RUN mkdir /var/run/sshd \\"
  echo " && sed -i -e 's|#PermitRootLogin|PermitRootLogin|' \\"
  echo "           -e 's|#RSAAuthentication.*|RSAAuthentication yes|' \\"
  echo "           -e 's|#PubkeyAuthentication.*|PubkeyAuthentication yes|' /etc/ssh/sshd_config"
  echo ""
  echo "# SSH login fix so user is not kicked off after login."
  echo "RUN sed -i -e 's|session\s*required\s*pam_loginuid.so|session optional pam_loginuid.so|' /etc/pam.d/sshd"
  echo ""
  echo "# Expose SSH port."
  echo "EXPOSE 22"
}

print_dockerfile() {
  print_license
  preamble
  install_packages
  create_user
if [ $cuda != no ] ; then
  install_cuda
fi
  install_freemarker

  install_compilers

  install_cmake
  install_python

  adjust_ldconfig
  configure_ssh

  install_bootjdks
  create_git_cache
}

main() {
  if [ $action = build ] ; then
    prepare_user
    print_dockerfile | docker $(build_cmd -)
  else
    print_dockerfile
  fi
}

command_line="$*"
parse_options "$@"
validate_options
main
