export DFLT_CONFIG_FILE=-Dtest.target.config=config.defaults
export RT_CONFIG_FILE=-Dtest.target.config=config.realtime.defaults
export REALTIME_PLATFORM=FALSE

ifeq ("$(PLATFORM)","linux_x86-32_hrt")
export DFLT_CONFIG_FILE=$(RT_CONFIG_FILE)
export REALTIME_PLATFORM=TRUE
endif

SYSV_CLEANUP :=perl $(JVM_TEST_ROOT)$(D)TestConfig$(D)scripts$(D)tools$(D)sysvcleanup.pl all
ifeq ("$(PLATFORM)", "wind") 
	SYSV_CLEANUP := ""
else ifeq ("$(PLATFORM)", "aix") 
	SYSV_CLEANUP :=perl $(JVM_TEST_ROOT)$(D)TestConfig$(D)scripts$(D)tools$(D)sysvcleanup.pl all aix
else ifeq ("$(PLATFORM)", "zos") 
	SYSV_CLEANUP :=perl $(JVM_TEST_ROOT)$(D)TestConfig$(D)scripts$(D)tools$(D)sysvcleanup.pl all zos
endif