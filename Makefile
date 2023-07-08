#-lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi
.PHONY: all
all: build

CFLAGS := -Wall -lm -lglfw -lvulkan

ifneq ($(RELEASE),1)
CFLAGS := -O0 -g3 -ggdb $(CFLAGS)
endif

ifeq ($(RELEASE),1)
CFLAGS := -O3 -DNDEBUG $(CFLAGS)
endif

COMP := $(CC) src/* -o bin/exe

.PHONY: shaders
shaders:
	glslc shaders/shader.vert -o bin/vert.spv
	glslc shaders/shader.frag -o bin/frag.spv

.PHONY: build
build: shaders
	$(COMP) $(CFLAGS) && bin/exe

.PHONY: valgrind
valgrind: shaders
	$(COMP) $(CFLAGS) && valgrind bin/exe

.PHONY: sanitize
sanitizer: shaders
	$(CC) -fsanitize=address,undefined $(CFLAGS) && bin/exe
