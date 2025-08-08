# native android build of vkdt

to install the sdk:

yay android-sdk-cmdline-tools-latest
yay android-sdk-build-tools
yay android-sdk-platform-tools
yay android-platform
yay android-tools
su -
pacman -Syu jdk21-openjdk
archlinux-java set java-21-openjdk 
# have write permissions. there are better ways 
# (see https://wiki.archlinux.org/title/Android), but fuck it:
chown -R you:you /opt/android-sdk
export ANDROID_HOME=/opt/android-sdk
sdkmanager --licenses

build:
./gradlew assemble

#---

# gui.c:
pull out glfwInit and glfwVulkanSupported to main
set vkdt.win.window to something not 0 (to tell win0 and win1 apart)


# use google tutorial for vk loader:
https://github.com/googlesamples/android-vulkan-tutorials/tree/master

# access resources:
androidApp->activity->assetManager
