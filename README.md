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

Latest stable version is v3.4.2

## Important URLs and files.

* The [KStars homepage](https://edu.kde.org/kstars)
* KStars [Git Repository](https://invent.kde.org/education/kstars)
* KStars [Web Chat](https://webchat.kde.org/#/room/#kstars:kde.org)
* Forum [where KStars is often discussed](https://indilib.org/forum.html)

## KStars documentation

The KStars handbook can be found in your $(KDEDIR)/share/doc/HTML/<lang>/kstars/
directory.  You can also easily access it from the Help menu, or by pressing
the [F1] key, or by visiting https://docs.kde.org/?application=kstars
Unfortunately, it's a bit out-of-date. We welcome volunteers to help
update it.

In addition, there are the following README files:

README:             This file; general information
README.planetmath:  Explanation of algorithms used to compute planet positions
README.customize:   Advanced customization options
README.images:      Copyright information for images used in KStars.
README.i18n:        Instructions for translators

## Development

Code can be cloned, viewed and merge requests can be made via the [KStars repository](https://invent.kde.org/education/kstars). If you are new to remote git repositories, please see the Git Tips section below.
Note: Previously KStars used Phabricator for its merge requests. That system is no longer in use.

### Integrated Development Environment IDE

If you plan to develop KStars, it is highly recommended to utilize an IDE. You can use any IDE of your choice, but QtCreator(https://www.qt.io/product) or KDevelop(https://www.kdevelop.org) are recommended as they are more suited for Qt/KDE development.

To open KStars in QtCreator, select the CMakeLists.txt file in the KStars source folder and then configure the build location and type.

### Building

1. Prerequisite Packages

To build and develop KStars, several packages may be required from your distribution. Here's a list.

* Required dependencies
    * GNU Make, GCC -- Essential tools for building
    * cmake -- buildsystem used by KDE
    * Qt Library > 5.9.0
    * Several KDE Frameworks: KConfig, KDocTools, KGuiAddons, KWidgetsAddons, KNewStuff, KI18n, KInit, KIO, KXmlGui, KPlotting, KIconThemes
    * eigen -- linear algebra library
    * zlib -- compression library
    * StellarSolver -- see [https://github.com/rlancaste/stellarsolver](https://github.com/rlancaste/stellarsolver)

* Optional dependencies
    * libcfitsio -- FITS library
    * libindi -- Instrument Neutral Distributed Interface, for controlling equipment.
    * xplanet
    * astrometry.net
    * libraw
    * wcslib
    * libgsl
    * qtkeychain


2. Installing Prerequisites

Debian/Ubuntu

The apt-add-respository command is needed for the apt-get's libstellarsolver-dev. Alternatively, you can skip the apt-add-repository, remove the libstellarsolver-dev from the apt-get, and build & install stellarsolver from https://github.com/rlancaste/stellarsolver.
```
sudo apt-add-repository ppa:mutlaqja/ppa
sudo apt-get -y install build-essential cmake git libstellarsolver-dev libeigen3-dev libcfitsio-dev zlib1g-dev libindi-dev extra-cmake-modules libkf5plotting-dev libqt5svg5-dev libkf5xmlgui-dev libkf5kio-dev kinit-dev libkf5newstuff-dev kdoctools-dev libkf5notifications-dev qtdeclarative5-dev libkf5crash-dev gettext libnova-dev libgsl-dev libraw-dev libkf5notifyconfig-dev wcslib-dev libqt5websockets5-dev xplanet xplanet-images qt5keychain-dev libsecret-1-dev breeze-icon-theme
```

Fedora
```
yum install cfitsio-devel eigen3-devel stellarsolver-devel cmake extra-cmake-modules.noarch kf5-kconfig-devel kf5-kdbusaddons-devel kf5-kdoctools-devel kf5-kguiaddons-devel kf5-ki18n-devel kf5-kiconthemes-devel kf5-kinit-devel kf5-kio-devel kf5-kjobwidgets-devel kf5-knewstuff-devel kf5-kplotting-devel kf5-ktexteditor-devel kf5-kwidgetsaddons-devel kf5-kwindowsystem-devel kf5-kxmlgui-devel libindi-devel libindi-static qt5-qtdeclarative-devel qt5-qtmultimedia-devel qt5-qtsvg-devel wcslib-devel xplanet zlib-devel
```

3. Compiling

Open a console and run in the following commands:
```
mkdir -p ~/Projects
git clone https://invent.kde.org/education/kstars.git
mkdir -p kstars-build
cd kstars-build
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=RelWithDebInfo ../kstars
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
Some IDEs (e.g. QtCreator) support automatic formatting for the code every time you save the file to disk.

### Making Updates to the Handbook

On linux run the following to install the necessary programs:

```
sudo apt-get install docbook docbook-utils
```

The source for the handbook is in kstars/doc.
You can edit those files, include them in commits and MRs like you would c++ files (see below).
You could figure out the markup by example, or learn from [online doc for docbook](https://opensource.com/article/17/9/docbook).
In general, it is best to first copy the entire kstars/doc directory to a temporary directory, and edit and generate the handbook there,
because if you ran meinproc in the main source directory, you would generate many .html files there,
and you don't want to commit the generated files to your git repository.

```
cp -pr kstars/doc ~/DOCBOOK
cd ~/DOCBOOK
meinproc5 index.docbook
```

The above should generate html files. Then, in a browser, you can simply open DOCBOOK/index.html and navigate your way to the part you want, e.g. just type something similar to this in the url bar of chrome: file:///home/YOUR_USER_NAME/DOCBOOK/doc/tool-ekos.html
Make changes to some of the .docbook files in ~/DOCBOOK/*.docbook. Regenerate the html files, and view your changes in the browser, as before. Iterate.

To check syntax, you might want to run:

```
checkXML5 index.docbook
```

Once you're happy, copy your modified files back to kstars/doc, and treat the edited/new files as usual with git,
including your modified files in a new commit and eventually a new merge request.

### Merge Request Descriptions

See the section below, Git Tips, on technical specifics of how to generate a Merge Request.
In the process of making the request, you will need to describe the request.
Please use a format similar to [this one](https://invent.kde.org/education/kstars/-/merge_requests/33)
which has sections for a summary of what was done, what was modified in each file, other relevant notes, and how to test your changes.

### Git Tips

You must be familiar with git to make changes to KStars, and this is not the place for such a tutorial. There
are many excellent resources for that on the web. The paragraph below, though, will give an overview of one way
to make a Merge Request, given you already have sufficient git experience to clone KStars, make a local branch,
modify the code as you like, commit your changes to your local branch, and test your code thoroughly.

Here's one good resource for a [fork-branch-git-workflow to make KStars changes](https://blog.scottlowe.org/2015/01/27/using-fork-branch-git-workflow). The steps below are inspired by that page.

**One-time KStars git environment setup.**

* [Make your KDE identity](https://community.kde.org/Infrastructure/Get_a_Developer_Account)
* **Login.** Go to the [KStars gitlab page](https://invent.kde.org/education/kstars) and login in the upper right corner.
* **Fork the project.** Then, still on the KStars gitlab page, Click FORK in the upper right hand corner, to create your own fork of the project.
* **Copy your URL.** Note the url of your fork. It should be https://invent.kde.org/YOUR_KDE_NAME/kstars
* **Clone KStars.** Back on your computer run these commands
    * mkdir -p ~/Projects
    * cd ~/Projects
    * git clone https://invent.kde.org/YOUR_KDE_NAME/kstars
    * cd kstars
* **Add your upstream.** Add the KStars main repo to your forked repo.
    * git remote add upstream https://invent.kde.org/education/kstars

You are set up now.

**Steps used for each change.** After the one-time setup (above), the steps below could be used for each new feature submission. In summary, you will make a feature branch in your local repository, make your desired changes there and test, push them to your fork, create a request to merge your fork with the main KStars repo, wait for feedback, and possibly iterate on your changes hoping for approval from an authority.

* **Create your feature branch.**
    * git checkout -b YOUR_BRANCH_NAME
* **Make your changes**
* **Commit your changes**
    * git commit -a
* **Push changes to your forked repo.**
    * git push origin YOUR_BRANCH_NAME
* **Create a Merge Request**
    * Use your browser to visit your forked repo at  https://invent.kde.org/YOUR_KDE_NAME/kstars
    * You should see an option to create a Merge Request for YOUR_BRANCH_NAME. Fill in the details (see the above section).
    * You should be able to see a new URL dedicated to that Merge Request.
* **Make Some Changes.** You may get requests to modify some of your code.
    * If so, you simply go back to your local branch, make and test your changes.
    * Commit your changes as above, inside your branch, with: git commit -a
    * Push your branch's changes to your forked repo as above with: git push origin YOUR_BRANCH_NAME
    * Your changes should automatically be added to your Merge Request. Check the Merge Request's page to be sure.
    * You may need to rebase your code--see below for details.

**Rebasing your changes.** Others may be making changes to KStars at the same time you are working on your feature.
Rebasing is updating your version of KStars and your particular changes to make it as if you changed the latest KStars version,
e.g. reflect changes to the codebase made after you cloned or updated your own KStars copy. This is a significant topic
you can Google, but the following instructions work most of the time.

Note that this is done before you create your merge request, when you are the only one seeing your code changes. 
Once you have started your merge request, your code is "public" and instead of rebasing, you should follow the merge procedure below.

```
cd ~/Projects/kstars
git checkout master
git pull upstream master  # Get the master from the main KStars repo onto your local clone
git push origin master    # Then push your updated local clone into your forked repo
git checkout YOUR_BRANCH_NAME
git rebase master
git push origin YOUR_BRANCH_NAME -f
```

If there are complications with the rebase, git will make suggestions on how to correct the issues.

**Merging others' changes.** Once you submit a merge request, your code can be seen (and edited) by
others. At this point, though you still may need to update to the latest KStars version, rebasing destroys
change information and can overwrite what others are doing. Instead it is best to 'merge' in the current 
version of KStars into your code.

```
cd ~/Projects/kstars
git checkout master
git pull upstream master  # Get the master from the main KStars repo onto your local clone
git push origin master    # Then push your updated local clone into your forked repo
git checkout YOUR_BRANCH_NAME
git merge master
git push origin YOUR_BRANCH_NAME 
```

The differences from the rebase section are the last 2 commands: 'git merge master' is used instead of 'git rebase master'.
Also the 'git push' doesn't use the -f option. The first time you run the 'git push',
you may be asked by git to add 'set-upstream origin' to the command. In that case, follow those instructions.

If you follow this procedure, you will find a new 'merge commit' added to your branch's git log.


**Your next change**. Once your Merge Request is complete (and possibly integrated into KStars), you may wish to move on and develop again.
The next change will use another (new) feature branch, and the first feature branch could be deleted.
You may want to run the following regularly to keep your master branch up-to-date with KStars.
```
cd ~/Projects/kstars
git checkout master
git pull upstream master  # Get the master from the main KStars repo onto your local clone
git push origin master    # Then push your updated local clone into your forked repo
```

## Writing Tests
Tests are stored in the `Tests` folder and use QTest as support framework:
* Unitary tests can be found in `auxiliary`, `capture`, `fitsviewer`, etc. They try to verify the behavior
  of a minimal set of classes, and are support for feature development.
* UI tests can be found in `kstars_lite_ui` and `kstars_ui`. They execute use cases as the end-user would do from the user
  interface, and focus on availability of visual feedback and stability of procedures.

### Writing Unitary Tests
1. Decide where your new unitary test will reside in `Tests`.
KStars classes should live in a folder matching their origin: for instance, auxiliary class tests live in `auxiliary`.
Find a suitable place for your test, based on the part of the system that is being tested.
As an example, a folder named `thatkstarscategory`.

2. Create a new unitary test class, or copy-paste an existing unitary test to a new one.
Check `Tests/kstars_ui_tests/kstars_ui_tests.h` as an example.
Name the `.h` and `.cpp` files as "test[lowercase kstars class]" (for instance "testthatkstarsclass"), and update them to match the following:
```
/* [Author+Licence header] */
#ifndef TESTTHATKSTARSCLASS_H
#define TESTTHATKSTARSCLASS_H

#include <QtTest>
#include <QObject>

class TestThatKStarsClass: public QObject
{
    Q_OBJECT
public:
    explicit TestThatKStarsClass(QObject *parent = null);

private slots:
    void initTestCase();                    // Will trigger once at beginning
    void cleanupTestCase();                 // Will trigger once at end

    void init();                            // Will trigger before each test
    void cleanup();                         // Will trigger after each test

    void testThisParticularFunction_data(); // Data fixtures for the test function (Qt 5.9+)
    void testThisParticularFunction();      // Test function
}
#endif // TESTTHATKSTARSCLASS_H
```
```
/* [Author+Licence header] */
#include "testthatkstarsclass.h"
TestThatKStarsClass::TestThatKStarsClass(QObject* parent): QObject(parent) {}
TestThatKStarsClass::initTestCase() {}
TestThatKStarsClass::cleanupTestCase() {}
TestThatKStarsClass::init() {}
TestThatKStarsClass::cleanup() {}

TestThatKStarsClass::testThisParticularFunction_data()
{
    // If needed, add data fixtures with QTest::AddColumn/QTest::AddRow, each will trigger testThisParticularFunction
}

TestThatKStarsClass::testThisParticularFunction()
{
    // Write your tests here, eventually using QFETCH to retrieve the current data fixture
}

QTEST_GUILESS_MAIN(TestThatKStarsClass);
```
You can use a single file to hold both declaration and definition, but you will need to `#include "testthatkstarsclass.moc"` between the declaration and the definition.

3. Update the CMake configuration to add your test.
If you created a new folder, create a new `CMakeLists.txt` to add your test:
```
ADD_EXECUTABLE( testthatkstarsclass testthatkstarsclass.cpp )
TARGET_LINK_LIBRARIES( testthatkstarsclass ${TEST_LIBRARIES})
ADD_TEST( NAME ThatKStarsClassTest COMMAND testthatkstarsclass )
```
Have the `CMakeLists.txt` residing one folder higher in the filesystem include that `CMakeLists.txt` by adding:
```
include_directories(
    ...
    ${kstars_SOURCE_DIR}/kstars/path/to/the/folder/of/the/kstars/class/you/are/testing
)
...
add_subdirectory(thatkstarscategory)
```
Make sure you add your `add_subdirectory` in the right dependency group. Ekos tests require `INDI_FOUND` for instance.

4. Write your tests
Make sure you document behavior with your tests. If you happen to find a bug, don't fix it, mark it with an `QEXPECT_FAIL` macro.
The test will document the incorrect behavior while the bug is alive, and will fail when the bug is fixed. Then only after that the test may be updated.
Also pay attention to Qt library version support. For instance, data fixtures require Qt 5.9+.

### Writing User Interface Tests

Follow the same steps as for unitary tests, but locate your test classes in `kstars_ui_tests`.

One important thing about UI tests is that they must all use `QStandardPaths::setTestModeEnabled(true)`, so that they execute with a separate user
configuration that is initially blank. User interface tests thus require a preliminary setup to function properly, such as using the new configuration
wizard or setting the geographical location up. For this reason, you need to add the execution of your test in `Tests/kstars_ui_tests/kstars_ui_tests.cpp`,
in `main()`, **after** the execution of `TestKStarsStartup`.

A second important thing about QTest generally is that test functions have no return code. One therefore needs to write macros to factor duplicated code.
You will find many existing macros in the header files of `kstars_ui_tests` test classes, to retrieve gadgets, to click buttons or to fill `QComboBox` widgets...

A third important thing about the KStars interface is that it mixes KDE and Qt UI elements. Thus, sometimes tests require verification code to be moved
to a `QTimer::singleShot` call, and sometimes even clicking on a button has to be made asynchronous for the test to remain in control (modal dialogs).
Fortunately, these hacks do not alter the execution of the tested code.

When testing, you need to make sure you always use elements that the end-user is able to use. Of course, if a test requires a setup that is not actually part of the
interesting calls, you may hack a direct call. For instance, some Ekos tests requiring the Telescope Simulator to be pointing at a specific location use
`QVERIFY(Ekos::Manager::Instance()->mountModule()->sync(ra,dec))`. Remember that sometimes you need to leave time for asynchronous signals to be emitted and caught.

## Credits
### The KStars Team
#### Original Author
Jason Harris <kstars@30doradus.org>
#### Current Maintainer
Jasem Mutlaq <mutlaqja@ikarustech.com>
#### Contributors (Alphabetical)
* Akarsh Simha <akarsh.simha@kdemail.net>
* Alexey Khudyakov <alexey.skladnoy@gmail.com>
* Artem Fedoskin <afedoskin3@gmail.com>
* Carsten Niehaus <cniehaus@kde.org>
* Chris Rowland <chris.rowland@cherryfield.me.uk>
* Csaba Kertesz <csaba.kertesz@gmail.com>
* Eric Dejouhanet <eric.dejouhanet@gmail.com>
* Harry de Valence <hdevalence@gmail.com>
* Heiko Evermann <heiko@evermann.de>
* Hy Murveit <murveit@gmail.com>
* James Bowlin <bowlin@mindspring.com>
* Jérôme Sonrier <jsid@emor3j.fr.eu.org>
* Mark Hollomon <mhh@mindspring.com>
* Martin Piskernig <martin.piskernig@stuwo.at>
* Médéric Boquien <mboquien@free.fr>
* Pablo de Vicente <pvicentea@wanadoo.es>
* Prakash Mohan <prakash.mohan@kdemail.net>
* Rafał Kułaga <rl.kulaga@gmail.com>
* Rishab Arora <ra.rishab@gmail.com>
* Robert Lancaster <rlancaste@gmail.com>
* Samikshan Bairagya <samikshan@gmail.com>
* Thomas Kabelmann <tk78@gmx.de>
* Valentin Boettcher <hiro@protagon.space>
* Victor Cărbune <victor.carbune@kdemail.net>
* Vincent Jagot <vincent.jagot@free.fr>
* Wolfgang Reissenberger <sterne-jaeger@t-online.de>
* Yuri Chornoivan <yurchor@ukr.net>

### Data Sources:

 Most of the catalog data came from the Astronomical Data Center, run by
 NASA.  The website is:
 http://adc.gsfc.nasa.gov/

 NGC/IC data is compiled by Christian Dersch from OpenNGC database.
 https://github.com/mattiaverga/OpenNGC (CC-BY-SA-4.0 license)

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


# Original Acknowledgement from the Project Founder

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
