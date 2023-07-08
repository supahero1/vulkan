#-lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi
.PHONY: all
all: build

.PHONY: shaders
shaders:
	glslc shaders/shader.vert -o bin/vert.spv
	glslc shaders/shader.frag -o bin/frag.spv

.PHONY: build
build: shaders
	$(CC) src/* -o bin/exe -O0 -g3 -ggdb -Wall -lm -lglfw -lvulkan && bin/exe

.PHONY: valgrind
valgrind: shaders
	$(CC) src/* -o bin/exe -O0 -g3 -ggdb -Wall -lm -lglfw -lvulkan && valgrind bin/exe

.PHONY: sanitize
sanitizer: shaders
	$(CC) src/* -o bin/exe -O0 -g3 -ggdb -Wall -fsanitize=address,undefined -lm -lglfw -lvulkan && bin/exe
