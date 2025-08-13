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

install rustup, not rust, and then
```bash
rustup default stable
cargo build --target aarch64-linux-android --release
```
also run a regular build using the makefile system, to generate
all the required intermediates (tooltips from docs, colour luts, etc).

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


# access resources:

* check `dt_pipe.basedir` accesses, replace by res.h
  * keyaccel.h
  * render_darkroom.c
  * render.c
  * widget_filteredlist.h
  * render_lighttable.c
  * nuklear_glfw_vulkan.h : fonts from apk
  * graph-run-modules.h
  * global.c
  * graph-io.c
  * graph.c
  * o-jpg/main.c : embed exif, simply don't
  * i-raw/main.cc

* have users for plain fopen: mlv,mcraw,hdr,exr.

TODO: opendir/readdir/closedir wrapper to work with apk

# debug:
```
adb shell
am start -S -n org.dreggn.vkdt/.VulkanActivity
```
and then look at log via 
```
logcat | grep vkdt grep " W "
```
