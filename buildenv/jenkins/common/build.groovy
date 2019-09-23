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
import groovy.json.JsonSlurper;

def get_source() {
    stage('Get Source') {
        // Setup REPO variables
        OPENJ9_REPO_OPTION = (OPENJ9_REPO != "") ? "-openj9-repo=${OPENJ9_REPO}" : ""
        OPENJ9_BRANCH_OPTION = (OPENJ9_BRANCH != "") ? "-openj9-branch=${OPENJ9_BRANCH}" : ""
        OPENJ9_SHA_OPTION = (OPENJ9_SHA != "") ? "-openj9-sha=${OPENJ9_SHA}" : ""
        OMR_REPO_OPTION = (OMR_REPO != "") ? "-omr-repo=${OMR_REPO}" : ""
        OMR_BRANCH_OPTION = (OMR_BRANCH != "")? "-omr-branch=${OMR_BRANCH}" : ""
        OMR_SHA_OPTION = (OMR_SHA != "") ? "-omr-sha=${OMR_SHA}" : ""

        // use sshagent with Jenkins credentials ID for all platforms except zOS
        // on zOS use the user's ssh key
        if (!SPEC.contains('zos') && (USER_CREDENTIALS_ID != '')) {
            get_sources_with_authentication()
        } else {
            get_sources()
        }
    }
}

def get_sources_with_authentication() {
    sshagent(credentials:["${USER_CREDENTIALS_ID}"]) {
        get_sources()
    }
}

def get_sources() {
    // Temp workaround for Windows clones
    // See #3633 and JENKINS-54612
    if (NODE_LABELS.contains("windows") || NODE_LABELS.contains("zos")) {
        cleanWs()
        CLONE_CMD = "git clone --reference ${OPENJDK_REFERENCE_REPO} -b ${OPENJDK_BRANCH} ${OPENJDK_REPO} ."
        if (USER_CREDENTIALS_ID && NODE_LABELS.contains("windows")) {
            sshagent(credentials:["${USER_CREDENTIALS_ID}"]) {
                sh "${CLONE_CMD}"
            }
        } else {
            sh "${CLONE_CMD}"
        }
    } else {
        def remote_config_parameters = [url: "${OPENJDK_REPO}"]
        if (USER_CREDENTIALS_ID) {
            remote_config_parameters.put('credentialsId', "${USER_CREDENTIALS_ID}")
        }

        checkout changelog: false,
                poll: false,
                scm: [$class: 'GitSCM',
                    branches: [[name: "${OPENJDK_BRANCH}"]],
                    doGenerateSubmoduleConfigurations: false,
                    extensions: [[$class: 'CheckoutOption', timeout: 30],
                                [$class: 'CloneOption',
                                    depth: 0,
                                    noTags: false,
                                    reference: "${OPENJDK_REFERENCE_REPO}",
                                    shallow: false,
                                    timeout: 30]],
                    submoduleCfg: [],
                    userRemoteConfigs: [remote_config_parameters]]
    }

    // Check if this build is a PR
    // Only PR builds (old and new) pass ghprbPullId
    if (params.ghprbPullId) {
        // Look for dependent changes and checkout PR(s)
        checkout_pullrequest()
    } else {
        sh "git checkout ${OPENJDK_SHA}"
        sh "bash ./get_source.sh ${EXTRA_GETSOURCE_OPTIONS} ${OPENJ9_REPO_OPTION} ${OPENJ9_BRANCH_OPTION} ${OPENJ9_SHA_OPTION} ${OMR_REPO_OPTION} ${OMR_BRANCH_OPTION} ${OMR_SHA_OPTION}"
    }
}

def checkout_pullrequest() {
    echo 'Look for Dependent Changes'

    def KEYWORD = "depends"
    def COMMENT = params.ghprbCommentBody.toLowerCase(Locale.US);
    def OPENJDK_PR
    def OMR_PR
    def OPENJ9_PR
    def omr_bool = false
    def omr_upstream = false
    def openjdk_bool = false
    def openj9_bool = false
    def JDK_REPO_SUFFIX = ""

    // Determine source repo of PR
    // Cannot depend on a change from the same repo as the source repo
    def SOURCE_REPO_MESSAGE
    switch (params.ghprbGhRepository) {
        case "eclipse/openj9-omr":
            omr_bool = true
            OMR_PR = params.ghprbPullId
            SOURCE_REPO_MESSAGE = "PullRequest source repo appears to be OpenJ9-OMR"
            break
        case "eclipse/openj9":
            openj9_bool = true
            OPENJ9_PR = params.ghprbPullId
            SOURCE_REPO_MESSAGE = "PullRequest source repo appears to be OpenJ9"
            break
        case ~/ibmruntimes(.*)/:
            openjdk_bool = true
            OPENJDK_PR = params.ghprbPullId
            SOURCE_REPO_MESSAGE = "PullRequest source repo appears to be OpenJDK"
            break
        default:
            error "Unable to determine source repo for PullRequest"
    }
    echo SOURCE_REPO_MESSAGE

    if (!COMMENT.contains(KEYWORD)) {
        echo "No dependent changes found"
    } else {
        // Gather everything after KEYWORD. Assumes a whitespace after KEYWORD, hence +1
        def DEPENDS_BLOB = COMMENT.substring(COMMENT.indexOf(KEYWORD) + KEYWORD.length() + 1 , COMMENT.length())
        def DEPENDS_ARRAY = DEPENDS_BLOB.tokenize(' ')

        // Setup PR_IDs for dependent changes
        for (DEPEND in DEPENDS_ARRAY) {
            String REPO = DEPEND.substring(DEPEND.indexOf("/") + 1, DEPEND.indexOf("#"));
            String PR_ID = DEPEND.substring(DEPEND.indexOf("#") + 1, DEPEND.length());
            switch(REPO) {
                case "omr":
                case "openj9-omr":
                    if (omr_bool) {
                        error "Cannot specify 2 OMR Repos"
                    }
                    omr_bool = true
                    if (REPO == 'omr') {
                        omr_upstream = true
                    }
                    OMR_PR = "${PR_ID}"
                    echo "Dependent OMR change found: #${PR_ID}"
                    break
                case "openj9":
                    if (openj9_bool) {
                        error "Cannot specify 2 OpenJ9 Repos"
                    }
                    openj9_bool = true
                    OPENJ9_PR = "${PR_ID}"
                    echo "Dependent OpenJ9 change found: #${PR_ID}"
                    break
                case ~/openj9-openjdk(.*)/:
                    if (openjdk_bool) {
                        error "Cannot specify 2 OpenJDK Repos"
                    }
                    openjdk_bool = true
                    OPENJDK_PR = "${PR_ID}"
                    echo "Dependent OpenJDK change found: #${PR_ID}"

                    // Verify OpenJDK Repo matches SDK Version of build.
                    // eg. Cannot checkout a JDK8 PR for a JDK9 build
                    echo "SDK: ${SDK_VERSION}"
                    echo "REPO: ${REPO}"
                    if (SDK_VERSION != "next") {
                        JDK_REPO_SUFFIX = SDK_VERSION
                    }
                    // For SDK 8/9/10, the Repo has the SDK version in the name
                    if (REPO != "openj9-openjdk-jdk${JDK_REPO_SUFFIX}") {
                        error "Trying to build SDK${JDK_REPO_SUFFIX} with a dependent change from repo '${REPO}' is not possible"
                    }
                    break
                default:
                    error "Bad Depends Repo: '${REPO}' in '${DEPEND}'"
            }
        }
    }

    if (openjdk_bool) {
        if (SDK_VERSION != "next") {
            JDK_REPO_SUFFIX = SDK_VERSION
        }
        checkout_pullrequest(OPENJDK_PR, "ibmruntimes/openj9-openjdk-jdk${JDK_REPO_SUFFIX}")
    }

    sh "bash ./get_source.sh ${EXTRA_GETSOURCE_OPTIONS}"

    // Checkout dependent PRs, if any were specified
    if (openj9_bool) {
        dir ('openj9') {
            checkout_pullrequest(OPENJ9_PR, 'eclipse/openj9')
        }
    }

    if (omr_bool) {
        dir ('omr') {
            if (omr_upstream) {
                sh "git config remote.origin.url https://github.com/eclipse/omr.git"
            }
            checkout_pullrequest(OMR_PR, 'eclipse/omr')
        }
    }
}

def checkout_pullrequest(ID, REPO) {
    // Determine if we are checking out a PR or a branch
    try {
        if (ID.isNumber()) {
            try {
                fetchRef("pull/${ID}/merge", "pr/${ID}/merge" )
                checkoutRef ("pr/${ID}/merge")
            } catch (e) {
                def JSONFILE = ""
                def BASEREF = ""
                def URL = "https://api.github.com/repos/${REPO}/pulls/${ID}"
                withCredentials([usernamePassword(credentialsId: "${GITHUB_API_CREDENTIALS_ID}", passwordVariable: 'PASS', usernameVariable: 'USER')]) {
                    JSONFILE = sh returnStdout: true, script: "curl -u ${USER} --pass ${PASS} ${URL}"
                }
                def slurper = new groovy.json.JsonSlurper()
                def jsonResultFile = slurper.parseText(JSONFILE)
                BASEREF = jsonResultFile.base.ref
                fetchRef ("heads/${BASEREF}", "${BASEREF}")
                checkoutRef (BASEREF)
            }
        } else {
            fetchRef ("heads/${ID}", ID)
            checkoutRef (ID)
        }
    } catch(e) {
        error("Fatal: Unable to checkout '${ID}'. If checking out a branch, please ensure the branch exists and the spelling is correct. If checking out a PullRequest, please ensure the PR is open, unmerged and has no merge conflicts.")
    }
}

def fetchRef (REF1, REF2) {
    sh "git fetch --progress origin +refs/${REF1}:refs/remotes/origin/${REF2}"
}

def checkoutRef (REF) {
    sh "git checkout refs/remotes/origin/${REF}"
}


def build() {
    stage('Compile') {
        // 'all' target dependencies broken for zos, use 'images test-image-openj9'
        def make_target = SPEC.contains('zos') ? 'images test-image-openj9' : 'all'
        OPENJDK_CLONE_DIR = "${env.WORKSPACE}/${OPENJDK_CLONE_DIR}"
        withEnv(BUILD_ENV_VARS_LIST) {
            dir(OPENJDK_CLONE_DIR) {
                try {
                    sh "${BUILD_ENV_CMD} bash configure --with-freemarker-jar='$FREEMARKER' --with-boot-jdk='$BOOT_JDK' $EXTRA_CONFIGURE_OPTIONS && make $EXTRA_MAKE_OPTIONS ${make_target}"
                } catch(e) {
                    archive_diagnostics()
                    throw e
                }
            }
        }
    }
    stage('Java Version') {
        dir(OPENJDK_CLONE_DIR) {
            sh "build/$RELEASE/images/$JDK_FOLDER/bin/java -version"
        }
    }
}

def archive_sdk() {
    stage('Archive') {
        def buildDir = "build/${RELEASE}/images/"
        def testDir = "test/openj9"

        dir(OPENJDK_CLONE_DIR) {
            sh "tar -C ${buildDir} -zcvf ${SDK_FILENAME} ${JDK_FOLDER}"
            // test if the test natives directory is present, only in JDK11+
            if (fileExists("${buildDir}${testDir}")) {
                if (SPEC.contains('zos')) {
                    // Note: to preserve the files ACLs set _OS390_USTAR=Y env variable (see variable files)
                    sh "pax  -wvz -s#${buildDir}${testDir}#native-test-libs#g -f ${TEST_FILENAME} ${buildDir}${testDir}"
                } else {
                    sh "tar -C ${buildDir} -zcvf ${TEST_FILENAME} ${testDir} --transform 's,${testDir},native-test-libs,'"
                }
            }
            if (ARTIFACTORY_SERVER) {
                def uploadSpec = """{
                        "files":[
                            {
                                "pattern": "${OPENJDK_CLONE_DIR}/${SDK_FILENAME}",
                                "target": "${ARTIFACTORY_UPLOAD_DIR}",
                                "props": "build.buildIdentifier=${BUILD_IDENTIFIER}"
                            },
                            {
                                "pattern": "${OPENJDK_CLONE_DIR}/${TEST_FILENAME}",
                                "target": "${ARTIFACTORY_UPLOAD_DIR}",
                                "props": "build.buildIdentifier=${BUILD_IDENTIFIER}"
                            }
                        ]
                    }"""
                upload_artifactory(uploadSpec)
                env.CUSTOMIZED_SDK_URL = "${ARTIFACTORY_URL}/${ARTIFACTORY_UPLOAD_DIR}${SDK_FILENAME}"
                currentBuild.description += "<br><a href=${CUSTOMIZED_SDK_URL}>${SDK_FILENAME}</a>"
                if (fileExists("${TEST_FILENAME}")) {
                    TEST_LIB_URL = "${ARTIFACTORY_URL}/${ARTIFACTORY_UPLOAD_DIR}${TEST_FILENAME}"
                    env.CUSTOMIZED_SDK_URL += " " + TEST_LIB_URL
                    currentBuild.description += "<br><a href=${TEST_LIB_URL}>${TEST_FILENAME}</a>"
                }
                echo "CUSTOMIZED_SDK_URL:'${CUSTOMIZED_SDK_URL}'"
            } else {
                echo "ARTIFACTORY server is not set saving artifacts on jenkins."
                archiveArtifacts artifacts: "**/${SDK_FILENAME},**/${TEST_FILENAME}", fingerprint: false, onlyIfSuccessful: true
            }
        }
    }
}

def archive_diagnostics() {
    sh "find . -name 'core.*.dmp' -o -name 'javacore.*.txt' -o -name 'Snap.*.trc' -o -name 'jitdump.*.dmp' | tar -zcvf ${DIAGNOSTICS_FILENAME} -T -"
    if (ARTIFACTORY_SERVER) {
        def uploadSpec = """{
            "files":[
                {
                    "pattern": "${DIAGNOSTICS_FILENAME}",
                    "target": "${ARTIFACTORY_UPLOAD_DIR}",
                    "props": "build.buildIdentifier=${BUILD_IDENTIFIER}"
                }
            ]
        }"""
        upload_artifactory(uploadSpec)
        DIAGNOSTICS_FILE_URL = "${ARTIFACTORY_URL}/${ARTIFACTORY_UPLOAD_DIR}${DIAGNOSTICS_FILENAME}"
        currentBuild.description += "<br><a href=${DIAGNOSTICS_FILE_URL}>${DIAGNOSTICS_FILENAME}</a>"
    } else {
        archiveArtifacts artifacts: "${DIAGNOSTICS_FILENAME}", fingerprint: false
    }
}

def upload_artifactory(uploadSpec) {
    def server = Artifactory.server ARTIFACTORY_SERVER
    // set connection timeout to 10 mins to avoid timeout on slow platforms
    server.connection.timeout = 600

    def buildInfo = Artifactory.newBuildInfo()
    buildInfo.retention maxBuilds: ARTIFACTORY_NUM_ARTIFACTS, deleteBuildArtifacts: true
    // Add BUILD_IDENTIFIER to the buildInfo. The UploadSpec adds it to the Artifact info
    buildInfo.env.filter.addInclude("BUILD_IDENTIFIER")
    buildInfo.env.capture = true

    //Retry uploading to Artifactory if errors occur
    def ret = false
    retry (3) {
        if (ret) {
            sleep time: 300, unit: 'SECONDS'
        } else {
            ret = true
        }
        server.upload spec: uploadSpec, buildInfo: buildInfo
        server.publishBuildInfo buildInfo
    }

    // Write URL to env so that we can pull it from the upstream pipeline job
    env.ARTIFACTORY_URL = server.getUrl()
    env.ARTIFACTORY_CREDS = server.getCredentialsId()
}

def add_node_to_description() {
    def TMP_DESC = (currentBuild.description) ? currentBuild.description + "<br>" : ""
    currentBuild.description = TMP_DESC + "<a href=${JENKINS_URL}computer/${NODE_NAME}>${NODE_NAME}</a>"
}

def build_all() {
    timeout(time: 6, unit: 'HOURS') {
        try {
            // Cleanup in case an old build left anything behind
            cleanWs()
            add_node_to_description()
            get_source()
            build()
            archive_sdk()
        } finally {
            // disableDeferredWipeout also requires deleteDirs. See https://issues.jenkins-ci.org/browse/JENKINS-54225
            cleanWs notFailBuild: true, disableDeferredWipeout: true, deleteDirs: true
        }
    }
}

return this
