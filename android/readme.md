# native android build of vkdt

to install the sdk:

```bash
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
# back to your own user
export ANDROID_HOME=/opt/android-sdk
/opt/android-sdk/cmdline-tools/latest/bin/sdkmanager --licenses
# until we have modules:
mkdir mod
```

build:
```bash
./gradlew assemble
```

something retarded about requiring
```c
#include <bits/signal_types.h>
#include <signal.h>
```
in the most ridiculous places (like the `native_app_glue/android_native_app_glue.c` in the sd).

#---

# gui.c:
pull out glfwInit and glfwVulkanSupported to main
set vkdt.win.window to something not 0 (to tell win0 and win1 apart)


# use google tutorial for vk loader:
https://github.com/googlesamples/android-vulkan-tutorials/tree/master

# access resources:
androidApp->activity->assetManager
AAsset* asset = AAssetManager_open(*assetManager, filename, AASSET_MODE_BUFFER);
