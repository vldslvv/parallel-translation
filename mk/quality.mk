##@ Quality

.PHONY: format
format: ## Auto-format source files with clang-format
	find src tests -name '*.cpp' -o -name '*.hpp' | xargs clang-format -i

.PHONY: tidy tidy-fix
tidy: ## Run clang-tidy static analysis on src/ and tests/
tidy-fix: ## Apply available clang-tidy fixes to src/ and tests/
tidy-fix: TIDY_FIX_FLAG := -fix

tidy tidy-fix:
	run-clang-tidy $(TIDY_FIX_FLAG) -quiet -p $(DEBUG_DIR) \
		-header-filter '^$(CURDIR)/(src|tests)/' \
		'^$(CURDIR)/(src|tests)/'
