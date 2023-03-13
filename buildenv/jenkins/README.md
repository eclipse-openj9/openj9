<!--
Copyright IBM Corp. and others 2017

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
[2] https://openjdk.org/legal/assembly-exception.html

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
-->

[Eclipse OpenJ9 Jenkins Builds](https://openj9-jenkins.osuosl.org/)

This folder contains Jenkins pipeline scripts that are used in the OpenJ9 Jenkins builds.

### Triggering Pull Request Builds from Github

- You can request a PR build to do compile or compile & test
- Current supported test levels are sanity & extended
- Current supported test groups are functional & system
- Current available platforms are
    - Linux on x86-64
        - Spec: x86-64_linux
        - Shortname: xlinux
    - Linux on x86-64 largeheap/non-compressed references
        - Spec: x86-64_linux_xl
        - Shortname: xlinuxlargeheap or xlinuxxl
    - Linux on x86-64 with JITServer
        - Spec x86-64_linux_jit
        - Shortname: xlinuxjit
    - Linux on s390x largeheap/non-compressed references
        - Spec: s390x_linux_xl
        - Shortname: zlinuxlargeheap or zlinuxxl
    - Linux on s390x
        - Spec: s390x_linux
        - Shortname: zlinux
    - Linux on ppc64le
        - Spec: ppc64le_linux
        - Shortname: plinux
    - Linux on ppc64le largeheap/non-compressed references
        - Spec: ppc64le_linux_xl
        - Shortname: plinuxlargeheap or plinuxxl
    - Linux on aarch64
        - Spec: aarch64_linux
        - Shortname: alinux64
    - Linux on aarch64 largeheap/non-compressed references
        - Spec: aarch64_linux_xl
        - Shortname: alinux64xl or alinux64largeheap
    - AIX on ppc64
        - Spec: ppc64_aix
        - Shortname: aix
    - AIX on ppc64 largeheap/non-compressed references
        - Spec: ppc64_aix_xl
        - Shortname: aixlargeheap or aixxl
    - Windows on x86-64
        - Spec: x86-64_windows
        - shortname: win
    - Windows on x86-64 largeheap/non-compressed references
        - Spec: x86-64_windows_xl
        - shortname: winlargeheap or winxl
    - Windows on x86 (32bit, supported on JDK8 only)
        - Spec: x86-32_windows
        - Shortname: win32
    - OSX on x86-64
        - Spec: x86-64_mac
        - Shortname: osx or xmac
    - OSX on x86-64 largeheap/non-compressed references
        - Spec: x86-64_mac_xl
        - Shortname: osxlargeheap or osxxl
    - OSX on aarch64 (currently largeheap only)
        - Spec: aarch64_mac
        - Shortname: amac
    - ALL
        - Launches a subset of 'all' platforms
        - ppc64le_linux, s390x_linux, x86-64_linux, aarch64_linux, ppc64_aix, x86-64_windows, x86-32_windows, x86-64_mac, aarch64_mac
- Many specs support a suffix of `_cm` or `_uma` (omit the leading underscore for shortnames) to override the default build system.
- OpenJ9 committers can request builds by commenting in a pull request
    - Format: `Jenkins <build type> <level>.<group>[+<test_flag>] <platform>[,<platform>,...,<platform>] jdk<version>[,jdk<version>,...,jdk<version>]`
    - `<build type>` is compile | test
    - `<level>` is sanity | extended (required only for "test" `<build type>`)
    - `<group>` is functional | system
    - `<test_flag>` Optional: any TEST_FLAG is supported. See notes below.
    - `<platform>` is one of the (short or full) platform names above
    - `<version>` is the number of the supported release, e.g. 8 | 11 | next
- Note: You can use keyword `all` for platform but not for test level/type or JDK versions.
- Note: For backward compatibility `<level>.<test type>` equal to `sanity` or `extended` is acceptable and will map to `sanity.functional` and `extended.functional` respectively.
- Note: TEST_FLAG is an optional argument to the test target. It is recommended to only launch 1 test target with 1 TEST_FLAG per comment/build. At the time of writing, the two supported test flags are `+jitaas` and `+aot`. Ex. `Jenkins test sanity.functional+jitaas xlinux jdk8`. Also note JITServer specs set the TEST_FLAG via the variable file so it is unnecessary to add it to the test target.

###### Examples
- Request a Compile-only build on all platforms and multiple versions by commenting in a PR
    - `Jenkins compile all jdk8,jdk11`
- Request a sanity functional build on zLinux and multiple versions
    - `Jenkins test sanity.functional zlinux jdk8,jdk11`
- Request an extended functional and system build on pLinux for a single version
    - `Jenkins test extended.functional,extended.system plinux jdk8`
- Request a sanity build on z,p Linux for multiple versions
    - `Jenkins test sanity zlinux,plinux jdk8,jdk11`
- Request sanity.system test on all platforms and multiple versions
    - `Jenkins test sanity.system all jdk8,jdk11`

You can request a Pull Request build from the Eclipse OpenJ9 repository - [openj9](https://github.com/eclipse-openj9/openj9) - or the Extensions OpenJDK\* for Eclipse OpenJ9 repositories:

- openj9-openjdk-jdk: https://github.com/ibmruntimes/openj9-openjdk-jdk
- openj9-openjdk-jdk`<version>`: `https://github.com/ibmruntimes/openj9-openjdk-jdk<version>`

##### Dependent Changes

- If you have dependent change(s) in either eclipse/omr, eclipse-openj9/openj9-omr, or ibmruntimes/openj9-openjdk-jdk\*, you can build & test with all needed changes
- Request a build by including the PR ref or branch name in your trigger comment
- Ex. Dependent change in OMR Pull Request `#123`
    - `Jenkins test sanity xlinux jdk8 depends eclipse/omr#123`
- Ex. Dependent change in eclipse/omr master branch (useful if a dependent OMR PR is already merged)
    - `Jenkins test sanity xlinux jdk8 depends eclipse/omr#master`
- Ex. Dependent change in OpenJ9-OMR Pull Request `#456`
    - `Jenkins test sanity xlinux jdk8 depends eclipse-openj9/openj9-omr#456`
- Ex. Dependent change in OpenJDK Pull Request `#789`
    - `Jenkins test sanity xlinux jdk8 depends ibmruntimes/openj9-openjdk-jdk8#789`
- Ex. Dependent changes in OMR and OpenJDK
    - `Jenkins test sanity all jdk8 depends eclipse/omr#123 ibmruntimes/openj9-openjdk-jdk8#789`
- Ex. If you have a dependent change and only want one platform, depends comes last
    - `Jenkins test sanity zlinux jdk8 depends eclipse/omr#123`

###### Note: When specifying a dependent change in an OpenJDK extensions repo, you can only build the SDK version that matches the repo where the dependent change lives. Eg. You cannot build JDK8 with a PR in openj9-openjdk-jdk11.

##### Building Release branch PRs

- If you have a PR against a release branch, it can be tested with a PR build that specifies the OMR and Extensions repos.
- Ex. Release PR against the `v0.20.0-release` branch specifying both OMR and Extensions
    - `Jenkins compile plinux jdk11 depends eclipse-openj9/openj9-omr#v0.20.0-release ibmruntimes/openj9-openjdk-jdk11#openj9-0.20.0`

##### Testing Changes to Pipeline code

- If you have changes to Jenkins Pipeline code (including the defaults.yml variable file), you can test most code paths via a PR build. Your PR build will load the Pipeline code from your PR (merge ref). Also note if you have dependent changes you have to launch your PR build through the OpenJ9 repo in order for the Pipeline changes to be loaded in your build.

##### Other Pull Requests builds

- To trigger a Line Endings Check
    - `Jenkins line endings check`

- To trigger a SignedOffBy Check (Only applicable to the Extensions repos)
    - `Jenkins signed off by check`

### Jenkins Pipelines

In this section:
- `<platform>` is the full spec name eg. ppc64_aix
- `<repo>` is the Eclipse OpenJ9 repository or an Extensions OpenJDK\* for Eclipse OpenJ9 repository, e.g. OpenJ9 | OpenJDK`<version>`

#### Pull Requests

Pull Requests for all platforms and versions are available [**here**](https://openj9-jenkins.osuosl.org/view/Pull%20Requests/).

For Compile & Test PRs, there is a single top level job (for each repository) that connects Jenkins and the Github repo.
This job will trigger downstream jobs based on what is requested in the pull request trigger comment (ghprbCommentBody)

- PullRequest-`<repo>`
    - Description:
        - Setup job that launches downstream Pipeline job(s)
    - Trigger:
        - Github PR comment `Jenkins (compile|test).*`

Other PR jobs

- PullRequest-LineEndingsCheck-`<repo>`
    - Description:
        - Checks the files modified in a pull request have correct line endings
    - Trigger:
        - Automatically builds on every PR
        - Retrigger with `Jenkins line endings check`

- PullRequest-signedOffByCheck-`<repo>`
    - Description:
        - Checks the commits in a pull request have proper sign-off
    - Trigger:
        - Trigger with `Jenkins signed off by check`

#### Pipelines

Pipelines for all platforms and versions are available [**here**](https://openj9-jenkins.osuosl.org/view/Pipelines/).

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
    - [![Build Status](https://openj9-jenkins.osuosl.org/buildStatus/icon?job=Pipeline-OMR-Acceptance)](https://openj9-jenkins.osuosl.org/job/Pipeline-OMR-Acceptance)
    - Description:
        - Compile and run sanity tests against new OMR content
        - Triggers:
            - Build-JDK`<version>`-`<platform>` and Test-Sanity-JDK`<version>`-`<platform>` across all platforms and versions
            - `Promote_OMR` once all testing is passed
    - Trigger: Triggered by `Mirror-OMR-to-OpenJ9-OMR`

#### Build

Build pipelines for all platforms and versions are available [**here**](https://openj9-jenkins.osuosl.org/view/Pipelines/).

- Any platform and version build pipeline:
    - Name: Build-JDK`<version>`-`<platform>`
    - Description:
        - Compiles Eclipse OpenJ9 on `<platform>` for `<version>`
        - Archives the SDK and test material for use in downstream jobs
    - Trigger:
        - This job is used in other pipelines but can be launched manually

**Note:** Windows 32 is available only for JDK8. Thus the only available build for win_x86 platform (Windows 32bits) is [Build_JDK8_x86-32_windows_Nightly](https://openj9-jenkins.osuosl.org/job/Build_JDK8_x86-32_windows_Nightly/).

#### Test

Test pipelines for all platforms and versions are available [**here**](https://openj9-jenkins.osuosl.org/view/Test/).

- Any platform and version build pipeline:
    - Name: Test-`<sanity|extended>.<functional|system>`-JDK`<version>`-`<platform>`
    - Description:
        - Runs sanity or extended tests
    - Trigger:
        - This job is used in other pipelines but can be launched manually

Mode details could be found on [Adoptium AQA Testing](https://github.com/adoptium/aqa-tests).
The [Running Adoptium AQA Tests](https://github.com/adoptium/aqa-tests/blob/master/doc/userGuide.md) provides further details on how to set up Jenkins test pipelines.

#### Infrastructure

Infrastructure pipelines are available [**here**](https://openj9-jenkins.osuosl.org/view/Infrastructure/)

- Mirror-OMR-to-OpenJ9-OMR
    - [![Build Status](https://openj9-jenkins.osuosl.org/buildStatus/icon?job=Mirror-OMR-to-OpenJ9-OMR)](https://openj9-jenkins.osuosl.org/job/Mirror-OMR-to-OpenJ9-OMR)
    - Description:
        - Mirrors [eclipse/omr/master](https://github.com/eclipse/omr/tree/master) to [eclipse-openj9/openj9-omr/master](https://github.com/eclipse-openj9/openj9-omr/tree/master)
        - Triggers `Pipeline-OMR-Acceptance` when there is new content
    - Trigger:
        - Build periodically, 15 minutes

- Promote_OMR
    - [![Build Status](https://openj9-jenkins.osuosl.org/buildStatus/icon?job=Promote_OMR)](https://openj9-jenkins.osuosl.org/job/Promote_OMR)
    - Description:
        - Promotes eclipse-openj9/openj9-omr branch master to branch openj9
    - Trigger:
        - Last step of `Pipeline-OMR-Acceptance`

- Deploy-OpenJ9-Website-to-Eclipse
    - [![Build Status](https://openj9-jenkins.osuosl.org/buildStatus/icon?job=Deploy-OpenJ9-Website-to-Eclipse)](https://openj9-jenkins.osuosl.org/job/Deploy-OpenJ9-Website-to-Eclipse)
    - Description:
        - Mirrors [github.com/eclipse-openj9/openj9-website](https://github.com/eclipse-openj9/openj9-website/tree/master) to the Eclipse.org repo
    - Trigger:
        - Poll Github repo for changes

### Adding Builds
- Always add pipeline style jobs so the code can be committed to the repo once it is ready
- Update this readme when your build is in production use
- Whenever possible, reuse existing builds or existing jenkins files
