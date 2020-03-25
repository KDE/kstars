# A Desktop Planetarium for KDE

KStars is free, open source, cross-platform Astronomy Software.

It provides an accurate graphical simulation of the night sky, from any location on Earth, at any date and time. The display includes up to 100 million stars, 13,000 deep-sky objects,all 8 planets, the Sun and Moon, and thousands of comets, asteroids, supernovae, and satellites. 

For students and teachers, it supports adjustable simulation speeds in order to view phenomena that happen over long timescales, the KStars Astrocalculator to predict conjunctions, and many common astronomical calculations. For the amateur astronomer, it provides an observation planner, a sky calendar tool, and an FOV editor to calculate field of view of equipment and display them. Find out interesting objects in the "What's up Tonight" tool, plot altitude vs. time graphs for any object, print high-quality sky charts, and gain access to lots of information and resources to help you explore the universe!

Included with KStars is Ekos astrophotography suite, a complete astrophotography solution that can control all INDI devices including numerous telescopes, CCDs, DSLRs, focusers, filters, and a lot more. Ekos supports highly accurate tracking using online and offline astrometry solver, autofocus and autoguiding capabilities, and capture of single or multiple images using the powerful built in sequence manager.

## Copyright

Copyright (c) 2001 - 2020 by The KStars Team:

KStars is Free Software, released under the GNU Public License. See COPYING for GPL license information.

## Downloads

KStars is available for Windows, MacOS, and Linux. You can download the latest version from [KStars official website](https://edu.kde.org/kstars).

On Linux, it is available for most Linux distributions.

Latest stable version is v3.4.1

## Important URLs and files.

* The [KStars homepage](https://edu.kde.org/kstars)
* KDE [KStars Git Repository](git://anongit.kde.org/kstars).You can obtain the latest version via git from:
* KStars [Web Chat](https://webchat.kde.org/#/room/#kstars:kde.org)

## KStars documentation

The KStars handbook can be found in your $(KDEDIR)/share/doc/HTML/<lang>/kstars/
directory.  You can also easily access it from the Help menu, or by pressing
the [F1] key, or by visiting https://docs.kde.org/?application=kstars

In addition, there are the following README files:

README:             This file; general information
README.planetmath:  Explanation of algorithms used to compute planet positions
README.customize:   Advanced customization options
README.images:      Copyright information for images used in KStars.
README.i18n:        Instructions for translators

## Development

Important Note: **Do not submit Pull Requests to Github**. KStars repository on Github is just a mirror. All PRs will be closed automatically. To send patches to KStars, please use KDE's [Phabricator](https://community.kde.org/Infrastructure/Phabricator) system.

### Integrated Development Environment IDE

If you plan to develop KStars, it is highly recommended to utilize an IDE. You can use any IDE of your choice, but QtCreator(https://www.qt.io/product) or KDevelop(https://www.kdevelop.org) are recommended as they are more suited for Qt/KDE development.

To open KStars in QtCreator, select the CMakeLists.txt file in the KStars source folder and then configure the build location and type.

### Building


#### Prerequisite Packages
To build and develop KStars, several packages may be required from your distribution. Here's a list.

# Required dependencies
* GNU Make, GCC -- Essential tools for building
* cmake -- buildsystem used by KDE
* Qt Library > 5.9.0
* Several KDE Frameworks: KConfig, KDocTools, KGuiAddons, KWidgetsAddons, KNewStuff, KI18n, KInit, KIO, KXmlGui, KPlotting, KIconThemes
* eigen -- linear algebra library
* zlib -- compression library

# Optional dependencies
* libcfitsio -- FITS library
* libindi -- Instrument Neutral Distributed Interface, for controlling equipment.
* xplanet
* astrometry.net
* libraw
* wcslib
* libgsl
* qtkeychain

#### Installing Prerequisites

Debian/Ubuntu
```
sudo apt-get -y install build-essential cmake git libeigen3-dev libcfitsio-dev zlib1g-dev libindi-dev extra-cmake-modules libkf5plotting-dev libqt5svg5-dev libkf5xmlgui-dev kio-dev kinit-dev libkf5newstuff-dev kdoctools-dev libkf5notifications-dev qtdeclarative5-dev libkf5crash-dev gettext libnova-dev libgsl-dev libraw-dev libkf5notifyconfig-dev wcslib-dev libqt5websockets5-dev xplanet xplanet-images qt5keychain-dev libsecret-1-dev breeze-icon-theme
```

Fedora
```
yum install cfitsio-devel eigen3-devel cmake extra-cmake-modules.noarch kf5-kconfig-devel kf5-kdbusaddons-devel kf5-kdoctools-devel kf5-kguiaddons-devel kf5-ki18n-devel kf5-kiconthemes-devel kf5-kinit-devel kf5-kio-devel kf5-kjobwidgets-devel kf5-knewstuff-devel kf5-kplotting-devel kf5-ktexteditor-devel kf5-kwidgetsaddons-devel kf5-kwindowsystem-devel kf5-kxmlgui-devel libindi-devel libindi-static qt5-qtdeclarative-devel qt5-qtmultimedia-devel qt5-qtsvg-devel wcslib-devel xplanet zlib-devel
```

#### Compiling

Open a console and run in the following commands:
```
mkdir -p ~/Projects/build/kstars
git clone git://anongit.kde.org/kstars.git
cd build/kstars
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug ~/Projects/kstars
make -j8
sudo make install
```

To run KStars, simply type **kstars** in the terminal.

### Code Style

KStars uses [Artistic Style](http://astyle.sourceforge.net) to format all the C++ source files. Please make sure to apply the following astyle rules to any code that is submitted to INDI. On Linux, you can create ***~/.astylerc*** file containing the following rules:
```
--style=allman
--align-reference=name
--indent-switches
--indent-modifiers
--indent-classes
--pad-oper
--indent-col1-comments
--lineend=linux
--max-code-length=124
```
Some IDEs (e.g. QtCreator) support automatic formatting for the code everytime you save the file to disk.

## Credits
### The KStars Team

Main contributors:
   Jason Harris <kstars@30doradus.org>
   Jasem Mutlaq <mutlaqja@ikarustech.com>
   Akarsh Simha <akarsh.simha@kdemail.net>
   James Bowlin <bowlin@mindspring.com>
   Heiko Evermann <heiko@evermann.de>
   Thomas Kabelmann <tk78@gmx.de>
   Pablo de Vicente <pvicentea@wanadoo.es>
   Mark Hollomon <mhh@mindspring.com>
   Carsten Niehaus <cniehaus@kde.org>
   Médéric Boquien <mboquien@free.fr>
   Alexey Khudyakov <alexey.skladnoy@gmail.com>
   Jérôme Sonrier <jsid@emor3j.fr.eu.org>
   Harry de Valence <hdevalence@gmail.com>
   Victor Carbune <victor.carbune@kdemail.net>
   Rafał Kułaga <rl.kulaga@gmail.com>
   Samikshan Bairagya <samikshan@gmail.com>
   Rishab Arora <ra.rishab@gmail.com>
   Robert Lancaster <rlancaste@gmail.com>

### Data Sources:

 Most of the catalog data came from the Astronomical Data Center, run by
 NASA.  The website is:
 http://adc.gsfc.nasa.gov/

 NGC/IC data is compiled by Christian Dersch from OpenNGC database.
 https://github.com/mattiaverga/OpenNGC
 Check LICENSE_OpenNGC for license details (CC-BY-SA-4.0)

 Supernovae data is from the Open Supernova Catalog project at https://sne.space
 Please refer to the published paper here: http://adsabs.harvard.edu/abs/2016arXiv160501054G

 KStars links to the excellent image collections and HTML pages put together
 by the Students for the Exploration and Development of Space, at:
 http://www.seds.org

 KStars links to the online Digitized Sky Survey images, which you can
 query at:
 http://archive.stsci.edu/cgi-bin/dss_form

 KStars links to images from the HST Heritage project, and from HST
 press releases:
 http://heritage.stsci.edu
 http://oposite.stsci.edu/pubinfo/pr.html

 KStars links to images from the Advanced Observer Program at
 Kitt Peak National Observatory.  If you are interested in astrophotography,
 you might consider checking out their program:
 http://www.noao.edu/outreach/aop/

 Credits for each image used in the program are listed in README.images


# Acknowledgements

 KStars is a labor of love.  It started as a personal hobby of mine, but
 very soon after I first posted the code on Sourceforge, it started to
 attract other developers.  I am just completely impressed and gratified
 by my co-developers.  I couldn't ask for a more talented, friendly crew.
 It goes without saying that KStars would be nowhere near what it is today
 without their efforts.  Together, we've made something we can all be
 proud of.

 We used (primarily) two books as a guide in writing the algorithms used
 in KStars:
 + "Practical Astronomy With Your Calculator" by Peter Duffett-Smith
 + "Astronomical Algorithms" by Jean Meeus

 Thanks to the developers of Qt and KDE whose peerless API made KStars
 possible.  Thanks also to the tireless efforts of the KDE translation
 teams, who bring KStars to a global audience.

 Thanks to everyone at the KDevelop message boards and on irc.kde.org,
 for answering my frequent questions.

 Thanks also to the many users who have submitted bug reports or other
 feedback.


You're still reading this? :)
Well, that's about it.  I hope you enjoy KStars!

Jason Harris
kstars@30doradus.org

KStars Development Mailing list
kstars-devel@kde.org

Send us ideas and feedback!
