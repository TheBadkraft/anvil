CC := gcc
PYTHON ?= python3
ANVIL_ROOT := ..
OUT_DIR := .handoff
MANIFEST := $(OUT_DIR)/binding-handoff.json

.PHONY: all generate node python dotnet contract handoff signoff pull-node clean

all: generate

generate: node python dotnet contract handoff
	@echo "[bindings] generation complete"

node:
	@echo "[bindings/node] generate"
	@if [ -x ./node/scripts/generate.sh ]; then \
		./node/scripts/generate.sh; \
	else \
		echo "[bindings/node] skip: node/scripts/generate.sh not found"; \
	fi

python:
	@echo "[bindings/python] generate"
	@if [ -x ./python/scripts/generate.sh ]; then \
		./python/scripts/generate.sh; \
	else \
		echo "[bindings/python] skip: python/scripts/generate.sh not found"; \
	fi

dotnet:
	@echo "[bindings/dotnet] generate"
	@if [ -x ./dotnet/scripts/generate.sh ]; then \
		./dotnet/scripts/generate.sh; \
	else \
		echo "[bindings/dotnet] skip: dotnet/scripts/generate.sh not found"; \
	fi

contract:
	@mkdir -p "$(OUT_DIR)"
	@$(PYTHON) ./scripts/handoff_manifest.py --root "$(ANVIL_ROOT)" --out "$(MANIFEST)"
	@echo "[bindings] wrote $(MANIFEST)"

handoff:
	@echo "[bindings] handoff bundle ready: $(MANIFEST)"

signoff:
	@echo "Use docs/maintainers/bindings-signoff-checklist.md"

pull-node:
	@$(PYTHON) ./scripts/pull_node_binding.py

clean:
	rm -rf "$(OUT_DIR)"
