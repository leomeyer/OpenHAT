SUBDIRS = linux_test WindowPlugin FritzBoxPlugin WeatherPlugin MQTTPlugin WebServerPlugin
#ArducomPlugin


UNAME_M = $(shell uname -m)
ifeq ($(UNAME_M),armv6l)
	SUBDIRS += rpi/gertboard rpi/RemoteSwitch
endif

.PHONY: subdirs $(SUBDIRS)

subdirs: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@
