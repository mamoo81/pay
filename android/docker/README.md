# Docker for Android

To compile and deploy Flowee Pay for native you need a compiler and some libraries installed,
nothing too complex or strange for software dev.

Building Flowee Pay for Android is a lot harder, though. Cross-compiler, the Android software
stack and naturally the same libraries but this time compiled for Android. Which are not
readilly available for most cases.

So, the solution, make a docker image that has all this and every compile or package action
can be done using this docker image.

In this directory you find the means to create this docker image. The recipe, as it were.

Its trivial if you just want to use it.
Basically, you make sure the docker infrastructure is running and then you run the shell script.
Wait for (quite) a while and your docker image is done, ready for use.

# Versions

The image has in /etc/versions all the versions of the main software we put in there.

Boost is a bit older since the Android libc seems to be older too.
The latest boost (1.79) gave me linking errors for the 'statx' symbol.

For OpenSSL, Qt and other other libs we try to use the latest release.

# Caching

If you are still here, reading, it implies you want to alter the build image recipe and maybe fix
or tweak things.

There is a pretty good chance that you'll end up running 'docker build' multiple times and
to avoid downloading all the software every time, I made a 'caching' concept.

Enable caching and repeat runs skip a lot of work and slow downloads,
but the result is a significantly larger image.
So, good during development of the image, like a debug build.

To enable this;

1. edit the build-docker.sh to enable the last 4 lines and allow it to copy the
downloaded stuff from the built image into the cache.

2. comment out the 'rm -rf /usr/local' at the bottom of the Dockerfile

3. remove comment (enable) the line in the Dockerfile:
    add cache /usr/local/cache/

Please note that should the build fail, the cached items will not be recovered
from that build, so better make sure you run though the stable build once to get
the cache filled up.

