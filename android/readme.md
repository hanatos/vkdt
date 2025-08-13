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

# assets
move to consistent place, i.e. presets and keyaccels out of data/

# debug:
```
adb shell
am start -S -n org.dreggn.vkdt/.VulkanActivity
```
and then look at log via 
```
logcat | grep vkdt grep " W "
```
