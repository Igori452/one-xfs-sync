IMAGE = one-xfs-sync-env
CONTAINER = one-xfs-sync-env-box
GCC_VER ?= 13

build-img:
	docker build -t $(IMAGE) .

dest-img:
	docker rmi $(IMAGE)

up:
	docker run -d --name $(CONTAINER) -v "$(PWD):/app" $(IMAGE)

down:
	docker rm $(CONTAINER)

start:
	docker start $(CONTAINER)

stop:
	docker stop $(CONTAINER)

sh:
	docker exec -it $(CONTAINER) bash

sync-headers:
	rm -rf .docker-include
	docker cp $(CONTAINER):/usr/include .docker-include
	mkdir -p .docker-include/gcc-include
	docker cp $(CONTAINER):/usr/lib/gcc/x86_64-linux-gnu/$(GCC_VER)/include/. .docker-include/gcc-include/

init-ide:
	mkdir -p .vscode
	printf '%s\n' \
	  '{' \
	  '  "configurations": [' \
	  '    {' \
	  '      "name": "Docker Linux C++20",' \
	  '      "compilerPath": "",' \
	  '      "includePath": [' \
	  '        "$${workspaceFolder}/cpp/include",' \
	  '        "$${workspaceFolder}/c/include",' \
	  '        "$${workspaceFolder}/.docker-include/c++/$(GCC_VER)",' \
	  '        "$${workspaceFolder}/.docker-include/x86_64-linux-gnu/c++/$(GCC_VER)",' \
	  '        "$${workspaceFolder}/.docker-include/c++/$(GCC_VER)/backward",' \
	  '        "$${workspaceFolder}/.docker-include/gcc-include",' \
	  '        "$${workspaceFolder}/.docker-include/x86_64-linux-gnu",' \
	  '        "$${workspaceFolder}/.docker-include",' \
	  '        "$${workspaceFolder}/**"' \
	  '      ],' \
	  '      "defines": [' \
	  '        "__linux__",' \
	  '        "__x86_64__",' \
	  '        "__GNUC__=$(GCC_VER)",' \
	  '        "_GNU_SOURCE"' \
	  '      ],' \
	  '      "cStandard": "c17",' \
	  '      "cppStandard": "c++20",' \
	  '      "intelliSenseMode": "linux-gcc-x64"' \
	  '    }' \
	  '  ],' \
	  '  "version": 4' \
	  '}' > .vscode/c_cpp_properties.json

.PHONY: build-img dest-img up down start stop sh compile sync-headers init-ide