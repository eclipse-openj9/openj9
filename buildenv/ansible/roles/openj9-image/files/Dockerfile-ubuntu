###############################################################################
# Copyright (c) 2018, 2018 Pavel Samolysov
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
###############################################################################
FROM ubuntu:16.04

MAINTAINER Pavel Samolysov <samolisov@gmail.com> (@samolisov)

# This section:
# - Gets the prerequisites for the add-apt-repository
# - Installs the required OS tools.
RUN apt-get update \
  && apt-get install -qq -y --no-install-recommends \
    software-properties-common \
    python-software-properties \
  && apt-get update

# Set the java home directory
ARG JAVA_HOME=/opt/openj9

# Set group and group id
ARG GROUP=openj9
ARG GROUPID=999

# Set user and user id
ARG USER=openj9
ARG USERID=999

# Set the version, operating system and platform
ARG VERSION=jdk8
ARG OS=linux
ARG PLATFORM=x86_64

# Explicitly set user/group IDs
RUN groupadd -r $GROUP --gid=$GROUPID && useradd -r -g $GROUP --uid=$USERID $USER

# Create the java home directory
RUN  mkdir -p $JAVA_HOME \
  && chown -R $USER:$GROUP $JAVA_HOME

# Copy in the JDK archive
ADD openj9-$VERSION-$OS-$PLATFORM.tar.gz $JAVA_HOME

# I have no idea why docker sets the owner of these folders
# to root:root, chown returns the correct owner and doesn't 
# increase the size of the image.
RUN chown -f $USER:$GROUP $JAVA_HOME/bin || : \
  && chown -f $USER:$GROUP $JAVA_HOME/conf || : \
  && chown -f $USER:$GROUP $JAVA_HOME/demo || : \
  && chown -f $USER:$GROUP $JAVA_HOME/include || : \
  && chown -f $USER:$GROUP $JAVA_HOME/jmods || : \
  && chown -f $USER:$GROUP $JAVA_HOME/jre || : \
  && chown -f $USER:$GROUP $JAVA_HOME/legal || : \
  && chown -f $USER:$GROUP $JAVA_HOME/lib || : \
  && chown -f $USER:$GROUP $JAVA_HOME/man || : \
  && chown -f $USER:$GROUP $JAVA_HOME/sample || :
  # small hach to ensure chown for a non-exist directory won't interrupt the build process.

# Change to the OpenJ9 user.
USER $USER

# Set the PATH environment variable
ENV PATH $PATH:$JAVA_HOME/bin

# Set the JAVA_HOME environment variable
ENV JAVA_HOME $JAVA_HOME

# Change the entry dir
WORKDIR $JAVA_HOME
