IMAGE = one-xfs-sync-env
CONTAINER = one-xfs-sync-env-box

build-img:
	docker build -t $(IMAGE) .

up:
	docker run -d --name $(CONTAINER) -v "$(PWD):/app" $(IMAGE)

sh:
	docker exec -it $(CONTAINER) bash

down:
	docker stop $(CONTAINER) && docker rm $(CONTAINER)

sync-headers:
	rm -rf .docker-include
	docker cp $(CONTAINER):/usr/include .docker-include

init-ide:
	rm -rf .docker-include
	docker cp $(CONTAINER):/usr/include .docker-include
	mkdir -p .vscode
	printf '{\n\
  "configurations": [\n\
    {\n\
      "name": "Docker Linux Boost",\n\
      "includePath": [\n\
        "$${workspaceFolder}/cpp/include",\n\
        "$${workspaceFolder}/**",\n\
        "$${workspaceFolder}/.docker-include",\n\
        "$${workspaceFolder}/.docker-include/x86_64-linux-gnu"\n\
      ],\n\
      "defines": [],\n\
      "cppStandard": "c++20",\n\
      "intelliSenseMode": "linux-gcc-x64"\n\
    }\n\
  ],\n\
  "version": 4\n\
}\n' > .vscode/c_cpp_properties.json

.PHONY: build-img up sh compile down sync-headers init-ide