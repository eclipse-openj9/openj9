/*******************************************************************************
 * Copyright (c) 2017, 2020 IBM Corp. and others
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
import groovy.json.JsonOutput;
pipelineFunctions = load 'buildenv/jenkins/common/pipeline-functions.groovy'

def get_source() {
    stage('Get Source') {
        // Setup REPO variables
        OPENJ9_REPO_OPTION = (OPENJ9_REPO != "") ? "-openj9-repo=${OPENJ9_REPO}" : ""
        OPENJ9_BRANCH_OPTION = (OPENJ9_BRANCH != "") ? "-openj9-branch=${OPENJ9_BRANCH}" : ""
        OPENJ9_SHA_OPTION = (OPENJ9_SHA != "") ? "-openj9-sha=${OPENJ9_SHA}" : ""

        OMR_REPO_OPTION = (OMR_REPO != "") ? "-omr-repo=${OMR_REPO}" : ""
        OMR_BRANCH_OPTION = (OMR_BRANCH != "")? "-omr-branch=${OMR_BRANCH}" : ""
        OMR_SHA_OPTION = (OMR_SHA != "") ? "-omr-sha=${OMR_SHA}" : ""

        if (OPENJDK_REFERENCE_REPO) {
            OPENJ9_REFERENCE = "-openj9-reference=${OPENJDK_REFERENCE_REPO}"
            OMR_REFERENCE = "-omr-reference=${OPENJDK_REFERENCE_REPO}"
        }

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

        CLONE_CMD = "git clone -b ${OPENJDK_BRANCH} ${OPENJDK_REPO} ."
        if (OPENJDK_REFERENCE_REPO) {
            CLONE_CMD += " --reference ${OPENJDK_REFERENCE_REPO}"
        }

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
        sh "bash get_source.sh ${EXTRA_GETSOURCE_OPTIONS} ${OPENJ9_REPO_OPTION} ${OPENJ9_BRANCH_OPTION} ${OPENJ9_SHA_OPTION} ${OPENJ9_REFERENCE} ${OMR_REPO_OPTION} ${OMR_BRANCH_OPTION} ${OMR_SHA_OPTION} ${OMR_REFERENCE}"
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
        case ~/.*\/openj9-omr/:
            omr_bool = true
            OMR_PR = params.ghprbPullId
            SOURCE_REPO_MESSAGE = "PullRequest source repo appears to be OpenJ9-OMR"
            break
        case ~/.*\/openj9/:
            openj9_bool = true
            OPENJ9_PR = params.ghprbPullId
            SOURCE_REPO_MESSAGE = "PullRequest source repo appears to be OpenJ9"
            break
        case ~/.*openj9-openjdk-jdk.*/:
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
            switch (REPO) {
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

    sh "bash get_source.sh ${EXTRA_GETSOURCE_OPTIONS} ${OPENJ9_REPO_OPTION} ${OPENJ9_BRANCH_OPTION} ${OPENJ9_SHA_OPTION} ${OPENJ9_REFERENCE} ${OMR_REPO_OPTION} ${OMR_BRANCH_OPTION} ${OMR_SHA_OPTION} ${OMR_REFERENCE}"

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
                fetchRef("pull/${ID}/merge", "pr/${ID}/merge")
                checkoutRef ("pr/${ID}/merge")
            } catch (e) {
                // If merge ref doesn't exist, checkout PR base branch.
                // Not going to fix this to work for internal PRs.
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
    } catch (e) {
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
        def make_target = SPEC.contains('zos') ? 'images test-image-openj9 debug-image' : 'all'
        OPENJDK_CLONE_DIR = "${env.WORKSPACE}/${OPENJDK_CLONE_DIR}"

        withEnv(BUILD_ENV_VARS_LIST) {
            dir(OPENJDK_CLONE_DIR) {
                try {
                    sh "${BUILD_ENV_CMD} bash configure --with-freemarker-jar=${FREEMARKER} --with-boot-jdk=${BOOT_JDK} ${EXTRA_CONFIGURE_OPTIONS} && make ${EXTRA_MAKE_OPTIONS} ${make_target}"
                } catch (e) {
                    archive_diagnostics()
                    throw e
                }
            }
        }
    }
    stage('Java Version') {
        dir(OPENJDK_CLONE_DIR) {
            sh "build/$RELEASE/images/$JDK_FOLDER/bin/java -Xjit -version"
        }
    }
}

def archive_sdk() {
    stage('Archive') {
        def buildDir = "build/${RELEASE}/images/"
        def debugImageDir = "${buildDir}debug-image/"
        def testDir = "test"

        dir(OPENJDK_CLONE_DIR) {
            // The archiver receives pathnames on stdin and writes to stdout.
            def archiveCmd = SPEC.contains('zos') ? 'pax -wvz -p x' : 'tar -cvz -T -'
            // Filter out unwanted files (most of which are available in the debug-image).
            def filterCmd = "sed -e '/\\.dbg\$/d' -e '/\\.debuginfo\$/d' -e '/\\.diz\$/d' -e '/\\.dSYM\\//d' -e '/\\.map\$/d' -e '/\\.pdb\$/d'"
            sh "( cd ${buildDir} && find ${JDK_FOLDER} -type f | ${filterCmd} | ${archiveCmd} ) > ${SDK_FILENAME}"
            // test if the test natives directory is present, only in JDK11+
            if (fileExists("${buildDir}${testDir}")) {
                if (SPEC.contains('zos')) {
                    // Note: to preserve the files ACLs set _OS390_USTAR=Y env variable (see variable files)
                    sh "pax -wvz '-s#${buildDir}${testDir}#test-images#' -f ${TEST_FILENAME} ${buildDir}${testDir}"
                } else {
                    sh "tar -C ${buildDir} -zcvf ${TEST_FILENAME} ${testDir} --transform 's,${testDir},test-images,'"
                }
            }
            // test if the debug-image directory is present
            if (fileExists("${debugImageDir}")) {
                if (SPEC.contains('aix')) {
                    // On AIX, .debuginfo files are simple copies of the shared libraries.
                    // Change all suffixes from .debuginfo to .so; then remove the .so suffix
                    // from names in the bin directory. This will result in an archive that
                    // can be extracted overtop of a jdk yielding one that is debuggable.
                    sh "tar -C ${debugImageDir} -zcvf ${DEBUG_IMAGE_FILENAME} . --transform 's#\\.debuginfo\$#.so#' --transform 's#\\(bin/.*\\)\\.so\$#\\1#' --show-stored-names"
                } else if (SPEC.contains('zos')) {
                    // Note: to preserve the files ACLs set _OS390_USTAR=Y env variable (see variable files)
                    sh "pax -wvz '-s#${debugImageDir}##' -f ${DEBUG_IMAGE_FILENAME} ${debugImageDir}"
                } else {
                    sh "tar -C ${debugImageDir} -zcvf ${DEBUG_IMAGE_FILENAME} ."
                }
            }
            if (params.ARCHIVE_JAVADOC) {
                def javadocDir = "docs"
                def javadocOpenJ9OnlyDir = "openj9-docs"
                def extractDir = "build/${RELEASE}/images/"
                if (SDK_VERSION == "8") {
                    extractDir = "build/${RELEASE}/"
                }
                if (fileExists("${extractDir}${javadocDir}")) {
                    if (SPEC.contains('zos')) {
                        // Note: to preserve the files ACLs set _OS390_USTAR=Y env variable (see variable files)
                        sh "pax -wvzf ${JAVADOC_FILENAME} ${extractDir}${javadocDir}"
                    } else {
                        sh "tar -C ${extractDir} -zcvf ${JAVADOC_FILENAME} ${javadocDir}"
                    }
                }
                if (fileExists("${extractDir}${javadocOpenJ9OnlyDir}")) {
                    if (SPEC.contains('zos')) {
                        // Note: to preserve the files ACLs set _OS390_USTAR=Y env variable (see variable files)
                        sh "pax -wvzf ${JAVADOC_OPENJ9_ONLY_FILENAME} ${extractDir}${javadocOpenJ9OnlyDir}"
                    } else {
                        sh "tar -C ${extractDir} -zcvf ${JAVADOC_OPENJ9_ONLY_FILENAME} ${javadocOpenJ9OnlyDir}"
                    }
                }
            }
            if (ARTIFACTORY_CONFIG) {
                def specs = []
                def sdkSpec = ["pattern": "${OPENJDK_CLONE_DIR}/${SDK_FILENAME}",
                               "target": "${ARTIFACTORY_CONFIG['uploadDir']}",
                               "props": "build.buildIdentifier=${BUILD_IDENTIFIER}"]
                specs.add(sdkSpec)
                def testSpec = ["pattern": "${OPENJDK_CLONE_DIR}/${TEST_FILENAME}",
                                "target": "${ARTIFACTORY_CONFIG['uploadDir']}",
                                "props": "build.buildIdentifier=${BUILD_IDENTIFIER}"]
                specs.add(testSpec)
                def debugImageSpec = ["pattern": "${OPENJDK_CLONE_DIR}/${DEBUG_IMAGE_FILENAME}",
                                "target": "${ARTIFACTORY_CONFIG['uploadDir']}",
                                "props": "build.buildIdentifier=${BUILD_IDENTIFIER}"]
                specs.add(debugImageSpec)
                if (params.ARCHIVE_JAVADOC) {
                    def javadocSpec = ["pattern": "${OPENJDK_CLONE_DIR}/${JAVADOC_FILENAME}",
                                       "target": "${ARTIFACTORY_CONFIG['uploadDir']}",
                                       "props": "build.buildIdentifier=${BUILD_IDENTIFIER}"]
                    specs.add(javadocSpec)
                    def javadocOpenJ9OnlySpec = ["pattern": "${OPENJDK_CLONE_DIR}/${JAVADOC_OPENJ9_ONLY_FILENAME}",
                                                 "target": "${ARTIFACTORY_CONFIG['uploadDir']}",
                                                 "props": "build.buildIdentifier=${BUILD_IDENTIFIER}"]
                    specs.add(javadocOpenJ9OnlySpec)
                }
                def uploadFiles = [files : specs]
                def uploadSpec = JsonOutput.toJson(uploadFiles)
                upload_artifactory(uploadSpec)

                // Always use the default server link for the description and for test.
                env.CUSTOMIZED_SDK_URL = "${ARTIFACTORY_CONFIG[ARTIFACTORY_CONFIG['defaultGeo']]['url']}/${ARTIFACTORY_CONFIG['uploadDir']}${SDK_FILENAME}"
                currentBuild.description += "<br><a href=${CUSTOMIZED_SDK_URL}>${SDK_FILENAME}</a>"

                if (fileExists("${TEST_FILENAME}")) {
                    TEST_LIB_URL = "${ARTIFACTORY_CONFIG[ARTIFACTORY_CONFIG['defaultGeo']]['url']}/${ARTIFACTORY_CONFIG['uploadDir']}${TEST_FILENAME}"
                    currentBuild.description += "<br><a href=${TEST_LIB_URL}>${TEST_FILENAME}</a>"
                    env.CUSTOMIZED_SDK_URL += " " + TEST_LIB_URL
                }
                if (fileExists("${DEBUG_IMAGE_FILENAME}")) {
                    DEBUG_IMAGE_LIB_URL = "${ARTIFACTORY_CONFIG[ARTIFACTORY_CONFIG['defaultGeo']]['url']}/${ARTIFACTORY_CONFIG['uploadDir']}/${DEBUG_IMAGE_FILENAME}"
                    currentBuild.description += "<br><a href=${DEBUG_IMAGE_LIB_URL}>${DEBUG_IMAGE_FILENAME}</a>"
                }
                if (params.ARCHIVE_JAVADOC) {
                    if (fileExists("${JAVADOC_FILENAME}")) {
                        JAVADOC_LIB_URL = "${ARTIFACTORY_CONFIG[ARTIFACTORY_CONFIG['defaultGeo']]['url']}/${ARTIFACTORY_CONFIG['uploadDir']}${JAVADOC_FILENAME}"
                        currentBuild.description += "<br><a href=${JAVADOC_LIB_URL}>${JAVADOC_FILENAME}</a>"
                        echo "Javadoc:'${JAVADOC_LIB_URL}'"
                    }
                    if (fileExists("${JAVADOC_OPENJ9_ONLY_FILENAME}")) {
                        JAVADOC_OPENJ9_ONLY_LIB_URL = "${ARTIFACTORY_CONFIG[ARTIFACTORY_CONFIG['defaultGeo']]['url']}/${ARTIFACTORY_CONFIG['uploadDir']}${JAVADOC_OPENJ9_ONLY_FILENAME}"
                        currentBuild.description += "<br><a href=${JAVADOC_OPENJ9_ONLY_LIB_URL}>${JAVADOC_OPENJ9_ONLY_FILENAME}</a>"
                        echo "Javadoc (OpenJ9 extensions only):'${JAVADOC_OPENJ9_ONLY_LIB_URL}'"
                    }
                }
                echo "CUSTOMIZED_SDK_URL:'${CUSTOMIZED_SDK_URL}'"
            } else {
                echo "ARTIFACTORY server is not set saving artifacts on jenkins."
                def ARTIFACTS_FILES = "**/${SDK_FILENAME},**/${TEST_FILENAME},**/${DEBUG_IMAGE_FILENAME}"
                if (params.ARCHIVE_JAVADOC) {
                    ARTIFACTS_FILES += ",**/${JAVADOC_FILENAME}"
                    ARTIFACTS_FILES += ",**/${JAVADOC_OPENJ9_ONLY_FILENAME}"
                }
                archiveArtifacts artifacts: ARTIFACTS_FILES, fingerprint: false, onlyIfSuccessful: true
            }
        }
    }
}

/*
 * When crash occurs in java, there will be an output in the log: IEATDUMP success for DSN='JENKINS.JVM.JENKIN*.*.*.X&DS'.
 * The X&DS is a macro that is passed to the IEATDUMP service on z/OS.
 * The dump is split into multiple files by the macro if the address space is large.
 * Each dataset is named in a similar way but there is a distinct suffix to specify the order.
 * For example, JVM.*.*.*.001, JVM.*.*.*.002, and JVM.*.*.*.003.
 * The function fetchFile moves each dataset into the current directory of the machine, and appends the data to the core.xxx.dmp at the same time.
 * The file core.xxx.dmp is uploaded to artifactory for triage.
 */
def fetchFile(src, dest) {
    // remove the user name from the dataset name
    def offset = USER.length() + 1
    def fileToMove = src.substring(src.indexOf("${USER}.") + offset)
    // append the dataset to core.xxx.dmp file
    sh "cat '//${fileToMove}' >> ${dest}"
    // remove the dataset
    sh "tso DELETE ${fileToMove}"
}

def archive_diagnostics() {
    if (SPEC.contains('zos')) {
        def logContent = currentBuild.rawBuild.getLog()
        // search for each occurrence of IEATDUMP success for DSN=
        def matches = logContent =~ /IEATDUMP success for DSN=.*/
        for (match in matches) {
            // extract the naming convention of datasets
            def beginIndex = match.indexOf('\'') + 1
            def endIndex = match.lastIndexOf('.')
            def filename = match.substring(beginIndex, endIndex)
            // count how many datasets created by the macro
            def num = sh returnStdout: true, script: "tso listcat | grep ${filename} | wc -l"
            def numFiles = num.trim().toInteger()
            for (i = 1; i <= numFiles; i++) {
                switch (i) {
                    case 1..9:
                        src = filename + '.' + 'X00' + i;
                        break;
                    case 10..99:
                        src = filename + '.' + 'X0' + i;
                        break;
                    case 100..999:
                        src = filename + '.' + 'X' + i;
                        break;
                }
                dest = 'core' + '.' + filename + '.' + 'dmp'
                fetchFile(src,dest)
            }
        }
        // Note: to preserve the files ACLs set _OS390_USTAR=Y env variable (see variable files)
        sh "find . -name 'core.*.dmp' -o -name 'javacore.*.txt' -o -name 'Snap.*.trc' -o -name 'jitdump.*.dmp' | sed 's#^./##' | pax -wzf ${DIAGNOSTICS_FILENAME}"
    } else if (SPEC.endsWith("_cm")) {
        sh "find . -name 'core.*.dmp' -o -name 'javacore.*.txt' -o -name 'Snap.*.trc' -o -name 'jitdump.*.dmp' -o -name ddr_info -o -name superset.dat -o -name j9ddr.dat -o -name '*.stub.h' -o -name '*.annt.h' | sed 's#^./##' | tar -zcvf ${DIAGNOSTICS_FILENAME} -T -"
    } else {
        sh "find . -name 'core.*.dmp' -o -name 'javacore.*.txt' -o -name 'Snap.*.trc' -o -name 'jitdump.*.dmp' | sed 's#^./##' | tar -zcvf ${DIAGNOSTICS_FILENAME} -T -"
    }
    if (ARTIFACTORY_CONFIG) {
        def uploadSpec = """{
            "files":[
                {
                    "pattern": "${DIAGNOSTICS_FILENAME}",
                    "target": "${ARTIFACTORY_CONFIG['uploadDir']}",
                    "props": "build.buildIdentifier=${BUILD_IDENTIFIER}"
                }
            ]
        }"""
        upload_artifactory_core(ARTIFACTORY_CONFIG['defaultGeo'], uploadSpec)
        DIAGNOSTICS_FILE_URL = "${ARTIFACTORY_CONFIG[ARTIFACTORY_CONFIG['defaultGeo']]['url']}/${ARTIFACTORY_CONFIG['uploadDir']}${DIAGNOSTICS_FILENAME}"
        currentBuild.description += "<br><a href=${DIAGNOSTICS_FILE_URL}>${DIAGNOSTICS_FILENAME}</a>"
    } else {
        archiveArtifacts artifacts: "${DIAGNOSTICS_FILENAME}", fingerprint: false
    }
}

def upload_artifactory(uploadSpec) {
    // Loop all the servers and upload if we've determined we should do so
    for (geo in ARTIFACTORY_CONFIG['geos']) {
        if (ARTIFACTORY_CONFIG[geo]['uploadBool']) {
            /*
            * If the server is behind a vpn and we aren't on a node behind the vpn,
            * save the sdk and upload it later on a machine behind the vpn
            */
            if (ARTIFACTORY_CONFIG[geo]['vpn'] == 'true' && !NODE_LABELS.contains("ci.geo.${geo}")) {
                if (!ARTIFACTORY_CONFIG['stashed']) {
                    // Stash only what test needs (CUSTOMIZED_SDK_URL)
                    stash includes: "**/${SDK_FILENAME},**/${TEST_FILENAME}", name: 'sdk'
                    ARTIFACTORY_CONFIG['stashed'] = true
                    ARTIFACTORY_CONFIG['uploadSpec'] = uploadSpec
                }
                ARTIFACTORY_CONFIG[geo]['uploaded'] = false
            } else {
                upload_artifactory_core(geo, uploadSpec)
                ARTIFACTORY_CONFIG[geo]['uploaded'] = true
            }
        }
    }
}

def upload_artifactory_core(geo, uploadSpec) {
    echo "Uploading to '${geo}'..."
    def server = Artifactory.server ARTIFACTORY_CONFIG[geo]['server']
    // set connection timeout to 10 mins to avoid timeout on slow platforms
    server.connection.timeout = 600

    def buildInfo = Artifactory.newBuildInfo()
    buildInfo.retention maxBuilds: ARTIFACTORY_CONFIG[geo]['numArtifacts'], maxDays: ARTIFACTORY_CONFIG[geo]['daysToKeepArtifacts'], deleteBuildArtifacts: true
    // Add BUILD_IDENTIFIER to the buildInfo. The UploadSpec adds it to the Artifact info
    buildInfo.env.filter.addInclude("BUILD_IDENTIFIER")
    buildInfo.env.capture = true

    //Retry uploading to Artifactory if errors occur
    pipelineFunctions.retry_and_delay({
        server.upload spec: uploadSpec, buildInfo: buildInfo;
        if (!ARTIFACTORY_CONFIG[geo]['vpn']) { server.publishBuildInfo buildInfo } },
        3, 300)

    ARTIFACTORY_CONFIG[geo]['url'] = server.getUrl()
    ARTIFACTORY_CONFIG[geo]['creds'] = server.getCredentialsId()
    // If this is the default server, save the creds to pass to test
    if (geo == ARTIFACTORY_CONFIG['defaultGeo']) {
        env.ARTIFACTORY_CREDS = ARTIFACTORY_CONFIG[ARTIFACTORY_CONFIG['defaultGeo']].creds
    }
}

def upload_artifactory_post() {
    // Determine if we didn't do any Artifactory uploads because of vpn.
    // At the time of writing this code, we would only hit this case if we compiled at OSU on plinux and needed to upload to UNB.
    if (ARTIFACTORY_CONFIG && ARTIFACTORY_CONFIG['stashed']) {
        for (geo in ARTIFACTORY_CONFIG['geos']) {
            if (ARTIFACTORY_CONFIG[geo]['uploadBool'] && !ARTIFACTORY_CONFIG[geo]['uploaded']) {
                // Grab a node with the same geo as the server so we can upload
                node("ci.geo.${geo}") {
                    try {
                        unstash 'sdk'
                        upload_artifactory_core(geo, ARTIFACTORY_CONFIG['uploadSpec'])
                    } finally {
                        cleanWs()
                    }
                }
            }
        }
    }
}

def add_node_to_description() {
    def TMP_DESC = (currentBuild.description) ? currentBuild.description + "<br>" : ""
    currentBuild.description = TMP_DESC + "<a href=${JENKINS_URL}computer/${NODE_NAME}>${NODE_NAME}</a>"
}

def build_all() {
    stage ('Queue') {
        timeout(time: 10, unit: 'HOURS') {
            node("${NODE}") {
                timeout(time: 5, unit: 'HOURS') {
                    try {
                        // Cleanup in case an old build left anything behind
                        cleanWs disableDeferredWipeout: true, deleteDirs: true
                        add_node_to_description()
                        // Setup Artifactory now that we are on a node. This determines which server(s) we push to.
                        variableFile.set_artifactory_config(BUILD_IDENTIFIER)
                        // initialize BOOT_JDK, FREEMARKER, OPENJDK_REFERENCE_REPO here 
                        // to correctly expand $HOME variable
                        variableFile.set_build_variables_per_node()
                        get_source()
                        build()
                        archive_sdk()
                    } finally {
                        KEEP_WORKSPACE = (params.KEEP_WORKSPACE) ?: false
                        if (!KEEP_WORKSPACE) {
                            // disableDeferredWipeout also requires deleteDirs. See https://issues.jenkins-ci.org/browse/JENKINS-54225
                            cleanWs notFailBuild: true, disableDeferredWipeout: true, deleteDirs: true
                        }
                    }
                }
            }
            upload_artifactory_post()
        }
    }
}

return this
