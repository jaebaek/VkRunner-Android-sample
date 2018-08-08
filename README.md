VkRunner Android sample
========

A simple Android app running VkRunner as a library.
For the information of VkRunner, see [here](https://github.com/Igalia/vkrunner).

### Build

If you cloned this app, you must first execute the following command:

```bash
git submodule init
```

Whenever you build it again, execute the following commands:

```bash
git submodule update
./gradlew build
```

### Install

```bash
./gradlew installDebug
```

### Generate VkRunner scripts

Current VkRunner scripts were used in this app are generated by
[generate\_scripts.sh](generate_scripts.sh).

### License

This app is copied from
[Google's android NDK samples](https://github.com/googlesamples/android-ndk).
Since the android NDK samples are based on
[Apache 2.0 license](https://github.com/googlesamples/android-ndk/blob/master/LICENSE),
most of files in this repo also follow it. However, some files e.g.,
[app/Android.mk](app/Android.mk) and [app/test.cpp](app/test.cpp) are copied
from VkRunner and slightly modified. They are under MIT license.
