# Makefile principal para compilar todo el TP

MODULES = utils worker storage master query_control

.PHONY: all clean debug release $(MODULES)

all: $(MODULES)

debug: $(MODULES)

release:
	@for module in $(MODULES); do \
		echo "=== Building $$module (release) ==="; \
		$(MAKE) -C $$module release; \
	done

$(MODULES):
	@echo "=== Building $@ ==="
	@$(MAKE) -C $@

# Dependencias entre módulos (utils debe compilarse primero)
worker: utils
master: utils
storage: utils
query_control: utils

clean:
	@for module in $(MODULES); do \
		echo "=== Cleaning $$module ==="; \
		$(MAKE) -C $$module clean; \
	done

# Limpiar y recompilar todo
rebuild: clean all
