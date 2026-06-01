CONAN_HOME_DIR := $(abspath $(BUILD_DIR)/conan-home)
CONAN_SETTINGS := $(CONAN_HOME_DIR)/settings.yml
CONAN_PROFILE := $(CONAN_HOME_DIR)/profiles/default
CONAN_CPPSTD := 23
CONAN := CONAN_HOME=$(CONAN_HOME_DIR) conan
MORPHEUS_RECIPE := conan/recipes/morpheus
MORPHEUS_VERSION := $(shell sed -n 's/^MORPHEUS_VERSION = "\([^"]*\)"/\1/p' conanfile.py)

.PHONY: conan-settings
conan-settings:
	@mkdir -p $(CONAN_HOME_DIR)
	@$(CONAN) config home >/dev/null
	@python3 conan/allow_gcc16.py $(CONAN_SETTINGS)

.PHONY: conan-profile
conan-profile: conan-settings
	$(CONAN) profile detect --force

.PHONY: morpheus-recipe
morpheus-recipe: conan-settings
	$(CONAN) export $(MORPHEUS_RECIPE) --version=$(MORPHEUS_VERSION)
