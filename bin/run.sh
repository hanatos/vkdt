#!/bin/bash
VK_SDK_PATH=/opt/vulkan/1.1.114.0/x86_64
export PATH=${PATH}:${VK_SDK_PATH}/bin
export LD_LIBRARY_PATH=${VK_SDK_PATH}/lib
# and the layers are in ~/.local/share/vulkan/explicit_layer.d, i.e.:
# mkdir -p ~/.local/share/vulkan && cp -r ${VULKAN_SDK}/etc/explicit_layer.d/ ~/.local/share/vulkan
gdb --args ./vkdt -g gui.cfg -d all
