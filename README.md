This is the repository of Flowee Pay.

Flowee Pay is a payment solution, also often called wallet, that allows
users to pay and receive Bitcoin Cash in a simple application with little
to no external parties or (indexing) servers it needs to depend on.
The only really required dependent is the peer to peer network, and thus
the Internet.

We use QML for the user interface, which allows fast turnaround for
skinning and a very strong model/view separation. The goal here is to have
multiple user interfaces for the one codebase. For instance you can have
a very different user experience and set of features on desktop than on
Android. You can simply "skin" an existing GUI and change it to have your
companies logo (we want you to do that! Just please use a different name
for the app you ship then!)


The goal of having a Free Software product like Flowee Pay is that average
users can use the community client and when companies that want to bundle a
wallet with their product (for instance to do some token thingy) they are
more than welcome to provide their own skinning instead of the ones that
are included in this repo.

Any companies or groups doing this are going to help increase the quality
of the main free software product and thus this benefits all.


BUILDING

Flowee Pay uses libraries from Flowee, you need to
either install the main flowee package via your package manager
or compile it before you compile Pay.
The minimum version required for the Flowee libraries is 2022.07.0

For ubuntu getting the latest is a matter of calling:

``` sh
  sudo add-apt-repository ppa:flowee/ppa
  sudo apt update
  sudo apt install flowee-libs
```

Additionally you will want to have some development packages installed;

``` sh
    sudo apt install build-essential libboost-all-dev libssl-dev cmake \
      libzxing-dev qdbus-qt6 qt6-l10n-tools, libqt6shadertools6-dev \
      libqt6svg6-dev qt6-base-dev qt6-declarative-dev qt6-tools-dev
```

If you use the older Ubuntu Jammy (2022-04), you need to also install
 `libgl-dev` since that was missing fromt the Qt packages back then.

After installing that succesfully, you can compile Flowee Pay by fetching
the sources and in the source tree run this sequence:

``` sh
  mkdir build
  cd build
  cmake ..
  make install
```

Running Flowee Pay requires Qt Quick, which on debian based Linux distro's
have been packaged to a painful number of tiny packages and as such you
will need these too:

``` sh
  sudo apt install \
     qml6-module-qtquick qml6-module-qtqml-workerscript \
     qml6-module-qtquick-controls qml6-module-qtquick-dialogs \
     qml6-module-qtquick-layouts qml6-module-qtquick-shapes \
     qml6-module-qtquick-templates qml6-module-qtquick-window \
```


### Manually compiling flowee-libs:

We depend on the libraries shipped in 'theHub' git repo, also from Flowee.
If you compile that yourself, you need to make sure you `make install` it.
The Flowee Pay buildsystem may not find this if the install directory is not the
same as the packaged. In that case you may want to replace the cmake call
above with this slightly more complex one:

``` sh
   cmake -DCMAKE_PREFIX_PATH=/path/to/the/thehub-installed-dir ..
```

## DEVS

Want to start Hacking, getting to know QML as well? Here is a video playlist of 5 short
videos explaining the tech [youtube](https://www.youtube.com/playlist?list=PL6CJYn40gN6h3usMQY3BSZJs08isz3jqa)

To develop on the app, especially if you will work on the QML, we suggest the
following workflow:

```
  mkdir build
  cd build
  cmake -Dlocal_qml=ON -DCMAKE_INSTALL_PREFIX=`pwd` ..
  make install
```

The executables will be in `pay/build/bin/` and by adding the `local_qml`
cmake option the build will bake in the path to your QML files. On
your local harddrive. This allows you to change the QML files and simply
restart the app without recompile.

For development you can run either **pay** or **pay_mobile**, depending on which
front-end you are working on.

To develop on the app we suggest starting one of those app with these
options:  
`bin/pay --offline --testnet4`


# Links

* Learn more about QML? Here is a great place to start; https://www.kdab.com/top-100-qml-resources-kdab/

* Upstream: https://codeberg.org/Flowee/pay
* Website: https://flowee.org
* Twitter: https://twitter.com/floweethehub
* Telegram: https://t.me/Flowee_org
