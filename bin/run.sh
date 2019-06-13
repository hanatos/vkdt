#!/bin/bash
VULKAN_SDK=/opt/vulkan/1.1.106.0/x86_64
export PATH=${PATH}:${VULKAN_SDK}/bin
export LD_LIBRARY_PATH=${VULKAN_SDK}/lib
# and the layers are in ~/.local/share/vulkan/explicit_layer.d.
./vkdt -g gui.cfg -d all
