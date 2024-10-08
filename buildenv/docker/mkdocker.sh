#!/bin/bash -

print_license() {
cat <<- EOF
# Copyright IBM Corp. and others 2019
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
# [2] https://openjdk.org/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
EOF
}

usage() {
  echo "Usage: $0 [OPTION]..."
  echo "Generate a Dockerfile for building OpenJ9"
  echo ""
  echo "Options:"
  echo "  --help|-h             print this help, then exit"
  echo "  --arch=...            specify the processor architecture (default: host architecture)"
  echo "  --build[=engine]      build the image (overrides '--print'). Optionally specify the engine (default: docker, or podman, if found)"
  echo "  --criu                include CRIU"
  echo "  --cuda                include CUDA header files"
  echo "  --dist=...            specify the Linux distribution (e.g. centos, ubuntu)"
  echo "  --freemarker          include freemarker.jar"
  echo "  --gitcache={yes|no}   generate the git cache (default: yes)"
  echo "  --jdk=...             specify which JDKs can be built (default: all)"
  echo "  --print               write the Dockerfile to stdout (default; overrides '--build')"
  echo "  --tag=...             specify a name for the docker image (may be repeated, default: none)"
  echo "  --user=...            specify the user name (default: 'jenkins')"
  echo "  --version=...         specify the distribution version (e.g. 6, 18.04)"
  echo ""
  local arch="$(uname -m)"
  echo "Supported build patterns on this host ($arch):"
if [ $arch = x86_64 ] ; then
  echo "  bash mkdocker.sh --tag=openj9/cent6 --dist=centos --version=6  --build"
fi
if [ $arch = x86_64 -o $arch = ppc64le ] ; then
  echo "  bash mkdocker.sh --tag=openj9/cent7 --dist=centos --version=7  --build"
fi
  echo "  bash mkdocker.sh --tag=openj9/ub18  --dist=ubuntu --version=18 --build"
  echo "  bash mkdocker.sh --tag=openj9/ub20  --dist=ubuntu --version=20 --build"
  echo "  bash mkdocker.sh --tag=openj9/ub22  --dist=ubuntu --version=22 --build"
  echo "  bash mkdocker.sh --tag=openj9/ub24  --dist=ubuntu --version=24 --build"
  exit 1
}

# Global configuration variables.
action=print
all_versions=
arch=
criu=no
cuda_src=
cuda_tag=
dist=unspecified
engine=docker
engine_specified=0
freemarker=no
gen_git_cache=yes
jdk_versions=all
tags=()
user=jenkins
userid=1000
version=unspecified

# Frequently used commands.
wget_O="wget --progress=dot:mega -O"

parse_options() {
  local arg
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
      --build=*)
        action=build
        engine="${arg#*=}"
        engine_specified=1
        ;;
      --criu)
        criu=3.17.1
        ;;
      --cuda)
        cuda_src=/usr/local/cuda-12.0
        cuda_tag=12.0.0-devel-ubuntu18.04
        ;;
      --dist=*)
        dist="${arg#*=}"
        ;;
      --freemarker)
        freemarker=yes
        ;;
      --gitcache=*)
        gen_git_cache="${arg#*=}"
        ;;
      --jdk=*)
        jdk_versions="${arg#*=}"
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

  # If --build was specified without an engine, and `docker` isn't on $PATH,
  # and `podman` is on $PATH, then assume they're okay to use `podman`.
  if [ "${action}" = build ] \
     && [ "${engine_specified}" -eq 0 ] \
     && ! command -v "$engine" >/dev/null 2>&1 \
     && command -v podman >/dev/null 2>&1 ; then
    engine=podman
  fi
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
        6)
          if [ $arch = ppc64le ] ; then
            echo "CentOS version 6 is not supported on $arch" >&2
            exit 1
          fi
          if [ $arch = x86_64 ] ; then
            # Certificates are old on CentOS:6 so we can't expect wget to check.
            wget_O="wget --no-check-certificate --progress=dot:mega -O"
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
        18 | 20 | 22 | 24)
          version=$version.04
          ;;
        18.04 | 20.04 | 22.04 | 24.04)
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
      if [ $version = 24.04 ] ; then
          # userid 1000 already exists in 24.04
          userid=1001
      fi
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

  # If CRIU is requested, validate the architecture.
  if [ $criu != no ] ; then
    case "$arch" in
      x86_64)
        if [ $dist = centos ] ; then
          # overwrite CRIU version
          criu=3.12
          if [ $version = 6 ] ; then
            echo "CRIU is not supported on CentOS version: '$version'" >&2
            exit 1
          fi
        fi
        ;;
      s390x)
        # overwrite CRIU version
        criu=3.12
        if [ $dist:$version != ubuntu:18.04 ] ; then
          echo "CRIU is only supported on ubuntu:18.04 for s390x" >&2
          exit 1
        fi
        ;;
      *)
        echo "CRIU is not supported on architecture: '$arch'" >&2
        exit 1
        ;;
    esac
  fi

  # If CUDA is requested, validate the architecture.
  if [ "x$cuda_tag" != x ] ; then
    case "$arch" in
      ppc64le | x86_64)
        ;;
      *)
        echo "CUDA is not supported on architecture: '$arch'" >&2
        exit 1
        ;;
    esac
  fi

  all_versions="8 11 17 21 23 next"
  local -A known_version
  local version
  for version in $all_versions ; do
    known_version[$version]=yes
  done

  if [ "$jdk_versions" = all ] ; then
    jdk_versions="$all_versions"
  elif [ -z "$jdk_versions" ] ; then
    echo "At least one JDK version must be specified" >&2
    exit 1
  fi

  for version in ${jdk_versions} ; do
    if [ "${known_version[$version]}" != yes ] ; then
      echo "Unsupported JDK version: '$version'" >&2
      exit 1
    fi
  done

  if [ "${action}" = build ] && ! command -v "$engine" >/dev/null 2>&1 ; then
    echo "Executable '$engine' could not be found. Update \$PATH or use another engine with '--build=engine'" >&2
    exit 1
  fi
}

build_cmd() {
  local cmd=build
  local file=$1
  local tag
  for tag in ${tags[@]} ; do
    cmd="$cmd --tag $tag"
  done
  echo $cmd --file $file .
}

preamble() {
  local tag
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
if [ "x$cuda_tag" != x ] ; then
  echo "FROM nvidia/cuda:$cuda_tag AS cuda-dev"
  echo ""
fi
  echo "FROM $dist:$version AS base"
}

install_centos_packages() {
if [ $version = 6 ] ; then
  echo "RUN sed -i -e 's|mirrorlist|#mirrorlist|' \\"
  echo "           -e 's|#baseurl=http://mirror.centos.org/centos/\$releasever|baseurl=https://vault.centos.org/6.10|' \\"
  echo "           /etc/yum.repos.d/CentOS-Base.repo"
  echo ""
fi
  echo "RUN yum -y update \\"
  echo " && yum install -y epel-release \\"
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
if [ $version != 6 ] ; then
  echo "    libstdc++-static \\"
fi
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
  echo "    perl-IPC-Cmd \\" # required for openssl v3 compiles
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
if [ $criu != no ] ; then
  echo "    iproute \\"
  echo "    libbsd \\"
  echo "    libnet \\"
  echo "    libnl3 \\"
  echo "    protobuf-c \\"
  echo "    protobuf-python \\"
  echo "    python-ipaddress \\"
fi
  echo " && yum clean all"
  echo ""
  local autoconf_version=2.69
  echo "# Install autoconf."
  echo "RUN cd /tmp \\"
  echo " && $wget_O autoconf.tar.gz https://ftp.gnu.org/gnu/autoconf/autoconf-$autoconf_version.tar.gz \\"
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
  echo "# Install gcc-11.2."
  echo "RUN cd /usr/local \\"
  echo " && $wget_O gcc-11.tar.xz 'https://ci.adoptopenjdk.net/userContent/gcc/gcc112.$arch.tar.xz' \\"
  echo " && tar -xJf gcc-11.tar.xz \\"
  echo " && rm -f gcc-11.tar.xz"
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
  echo " && tar -xzf git.tar.gz --no-same-owner \\"
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
if [ $version != 6 ] ; then
  echo " && ./autogen.sh --skip-gnulib \\"
fi
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
  echo "ARG DEBIAN_FRONTEND=noninteractive"
  echo "ENV TZ=America/Toronto"
  echo ""
  echo "RUN apt-get update \\"
if [ $version = 18.04 ] || [ $version = 20.04 ] ; then
  echo " && apt-get install -qq -y --no-install-recommends \\"
  echo "    software-properties-common \\"
  echo " && add-apt-repository ppa:ubuntu-toolchain-r/test \\"
  echo " && apt-get update \\"
fi
  echo " && apt-get install -qq -y --no-install-recommends \\"
  echo "    ant \\"
  echo "    ant-contrib \\"
  echo "    autoconf \\"
  echo "    build-essential \\"
  echo "    ca-certificates \\"
  echo "    cmake \\"
  echo "    cpio \\"
if [ $criu != no ] && [ $version != 20.04 ] ; then
  echo "    criu \\"
fi
  echo "    curl \\"
  echo "    file \\"
if [ $version = 18.04 ] ; then
  echo "    g++-8 \\"
  echo "    gcc-8 \\"
elif [ $version = 20.04 ] ; then
  echo "    g++-10 \\"
  echo "    gcc-10 \\"
elif [ $version = 22.04 ] ; then
  echo "    g++-11 \\"
  echo "    gcc-11 \\"
else
  echo "    g++-13 \\"
  echo "    gcc-13 \\"
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
  echo "    ssh \\"
  echo "    systemtap-sdt-dev \\"
  echo "    unzip \\"
  echo "    wget \\"
  echo "    xvfb \\"
  echo "    zip \\"
  echo "    zlib1g-dev \\"
if [ $criu != no ] ; then
if [ $version = 18.04 ] ; then
  echo "    libcap2-bin \\"
fi
  echo "    libnet1 \\"
  echo "    libnl-3-200 \\"
if [ $version = 20.04 ] ; then
  echo "    libnftables1 \\"
fi
  echo "    libprotobuf-c1 \\"
  echo "    python3-protobuf \\"
fi
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
  echo "RUN useradd -ms /bin/bash --uid $userid $user --home-dir /home/$user \\"
  echo " && mkdir /home/$user/.ssh \\"
  echo " && chmod 700 /home/$user/.ssh"
  echo "COPY authorized_keys known_hosts /home/$user/.ssh/"
  echo "RUN chmod -R go= /home/$user/.ssh"
}

adjust_user_directory_perms() {
  echo "RUN chown -R $user:$user /home/$user"
}

set_user() {
  echo "USER $user"
}

install_freemarker() {
  echo ""
  local freemarker_version=2.3.8
  echo "# Download and extract freemarker.jar to /home/$user/freemarker.jar."
  echo "RUN cd /home/$user \\"
  echo " && $wget_O freemarker.tar.gz https://sourceforge.net/projects/freemarker/files/freemarker/$freemarker_version/freemarker-$freemarker_version.tar.gz/download \\"
  echo " && tar -xzf freemarker.tar.gz freemarker-$freemarker_version/lib/freemarker.jar --strip-components=2 \\"
  echo " && rm -f freemarker.tar.gz"
}

bootjdk_dirs() {
  local version
  for version in $@ ; do
    echo /home/$user/bootjdks/jdk$version
  done
}

bootjdk_url() {
  local jdk_arch=${arch/x86_64/x64}
  local jdk_version=$1
  if [ $jdk_version -le 23 ] ; then
    echo https://api.adoptopenjdk.net/v3/binary/latest/$jdk_version/ga/linux/$jdk_arch/jdk/openj9/normal/adoptopenjdk
  else
    echo https://api.adoptium.net/v3/binary/latest/$jdk_version/ga/linux/$jdk_arch/jdk/hotspot/normal/eclipse
  fi
}

bootjdk_version() {
  local jdk_version=$1
  case $jdk_version in
    8 | 11 | 17)
      echo $jdk_version
      ;;
    21)
      echo 21
      ;;
    23 | next)
      echo 23
      ;;
    *)
      echo "Unsupported JDK version: '$jdk_version'" >&2
      exit 1
      ;;
  esac
}

install_bootjdks() {
  local version
  local bootjdk_versions=$(for version in $jdk_versions ; do bootjdk_version $version ; done | sort -n -u)
  echo ""
  echo "# Download and install boot JDKs."
  echo "RUN cd /tmp \\"
for version in $bootjdk_versions ; do
  echo " && $wget_O jdk$version.tar.gz $(bootjdk_url $version) \\"
done
  echo " && mkdir -p" $(bootjdk_dirs $bootjdk_versions) "\\"
for version in $bootjdk_versions ; do
  echo " && tar -xzf jdk$version.tar.gz --directory=$(bootjdk_dirs $version)/ --strip-components=1 \\"
done
  echo " && rm -f jdk*.tar.gz"
}

install_criu() {
  echo ""
  local criu_name=criu-${criu}
  echo "# Install prerequisites for CRIU build."
  echo "FROM base AS criu-builder"
if [ $dist = centos ] ; then
  echo "RUN yum -y install \\"
  echo "    gnutls-devel \\"
  echo "    iptables \\"
  echo "    libbsd-devel \\"
  echo "    libcap-devel \\"
  echo "    libnet-devel \\"
  echo "    libnl3-devel \\"
  echo "    pkgconfig \\"
  echo "    protobuf-c-devel \\"
  echo "    protobuf-devel"
else
  echo "RUN apt-get update \\"
  echo " && apt-get install -qq -y --no-install-recommends \\"
  echo "    iptables \\"
  echo "    libbsd-dev \\"
  echo "    libcap-dev \\"
  echo "    libnet1-dev \\"
  echo "    libgnutls28-dev \\"
  echo "    libgnutls30 \\"
if [ $version = 20.04 ] ; then
  echo "    libnftables-dev \\"
fi
  echo "    libnl-3-dev \\"
  echo "    libprotobuf-c-dev \\"
  echo "    libprotobuf-dev \\"
  echo "    python3-distutils \\"
  echo "    protobuf-c-compiler \\"
  echo "    protobuf-compiler"
fi
  echo "# Build CRIU from source."
  echo "RUN cd /tmp \\"
  echo " && $wget_O criu.tar.gz https://github.com/checkpoint-restore/criu/archive/refs/tags/v${criu}.tar.gz \\"
  echo " && tar -xzf criu.tar.gz \\"
  echo " && mv $criu_name criu-build \\"
  echo " && cd criu-build \\"
  echo " && make \\"
  echo " && make DESTDIR=/tmp/$criu_name install-lib install-criu \\"
  echo " && cd .. \\"
  echo " && rm -fr criu.tar.gz criu-build"
  echo "# Install CRIU."
  echo "FROM base"
  echo "COPY --from=criu-builder /tmp/$criu_name/usr/local /usr/local"
  echo "# Set CRIU capabilities."
if [ $dist = centos ] ; then
  echo "RUN setcap cap_chown,cap_dac_override,cap_dac_read_search,cap_fowner,cap_fsetid,cap_kill,cap_setgid,cap_setuid,cap_setpcap,cap_net_bind_service,cap_net_broadcast,cap_net_admin,cap_ipc_lock,cap_ipc_owner,cap_sys_module,cap_sys_rawio,cap_sys_chroot,cap_sys_ptrace,cap_sys_pacct,cap_sys_admin,cap_sys_resource,cap_sys_time,cap_lease,cap_audit_control,cap_setfcap,cap_syslog=eip /usr/local/sbin/criu"
else
  echo "RUN setcap cap_chown,cap_dac_override,cap_dac_read_search,cap_fowner,cap_fsetid,cap_kill,cap_setgid,cap_setuid,cap_setpcap,cap_net_admin,cap_sys_chroot,cap_sys_ptrace,cap_sys_admin,cap_sys_resource,cap_sys_time,cap_audit_control=eip /usr/local/sbin/criu"
fi
}

install_cuda() {
  echo ""
  echo "# Copy header files necessary to build a VM with CUDA support."
  echo "RUN mkdir -p /usr/local/cuda/nvvm"
  echo "COPY --from=cuda-dev $cuda_src/include      /usr/local/cuda/include"
  echo "COPY --from=cuda-dev $cuda_src/nvvm/include /usr/local/cuda/nvvm/include"
  echo "ENV CUDA_HOME=/usr/local/cuda"
}

install_python() {
  local python_version=3.7.3
  local python_short_version=${python_version%.*}
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
if [ $criu != no ] && [ $dist = ubuntu ]; then
  echo ""
  echo "# Set capabilities for python3."
  echo "RUN setcap cap_sys_admin=eip /usr/local/bin/python$python_short_version"
fi
}

adjust_ldconfig() {
  echo ""
  echo "# Run ldconfig to discover newly installed shared libraries."
  echo "RUN for dir in lib lib64 lib/x86_64-linux-gnu ; do echo /usr/local/\$dir ; done > /etc/ld.so.conf.d/usr-local.conf \\"
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
  local version
  local -A wanted
  for version in $jdk_versions ; do
    wanted[$version]=yes
  done
  # The jdk17 remote is fetched first because that repository was subjected to
  # 'git gc --aggressive --prune=all' before it was first published making it much
  # smaller than some other jdk repositories. There is a large degree of overlap
  # among the jdk repositories so a relatively small number of commits must be
  # fetched from the others.
  echo ""
  echo "# Setup a reference repository cache for faster clones in containers."
  echo "RUN mkdir $git_cache_dir \\"
  echo " && cd $git_cache_dir \\"
  echo " && git init --bare \\"
for version in $all_versions ; do
  if [ "${wanted[$version]}" = yes ] ; then
    add_git_remote jdk$version https://github.com/ibmruntimes/openj9-openjdk-jdk${version/next/}.git
  fi
done
  add_git_remote omr     https://github.com/eclipse-openj9/openj9-omr.git
  add_git_remote openj9  https://github.com/eclipse-openj9/openj9.git
  echo " && echo Fetching repository cache... \\"
if [ "${wanted[17]}" = yes ] ; then
  echo " && git fetch jdk17 \\"
fi
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
if [ $criu != no ] ; then
  install_criu
fi
  create_user
if [ "x$cuda_tag" != x ] ; then
  install_cuda
fi
if [ $freemarker = yes ] ; then
  install_freemarker
fi
  install_cmake
  install_python

  adjust_ldconfig
  configure_ssh

  install_bootjdks
if [ $gen_git_cache = yes ] ; then
  create_git_cache
fi
  adjust_user_directory_perms
  set_user
}

main() {
  if [ $action = build ] ; then
    prepare_user
    print_dockerfile | $engine $(build_cmd -)
  else
    print_dockerfile
  fi
}

command_line="$*"
parse_options "$@"
validate_options
main
