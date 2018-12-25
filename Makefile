#@IgnoreInspection BashAddShebang
TOP := $(CURDIR)
BUILD := $(TOP)/build
MAKE := $(MAKE)
CMAKE := cmake

all:
	test -d $(BUILD) || (mkdir -p $(BUILD) && cd $(BUILD) && $(CMAKE) -D CMAKE_BUILD_TYPE=Release $(TOP))
	cd $(BUILD) && $(MAKE)
.PHONY: all


prebuild:
	mkdir -p $(BUILD) || true
.PHONY: prebuild


debug: prebuild
	cd $(BUILD) && $(CMAKE) -D CMAKE_BUILD_TYPE=Debug $(TOP) && $(MAKE)
.PHONY: debug


release: prebuild
	cd $(BUILD) && $(CMAKE) -D CMAKE_BUILD_TYPE=Release $(TOP) && $(MAKE)
.PHONY: release

test:
	cd $(BUILD) && $(MAKE) test
.PHONY: test

clean:
	cd $(BUILD) && $(MAKE) clean
.PHONY: clean


distclean:
	-rm -rf $(BUILD)
.PHONY: distclean
dc: distclean
