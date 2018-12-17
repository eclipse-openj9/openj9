<!--
Copyright (c) 2018, 2018 IBM Corp. and others
 
 This program and the accompanying materials are made available under
 the terms of the Eclipse Public License 2.0 which accompanies this
 distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 or the Apache License, Version 2.0 which accompanies this distribution 
and
 is available at https://www.apache.org/licenses/LICENSE-2.0.
 
 This Source Code may also be made available under the following
 Secondary Licenses when the conditions for such availability set
 forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 General Public License, version 2 with the GNU Classpath
 Exception [1] and GNU General Public License, version 2 with the
 OpenJDK Assembly Exception [2].
 
 [1] https://www.gnu.org/software/classpath/license.html
 [2] http://openjdk.java.net/legal/assembly-exception.html
 
 SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH 
Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
-->
# Docker
## About
Dockerfiles are used to define how a docker container/image
should be built. Docker containers are essentially virtual
machines designed for short term use. This makes them very
useful for testing purposes as you can generate a new container
for every test and you are guaranteed to run against the
same environment every time.

## Status of Dockerfiles on different Platforms
|           |     x86      |     s390x     |    ppc64le    |
|:---------:|:------------:|:-------------:|:-------------:|
| Ubuntu16  |  Test Only   | Build & Test  |   Test Only   |
| Ubuntu18  |  Test Only   |   Test Only   |   Test Only   |
| Centos6.9 | Build & Test | Not Available | Not Available |
| Centos7   | To be Added  |  To be Added  | Build & Test  |
| Windows   | To be Added  | Not Available | Not Available |

## Eclipse OpenJ9 CI
The Dockerfiles are used by two Jenkins jobs at Eclipse to
generate docker containers. The first [job](https://ci.eclipse.org/openj9/view/Build/job/Build-Jenkins-Agent-Container/) is run manually
by supplying the architecture and os. It should be run after
a Dockerfile change has been merged. The second [job](https://ci.eclipse.org/openj9/view/Pull%20Requests/job/PullRequest-Build-Jenkins-Agent-Container/) is used
to test pull requests. Whenever a PR makes changes to a Dockerfile,
a PR build should be launched for each Dockerfile changed. A PR
build can be launched by making a comment in the PR with the following format
```
Jenkins build docker <ARCH> <OS>
```
Both jobs will push the Docker images to Docker Hub under the repo 
`eclipse/openj9-jenkins-agent-<ARCH>-<OS>:<TAG>`
If a manual build is run the tag will be the build number of
the job, as well as the `latest` tag. If a PR build is run the
tag will contain `PR` followed by the ID of the pull request used to
build the job.
## Using containers from a terminal
If you have [docker installed](https://docs.docker.com/install/) then you can run
`docker pull eclipse/openj9-jenkins-agent-<ARCH>-<OS>:<TAG>` to pull
a docker image of the specified architecture and os.
You can use `docker run -it eclipse/openj9-jenkins-agent-<ARCH>-<OS>:<TAG> /bin/bash`
to start up a new container and enter it. Once inside of a
container, run `su - jenkins` to switch to the jenkins user.
This way when you run your code you won't have root privileges.
If the container is capable of building openj9 it will also have
reference repos and bootJDKs. The reference repos can be used to
improve clone times, and the bootJDKs can be used for compiling
openj9. The locations of the reference repos and bootJDKs can be
found [here](https://github.com/eclipse/openj9/blob/master/buildenv/jenkins/variables/defaults.yml).
A full list of docker commands for managing/using containers can
be found [here](https://docs.docker.com/engine/reference/commandline/docker/).
## Docker and Jenkins
There is a Docker plugin for Jenkins which allows Jenkins to
orchestrate running Docker containers on remote machine(s). Details
of the plugin can be found [here](https://plugins.jenkins.io/docker-plugin). After the plugin is installed,
you need a machine with Docker installed, and [enable its remote API](https://medium.com/@sudarakayasindu/enabling-and-accessing-docker-engine-api-on-a-remote-docker-host-on-ubuntu-16-04-2c15f55f5d39).
In the Jenkins Configure System page under the Cloud section, add
a new Docker Cloud. Fill in the details for the remote host and add
one or more Agent templates with the appropriate details. Add the
labels as you would a normal agent. Additionally add label `hw.arch.docker`
to indicate that the agent is running inside a container.
