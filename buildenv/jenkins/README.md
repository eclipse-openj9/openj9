<!--
Copyright (c) 2017, 2018 IBM Corp. and others

This program and the accompanying materials are made available under
the terms of the Eclipse Public License 2.0 which accompanies this
distribution and is available at https://www.eclipse.org/legal/epl-2.0/
or the Apache License, Version 2.0 which accompanies this distribution and
is available at https://www.apache.org/licenses/LICENSE-2.0.

This Source Code may also be made available under the following
Secondary Licenses when the conditions for such availability set
forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
General Public License, version 2 with the GNU Classpath
Exception [1] and GNU General Public License, version 2 with the
OpenJDK Assembly Exception [2].

[1] https://www.gnu.org/software/classpath/license.html
[2] http://openjdk.java.net/legal/assembly-exception.html

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
-->

[Eclipse OpenJ9 Jenkins Builds](https://ci.eclipse.org/openj9/)

This folder contains Jenkins pipeline scripts that are used in the OpenJ9 Jenkins builds.

### Triggering Pull Request Builds from Github

- You can request a PR build to do compile or compile & test
- Current supported test levels are functional sanity and functional extended
- Current available platforms are 
    - Linux x86 (xLinux)
    - Linux x86 largeheap/non-compressed refs (xlinuxlargeheap)
    - Linux s390x (zLinux)
    - Linux PPCLE (pLinux)
    - AIX PPC (aix)
    - Windows 64 bits (win)
    - Windows 32 bits (win32) - supported on JDK8 only
- OpenJ9 committers can request builds by commenting in a pull request
    - Format: `Jenkins <build type> <level> <platform>[,<platform>,...,<platform>] jdk<version>[,jdk<version>,...,jdk<version>]`
    - `<build type>` is compile | test
    - `<level>` is sanity | extended (required only for "test" `<build type>`)
    - `<platform>` is aix | xlinux | xlinuxlargeheap | zlinux | plinux | win | win32
    - `<version>` is the number of the supported release, e.g. 8 | 11 | n 
- Note: You can use keyword `all` for platform

###### Examples
- Request a Compile-only build on all platforms and multiple versions by commenting in a PR
    - `Jenkins compile all jdk8,jdk11`
- Request a Sanity build on zLinux and multiple versions
    - `Jenkins test sanity zlinux jdk8,jdk11`
- Request an Extended build on pLinux for a single version
    - `Jenkins test extended plinux jdk8`
- Request a Sanity build on z,p Linux for multiple versions
    - `Jenkins test sanity zlinux,plinux jdk8,jdk9`
- Request Sanity tests on all platforms and multiple versions
    - `Jenkins test sanity all jdk8,jdk11`

You can also request a Pull Request build from the Eclipse OpenJ9 repository - [openj9](https://github.com/eclipse/openj9) - or the Extensions OpenJDK\* for Eclipse OpenJ9 repositories:

- openj9-openjdk-jdk: https://github.com/ibmruntimes/openj9-openjdk-jdk
- openj9-openjdk-jdk`<version>`: `https://github.com/ibmruntimes/openj9-openjdk-jdk<version>`

###### Note: When specifying a dependent change in an OpenJDK extensions repo, you can only build the SDK version that matches the repo where the dependent change lives. Eg. You cannot build JDK8 with a PR in openj9-openjdk-jdk9.

##### Dependent Changes

- If you have dependent change(s) in either eclipse/omr, eclipse/openj9-omr, or ibmruntimes/openj9-openjdk-jdk\*, you can build & test with all needed changes
- Request a build by including the PR ref or branch name in your trigger comment
- Ex. Dependent change in OMR Pull Request `#123`
    - `Jenkins test sanity xlinux jdk8 depends eclipse/omr#123`
- Ex. Dependent change in eclipse/omr master branch (useful if a dependent OMR PR is already merged)
    - `Jenkins test sanity xlinux jdk8 depends eclipse/omr#master`
- Ex. Dependent change in OpenJ9-OMR Pull Request `#456`
    - `Jenkins test sanity xlinux jdk8 depends eclipse/openj9-omr#456`
- Ex. Dependent change in OpenJDK Pull Request `#789`
    - `Jenkins test sanity xlinux jdk8 depends ibmruntimes/openj9-openjdk-jdk9#789`
- Ex. Dependent changes in OMR and OpenJDK
    - `Jenkins test sanity all jdk9 depends eclipse/omr#123 ibmruntimes/openj9-openjdk-jdk9#789`
- Ex. If you have a dependent change and only want one platform, depends comes last
    - `Jenkins test sanity zlinux jdk8 depends eclipse/omr#123`

##### Other Pull Requests builds

- To trigger a Line Endings Check
   - `Jenkins line endings check`

- To trigger a Copyright Check
   - `Jenkins copyright check`

- To trigger a SignedOffBy Check
   - `Jenkins signed off by check`

##### PullRequest Trigger Regexes
Having a complicated regex in the pull request trigger is what allows us to launch exactly the right combination of builds we need without having to make several trigger comments. The following are examples of what regexes we use in the various jobs.

- Openj9 Repo
    - Compile
        - `.*(\n)?\bjenkins\s+compile\b\s*(((all|(([a-z]+(32)?,)*<platform>(,[a-z]+(32)?)*))\s*(jdk[0-9n]+,)*jdk<version>(,jdk[0-9n]+)*)(\s+depends.*)?)(\n)?.*`
    - Test
        - `.*(\n)?\bjenkins\s+test\s+<level>\b\s*(((all|(([a-z]+(32)?,)*<platform>(,[a-z]+(32)?)*))\s*(jdk[0-9n]+,)*jdk<version>(,jdk[0-9n]+)*)(\s+depends.*)?)(\n)?.*`

- OpenJDK Extensions repos
    - Compile
        - `.*(\n)?\bjenkins\s+compile\b\s*(((all|(([a-z]+(32)?,)*<platform>(,[a-z]+(32)?)*))\s*jdk<version>)(\s+depends.*)?))(\n)?.*`
    - Test
        - `.*(\n)?\bjenkins\s+test\s+<level>\b\s*(((all|([a-z]+(32)?,)*<platform>(,[a-z]+(32?))*)\s*jdk<version>)(\s+depends.*)?)(\n)?.*`


### Jenkins Pipelines

In this section:
- `<platform>` is aix_ppc-64_cmprssptrs | linux_390-64_cmprssptrs | linux_ppc-64_cmprssptrs_le | linux_x86-64 | linux_x86-64_cmprssptrs | win_x86-64_cmprssptrs | win_x86
- `<repo>` is the Eclipse OpenJ9 repository or an Extensions OpenJDK\* for Eclipse OpenJ9 repository, e.g. OpenJ9 | OpenJDK`<version>`

#### Pull Requests

Pull Requests for all platforms and versions are available [**here**](https://ci.eclipse.org/openj9/view/Pull%20Requests/).

- PullRequest-Compile-JDK`<version>`-`<platform>`-`<repo>`
    - Description:
        - Compile Eclipse OpenJ9 on `<platform>` for Extensions OpenJDK`<version>`
    - Trigger:
        - Github PR comment example `Jenkins compile <platform> jdk<version>`

- PullRequest-Extended-JDK`<version>`-`<platform>`-`<repo>`
    - Description:
        - Compile Eclipse OpenJ9 on `<platform>` for Extensions OpenJDK`<version>` and run extended tests
    - Trigger:
        - Github PR comment `Jenkins test extended <platform> jdk<version>`

- PullRequest-Sanity-JDK`<version>`-`<platform>`-`<repo>`
    - Description:
        - Compile Eclipse OpenJ9 on `<platform>` for Extensions OpenJDK`<version>` and run sanity tests
    - Trigger:
        - Github PR comment `Jenkins test sanity <platform> jdk<version>`

- PullRequest-LineEndingsCheck-`<repo>`
    - Description:
        - Checks the files modified in a pull request have correct line endings
    - Trigger:
        - Automatically builds on every PR
        - Retrigger with `Jenkins line endings check`

- PullRequest-CopyrightCheck-`<repo>`
    - Description:
        - Checks the files modified in a pull request have copyright with current year
    - Trigger:
        - Automatically builds on every PR
        - Retrigger with `Jenkins copyright check`

- PullRequest-signedOffByCheck-`<repo>`
    - Description:
        - Checks the commits in a pull request have proper sign-off
    - Trigger:
        - Trigger with `Jenkins signed off by check`

#### Pipelines

Pipelines for all platforms and versions are available [**here**](https://ci.eclipse.org/openj9/view/Pipelines/).

- Any platform and version pipeline:
    - Name: Pipeline-Build-Test-JDK`<version>`-`<platform>`
    - Description:
        - Compile Eclipse OpenJ9 on `<platform>` for Extensions OpenJDK`<version>` and run sanity & extended test suites
        - Triggers:
            - Build-JDK`<version>`-`<platform>`
            - Test-`<sanity|extended>.<functional|system>`-JDK`<version>`-`<platform>`
    - Trigger:
        - build periodically, @midnight

- Pipeline-OMR-Acceptance
    - [![Build Status](https://ci.eclipse.org/openj9/buildStatus/icon?job=Pipeline-OMR-Acceptance)](https://ci.eclipse.org/openj9/job/Pipeline-OMR-Acceptance)
    - Description:
        - Compile and run sanity tests against new OMR content
        - Triggers:
            - Build-JDK`<version>`-`<platform>` and Test-Sanity-JDK`<version>`-`<platform>` across all platforms and JDK versions 8, 10
            - `Promote-OpenJ9-OMR-master-to-openj9` once all testing is passed
    - Trigger: Triggered by `Mirror-OMR-to-OpenJ9-OMR`


#### Build

Build pipelines for all platforms and versions are available [**here**](https://ci.eclipse.org/openj9/view/Build/).

- Any platform and version build pipeline:
    - Name: Build-JDK`<version>`-`<platform>`
    - Description:
        - Compiles Eclipse OpenJ9 on `<platform>` for `<version>`
        - Archives the SDK and test material for use in downstream jobs
    - Trigger:
        - This job is used in other pipelines but can be launched manually

**Note:** Windows 32 is available only for JDK8. Thus the only available build for win_x86 platform (Windows 32bits) is [Build-JDK8-win_x86](https://ci.eclipse.org/openj9/view/Build/job/Build-JDK8-win_x86/).


#### Test

Test pipelines for all platforms and versions are available [**here**](https://ci.eclipse.org/openj9/view/Test/).

- Any platform and version build pipeline:
    - Name: Test-`<sanity|extended>.<functional|system>`-JDK`<version>`-`<platform>`
    - Description:
        - Runs sanity or extended tests
    - Trigger:
        - This job is used in other pipelines but can be launched manually

Mode details could be found on [AdoptOpenJDK Testing](https://github.com/AdoptOpenJDK/openjdk-tests).
The [Running AdoptOpenJDK Tests](https://github.com/AdoptOpenJDK/openjdk-tests/blob/master/doc/userGuide.md) provides further details on how to set up Jenkins test pipelines.

#### Infrastructure

Infrastructure pipelines are available [**here**](https://ci.eclipse.org/openj9/view/Infrastructure/)

- Mirror-OMR-to-OpenJ9-OMR
    - [![Build Status](https://ci.eclipse.org/openj9/buildStatus/icon?job=Mirror-OMR-to-OpenJ9-OMR)](https://ci.eclipse.org/openj9/job/Mirror-OMR-to-OpenJ9-OMR)
    - Description:
        - Mirrors [eclipse/omr/master](https://github.com/eclipse/omr/tree/master) to [eclipse/openj9-omr/master](https://github.com/eclipse/openj9-omr/tree/master)
        - Triggers `Pipeline-OMR-Acceptance` when there is new content        
    - Trigger:
        - Build periodically, 15 minutes

- Promote-OpenJ9-OMR-master-to-openj9
    - [![Build Status](https://ci.eclipse.org/openj9/buildStatus/icon?job=Promote-OpenJ9-OMR-master-to-openj9)](https://ci.eclipse.org/openj9/job/Promote-OpenJ9-OMR-master-to-openj9)
    - Description:
        - Promotes eclipse/openj9-omr branch master to branch openj9
        - Lays a tag down on the promoted SHA in the format `omr_merge_YYYYMMDD_HHMMSS` with annotations including the current OpenJ9 and OpenJDK SHAs
    - Trigger:
        - Last step of `Pipeline-OMR-Acceptance`

- Mirror-OpenJ9-Website-to-Eclipse
    - [![Build Status](https://ci.eclipse.org/openj9/buildStatus/icon?job=Mirror-OpenJ9-Website-to-Eclipse)](https://ci.eclipse.org/openj9/job/Mirror-OpenJ9-Website-to-Eclipse)
    - Description:
        - Mirrors [github.com/eclipse/openj9-website](https://github.com/eclipse/openj9-website/tree/master) to the Eclipse.org repo
    - Trigger:
        - Poll Github repo for changes


### Adding Builds
- Always add pipeline style jobs so the code can be committed to the repo once it is ready
- Update this readme when your build is in production use
- Whenever possible, reuse existing builds or existing jenkins files
