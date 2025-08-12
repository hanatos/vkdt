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
androidApp->activity->assetManager
`AAsset* asset = AAssetManager_open(*assetManager, filename, AASSET_MODE_BUFFER);`
use `android_fopen`
place android app pointer somewhere on `dt_pipe`?

check `dt_pipe.basedir` accesses, only these go through `android_fopen`!
* how many fopen do we really have (replace by `dt_fopen`?)
* only basedir->apk
* some fopen stuff will undoubtedly go through basedir || homedir (need polymorphism?)
* images are opened the regular way (raw, mcraw, jpg, ..)
* need `fs_copy` from apk too
* have users for plain fopen: mlv,mcraw,hdr,exr.
* i-lut needs to work with apk, but we can write that manually

open:
* resource of specific graph (e.g. image for cfg, data/.lut, ..)
* resource in homedir or basedir (e.g. font, styles, cm,..)
```
DTFILE* dt_open(
    dt_graph_t *g,        // or 0 if not associated with a graph
    const char *filename, // relative to graph, home, or basedir(apk)
    const char mode)
```
and similar again for filename to hand to c++ or rust (in which case the
apk/basedir will not work)

# debug:
```
adb shell
am start -S -n org.dreggn.vkdt/.VulkanActivity
```
and then look at log via 
```
logcat | grep vkdt grep " W "
```
