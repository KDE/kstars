/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef INDICOMMON_H
#define INDICOMMON_H

#include <QString>
#include <QMap>

/*!
\page INDI "INDI Overview"
\tableofcontents

\section Intro Introduction

INDI is a simple XML-like communications protocol described for interactive and automated remote control of diverse instrumentation. INDI is small, easy to parse, and stateless.
The main key concept in INDI is that devices have the ability to describe themselves. This is accomplished by using XML to describe a generic hierarchy that can represent both canonical and
 non-canonical devices. All devices may contain one or more properties. A property in the INDI paradigm describes a specific function of the driver.
Any property may contain one or more elements. There are four types of INDI properties:

<ul>
<li><b>Text property</b>: Property to transfer simple text information in ISO-8859-1. The text is not encoded or encrypted on transfer. If the text includes elements that can break XML syntax, a BLOB property should be used to make the transfer.</li>
<li><b>Number property</b>: Property to transfer numeric information with configurable minimum, maximum, and step values. The supported number formats are decimal and sexigesmal. The property includes a GUI hint element in printf style format to enable clients to properly display numeric properties.</li>
<li><b>Switch property</b>: Property to hold a group of options or selections (Represented in GUI by buttons and check boxes).</li>
<li><b>Light property</b>: Property to hold a group of status indicators (Represented in GUI by colored LEDs).</li>
<li><b>BLOB property</b>: BLOB is a Binary Large OBject used to transfer binary data to and from drivers.</li>
</ul>

For example, all INDI devices share the CONNECTION standard switch property. The CONNECTION property has two elements: CONNECT and DISCONNECT switches.
GUI clients parse the generic XML description of properties and builds a GUI representation suitable for direct human interaction.

KStars is a fully featured INDI client. INDI Data (i.e. devices and properties) and GUI (how they appear on the screen) are separated. INDI adopts is a server/client architecture where one or more servers
communicate with one or more drivers, each driver can communicate with one or more actual hardware devices. Clients connect to the server, obtain a list of devices, and generate GUI to represent the
devices for user interaction and control. The creation of GUI happens in run time via introspection.

\section Structure Hierarchy

The following is a description of some of the various classes that support INDI:

<ul>
<li>DriverManager: Manages local drivers as installed by INDI library. Enables users to start/stop local INDI servers (ServerManager) running one or more drivers (DriverInfo). Also enables connecting to local or remote
INDI servers. For both local and remote connection, it creates a ClientManager to handle incoming data from each INDI server started, and creates a GUIManager to render the devices in INDI Control Panel.
The user may also choose to only start an INDI server without a client manager and GUI.</li>
<li>ClientManager: Manages sending and receiving data from and to an INDI server. The ClientManager sends notifications (signals) when a new device or property is defined, updated, or deleted.</li>
<li>GUIManager: Handles creation of GUI interface for devices (INDI_D) and their properties and updates the interface in accord with any data emitted by the associated ClientManager. The GUI manager supports
multiple ClientManagers and consolidate all devices from all the ClientManagers into a single INDI Control Panel where each device is created as a tab.</li>
<li>INDIListener: Once a ClientManager is created in DriverManager after successfully connecting to an INDI server, it is added to INDIListener where it monitors any new devices and if a new
device is detected, it creates an ISD::GenericDevice to hold the data of the device. It also monitors new properties and registers them. If it detects an INDI standard property associated with a particular device family
(e.g. Property EQUATORIAL_EOD_COORD is a standard property of a Telescope device), then it extends the ISD::GenericDevice to the particular specialized device type using the <a href="http://en.wikipedia.org/wiki/Decorator_pattern">
Decorator Pattern</a>. All updated properties from INDI server are delegated to their respective devices.</li>
<li>ServerManager</li> Manages establishment and shutdown of local INDI servers.</li>
<li>DriverInfo</li>: Simple class that holds information on INDI drivers such as name, version, device type..etc.</li>
<li>ISD::GDInterface: Abstract class where the ISD::DeviceDecorator and ISD::GenericDevice are derived.</li>
<li>ISD::DeviceDecorator: Base class of all specialized decorators such as ISD::Telescope, ISD::Filter, and ISD::CCD devices.</li>
<li>ISD::GenericDevice: Base class for all INDI devices. It implements processes shared across all INDI devices such as handling connection/disconnection, setting of configuration..etc.. When a
specialized decorator is created (e.g. ISD::Telescope), the decorator is passed an ISD::GenericDevice pointer. Since ISD::Telescope is a child of ISD::DeviceDecorator, it can override some or all
 the device specific functions as defined in ISD::GDInterface (e.g. ProcessNumber(INumberVectorProperty *nvp)). For any function that is not overridden, the parent ISD::DeviceDecorator will invoke
 the function in ISD::GenericDevice instead. Moreover, The specialized decorator device can explicitly call DeviceDecorator::ProcessNumber(INumberVectorProperty *nvp) for example to cause the ISD::DeviceDecorator to invoke
 the same function but as defined in ISD::GenericDevice.</li>
 </ul>

\section Example Example

Suppose the user wishes to control an EQMod mount with \e indi_eqmod_telescope driver. From the DriverManager GUI, the user selects the driver and \e starts INDI services. The DriverManager
will create a ServerManager instance to run an INDI server with the EQMod driver executable. After establishing the server, the DriverManager will create an instance of ClientManager and set
its INDI server address as the host and port of the server created in ServerManager. For local servers, the host name is \e localhost and the default INDI port is 7624. If connection to the INDI server
is successful, DriverManager then adds the ClientManager to both INDIListener (to handle data), and GUIManager (to handle GUI). Since the ClientManager emits signals whenever a new, updated, or deleted
device/property is received from INDI server, it affects both the data handling part as well as GUI rendering.

INDIListener holds a list of all INDI devices in KStars regardless of their origin. Once INDIListener receives a new device from the ClientManager, it creates a new ISD::GenericDevice. At the GUIManager side, it creates INDI Control Panel and adds a new tab with the device name.
It also creates a separate tab for each property group received. The user is presented with a basic GUI to set the connection port of EQMod and to connect/disconnect to/from the telescope. If the
user clicks connect, the status of the connection property is updated, and INDI_P sends the new switch status (CONNECT=ON) to INDI server via the ClientManager. If the connection is successful
at the driver side, it will start defining new properties to cover the complete functionality of the EQMod driver, one of the standard properties is EQUATORIAL_EOD_COORD which will be detected
in INDIListener. Upon detection of this key signature property, INDIListener creates a new ISD::Telescope device while passing to it the ISD::GenericDevice instance created earlier.

Now suppose an updated Number property arrives from INDI server, the ClientManager emits a signal indicating a number property has a new updated value and INDIListener delegates the INDI Number
property to the device, which is now of type ISD::Telescope. The ISD::Telescope overridden the processNumber(INumberVectorProperty *nvp) function in ISD::DeviceDecorator because it wants to handle some telescope
specific numbers such as EQUATORIAL_EOD_COORD in order to move the telescope marker on the sky map as it changes. If the received property was indeed EQUATORIAL_EOD_COORD or any property handled
by the ISD::Telescope ProcessNumber() function, then there is no further action needed. But what if the property is not processed in ISD::Telescope ProcessNumber() function? In this case, the
ProcessNumber() function simply calls DeviceDecorator::ProcessNumber() and it will delgate the call to ISD::GenericDevice ProcessNumber() to process. This is how the Decorator pattern work,
the decorator classes implements extended functionality, but the basic class is still responsible for handling all of the basic functions.

*/

#define INDIVERSION 1.7 /* we support this or less */

typedef enum { PRIMARY_XML, THIRD_PARTY_XML, EM_XML, HOST_SOURCE, CUSTOM_SOURCE, GENERATED_SOURCE } DriverSource;

typedef enum { SERVER_CLIENT, SERVER_ONLY } ServerMode;

typedef enum { DATA_FITS, DATA_VIDEO, DATA_CCDPREVIEW, DATA_ASCII, DATA_OTHER } INDIDataTypes;

typedef enum { LOAD_LAST_CONFIG, SAVE_CONFIG, LOAD_DEFAULT_CONFIG, PURGE_CONFIG } INDIConfig;

typedef enum { NO_DIR = 0, RA_INC_DIR, RA_DEC_DIR, DEC_INC_DIR, DEC_DEC_DIR } GuideDirection;

/* GUI layout */
#define PROPERTY_LABEL_WIDTH 100
#define PROPERTY_LABEL_HEIGHT 30
#define ELEMENT_LABEL_WIDTH  175
#define ELEMENT_LABEL_HEIGHT 30
#define ELEMENT_READ_WIDTH   175
#define ELEMENT_WRITE_WIDTH  175
#define ELEMENT_FULL_WIDTH   340
#define MIN_SET_WIDTH        50
#define MAX_SET_WIDTH        110
#define MED_INDI_FONT        2
#define MAX_LABEL_LENGTH     20

typedef enum { PG_NONE = 0, PG_TEXT, PG_NUMERIC, PG_BUTTONS, PG_RADIO, PG_MENU, PG_LIGHTS, PG_BLOB } PGui;

/* new versions of glibc define TIME_UTC as a macro */
#undef TIME_UTC

/* INDI std properties */
enum stdProperties
{
    CONNECTION,
    DEVICE_PORT,
    TIME_UTC,
    TIME_LST,
    TIME_UTC_OFFSET,
    GEOGRAPHIC_COORD, /* General */
    EQUATORIAL_COORD,
    EQUATORIAL_EOD_COORD,
    EQUATORIAL_EOD_COORD_REQUEST,
    HORIZONTAL_COORD, /* Telescope */
    TELESCOPE_ABORT_MOTION,
    ON_COORD_SET,
    SOLAR_SYSTEM,
    TELESCOPE_MOTION_NS, /* Telescope */
    TELESCOPE_MOTION_WE,
    TELESCOPE_PARK, /* Telescope */
    CCD_EXPOSURE,
    CCD_TEMPERATURE_REQUEST,
    CCD_FRAME, /* CCD */
    CCD_FRAME_TYPE,
    CCD_BINNING,
    CCD_INFO,
    CCD_VIDEO_STREAM, /* Video */
    FOCUS_SPEED,
    FOCUS_MOTION,
    FOCUS_TIMER, /* Focuser */
    FILTER_SLOT, /* Filter */
    WATCHDOG_HEARTBEAT
}; /* Watchdog */

/* Devices families that we explicitly support (i.e. with std properties) */
typedef enum
{
    KSTARS_ADAPTIVE_OPTICS,
    KSTARS_AGENT,
    KSTARS_AUXILIARY,
    KSTARS_CCD,
    KSTARS_DETECTORS,
    KSTARS_DOME,
    KSTARS_FILTER,
    KSTARS_FOCUSER,
    KSTARS_ROTATOR,
    KSTARS_SPECTROGRAPHS,
    KSTARS_TELESCOPE,
    KSTARS_WEATHER,
    KSTARS_UNKNOWN
} DeviceFamily;

const QMap<DeviceFamily, QString> DeviceFamilyLabels =
{
    {KSTARS_ADAPTIVE_OPTICS, "Adaptive Optics"},
    {KSTARS_AGENT, "Agent"},
    {KSTARS_AUXILIARY, "Auxiliary"},
    {KSTARS_CCD, "CCDs"},
    {KSTARS_DETECTORS, "Detectors"},
    {KSTARS_DOME, "Domes"},
    {KSTARS_FILTER, "Filter Wheels"},
    {KSTARS_FOCUSER, "Focusers"},
    {KSTARS_ROTATOR, "Rotators"},
    {KSTARS_SPECTROGRAPHS, "Spectrographs"},
    {KSTARS_TELESCOPE, "Telescopes"},
    {KSTARS_WEATHER, "Weather"},
    {KSTARS_UNKNOWN, "Unknown"},
};

typedef enum { FRAME_LIGHT, FRAME_BIAS, FRAME_DARK, FRAME_FLAT, FRAME_NONE } CCDFrameType;

const QMap<CCDFrameType, QString> CCDFrameTypeNames =
{
    {FRAME_LIGHT, "Light"},
    {FRAME_DARK, "Dark"},
    {FRAME_BIAS, "Bias"},
    {FRAME_FLAT, "Flat"},
    {FRAME_NONE, ""},
};


typedef enum { SINGLE_BIN, DOUBLE_BIN, TRIPLE_BIN, QUADRAPLE_BIN } CCDBinType;

typedef enum
{
    INDI_SEND_COORDS,
    INDI_FIND_TELESCOPE,
    INDI_CENTER_LOCK,
    INDI_CENTER_UNLOCK,
    INDI_CUSTOM_PARKING,
    INDI_SET_PORT,
    INDI_CONNECT,
    INDI_DISCONNECT,
    INDI_SET_FILTER,
    INDI_SET_FILTER_NAMES,
    INDI_CONFIRM_FILTER,
    INDI_SET_ROTATOR_TICKS,
    INDI_SET_ROTATOR_ANGLE
} DeviceCommand;

typedef enum { SOURCE_MANUAL, SOURCE_FLATCAP, SOURCE_WALL, SOURCE_DAWN_DUSK, SOURCE_DARKCAP } FlatFieldSource;

typedef enum { DURATION_MANUAL, DURATION_ADU } FlatFieldDuration;

#endif // INDICOMMON_H
