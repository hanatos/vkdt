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
```

something to create modlist.h, i.e. call `create_cmake.sh`

install rustup, not rust, and then
```bash
rustup default stable
rustup target add aarch64-linux-android
cd src/pipe/modules/i-raw/rawloader-c
cargo build --target aarch64-linux-android --release
```
also run a regular build using the makefile system, to generate
all the required intermediates (tooltips from docs, colour luts, etc).

build:
```bash
./gradlew assemble
```


#---

# debug:
make sure usb debugging is on in your phone (something esoteric developer mode + settings switch)
then:
```
./gradlew installDebug
adb shell
am start -S -n org.dreggn.vkdt/.VulkanActivity
```
and then look at log via 
```
adb shell
logcat | grep vkdt | grep " W "
```
or, for stack trace instead of our log/warning messages
```
logcat | grep vkdt | grep " F "
```

