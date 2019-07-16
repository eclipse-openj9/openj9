/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

def get_shas(OPENJDK_REPO, OPENJDK_BRANCH, OPENJ9_REPO, OPENJ9_BRANCH, OMR_REPO, OMR_BRANCH, VENDOR_TEST_REPOS_MAP=null, VENDOR_TEST_BRANCHES_MAP=null, VENDOR_TEST_SHAS_MAP=null) {
    // Get a set of SHAs for a standard OpenJ9 build
    def SHAS = [:]

    // check if the SHAs are set as build parameters
    // if not set, sniff the remote repositories references

    stage ('Sniff Repos') {
        def description = ''

        if (OPENJDK_SHA instanceof Map && OPENJDK_REPO instanceof Map && OPENJDK_BRANCH instanceof Map) {
            // fetch SHAs for all OpenJDK repositories
            def OPENJDK_SHAS_BY_RELEASES = [:]

            // e.g. OPENJDK_SHA = [ 8: [linux_x86-64: sha1, linux_x86-64_cmprssptrs: sha2, ...], 
            //                     10: [linux_x86-64: sha1, linux_x86-64_cmprssptrs: sha2, ...],...]
            OPENJDK_SHA.each { release, shas_by_specs ->
                def unique_shas = [:]

                shas_by_specs.each { spec, sha ->
                    def repoUrl = OPENJDK_REPO.get(release).get(spec)

                    if (!unique_shas.containsKey(repoUrl)) {
                        if (!sha) {
                            sha = get_repository_sha(repoUrl, OPENJDK_BRANCH.get(release).get(spec))
                        }
                        unique_shas.put(repoUrl, sha)

                        def repo_short_name = repoUrl.substring(repoUrl.lastIndexOf('/') + 1, repoUrl.indexOf('.git'))
                        description += "<br/>OpenJDK${release}: ${get_short_sha(sha)} - ${repo_short_name}"
                        echo "OPENJDK${release}_SHA:${sha} - ${repo_short_name}"
                    }
                }
                OPENJDK_SHAS_BY_RELEASES.put(release, unique_shas)
            }

            SHAS['OPENJDK'] = OPENJDK_SHAS_BY_RELEASES
        } else {
            // fetch SHA for an OpenJDK repository
            SHAS['OPENJDK'] = OPENJDK_SHA
            if (!SHAS['OPENJDK']) {
                SHAS['OPENJDK'] = get_repository_sha(OPENJDK_REPO, OPENJDK_BRANCH)
            }
            description += "<br/>OpenJDK: ${get_short_sha(SHAS['OPENJDK'])}"
            echo "OPENJDK_SHA:${SHAS['OPENJDK']}"
        }

        // fetch extensions SHAs
        SHAS['OPENJ9'] = OPENJ9_SHA
        if (!SHAS['OPENJ9'] && (OPENJ9_REPO && OPENJ9_BRANCH)) {
            SHAS['OPENJ9'] = get_repository_sha(OPENJ9_REPO, OPENJ9_BRANCH)
        }

        SHAS['OMR'] = OMR_SHA
        if (!SHAS['OMR'] && (OMR_REPO && OMR_BRANCH)){
            SHAS['OMR'] = get_repository_sha(OMR_REPO, OMR_BRANCH)
        }

        // Write the SHAs to the Build Description
        echo "OPENJ9_SHA:${SHAS['OPENJ9']}"
        echo "OMR_SHA:${SHAS['OMR']}"
        def TMP_DESC = (currentBuild.description) ? currentBuild.description + "<br>" : ""
        currentBuild.description = TMP_DESC + "OpenJ9: ${get_short_sha(SHAS['OPENJ9'])}<br/>OMR: ${get_short_sha(SHAS['OMR'])}${description}"

        if (VENDOR_TEST_REPOS_MAP && VENDOR_TEST_BRANCHES_MAP) {
            // fetch SHAs for vendor test repositories
            VENDOR_TEST_REPOS_MAP.each { repoName, vendorRepoURL ->
                if (!VENDOR_TEST_SHAS_MAP[repoName]) {
                    VENDOR_TEST_SHAS_MAP[repoName] = get_repository_sha(vendorRepoURL, VENDOR_TEST_BRANCHES_MAP[repoName])
                }

                // update build description
                currentBuild.description += "<br/>${repoName}: ${get_short_sha(VENDOR_TEST_SHAS_MAP[repoName])}"
                echo "${repoName}_SHA: ${VENDOR_TEST_SHAS_MAP[repoName]}"
            }

            // add vendor test repositories SHAs to the list
            SHAS['VENDOR_TEST'] = VENDOR_TEST_SHAS_MAP
        }

        return SHAS
    }
}

def get_repository_sha(REPO, BRANCH) {
   // use ssh-agent to avoid permission denied on private repositories
    if (USER_CREDENTIALS_ID != '') {
        return sshagent(credentials:["${USER_CREDENTIALS_ID}"]) {
            get_sha(REPO, BRANCH)
        }
    }

    return get_sha(REPO, BRANCH)
}

def get_sha(REPO, BRANCH) {
    // Get the SHA at the tip of the BRANCH in REPO.
    // Allows Pipelines to kick off multiple builds and have the same SHA built everywhere.
    return sh (
            // "git ls-remote $REPO" will return all refs, adding "$BRANCH" will only return the specific branch we are interested in
            // return the full 40 characters sha instead of the short version 
            // to avoid errors due to short sha ambiguousness due to multiple matches for a short sha
            script: "git ls-remote $REPO refs/heads/$BRANCH | cut -c1-40",
            returnStdout: true
        ).trim()
}

def get_short_sha(SHA) {
    if (SHA) {
        // return the first 7 characters of a given SHA.
        return SHA.take(7)
    }

    return SHA
}

def git_push_auth(REPO, OPTION, CRED_ID) {
    REPO -= 'https://'
    withCredentials([usernamePassword(credentialsId: "${CRED_ID}", usernameVariable: 'USERNAME', passwordVariable: 'PASSWORD')]) {
        sh "git push https://${USERNAME}:${PASSWORD}@${REPO} ${OPTION}"
    }
}

/*
* Set Github Commit Status
*
* Args:
* - REPO: Repository to update status
* - CONTEXT: Usually Build Name
* - SHA: Git commit to update
* - URL: Link to build details
* - STATE: Build Result, one of PENDING, SUCCESS, FAILURE, ERROR
* - MESSAGE: Build result message
*/
def set_build_status(REPO, CONTEXT, SHA, URL, STATE, MESSAGE) {
    step([
        $class: "GitHubCommitStatusSetter",
        reposSource: [$class: "ManuallyEnteredRepositorySource", url: REPO],
        contextSource: [$class: "ManuallyEnteredCommitContextSource", context: CONTEXT],
        errorHandlers: [[$class: "ChangingBuildStatusErrorHandler", result: "UNSTABLE"]],
        commitShaSource: [$class: "ManuallyEnteredShaSource", sha: SHA ],
        statusBackrefSource: [$class: "ManuallyEnteredBackrefSource", backref: URL],
        statusResultSource: [$class: "ConditionalStatusResultSource", results: [[$class: "AnyBuildResult", message: MESSAGE, state: STATE]] ]
    ]);
}


def cancel_running_builds(JOB_NAME, BUILD_IDENTIFIER) {
    echo "Looking to stop running jobs:'${JOB_NAME}' from build:'${BUILD_IDENTIFIER}'"
    Jenkins.instance.getItemByFullName(JOB_NAME, Job.class)._getRuns().each { number, run ->
        // get all runs/jobs from the build
        // Class org.jenkinsci.plugins.workflow.job.WorkflowRun
        // Only look for running jobs with matching BUILD_IDENTIFIER
        if ((run) && (run.isBuilding()) && (run.getEnvironment().get('BUILD_IDENTIFIER') == "${BUILD_IDENTIFIER}")) {
            echo "Cancelling Job:'${run}'"
            run.doStop()
        }
    }
    echo "Done stopping jobs"
}

def build(BUILD_JOB_NAME, OPENJDK_REPO, OPENJDK_BRANCH, OPENJDK_SHA, OPENJ9_REPO, OPENJ9_BRANCH, OPENJ9_SHA, OMR_REPO, OMR_BRANCH, OMR_SHA, VARIABLE_FILE, VENDOR_REPO, VENDOR_BRANCH, VENDOR_CREDENTIALS_ID, NODE, SETUP_LABEL, BUILD_IDENTIFIER, ghprbGhRepository, ghprbActualCommit, GITHUB_SERVER, EXTRA_GETSOURCE_OPTIONS, EXTRA_CONFIGURE_OPTIONS, EXTRA_MAKE_OPTIONS, OPENJDK_CLONE_DIR, CUSTOM_DESCRIPTION, ghprbPullId, ghprbCommentBody, ghprbTargetBranch) {
    stage ("${BUILD_JOB_NAME}") {
        SCM_BRANCH = (ghprbPullId && ghprbGhRepository == GHPRB_REPO_OPENJ9) ? "origin/pr/${ghprbPullId}/merge" : 'refs/heads/master'
        SCM_REFSPEC = (ghprbPullId && ghprbGhRepository == GHPRB_REPO_OPENJ9) ? "+refs/pull/${ghprbPullId}/merge:refs/remotes/origin/pr/${ghprbPullId}/merge" : ''
        return build_with_slack(BUILD_JOB_NAME, ghprbGhRepository, ghprbActualCommit, GITHUB_SERVER,
            [string(name: 'OPENJDK_REPO', value: OPENJDK_REPO),
            string(name: 'OPENJDK_BRANCH', value: OPENJDK_BRANCH),
            string(name: 'OPENJDK_SHA', value: OPENJDK_SHA),
            string(name: 'OPENJ9_REPO', value: OPENJ9_REPO),
            string(name: 'OPENJ9_BRANCH', value: OPENJ9_BRANCH),
            string(name: 'OPENJ9_SHA', value: OPENJ9_SHA),
            string(name: 'OMR_REPO', value: OMR_REPO),
            string(name: 'OMR_BRANCH', value: OMR_BRANCH),
            string(name: 'OMR_SHA', value: OMR_SHA),
            string(name: 'VARIABLE_FILE', value: VARIABLE_FILE),
            string(name: 'VENDOR_REPO', value: VENDOR_REPO),
            string(name: 'VENDOR_BRANCH', value: VENDOR_BRANCH),
            string(name: 'VENDOR_CREDENTIALS_ID', value: VENDOR_CREDENTIALS_ID),
            string(name: 'NODE', value: NODE),
            string(name: 'SETUP_LABEL', value: SETUP_LABEL),
            string(name: 'BUILD_IDENTIFIER', value: BUILD_IDENTIFIER),
            string(name: 'EXTRA_GETSOURCE_OPTIONS', value: EXTRA_GETSOURCE_OPTIONS),
            string(name: 'EXTRA_CONFIGURE_OPTIONS', value: EXTRA_CONFIGURE_OPTIONS),
            string(name: 'EXTRA_MAKE_OPTIONS', value: EXTRA_MAKE_OPTIONS),
            string(name: 'OPENJDK_CLONE_DIR', value: OPENJDK_CLONE_DIR),
            string(name: 'CUSTOM_DESCRIPTION', value: CUSTOM_DESCRIPTION),
            string(name: 'ghprbPullId', value: ghprbPullId),
            string(name: 'ghprbGhRepository', value: ghprbGhRepository),
            string(name: 'ghprbCommentBody', value: ghprbCommentBody),
            string(name: 'ghprbTargetBranch', value: ghprbTargetBranch),
            string(name: 'SCM_BRANCH', value: SCM_BRANCH),
            string(name: 'SCM_REFSPEC', value: SCM_REFSPEC)])
    }
}

def build_with_one_upstream(JOB_NAME, UPSTREAM_JOB_NAME, UPSTREAM_JOB_NUMBER, NODE, OPENJ9_REPO, OPENJ9_BRANCH, OPENJ9_SHA, VENDOR_TEST_REPOS, VENDOR_TEST_BRANCHES, VENDOR_TEST_SHAS, VENDOR_TEST_DIRS, USER_CREDENTIALS_ID, TEST_FLAG, BUILD_IDENTIFIER, ghprbGhRepository, ghprbActualCommit, GITHUB_SERVER, ADOPTOPENJDK_REPO, ADOPTOPENJDK_BRANCH) {
    stage ("${JOB_NAME}") {
        return build_with_slack(JOB_NAME, ghprbGhRepository, ghprbActualCommit, GITHUB_SERVER,
            [string(name: 'UPSTREAM_JOB_NAME', value: UPSTREAM_JOB_NAME),
            string(name: 'UPSTREAM_JOB_NUMBER', value: "${UPSTREAM_JOB_NUMBER}"),
            string(name: 'LABEL', value: NODE),
            string(name: 'ADOPTOPENJDK_REPO', value: ADOPTOPENJDK_REPO),
            string(name: 'ADOPTOPENJDK_BRANCH', value: ADOPTOPENJDK_BRANCH),
            string(name: 'OPENJ9_REPO', value: OPENJ9_REPO),
            string(name: 'OPENJ9_BRANCH', value: OPENJ9_BRANCH),
            string(name: 'OPENJ9_SHA', value: OPENJ9_SHA),
            string(name: 'VENDOR_TEST_REPOS', value: VENDOR_TEST_REPOS),
            string(name: 'VENDOR_TEST_BRANCHES', value: VENDOR_TEST_BRANCHES),
            string(name: 'VENDOR_TEST_SHAS', value: VENDOR_TEST_SHAS),
            string(name: 'VENDOR_TEST_DIRS', value: VENDOR_TEST_DIRS),
            string(name: 'USER_CREDENTIALS_ID', value: USER_CREDENTIALS_ID),
            string(name: 'TEST_FLAG', value: TEST_FLAG),
            string(name: 'KEEP_REPORTDIR', value: 'false'),
            string(name: 'BUILD_IDENTIFIER', value: BUILD_IDENTIFIER)])

    }
}

def build_with_artifactory(JOB_NAME, NODE, OPENJ9_REPO, OPENJ9_BRANCH, OPENJ9_SHA, VENDOR_TEST_REPOS, VENDOR_TEST_BRANCHES, VENDOR_TEST_SHAS, VENDOR_TEST_DIRS, USER_CREDENTIALS_ID, CUSTOMIZED_SDK_URL, ARTIFACTORY_CREDS, TEST_FLAG, BUILD_IDENTIFIER, ghprbGhRepository, ghprbActualCommit, GITHUB_SERVER, ADOPTOPENJDK_REPO, ADOPTOPENJDK_BRANCH) {
    stage ("${JOB_NAME}") {
        return build_with_slack(JOB_NAME, ghprbGhRepository, ghprbActualCommit, GITHUB_SERVER,
            [string(name: 'LABEL', value: NODE),
            string(name: 'ADOPTOPENJDK_REPO', value: ADOPTOPENJDK_REPO),
            string(name: 'ADOPTOPENJDK_BRANCH', value: ADOPTOPENJDK_BRANCH),
            string(name: 'OPENJ9_REPO', value: OPENJ9_REPO),
            string(name: 'OPENJ9_BRANCH', value: OPENJ9_BRANCH),
            string(name: 'OPENJ9_SHA', value: OPENJ9_SHA),
            string(name: 'VENDOR_TEST_REPOS', value: VENDOR_TEST_REPOS),
            string(name: 'VENDOR_TEST_BRANCHES', value: VENDOR_TEST_BRANCHES),
            string(name: 'VENDOR_TEST_SHAS', value: VENDOR_TEST_SHAS),
            string(name: 'VENDOR_TEST_DIRS', value: VENDOR_TEST_DIRS),
            string(name: 'USER_CREDENTIALS_ID', value: USER_CREDENTIALS_ID),
            string(name: 'CUSTOMIZED_SDK_URL', value: CUSTOMIZED_SDK_URL),
            string(name: 'CUSTOMIZED_SDK_URL_CREDENTIAL_ID', value: ARTIFACTORY_CREDS),
            string(name: 'TEST_FLAG', value: TEST_FLAG),
            string(name: 'KEEP_REPORTDIR', value: 'false'),
            string(name: 'BUILD_IDENTIFIER', value: BUILD_IDENTIFIER)])
    }
}

def get_causes(job) {
    def cause_string = ''
    for (cause in job.rawBuild.getCauses()) {
        cause_string += get_causes_recursively(cause)
    }
    return cause_string
}

def get_causes_recursively(cause) {
    def cause_string = cause.getShortDescription()
    if (cause instanceof hudson.model.Cause.UpstreamCause) {
        for (upCause in cause.getUpstreamCauses()) {
            cause_string += '\n' + get_causes_recursively(upCause)
        }
    }
    return cause_string
}

def build_with_slack(DOWNSTREAM_JOB_NAME, ghprbGhRepository, ghprbActualCommit, GITHUB_SERVER, PARAMETERS) {
    GITHUB_REPO = "https://${GITHUB_SERVER}/${ghprbGhRepository}"
    // Set Github Commit Status
    if (ghprbActualCommit) {
        node('master') {
            set_build_status(GITHUB_REPO, DOWNSTREAM_JOB_NAME, ghprbActualCommit, BUILD_URL, 'PENDING', "Build Started")
        }
    }

    def JOB = build job: DOWNSTREAM_JOB_NAME, parameters: PARAMETERS, propagate: false
    def DOWNSTREAM_JOB_NUMBER = JOB.getNumber()
    def DOWNSTREAM_JOB_URL = JOB.getAbsoluteUrl()
    def DOWNSTREAM_JOB_TIME = JOB.getDurationString()

    if (JOB.resultIsWorseOrEqualTo('UNSTABLE')) {
        // Get build causes
        build_causes_string = get_causes(currentBuild)

        if ((JOB.result == "UNSTABLE") || (JOB.result == "ABORTED")) { // Job unstable (YELLOW) or aborted (GREY)
            // If the job was ABORTED it either was killed by someone or
            // killed by cancel_running_builds(). In this case we don't
            // want to offer the restart option as it is not desired for PR builds.
            // If the job was UNSTABLE this indicates test(s) failed and we don't want to offer the restart option.
            echo "WARNING: Downstream job ${DOWNSTREAM_JOB_NAME} is ${JOB.result} after ${DOWNSTREAM_JOB_TIME}. Job Number: ${DOWNSTREAM_JOB_NUMBER} Job URL: ${DOWNSTREAM_JOB_URL}"
            if (JOB.result == "UNSTABLE") {
                unstable "Setting overall pipeline status to UNSTABLE"
            } else {
                currentBuild.result = JOB.result
            }
            if (SLACK_CHANNEL) {
                slackSend channel: SLACK_CHANNEL, color: 'warning', message: "${JOB.result}: ${DOWNSTREAM_JOB_NAME} #${DOWNSTREAM_JOB_NUMBER} (<${DOWNSTREAM_JOB_URL}|Open>)\nStarted by ${JOB_NAME} #${BUILD_NUMBER} (<${BUILD_URL}|Open>)\n${build_causes_string}"
            }
            // Set Github Commit Status
            if (ghprbActualCommit) {
                node('master') {
                    set_build_status(GITHUB_REPO, DOWNSTREAM_JOB_NAME, ghprbActualCommit, DOWNSTREAM_JOB_URL, 'FAILURE', "Build ${JOB.result}")
                }
            }
        } else { // Job failed (RED)
            if (SLACK_CHANNEL) {
                slackSend channel: SLACK_CHANNEL, color: 'danger', message: "Failure: ${DOWNSTREAM_JOB_NAME} #${DOWNSTREAM_JOB_NUMBER} (<${DOWNSTREAM_JOB_URL}|Open>)\nStarted by ${JOB_NAME} #${BUILD_NUMBER} (<${BUILD_URL}|Open>)\n${build_causes_string}\nWould you like to restart the job? (<${RUN_DISPLAY_URL}|Open>)"
            }
            // Set Github Commit Status
            if (ghprbActualCommit) {
                node('master') {
                    set_build_status(GITHUB_REPO, DOWNSTREAM_JOB_NAME, ghprbActualCommit, DOWNSTREAM_JOB_URL, 'FAILURE', "Build FAILED")
                }
            }
            timeout(time: RESTART_TIMEOUT.toInteger(), unit: RESTART_TIMEOUT_UNITS) {
                input message: "Downstream job ${DOWNSTREAM_JOB_NAME} failed after ${DOWNSTREAM_JOB_TIME}. Job Number: ${DOWNSTREAM_JOB_NUMBER} Job URL: ${DOWNSTREAM_JOB_URL}\nRestart failed job '${DOWNSTREAM_JOB_NAME}'?", ok: 'Restart'
                // If restart is aborted or is timed-out, an error is thrown
            }
            // If restart is approved, recursively call this function until we get a pass or a restart-rejection
            return build_with_slack(DOWNSTREAM_JOB_NAME, ghprbGhRepository, ghprbActualCommit, GITHUB_SERVER, PARAMETERS)
        }
    } else {
        echo "Downstream job ${DOWNSTREAM_JOB_NAME} PASSED after ${DOWNSTREAM_JOB_TIME}"
        // Set Github Commit Status
        if (ghprbActualCommit) {
            node('master') {
                set_build_status(GITHUB_REPO, DOWNSTREAM_JOB_NAME, ghprbActualCommit, DOWNSTREAM_JOB_URL, 'SUCCESS', "Build PASSED")
            }
        }
    }
    return JOB
}

def workflow(SDK_VERSION, SPEC, SHAS, OPENJDK_REPO, OPENJDK_BRANCH, OPENJ9_REPO, OPENJ9_BRANCH, OMR_REPO, OMR_BRANCH, TESTS_TARGETS, VENDOR_TEST_REPOS_MAP, VENDOR_TEST_BRANCHES_MAP, VENDOR_TEST_DIRS_MAP, USER_CREDENTIALS_ID, SETUP_LABEL, ghprbGhRepository, ghprbActualCommit, EXTRA_GETSOURCE_OPTIONS, EXTRA_CONFIGURE_OPTIONS, EXTRA_MAKE_OPTION, OPENJDK_CLONE_DIR, ADOPTOPENJDK_REPO, ADOPTOPENJDK_BRANCH, BUILD_JOB_NAME, CUSTOM_DESCRIPTION) {
    def jobs = [:]

    // Set ghprbGhRepository and ghprbActualCommit for the purposes of Github commit status updates
    // Only for Nightly build
    // PR build will already have them set
    GITHUB_SERVER = OPENJ9_REPO.substring(OPENJ9_REPO.indexOf('github.'), OPENJ9_REPO.indexOf('.com') +4)
    if (BUILD_IDENTIFIER.toLowerCase() == "nightly") {
        ghprbGhRepository = OPENJ9_REPO.substring(OPENJ9_REPO.indexOf('.com') +5, OPENJ9_REPO.indexOf('.git'))
        ghprbActualCommit = SHAS['OPENJ9']
    }
    echo "Repo:${ghprbGhRepository}, Commit:${ghprbActualCommit}, GITHUB_SERVER:${GITHUB_SERVER}"

    // compile the source and build the SDK
    jobs["build"] = build(BUILD_JOB_NAME, OPENJDK_REPO, OPENJDK_BRANCH, SHAS['OPENJDK'], OPENJ9_REPO, OPENJ9_BRANCH, SHAS['OPENJ9'], OMR_REPO, OMR_BRANCH, SHAS['OMR'], params.VARIABLE_FILE, params.VENDOR_REPO, params.VENDOR_BRANCH, params.VENDOR_CREDENTIALS_ID, params.BUILD_NODE, SETUP_LABEL, BUILD_IDENTIFIER, ghprbGhRepository, ghprbActualCommit, GITHUB_SERVER, EXTRA_GETSOURCE_OPTIONS, EXTRA_CONFIGURE_OPTIONS, EXTRA_MAKE_OPTIONS, OPENJDK_CLONE_DIR, CUSTOM_DESCRIPTION, ghprbPullId, ghprbCommentBody, ghprbTargetBranch)

    // Determine if Build job archived to Artifactory
    def BUILD_JOB_ENV = jobs["build"].getBuildVariables()
    ARTIFACTORY_CREDS = ''
    echo "BUILD_JOB_ENV:'${BUILD_JOB_ENV}'"
    if (BUILD_JOB_ENV['CUSTOMIZED_SDK_URL']) {
        CUSTOMIZED_SDK_URL = BUILD_JOB_ENV['CUSTOMIZED_SDK_URL']
        ARTIFACTORY_CREDS = BUILD_JOB_ENV['ARTIFACTORY_CREDS']
        ARTIFACTORY_SERVER = BUILD_JOB_ENV['ARTIFACTORY_SERVER']
        ARTIFACTORY_REPO = BUILD_JOB_ENV['ARTIFACTORY_REPO']
        ARTIFACTORY_NUM_ARTIFACTS = BUILD_JOB_ENV['ARTIFACTORY_NUM_ARTIFACTS']
        ARTIFACTORY_MANUAL_CLEANUP = BUILD_JOB_ENV['ARTIFACTORY_MANUAL_CLEANUP']
        echo "Passing CUSTOMIZED_SDK_URL:'${CUSTOMIZED_SDK_URL}'"
        echo "Using ARTIFACTORY_CREDS:'${ARTIFACTORY_CREDS}'"
        echo "Using ARTIFACTORY_SERVER:'${ARTIFACTORY_SERVER}'"
        echo "Passing ARTIFACTORY_MANUAL_CLEANUP:'${ARTIFACTORY_MANUAL_CLEANUP}'"

        cleanup_artifactory(ARTIFACTORY_MANUAL_CLEANUP, BUILD_JOB_NAME, ARTIFACTORY_SERVER, ARTIFACTORY_REPO, ARTIFACTORY_NUM_ARTIFACTS)
    }

    if (TESTS_TARGETS.trim() != "none") {
        def testjobs = [:]
        def TARGET_NAMES = get_test_target_names()

        if (SHAS['VENDOR_TEST']) {
            // the downstream job is expecting comma separated SHAs
            // convert vendor shas map to a string
            VENDOR_TEST_SHAS = SHAS['VENDOR_TEST'].values().join(',')
        }

        echo "Using VENDOR_TEST_REPOS = ${VENDOR_TEST_REPOS}, VENDOR_TEST_BRANCHES = ${VENDOR_TEST_BRANCHES}, VENDOR_TEST_SHAS = ${VENDOR_TEST_SHAS}, VENDOR_TEST_DIRS = ${VENDOR_TEST_DIRS}"

        // For PullRequest Builds, overwrite the OpenJ9 sha for test jobs so they checkout the PR (OpenJ9 PRs only)
        if (params.ghprbPullId && params.ghprbGhRepository == GHPRB_REPO_OPENJ9) {
            SHAS['OPENJ9'] = "origin/pr/${params.ghprbPullId}/merge"
        }

        for (name in TARGET_NAMES) {
            // Checking to see if the test should be excluded
            if (EXCLUDED_TESTS.contains(get_target_name(name))){
                echo "The '${name}' test suite will be excluded"
                continue
            }
            def TEST_FLAG = (name.contains('+')) ? name.substring(name.indexOf('+')+1).toUpperCase() : ''
            def TEST_JOB_NAME = get_test_job_name(get_target_name(name), SPEC, SDK_VERSION, BUILD_IDENTIFIER)

            testjobs["${TEST_JOB_NAME}"] = {
                if (params.ghprbPullId) {
                    cancel_running_builds(TEST_JOB_NAME, BUILD_IDENTIFIER)
                }
                if (ARTIFACTORY_CREDS) {
                    cleanup_artifactory(ARTIFACTORY_MANUAL_CLEANUP, TEST_JOB_NAME, ARTIFACTORY_SERVER, ARTIFACTORY_REPO, ARTIFACTORY_NUM_ARTIFACTS)
                    jobs["${TEST_JOB_NAME}"] = build_with_artifactory(TEST_JOB_NAME, TEST_NODE, OPENJ9_REPO, OPENJ9_BRANCH, SHAS['OPENJ9'], VENDOR_TEST_REPOS, VENDOR_TEST_BRANCHES, VENDOR_TEST_SHAS, VENDOR_TEST_DIRS, USER_CREDENTIALS_ID, CUSTOMIZED_SDK_URL, ARTIFACTORY_CREDS, TEST_FLAG, BUILD_IDENTIFIER, ghprbGhRepository, ghprbActualCommit, GITHUB_SERVER, ADOPTOPENJDK_REPO, ADOPTOPENJDK_BRANCH)
                } else {
                    jobs["${TEST_JOB_NAME}"] = build_with_one_upstream(TEST_JOB_NAME, BUILD_JOB_NAME, jobs["build"].getNumber(), TEST_NODE, OPENJ9_REPO, OPENJ9_BRANCH, SHAS['OPENJ9'], VENDOR_TEST_REPOS, VENDOR_TEST_BRANCHES, VENDOR_TEST_SHAS, VENDOR_TEST_DIRS, USER_CREDENTIALS_ID, TEST_FLAG, BUILD_IDENTIFIER, ghprbGhRepository, ghprbActualCommit, GITHUB_SERVER, ADOPTOPENJDK_REPO, ADOPTOPENJDK_BRANCH)
                }
            }
        }
        if (params.AUTOMATIC_GENERATION != 'false'){
            generate_test_jobs(TARGET_NAMES, SPEC, ARTIFACTORY_SERVER, ARTIFACTORY_REPO)
        }
        parallel testjobs
    }

    // return jobs for further reference
    return jobs
}

def get_test_target_names() {
    def targetNames = []

    if (TESTS_TARGETS && TESTS_TARGETS.trim() != 'none') {
        for (target in TESTS_TARGETS.trim().replaceAll("\\s","").toLowerCase().tokenize(',')) {
            if (VARIABLES.tests_targets && VARIABLES.tests_targets."${target}") {
                targetNames.addAll(VARIABLES.tests_targets."${target}".keySet())
            } else {
                targetNames.add(target)
            }
        }
    }

    return targetNames
}

def get_target_name(name) {
    if (name.contains('+')) {
        name = name.substring(0, name.indexOf('+'))
    }

    return name
}

def get_build_job_name(spec, version, identifier) {
    id = convert_build_identifier(identifier)
    return "Build_JDK${version}_${spec}_${id}"
}

def get_test_job_name(targetName, spec, version, identifier) {
    spec = strip_cmake_specs(spec)
    id = convert_build_identifier(identifier)
    return "Test_openjdk${version}_j9_${targetName}_${spec}_${id}"
}

def convert_build_identifier(val) {
    switch (val) {
        case "Nightly":
        case "Release":
            return val
            break
        case ~/^.*-Acceptance$/:
            return val.substring(0,val.indexOf('-'))
            break
        default:
            return "Personal"
    }
}
/*
* Finds the downstream builds of a build.
* Limits the search only to the downstream builds with given names.
* Returns a map of builds.
*/
def get_downstream_builds(upstreamBuild, upstreamJobName, downstreamJobNames) {
    def downstreamBuilds = [:]
    def pattern = /.*${upstreamJobName}.*${upstreamBuild.number}.*/
    def startTime = System.currentTimeMillis()

    for (name in downstreamJobNames.sort()) {
        def job = Jenkins.getInstance().getItemByFullName(name)
        if (job) {
            def builds = [:]
            //find downstream jobs
            for (build in job.getBuilds().byTimestamp(upstreamBuild.getStartTimeInMillis(), System.currentTimeMillis())) {
                if ((build && build.getCause(hudson.model.Cause.UpstreamCause) && build.getCause(hudson.model.Cause.UpstreamCause).upstreamRun)
                    && (build.getCause(hudson.model.Cause.UpstreamCause).upstreamRun==~pattern)) {
                        // cache all builds (in case of multiple runs)
                        builds.put(build.getNumber(), build)
                }
            }

            // fetch last build
            if (!builds.isEmpty()) {
                lastBuildId = builds.keySet().max()
                downstreamBuilds.put(name, builds.get(lastBuildId))
            }
        }
    }

    elapsedTime = (System.currentTimeMillis() - startTime)/1000
    echo "Time spent fetching downstream jobs for ${upstreamJobName}: ${elapsedTime} seconds"

    return downstreamBuilds
}

/*
* Returns the build status icon.
* Requires Embeddable Build Status plugin.
*/
def get_build_embedded_status_link(build) {
    if (build) {
        def buildLink = "${JENKINS_URL}${build.getUrl()}"
        return "<a href=\"${buildLink}\"><img src=\"${buildLink}badge/icon\"></a>"
    }
    return ''
}

/*
* Returns a map storing the downstream job names for given version and spec.
*/
def get_downstream_job_names(spec, version, identifier) {
    /* e.g. [build:                 Build-JDK11-linux_390-64_cmprssptrs,
             extended.functional:   Test-extended.functional-JDK11-linux_390-64_cmprssptrs,
             extended.system:       Test-extended.system-JDK11-linux_390-64_cmprssptrs,
             sanity.functional:     Test-sanity.functional-JDK11-linux_390-64_cmprssptrs,
             sanity.system:         Test-sanity.system-JDK11-linux_390-64_cmprssptrs]
    */

    downstreamJobNames = [:]
    downstreamJobNames.put('build', get_build_job_name(spec, version, identifier))

    for (target in get_test_target_names().sort()) {
        target = get_target_name(target)
        downstreamJobNames.put(target, get_test_job_name(target, spec, version, identifier))
    }

    return downstreamJobNames
}

def cleanup_artifactory(artifactory_manual_cleanup, job_name, artifactory_server, artifactory_repo, artifactory_num_artifacts){
    if (artifactory_manual_cleanup == 'true'){
        try {
            def cleanup_job_params = [
                string(name: 'JOB_TYPE', value: 'COUNT'),
                string(name: 'JOB_TO_CHECK', value: job_name),
                string(name: 'ARTIFACTORY_SERVER', value: artifactory_repo),
                string(name: 'ARTIFACTORY_REPO', value: artifactory_repo),
                string(name: 'ARTIFACTORY_NUM_ARTIFACTS', value: artifactory_num_artifacts)]

            build job: 'Cleanup_Artifactory', parameters: cleanup_job_params, wait: false
        } catch (any){
            echo 'The Cleanup_Artifactory job is not available'
        }
    }
}

def generate_test_jobs(TARGET_NAMES, SPEC, ARTIFACTORY_SERVER, ARTIFACTORY_REPO){
    def levels = []
    def groups = []

    TARGET_NAMES.each { target ->
        def target_name = get_target_name(target)
        if (!EXCLUDED_TESTS.contains(target_name)){
            def split_target = target_name.tokenize('.')
            levels.add(split_target[0])
            groups.add(split_target[1])
        }
    }
    levels.unique(true)
    groups.unique(true)

    spec = strip_cmake_specs(SPEC)
    if (levels && groups) {
        def parameters = [
            string(name: 'LEVELS', value: levels.join(',')),
            string(name: 'GROUPS', value: groups.join(',')),
            string(name: 'JDK_VERSIONS', value: SDK_VERSION),
            string(name: 'SUFFIX', value: "_${convert_build_identifier(BUILD_IDENTIFIER)}"),
            string(name: 'ARCH_OS_LIST', value: spec),
            string(name: 'JDK_IMPL', value: 'openj9'),
            string(name: 'ARTIFACTORY_SERVER', value: ARTIFACTORY_SERVER),
            string(name: 'ARTIFACTORY_REPO', value: ARTIFACTORY_REPO),
            string(name: 'BUILDS_TO_KEEP', value: DISCARDER_NUM_BUILDS)
        ]
        build job: 'Test_Job_Auto_Gen', parameters: parameters, propagate: false
    }
}

def setup_pull_request() {
    // Parse Github trigger comment
    // Jenkins test sanity <platform>*
    // Jenkins test extended <platform>*
    // Jenkins compile <platform>*
    // Jenkins test <comma_separated_test_target> <platform> *
    /*
    * Comment indexes - '+' is an offset of 0/1 if running tests or not
    * 0 - Jenkins
    * 1 - test / compile
    * 1+ - test target
    * 2+ - platforms
    * 3+ - jdk versions
    *
    * Note: Depends logic is already part of the build/compile job and is located in the checkout_pullrequest() function.
    */
    def PARSED_COMMENT = params.ghprbCommentBody.toLowerCase().tokenize(' ')
    // Don't both checking PARSED_COMMENT[0] since it is hardcoded in the trigger regex of the Jenkins job.

    // Setup TESTS_TARGETS
    switch ("${PARSED_COMMENT[1]}") {
        case "compile":
            if (PARSED_COMMENT.size() < 4) {
                error("Pull Request trigger comment needs to specify PLATFORM and SDK_VERSION")
            }
            TESTS_TARGETS = 'none'
            OFFSET = 0
            break
        case "test":
            if (PARSED_COMMENT.size() < 5) {
                error("Pull Request trigger comment needs to specify TESTS_TARGET, PLATFORM and SDK_VERSION")
            }
            TESTS_TARGETS = "${PARSED_COMMENT[2]}"
            if ("${TESTS_TARGETS}" == "all") {
                error("Test Target 'all' not allowed for Pull Request builds")
            }
            OFFSET = 1
            break
        default:
            error("Unable to parse trigger comment. Unknown build option:'${PARSED_COMMENT[1]}'")
    }
    echo "TESTS_TARGETS:'${TESTS_TARGETS}'"

    // Setup JDK VERSIONS
    switch (ghprbGhRepository) {
        case   ~/.*openj9-openjdk-jdk.*/:
            TMP_VERSION = ghprbGhRepository.substring(ghprbGhRepository.indexOf('-jdk')+4)
            if ("${TMP_VERSION}" == "") {
                TMP_VERSION = 'next'
            }
            RELEASES.add(TMP_VERSION)
            MIN_COMMENT_SIZE = 3
            break
        case "eclipse/openj9":
            TMP_VERSIONS = PARSED_COMMENT[3+OFFSET].tokenize(',')
            TMP_VERSIONS.each { VERSION ->
                echo "VERSION:'${VERSION}'"
                if ("${VERSION}" == "all") {
                    error("JDK version 'all' not allowed for Pull Request builds\nExpected 'jdkX' where X is one of ${CURRENT_RELEASES}")
                }
                if (!("${VERSION}" ==~ /jdk.*/)) {
                    error("Incorrect syntax for requested JDK version:'${VERSION}'\nExpected 'jdkX' where X is one of ${CURRENT_RELEASES}")
                }
                if (CURRENT_RELEASES.contains(VERSION.substring(3))) {
                    RELEASES.add(VERSION.substring(3))
                } else {
                    error("Unsupported JDK version or incorrect syntax:'${VERSION}'\nExpected 'jdkX' where X is one of ${CURRENT_RELEASES}")
                }
            }
            MIN_COMMENT_SIZE = 4
            break
        default:
            error("Unknown source repo:'${ghprbGhRepository}'")
    }

    echo "RELEASES:'${RELEASES}'"

    if (PARSED_COMMENT.size() < MIN_COMMENT_SIZE) { error("Pull Request trigger comment does not contain enough elements.")}

    // Setup PLATFORMS
    PARSED_PLATFORMS = PARSED_COMMENT[2+OFFSET].tokenize(',')
    PARSED_PLATFORMS.each { SHORT ->
        LONG_PLATFORM = SHORT_NAMES["${SHORT}"]
        if (LONG_PLATFORM) {
            PLATFORMS.addAll(LONG_PLATFORM)
        } else {
            error("Unknown PLATFORM short:'${SHORT}'\nExpected one of:${SHORT_NAMES}")
        }
    }
    echo "PLATFORMS:'${PLATFORMS}'"

    PERSONAL_BUILD = 'true'
}

def strip_cmake_specs(spec) {
    // Strip off cmake from SPEC name
    // Since cmake is a method for how the SDK is built,
    // from a test perspective it is the same sdk. Therefore
    // we will reuse the same test jobs as the non-cmake specs.
    return spec - '_cm'
}
return this
