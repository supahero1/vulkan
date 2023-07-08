#-lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi
.PHONY: all
all: build

ifdef OS
CFLAGS := -Wall -lm -lglfw3 -lvulkan-1
OUTPUT := bin/exe.exe
endif
ifndef OS
CFLAGS := -Wall -lm -lglfw -lvulkan
OUTPUT := bin/exe
endif

ifeq ($(RELEASE),1)
CFLAGS := -O3 -DNDEBUG $(CFLAGS)
endif
ifneq ($(RELEASE),1)
CFLAGS := -O0 -g3 -ggdb $(CFLAGS)
endif

COMP := $(CC) src/* -o bin/exe



.PHONY: shaders
shaders:
	glslc shaders/shader.vert -o bin/vert.spv
	glslc shaders/shader.frag -o bin/frag.spv

.PHONY: build
build: shaders
	$(COMP) $(CFLAGS) && $(OUTPUT)

.PHONY: valgrind
valgrind: shaders
	$(COMP) $(CFLAGS) && valgrind $(OUTPUT)

.PHONY: sanitize
sanitizer: shaders
	$(CC) -fsanitize=address,undefined $(CFLAGS) && $(OUTPUT)
