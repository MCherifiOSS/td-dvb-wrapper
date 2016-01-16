# td-dvb-wrapper
This is a wrapper driver which presents the TD frontend as a Linux-DVB compliant device.

**This driver will never be useful anywhere else than on a Tripledragon set-top-box!**

The Tripledragon drivers present an userspace API which is similar to, but different from the standard Linux DVB API.
This driver creates a standard compliant `/dev/dvb/adapter0/frontend0` device and translates the Linux DVB API ioctls into Tripledragon API ioctls.

The way this is done is both ugly and elegant ;-)
There are a few linitations that need to be observed:
  * the kernel version is fixed at version 2.6.12, the sources from which the kernel was built are not available
  * the sources for the audio/video/tuner drivers are not available

This means that there is no way of just "building a proper driver" or "just use CUSE to do what you want".

So this driver just opens the "original" device file `/dev/stb/tdtuner` from kernel space and then acts on this device as if it were userspace, translating the requests from one API to the other. That's the ugly part.

The elegant part is, that it only needs this relatively small driver loaded and the frontend will just work. Alternatives (like backporting CUSE to kernel 2.6.12 and using a userspace daemon for converting the ioctls) will be much more complex to set up and to make sure it works as intended.

**Note:** the subset of DVB API which is implemented by this driver is exactly the part that is used by [neutrino-mp](https://github.com/neutrino-mp/), as this is the only user of this driver I know of.

**Will there be wrappers for demux and audio / video devices, too?**
That's very unlikely. Neutrino-mp uses a library (libstb-hal) to access demux and A/V devices, so this stuff is abstracted in that library (which has hardware specific parts for that anyway). Additionally, there are subtle differences in demux and media interfaces between the standard Linux API and Tripldragon API which makes it much harder to simply translate them like in the frontend case.
