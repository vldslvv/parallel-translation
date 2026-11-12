##@ Quality

.PHONY: format
format: ## Auto-format source files with clang-format
	find src tests -name '*.cpp' -o -name '*.hpp' | xargs clang-format -i

.PHONY: tidy
tidy: ## Run clang-tidy static analysis on src/ and tests/
	run-clang-tidy -quiet -p $(DEBUG_DIR) \
		-header-filter '^$(CURDIR)/(src|tests)/' \
		'^$(CURDIR)/(src|tests)/'
