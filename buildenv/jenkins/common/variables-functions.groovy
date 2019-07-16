/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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

def VARIABLES

/*
* Parses the Jenkins job variables file and populates the VARIABLES collection.
* If a variables file is not set, parse the default variables file.
* The variables file is in YAML format and contains configuration settings
* required to compile and test the OpenJ9 extensions for OpenJDK on multiple
* platforms.
*/
def parse_variables_file(){
    DEFAULT_VARIABLE_FILE = "buildenv/jenkins/variables/defaults.yml"

    // check if a variable file is passed as a Jenkins build parameter
    // if not use default configuration settings
    VARIABLE_FILE = get_variables_file()
    if (!VARIABLE_FILE) {
        VARIABLE_FILE = "${DEFAULT_VARIABLE_FILE}"
    }

    if (!fileExists("${VARIABLE_FILE}")) {
        error("Missing variable file: ${VARIABLE_FILE}")
    }

    echo "Using variables file: ${VARIABLE_FILE}"
    VARIABLES = readYaml file: "${VARIABLE_FILE}"
}

/*
* Fetch the user provided variable file.
*/
def get_variables_file() {
    VARIABLE_FILE = (params.VARIABLE_FILE) ? params.VARIABLE_FILE : ""
    echo "VARIABLE_FILE:'${VARIABLE_FILE}'"
    VENDOR_REPO = (params.VENDOR_REPO) ? params.VENDOR_REPO : ""
    echo "VENDOR_REPO:'${VENDOR_REPO}'"
    VENDOR_BRANCH = (params.VENDOR_BRANCH) ? params.VENDOR_BRANCH : ""
    echo "VENDOR_BRANCH:'${VENDOR_BRANCH}'"
    VENDOR_CREDENTIALS_ID = (params.VENDOR_CREDENTIALS_ID) ? params.VENDOR_CREDENTIALS_ID : ""
    echo "VENDOR_CREDENTIALS_ID:'${VENDOR_CREDENTIALS_ID}'"
    if (VARIABLE_FILE && VENDOR_REPO && VENDOR_BRANCH) {
        if (VENDOR_CREDENTIALS_ID) {
            sshagent(credentials:[VENDOR_CREDENTIALS_ID]) {
                checkout_file()
            }
        } else {
            checkout_file()
        }
    }
    return VARIABLE_FILE
}

/*
* Check out the user variable file.
*/
def checkout_file() {
    sh "git config remote.vendor.url ${VENDOR_REPO}"
    sh "git config remote.vendor.fetch +refs/heads/${VENDOR_BRANCH}:refs/remotes/vendor/${VENDOR_BRANCH}"
    sh "git fetch vendor"
    sh "git checkout vendor/${VENDOR_BRANCH} -- ${VARIABLE_FILE}"
}

/*
* Sets the Git repository URLs and branches for all sources required to build
* the OpenJ9 extensions for OpenJDK.
* Initializes variables to the value passed as build parameters.
* When no values are available as build parameters set them to the values
* available with the variable file otherwise set them to empty strings.
*/
def set_repos_variables(BUILD_SPECS=null) {
    // check if passed as Jenkins build parameters
    // if not set as params, check the variables file

    if (!BUILD_SPECS) {
        // set variables (as strings) for a single OpenJDK release build

        // fetch OpenJDK repo and branch from variables file as a map
        def openjdk_info = get_openjdk_info(SDK_VERSION, [SPEC], false)

        OPENJDK_REPO = openjdk_info.get(SPEC).get('REPO')
        OPENJDK_BRANCH = openjdk_info.get(SPEC).get('BRANCH')
        OPENJDK_SHA = openjdk_info.get(SPEC).get('SHA')

        echo "Using OPENJDK_REPO = ${OPENJDK_REPO} OPENJDK_BRANCH = ${OPENJDK_BRANCH} OPENJDK_SHA = ${OPENJDK_SHA}"

        // set OpenJ9 and OMR repos, branches and SHAs
        set_extensions_variables()

     } else {
        // set OpenJDK variables (as maps) for a multiple releases build

        def build_releases = get_build_releases(BUILD_SPECS)
        build_releases.each { release ->
            // fetch OpenJDK repo and branch from variables file as a map
            def openjdk_info = get_openjdk_info(release, get_build_specs_by_release(release, BUILD_SPECS), true)

            def repos = [:]
            def branches = [:]
            def shas = [:]

            openjdk_info.each { build_spec, info_map ->
                repos.put(build_spec, info_map['REPO'])
                branches.put(build_spec, info_map['BRANCH'])
                shas.put(build_spec, info_map['SHA'])
            }

            OPENJDK_REPO[release] = repos
            OPENJDK_BRANCH[release] = branches
            OPENJDK_SHA[release] = shas
        }

        echo "Using OPENJDK_REPO = ${OPENJDK_REPO.toString()} OPENJDK_BRANCH = ${OPENJDK_BRANCH.toString()} OPENJDK_SHA = ${OPENJDK_SHA.toString()}"

        // default URL and branch for the OpenJ9 and OMR repositories (no entries in defaults.yml)
        EXTENSIONS = ['OpenJ9': ['repo': 'https://github.com/eclipse/openj9.git',     'branch': 'master'],
                      'OMR'   : ['repo': 'https://github.com/eclipse/openj9-omr.git', 'branch': 'openj9']]

        // set OpenJ9 and OMR repos, branches and SHAs
        set_extensions_variables(EXTENSIONS)
    }
}

/*
* Initializes OpenJDK repository URL, branch, SHA for given release and returns
* them as a map.
* Build parameters take precedence over custom variables (see variables file).
* Throws an error if repository URL or branch are not provided. 
*/
def get_openjdk_info(SDK_VERSION, SPECS, MULTI_RELEASE) {
    // map to store git repository information by spec
    def openjdk_info = [:]

    // fetch values from variables file
    def default_openjdk_info = get_value(VARIABLES.openjdk, SDK_VERSION)

    // the build expects the following parameters:
    // OPENJDK*_REPO, OPENJDK*_BRANCH, OPENJDK*_SHA,
    // OPENJDK*_REPO_<spec>, OPENJDK*_BRANCH_<spec>, OPENJDK*_SHA_<spec>
    def param_name = "OPENJDK"
    if (MULTI_RELEASE) {
        // if a wrapper pipeline build, expect parameters names to be prefixed with "OPENJDK<version>_"
        param_name += SDK_VERSION
    }

    for (build_spec in SPECS) {
        // default values from variables file
        def repo = default_openjdk_info.get('default').get('repoUrl')
        def branch = default_openjdk_info.get('default').get('branch')
        def sha = ''

        if (default_openjdk_info[build_spec]) {
            repo = default_openjdk_info.get(build_spec).get('repoUrl')
            branch = default_openjdk_info.get(build_spec).get('branch')
        }

        // check build parameters
        if (params."${param_name}_REPO") {
            // got OPENJDK*_REPO build parameter, overwrite repo
            repo = params."${param_name}_REPO"
        }

        if (params."${param_name}_BRANCH") {
            // got OPENJDK*_BRANCH build parameter, overwrite branch
            branch = params."${param_name}_BRANCH"
        }

        if (params."${param_name}_SHA") {
            // got OPENJDK*_SHA build parameter, overwrite sha
            sha = params."${param_name}_SHA"
        }

        if (MULTI_RELEASE) {
            if (params."${param_name}_REPO_${build_spec}") {
                // got OPENJDK*_REPO_<spec> build parameter, overwrite repo
                repo = params."${param_name}_REPO_${build_spec}"
            }

            if (params."${param_name}_BRANCH_${build_spec}") {
                // got OPENJDK*_BRANCH_<spec> build parameter, overwrite branch
                branch = params."${param_name}_BRANCH_${build_spec}"
            }

            if (params."${param_name}_SHA_${build_spec}") {
                // got OPENJDK*_SHA_<spec> build parameter, overwrite sha
                sha = params."${param_name}_SHA_${build_spec}"
            }
        }

        // populate the map
        openjdk_info.put(build_spec, ['REPO': convert_url(repo), 'BRANCH': branch, 'SHA': sha])
    }

    return openjdk_info
}

/*
* Initializes the OpenJ9 and OMR repositories variables with values from
* the variables file if they are not set as build parameters. 
* If no values available in the variable file then initialize these variables
* with default values, otherwise set them to empty strings (to avoid 
* downstream builds errors - Jenkins build parameters should not be null).
*/
def set_extensions_variables(defaults=null) {
    OPENJ9_REPO = params.OPENJ9_REPO
    if (!OPENJ9_REPO) {
        // set it to the value set into the variable file
        OPENJ9_REPO = VARIABLES.openj9_repo
    }

    if (!OPENJ9_REPO) {
        OPENJ9_REPO = ''
        if (defaults) {
            OPENJ9_REPO = defaults.get('OpenJ9').get('repo')
        }
    }
    OPENJ9_REPO = convert_url(OPENJ9_REPO)

    OPENJ9_BRANCH = params.OPENJ9_BRANCH
    if (!OPENJ9_BRANCH) {
         OPENJ9_BRANCH = VARIABLES.openj9_branch
    }

    if (!OPENJ9_BRANCH) {
        OPENJ9_BRANCH = ''
        if (defaults) {
            OPENJ9_BRANCH = defaults.get('OpenJ9').get('branch')
        }
    }

    OPENJ9_SHA = params.OPENJ9_SHA
    if (!OPENJ9_SHA) {
        OPENJ9_SHA = ''
    }

    OMR_REPO = params.OMR_REPO
    if (!OMR_REPO) {
        // set it to the value set into the variable file
        OMR_REPO = VARIABLES.omr_repo
    }

    if (!OMR_REPO) {
        OMR_REPO = ''
        if (defaults) {
            OMR_REPO = defaults.get('OMR').get('repo')
        }
    }
    OMR_REPO = convert_url(OMR_REPO)

    OMR_BRANCH = params.OMR_BRANCH
    if (!OMR_BRANCH) {
        // set it to the value set into the variable file
        OMR_BRANCH = VARIABLES.omr_branch
    }

    if (!OMR_BRANCH) {
        OMR_BRANCH = ''
        if (defaults) {
            OMR_BRANCH = defaults.get('OMR').get('branch')
        }
    }

    OMR_SHA = params.OMR_SHA
    if (!OMR_SHA) {
        OMR_SHA = ''
    }

    echo "Using OPENJ9_REPO = ${OPENJ9_REPO} OPENJ9_BRANCH = ${OPENJ9_BRANCH} OPENJ9_SHA = ${OPENJ9_SHA}"
    echo "Using OMR_REPO = ${OMR_REPO} OMR_BRANCH = ${OMR_BRANCH} OMR_SHA = ${OMR_SHA}"
}

/*
* Returns the value of the element identified by the given key or an empty string
* if the map contains no mapping for the key.
*/
def get_value(MAP, KEY) {
    if (MAP != null) {
        for (item in MAP) {
            if ("${item.key}" == "${KEY}") {
                if ((item.value instanceof Map) || (item.value instanceof java.util.List)) {
                    return item.value
                } else {
                    // stringify value
                    return "${item.value}"
                }
            }
        }
    }

    return ''
}

/*
* Returns Jenkins credentials ID required to connect to GitHub using
* the ssh protocol.
* Returns empty string if no values is provided in the variables file (not
* required for public repositories).
*/
def get_git_user_credentials_id() {
    return get_user_credentials_id("git")
}

/*
* Returns Jenkins credentials ID required to connect to GitHub using
* API (username & password)
* Returns empty string if no values is provided in the variables file
*/
def get_github_user_credentials_id() {
    return get_user_credentials_id("github")
}

/*
* Returns Jenkins credentials ID from the variable file for given key.
*/
def get_user_credentials_id(KEY) {
    if (VARIABLES.credentials && VARIABLES.credentials."${KEY}") {
        return VARIABLES.credentials."${KEY}"
    }

    return ''
}

/*
* Sets the NODE variable to the labels identifying nodes by platform suitable
* to run a Jenkins job.
* Fetches the labels from the variables file when the node is not set as build
* parameter.
* The node's labels could be a single label - e.g. label1 - or a boolean
* expression - e.g. label1 && label2 || label3.
*/
def set_node(job_type) {
    // fetch labels for given platform/spec
    NODE = ''
    for (key in ['NODE', 'LABEL']) {
        // check the build parameters
        if (params.containsKey(key)) {
            NODE = params.get(key)
            break
        }
    }

    if (!NODE) {
        // fetch from variables file
        NODE = get_value(VARIABLES."${SPEC}".node_labels."${job_type}", SDK_VERSION)
        if (!NODE) {
            error("Missing ${job_type} NODE!")
        }
    }
}

/*
* Set the RELEASE variable with the value provided in the variables file.
*/
def set_release() {
    RELEASE = get_value(VARIABLES."${SPEC}".release, SDK_VERSION)
}

/*
* Set the JDK_FOLDER variable with the value provided in the variables file.
*/
def set_jdk_folder() {
    JDK_FOLDER = get_value(VARIABLES.jdk_image_dir, SDK_VERSION)
}

/*
* Sets variables for a job that builds the OpenJ9 extensions for OpenJDK for a
* given platform and version.
*/
def set_build_variables() {
    set_repos_variables()

    // fetch values per spec and Java version from the variables file
    BOOT_JDK = get_value(VARIABLES."${SPEC}".boot_jdk, SDK_VERSION)
    FREEMARKER = VARIABLES."${SPEC}".freemarker
    OPENJDK_REFERENCE_REPO = VARIABLES."${SPEC}".openjdk_reference_repo
    set_release()
    set_jdk_folder()
    set_build_extra_options()

    // set variables for the build environment configuration
    // check job parameters, if not provided default to variables file
    BUILD_ENV_VARS = params.BUILD_ENV_VARS
    if (!BUILD_ENV_VARS && VARIABLES."${SPEC}".build_env) {
        BUILD_ENV_VARS = get_value(VARIABLES."${SPEC}".build_env.vars, SDK_VERSION)
    }

    BUILD_ENV_VARS_LIST = []
    if (BUILD_ENV_VARS) {
        // expect BUILD_ENV_VARS to be a space separated list of environment variables
        // e.g. <variable1>=<value1> <variable2>=<value2> ... <variableN>=<valueN>
        // convert space separated list to array
        BUILD_ENV_VARS_LIST.addAll(BUILD_ENV_VARS.tokenize(' '))
    }

    EXTRA_BUILD_ENV_VARS = params.EXTRA_BUILD_ENV_VARS
    if (EXTRA_BUILD_ENV_VARS) {
        BUILD_ENV_VARS_LIST.addAll(EXTRA_BUILD_ENV_VARS.tokenize(' '))
    }

    BUILD_ENV_CMD = params.BUILD_ENV_CMD
    if (!BUILD_ENV_CMD && VARIABLES."${SPEC}".build_env) {
        BUILD_ENV_CMD = get_value(VARIABLES."${SPEC}".build_env.cmd, SDK_VERSION)
    }

    if (BUILD_ENV_CMD) {
        // BUILD_ENV_CMD is used as preamble command
        BUILD_ENV_CMD += ' && '
    } else {
        BUILD_ENV_CMD = ''
    }

    echo "Using BUILD_ENV_CMD = ${BUILD_ENV_CMD}, BUILD_ENV_VARS_LIST = ${BUILD_ENV_VARS_LIST.toString()}"
}

def set_sdk_variables() {
    DATESTAMP = get_date()
    SDK_FILENAME = "OpenJ9-JDK${SDK_VERSION}-${SPEC}-${DATESTAMP}.tar.gz"
    TEST_FILENAME = "native-test-libs.tar.gz"
    echo "Using SDK_FILENAME = ${SDK_FILENAME}"
    echo "Using TEST_FILENAME = ${TEST_FILENAME}"
}

def get_date() {
    //Returns a string for the current date YYYYMMDD-HHMMSS
    return sh (
        script: 'date +%Y%m%d-%H%M%S',
        returnStdout: true
    ).trim()
}

/*
* Set TESTS_TARGETS, indicating the level of testing.
*/
def set_test_targets() {
    TESTS_TARGETS = params.TESTS_TARGETS
    if (!TESTS_TARGETS) {
        // set default TESTS_TARGETS for pipeline job (fetch from variables file)
        TESTS_TARGETS = get_default_test_targets()
    }

    EXCLUDED_TESTS = []
    if (VARIABLES."${SPEC}".excluded_tests) {
        def excludedTests = get_value(VARIABLES."${SPEC}".excluded_tests, SDK_VERSION)
        if (excludedTests) {
            EXCLUDED_TESTS.addAll(excludedTests.keySet())
        }
    }

    echo "TESTS_TARGETS: ${TESTS_TARGETS}"
    echo "EXCLUDED_TESTS: ${EXCLUDED_TESTS}"
}

def get_default_test_targets() {
    if (VARIABLES.tests_targets && VARIABLES.tests_targets.default) {
        // VARIABLES.tests_targets.default is a map where all values are null
        return VARIABLES.tests_targets.default.keySet().join(',')
    }

    return ''
}

def set_slack_channel() {
    SLACK_CHANNEL = params.SLACK_CHANNEL
    if (!SLACK_CHANNEL && (!params.PERSONAL_BUILD || params.PERSONAL_BUILD != 'true')) {
        SLACK_CHANNEL = VARIABLES.slack_channel
    }
}

def set_artifactory_config() {
    ARTIFACTORY_SERVER = VARIABLES.artifactory_server
    if (ARTIFACTORY_SERVER) {
        env.ARTIFACTORY_SERVER = ARTIFACTORY_SERVER
        env.ARTIFACTORY_REPO = VARIABLES.artifactory_repo
        env.ARTIFACTORY_NUM_ARTIFACTS = VARIABLES.artifactory_num_artifacts
        env.ARTIFACTORY_DAYS_TO_KEEP_ARTIFACTS = VARIABLES.artifactory_days_to_keep_artifacts // This is being used by the cleanup script
        env.ARTIFACTORY_MANUAL_CLEANUP = VARIABLES.artifactory_manual_cleanup // This is being used by the cleanup script
        echo "Using artifactory server/repo: ${ARTIFACTORY_SERVER} / ${ARTIFACTORY_REPO}"
        echo "Keeping '${ARTIFACTORY_NUM_ARTIFACTS}' artifacts"
        echo "Artifactory Manual Cleanup: ${env.ARTIFACTORY_MANUAL_CLEANUP}"
    }
}

def set_restart_timeout() {
    RESTART_TIMEOUT = params.RESTART_TIMEOUT
    if (!RESTART_TIMEOUT) {
        RESTART_TIMEOUT = VARIABLES.restart_timeout.time
    }
    RESTART_TIMEOUT_UNITS = params.RESTART_TIMEOUT_UNITS
    if (!RESTART_TIMEOUT_UNITS) {
        RESTART_TIMEOUT_UNITS = VARIABLES.restart_timeout.units
    }
}

def set_build_identifier() {
    BUILD_IDENTIFIER = params.BUILD_IDENTIFIER
    if (!BUILD_IDENTIFIER) {
        BUILD_IDENTIFIER = ''
        if (params.ghprbPullId && params.ghprbGhRepository) {
            // If Pull Request build
            BUILD_IDENTIFIER = params.ghprbGhRepository + '#' + params.ghprbPullId
        } else if (PERSONAL_BUILD) {
            // If Personal build
            wrap([$class: 'BuildUser']) {
                BUILD_IDENTIFIER = "${BUILD_USER_EMAIL}"
            }
        } else {
            echo "WARNING: Can't determine appropriate BUILD_IDENTIFIER automatically. Please add it as a build parameter if you would like it set."
        }
    }
    if (BUILD_IDENTIFIER) {
        currentBuild.displayName = "#${BUILD_NUMBER} - ${BUILD_IDENTIFIER}"
    }
}

def set_custom_description() {
    if (params.CUSTOM_DESCRIPTION){
        def TMP_DESC = (currentBuild.description) ? "<br>" + currentBuild.description : ""
        currentBuild.description = CUSTOM_DESCRIPTION + TMP_DESC
    }
}

/*
* Adds a String type Parameter
* Takes a Key-Value map which are used as the param name and default value
*/
def add_string_params(PARAMETERS_TO_ADD) {
    PARAMETERS_TO_ADD.each { key, value ->
        PARAMETERS.add(string(name: key, defaultValue: value, description: '', trim: true))
    }
}

def add_pr_to_description() {
    if (params.ghprbPullId) {
        def TMP_DESC = (currentBuild.description) ? currentBuild.description + "<br>" : ""
        currentBuild.description = TMP_DESC + "<a href=https://github.com/${params.ghprbGhRepository}/pull/${params.ghprbPullId}>PR #${params.ghprbPullId}</a>"
    }
}

/*
* Initializes all of the required variables for a Jenkins job by given job type.
*/
def set_job_variables(job_type) {

    // initialize VARIABLES
    parse_variables_file()

    // fetch credentials required to connect to GitHub using ssh protocol
    set_user_credentials()

    // Setup Build Identifier so we can determine where a build belongs
    set_build_identifier()

    // check mandatory parameters if any
    try {
        validate_arguments(ARGS)
    } catch(MissingPropertyException e){
        // ignore when ARGS is not set
    }

    // Add custom description
    set_custom_description()

    switch (job_type) {
        case "build":
            // set the node the Jenkins build would run on
            set_node(job_type)
            // set variables for a build job
            set_build_variables()
            set_sdk_variables()
            set_artifactory_config()
            add_pr_to_description()
            break
        case "pipeline":
            // set variables for a pipeline job
            set_repos_variables()
            set_adoptopenjdk_tests_repository()
            set_vendor_variables()
            set_build_extra_options()
            set_test_targets()
            set_slack_channel()
            set_restart_timeout()
            add_pr_to_description()
            set_misc_variables()
            break
        case "wrapper":
            //set variable for pipeline all/personal
            set_repos_variables(BUILD_SPECS)
            set_build_extra_options(BUILD_SPECS)
            set_adoptopenjdk_tests_repository(get_build_releases(BUILD_SPECS))
            set_restart_timeout()
            set_misc_variables()
            break
        default:
            error("Unknown Jenkins job type:'${job_type}'")
    }
}

/*
* Checks if mandatory arguments are set.
*/
def validate_arguments(ARGS) {
    for (arg in ARGS) {
        if (!params.get(arg)) {
            error("Must specify ${arg}")
        }

        if (arg == 'PLATFORM') {
            // set SPEC - a groovy variable - here
            // SPEC cannot be a build parameter because build parameters are
            // exported as environment variables and it will overwrite the
            // make SPEC variable set in the configure phase in build.build() method
            SPEC = params.get(arg)
        }
    }
}

/*
* Prints the error stack trace.
*/
def printStackTrace(e) {
    def writer = new StringWriter()
    e.printStackTrace(new PrintWriter(writer))
    echo e.toString()
}

/*
* Sets USER_CREDENTIALS_ID to the value provided in the variables file or an
* empty string.
*/
def set_user_credentials() {
    USER_CREDENTIALS_ID = get_git_user_credentials_id()
    if (!USER_CREDENTIALS_ID) {
         USER_CREDENTIALS_ID = ''
    }
     GITHUB_API_CREDENTIALS_ID = get_github_user_credentials_id()
     if (!GITHUB_API_CREDENTIALS_ID) {
         GITHUB_API_CREDENTIALS_ID = ''
    }
}

/*
* Converts from SSH to HTTPS protocol the public Git repository URL and from
* HTTPS to SSH the private repositories.
*/
def convert_url(value) {
    if ((value && value.contains('git@')) && !USER_CREDENTIALS_ID) {
        // external repos, no credentials, use HTTPS protocol
        return value.replace(':', '/').replace('git@', 'https://')
    }

    if ((value && value.startsWith('https://')) && USER_CREDENTIALS_ID) {
        // private repos, have credentials, use SSH protocol
        return value.replace('https://', 'git@').replaceFirst('/', ':')
    }

    return value
}

/*
* Checks for vendor test sources and initializes variables required to clone the
* vendor test source Git repositories. Uses variable files when build parameters
* are not provided.
*/
def set_vendor_variables() {
    VENDOR_TEST_REPOS_MAP = [:]
    VENDOR_TEST_BRANCHES_MAP = [:]
    VENDOR_TEST_DIRS_MAP = [:]
    VENDOR_TEST_SHAS_MAP = [:]

    // initialize variables from build parameters
    // VENDOR_TEST* build parameters are comma separated string parameters
    // convert each comma separated string parameter to array
    def branches = []
    if (params.VENDOR_TEST_BRANCHES) {
        branches.addAll(params.VENDOR_TEST_BRANCHES.tokenize(','))
    }

    def shas = []
    if (params.VENDOR_TEST_SHAS) {
        shas.addAll(params.VENDOR_TEST_SHAS.tokenize(','))
    }

    def dirs = []
    if (params.VENDOR_TEST_DIRS) {
        dirs.addAll(params.VENDOR_TEST_DIRS.tokenize(','))
    }

    if (params.VENDOR_TEST_REPOS) {
        // populate the VENDOR_TEST_* maps

        params.VENDOR_TEST_REPOS.tokenize(',').eachWithIndex { repoURL, index ->
            repoURL = repoURL.trim()
            REPO_NAME = get_repo_name_from_url(repoURL)
            VENDOR_TEST_REPOS_MAP[REPO_NAME] = repoURL

            if (branches[index]) {
                VENDOR_TEST_BRANCHES_MAP[REPO_NAME] = branches[index].trim()
            } else {
                error("Missing branch for ${repoURL} from VENDOR_TEST_BRANCHES parameter")
            }

            if (dirs[index]) {
                VENDOR_TEST_DIRS_MAP[REPO_NAME] = dirs[index].trim()
            }

            if (shas[index]) {
                VENDOR_TEST_SHAS_MAP[REPO_NAME] = shas[index].trim()
            }
        }
    } else {
        // fetch from variables file

        if (VARIABLES.vendor_source && VARIABLES.vendor_source.test) {
            // parse the vendor source multi-map
            // and populate the VENDOR_TEST_* maps

            VARIABLES.vendor_source.test.each { entry ->
                REPO_NAME = get_repo_name_from_url(entry.get('repo'))
                VENDOR_TEST_REPOS_MAP[REPO_NAME] = entry.get('repo')
                VENDOR_TEST_BRANCHES_MAP[REPO_NAME] = entry.get('branch')
                VENDOR_TEST_DIRS_MAP[REPO_NAME] = entry.get('directory')
            }
        }
    }

    // the downstream job expects VENDOR_TEST* parameters to be comma separated strings
    // convert VENDOR_TEST* maps to strings
    if (VENDOR_TEST_REPOS_MAP) {
        VENDOR_TEST_REPOS = VENDOR_TEST_REPOS_MAP.values().join(',')
    } else {
        VENDOR_TEST_REPOS = ''
    }

    if (VENDOR_TEST_BRANCHES_MAP) {
        VENDOR_TEST_BRANCHES = VENDOR_TEST_BRANCHES_MAP.values().join(',')
    } else {
        VENDOR_TEST_BRANCHES = ''
    }

    if (!VENDOR_TEST_SHAS_MAP) {
        VENDOR_TEST_SHAS = ''
    }

    if (VENDOR_TEST_DIRS_MAP) {
        VENDOR_TEST_DIRS = VENDOR_TEST_DIRS_MAP.values().join(',')
    } else {
        VENDOR_TEST_DIRS = ''
    }
}

/*
*  Extracts the Git repository name from URL and converts it to upper case.
*/
def get_repo_name_from_url(URL) {
    return URL.substring(URL.lastIndexOf('/') + 1, URL.lastIndexOf('.git')).toUpperCase()
}

/*
* Returns a list of releases to build.
*/
def get_build_releases(releases_by_specs) {
    def build_releases = []

    // find releases to build based on given specs
    releases_by_specs.values().each { versions ->
        build_releases.addAll(versions)
    }

    return build_releases.unique()
}

/*
* Returns a list of specs for a given version.
*/
def get_build_specs_by_release(version, specs) {
    def specs_by_release = []

    specs.each { build_spec, build_releases ->
        if (build_releases.contains(version)) {
            specs_by_release.add(build_spec)
        }
    }

    return specs_by_release
}

/**
 * Parses the build parameters to identify the specs and the releases to build
 * and test. Returns a map of releases by specs.
 */
def get_specs(SUPPORTED_SPECS) {
    def SPECS = [:]

    if (!RELEASES) {
        // get releases
        params.each { key, value  ->
            if ((key.indexOf('Java') != -1) && (value == true)) {
                RELEASES.add(key.substring(4))
            }
        }
    }
    echo "RELEASES:'${RELEASES}'"

    if (!PLATFORMS) {
        // get specs
        if (!params.PLATFORMS || (params.PLATFORMS.trim() == 'all')) {
            // build and test all supported specs
            PLATFORMS.addAll(SUPPORTED_SPECS.keySet())
        } else {
            PLATFORMS.addAll(params.PLATFORMS.split(","))
        }
    }
    echo "PLATFORMS:'${PLATFORMS}'"
    def RELEASES_STR = RELEASES.join(',')

    // Setup SPEC map
    PLATFORMS.each { SPEC ->
        SPEC = SPEC.trim()
        if (!SUPPORTED_SPECS.keySet().contains(SPEC)) {
            echo "Warning: Unknown or unsupported platform: ${SPEC}"
            return
        }

        if (RELEASES) {
            // personal build
            SPEC_RELEASES = SUPPORTED_SPECS[SPEC].intersect(RELEASES)
         } else {
            SPEC_RELEASES = SUPPORTED_SPECS[SPEC]
         }

        if (SPEC_RELEASES) {
            // populate the releases by specs map
            SPECS[SPEC] = SPEC_RELEASES
        } else {
            echo "Warning: Selected Java versions (${RELEASES_STR}) for ${SPEC} is not supported(nothing to do)!"
        }
    }

    if (!SPECS) {
        error("Invalid PLATFORMS (${params.PLATFORMS}) and/or Java versions selections(${RELEASES_STR})!")
    }
    echo "SPECS:'${SPECS}'"
    return SPECS
}

/*
* Set build extra options
*/
def set_build_extra_options(build_specs=null) {
    if (!build_specs) {
        // single release
        EXTRA_GETSOURCE_OPTIONS = params.EXTRA_GETSOURCE_OPTIONS
        if (!EXTRA_GETSOURCE_OPTIONS) {
            EXTRA_GETSOURCE_OPTIONS = get_value(VARIABLES."${SPEC}".extra_getsource_options, SDK_VERSION)
        }

        EXTRA_CONFIGURE_OPTIONS = params.EXTRA_CONFIGURE_OPTIONS
        if (!EXTRA_CONFIGURE_OPTIONS) {
            EXTRA_CONFIGURE_OPTIONS = get_value(VARIABLES."${SPEC}".extra_configure_options, SDK_VERSION)
        }

        EXTRA_MAKE_OPTIONS = params.EXTRA_MAKE_OPTIONS
        if (!EXTRA_MAKE_OPTIONS) {
            EXTRA_MAKE_OPTIONS = get_value(VARIABLES."${SPEC}".extra_make_options, SDK_VERSION)
        }

        OPENJDK_CLONE_DIR = params.OPENJDK_CLONE_DIR
        if (!OPENJDK_CLONE_DIR) {
            OPENJDK_CLONE_DIR = ''
        }
    } else {
        // multiple releases
        EXTRA_GETSOURCE_OPTIONS_MAP = [:]
        EXTRA_CONFIGURE_OPTIONS_MAP = [:]
        EXTRA_MAKE_OPTIONS_MAP = [:]
        OPENJDK_CLONE_DIR_MAP = [:]

        def build_releases = get_build_releases(build_specs)
        build_releases.each { release ->
            def extra_getsource_options_by_specs = [:]
            def extra_configure_options_by_specs = [:]
            def extra_make_options_by_specs = [:]
            def clone_dir_by_specs = [:]

            build_specs.each { spec, releases ->
                if (releases.contains(release)) {
                    ['EXTRA_GETSOURCE_OPTIONS', 'EXTRA_CONFIGURE_OPTIONS', 'EXTRA_MAKE_OPTIONS'].each { it ->
                        // look up extra options by release and spec/platform provided as build parameters:
                        // e.g. EXTRA_GETSOURCE_OPTIONS_JDK<release>_<spec>
                        //      EXTRA_CONFIGURE_OPTIONS_JDK<release>_<spec>
                        //      EXTRA_MAKE_OPTIONS_JDK<release>_<spec>
                        def extraOptions = params."${it}_JDK${release}_${spec}"
                        if (!extraOptions && VARIABLES."${spec}") {
                            extraOptions = get_value(VARIABLES."${spec}"."${it.toLowerCase()}", release)
                        }

                        if (extraOptions) {
                            // populate maps by specs
                            switch (it) {
                                case 'EXTRA_GETSOURCE_OPTIONS':
                                    extra_getsource_options_by_specs.put(spec, extraOptions)
                                    break
                                case 'EXTRA_CONFIGURE_OPTIONS':
                                    extra_configure_options_by_specs.put(spec, extraOptions)
                                    break
                                case 'EXTRA_MAKE_OPTIONS':
                                    extra_make_options_by_specs.put(spec, extraOptions)
                                    break
                            }
                        }
                    }

                    def cloneDir = params."OPENJDK${release}_CLONE_DIR_${spec}"
                    if (!cloneDir) {
                        cloneDir = params."OPENJDK${release}_CLONE_DIR"
                    }
                    if (cloneDir) {
                        clone_dir_by_specs.put(spec, cloneDir)
                    }
                }
            }

            // populate maps by releases and specs
            if (extra_getsource_options_by_specs) {
                EXTRA_GETSOURCE_OPTIONS_MAP.put(release, extra_getsource_options_by_specs)
            }
            if (extra_configure_options_by_specs) {
                EXTRA_CONFIGURE_OPTIONS_MAP.put(release, extra_configure_options_by_specs)
            }
            if (extra_make_options_by_specs) {
                EXTRA_MAKE_OPTIONS_MAP.put(release, extra_make_options_by_specs)
            }
            if (clone_dir_by_specs) {
                OPENJDK_CLONE_DIR_MAP.put(release, clone_dir_by_specs)
            }
        }
    }
}

/*
* Set the Git repository URL and branch for the AdoptOpenJDK Testing material.
*/
def set_adoptopenjdk_tests_repository(build_releases=null) {
    ADOPTOPENJDK_MAP = [:]

    if (build_releases) {
        for (release in build_releases) {
            // fetch from the variables file
            def adoptOpenJdkByReleaseMap = [:]
            if (get_value(VARIABLES.adoptopenjdk, release)) {
                adoptOpenJdkByReleaseMap.putAll(get_value(VARIABLES.adoptopenjdk, release))
            } else {
                adoptOpenJdkByReleaseMap.putAll(VARIABLES.adoptopenjdk.default)
            }

            if (params."ADOPTOPENJDK${release}_REPO") {
                // override repo with user value
                adoptOpenJdkByReleaseMap['repoUrl'] = params."ADOPTOPENJDK${release}_REPO"
            }

            if (params."ADOPTOPENJDK${release}_BRANCH") {
                // override branch with user value
                adoptOpenJdkByReleaseMap['branch'] = params."ADOPTOPENJDK${release}_BRANCH"
            }

            if (adoptOpenJdkByReleaseMap) {
                ADOPTOPENJDK_MAP.put(release, adoptOpenJdkByReleaseMap)
            }
        }

        echo "ADOPTOPENJDK_MAP = ${ADOPTOPENJDK_MAP.toString()}"
    } else {
        // fetch from the variables file
        def adoptOpenJdkByReleaseMap = VARIABLES.adoptopenjdk.default

        ADOPTOPENJDK_REPO = params.ADOPTOPENJDK_REPO
        if (!ADOPTOPENJDK_REPO && adoptOpenJdkByReleaseMap) {
            ADOPTOPENJDK_REPO = adoptOpenJdkByReleaseMap.get('repoUrl')
        }

        ADOPTOPENJDK_BRANCH = params.ADOPTOPENJDK_BRANCH
        if (!ADOPTOPENJDK_BRANCH && adoptOpenJdkByReleaseMap) {
            ADOPTOPENJDK_BRANCH = adoptOpenJdkByReleaseMap.get('branch')
        }

        echo "Using ADOPTOPENJDK_REPO = ${ADOPTOPENJDK_REPO} ADOPTOPENJDK_BRANCH = ${ADOPTOPENJDK_BRANCH}"
    }
}

// Creates a job using the job DSL plugin on Jenkins
def create_job(JOB_NAME, SDK_VERSION, SPEC, downstreamJobType, id){

    // NOTE: The following 4 DEFAULT variables can be configured in the Jenkins Global Config.
    // This allows us to know what the default values are without being told explicitly.
    // These DEFAULT values are typically constant per Jenkins Server.
    if (!env.VARIABLE_FILE_DEFAULT) {
        VARIABLE_FILE_DEFAULT = ''
    }
    if (!env.VENDOR_REPO_DEFAULT) {
        VENDOR_REPO_DEFAULT = ''
    }
    if (!env.VENDOR_BRANCH_DEFAULT) {
        VENDOR_BRANCH_DEFAULT = ''
    }
    if (!env.VENDOR_CREDENTIALS_ID_DEFAULT) {
        VENDOR_CREDENTIALS_ID_DEFAULT = ''
    }

    // Configuring the build discarder in the downstream project

    DISCARDER_NUM_BUILDS = get_value(VARIABLES.build_discarder.logs, id)

    def params = [:]
    params.put('JOB_NAME', JOB_NAME)
    params.put('SDK_VERSION', SDK_VERSION)
    params.put('PLATFORM', SPEC)
    params.put('VARIABLE_FILE_DEFAULT', VARIABLE_FILE_DEFAULT)
    params.put('VENDOR_REPO_DEFAULT', VENDOR_REPO_DEFAULT)
    params.put('VENDOR_BRANCH_DEFAULT', VENDOR_BRANCH_DEFAULT)
    params.put('VENDOR_CREDENTIALS_ID_DEFAULT', VENDOR_CREDENTIALS_ID_DEFAULT)
    params.put('jobType', downstreamJobType)
    params.put('DISCARDER_NUM_BUILDS', DISCARDER_NUM_BUILDS)

    def templatePath = 'buildenv/jenkins/jobs/pipelines/Pipeline_Template.groovy'

    create = jobDsl targets: templatePath, ignoreExisting: false, additionalParameters: params
}

def set_misc_variables() {
    GHPRB_REPO_OPENJ9 = VARIABLES.ghprbGhRepository_openj9
}

return this
