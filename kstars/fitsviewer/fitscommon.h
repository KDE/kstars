/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include "dms.h"

#include <ki18n_version.h>
#if KI18N_VERSION >= QT_VERSION_CHECK(5, 89, 0)
#include <KLazyLocalizedString>
#define kde_translate kli18n
#else
#include <KLocalizedString>
#define kde_translate ki18n
#endif

typedef enum { FITS_NORMAL, FITS_FOCUS, FITS_GUIDE, FITS_CALIBRATE, FITS_ALIGN, FITS_UNKNOWN } FITSMode;

// Focus States
#if KI18N_VERSION >= QT_VERSION_CHECK(5, 89, 0)
static const QList<KLazyLocalizedString> FITSModes =
#else
static const QList<KLocalizedString> FITSModes =
#endif
{
    kde_translate("Normal"),
    kde_translate("Focus"),
    kde_translate("Guide"),
    kde_translate("Calibrate"),
    kde_translate("Align"),
    kde_translate("Unknown")
};

const QString getFITSModeStringString(FITSMode mode);

typedef enum { FITS_CLIP, FITS_HFR, FITS_WCS, FITS_VALUE, FITS_POSITION, FITS_ZOOM, FITS_RESOLUTION, FITS_LED, FITS_MESSAGE} FITSBar;

typedef enum
{
    FITS_NONE,
    FITS_AUTO_STRETCH,
    FITS_HIGH_CONTRAST,
    FITS_EQUALIZE,
    FITS_HIGH_PASS,
    FITS_MEDIAN,
    FITS_GAUSSIAN,
    FITS_ROTATE_CW,
    FITS_ROTATE_CCW,
    FITS_MOUNT_FLIP_H,
    FITS_MOUNT_FLIP_V,
    FITS_AUTO,
    FITS_LINEAR,
    FITS_LOG,
    FITS_SQRT,
    FITS_CUSTOM
} FITSScale;

typedef enum { ZOOM_FIT_WINDOW, ZOOM_KEEP_LEVEL, ZOOM_FULL } FITSZoom;

typedef enum { HFR_AVERAGE, HFR_MEDIAN, HFR_HIGH, HFR_MAX, HFR_ADJ_AVERAGE } HFRType;

typedef enum { ALGORITHM_GRADIENT, ALGORITHM_CENTROID, ALGORITHM_THRESHOLD, ALGORITHM_SEP, ALGORITHM_BAHTINOV } StarAlgorithm;

typedef enum { RED_CHANNEL, GREEN_CHANNEL, BLUE_CHANNEL } ColorChannels;

typedef enum { CAT_SKYMAP, CAT_SIMBAD, CAT_VIZIER, CAT_NED, CAT_MAX_REASONS } CatType;

static const QString CatTypeStr[CAT_MAX_REASONS] =
{
    "Sky Map",
    "Simbad",
};

// Size of box drawn on fitsview for each Catalog Object
static constexpr double CAT_OBJ_BOX_SIZE = 10.0;

// Structure to hold catalog object data information
typedef struct
{
    int num;
    CatType catType;
    QString name;
    QString typeCode;
    QString typeLabel;
    dms r;
    dms d;
    double dist; // distance from search center in arcsecs
    double magnitude;
    double size;
    double x; // pixel x, y
    double y;
    bool highlight;
    bool show;
} CatObject;

// Simbad catalog object type structure
struct CatObjType
{
    int depth;
    QString code;
    QString candidateCode;
    QString label;
    QString description;
    QString comments;
};

static constexpr int CAT_OBJ_MAX_DEPTH = 5;
static constexpr int MAX_CAT_OBJ_TYPES = 153;
static CatObjType catObjTypes[MAX_CAT_OBJ_TYPES] =
{
    // STARS
    {0, "*", "", "Star", "Star", ""},

    // Massive Stars and their Remnants
    {1, "Ma*", "Ma?", "Massiv*", "Massive Star", "Initial mass > 8-10 Mo"},
    {2, "bC*", "bC?", "bCepV*", "Massiv*", ""},
    {2, "sg*", "sg?", "Supergiant", "Evolved Supergiant", "Luminosity type 0,Ia,Iab,(I). Includes A-type SG"},
    {3, "s*r", "s?r", "RedSG", "Red Supergiant", "SpT like K/M 0,Ia,Iab,(I)"},
    {3, "s*y", "s?y", "YellowSG", "Yellow Supergiant", "SpT like F/G 0,Ia,Iab,(I)"},
    {3, "s*b", "s?b", "BlueSG", "Blue Supergiant", "SpT like O/B 0,Ia,Iab,(I)"},
    {4, "WR*", "WR?", "WolfRayet*", "Wolf-Rayet", "SpT like W"},
    {2, "N*", "N*?", "Neutron*", "Neutron Star", ""},
    {3, "Psr", "", "Pulsar", "Pulsar", ""},

    // Young Stellar Objects (Pre-Main Sequence Stars)
    {1, "Y*O", "Y*?", "YSO", "Young Stellar Object", "Pre-Main Sequence"},
    {2, "Or*", "", "OrionV*", "Orion Variable", ""},
    {2, "TT*", "TT?", "TTauri*", "T Tauri Star", "Includes classical and weak-line T Tauri"},
    {2, "Ae*", "Ae?", "Ae*", "Herbig Ae/Be Star", ""},
    {2, "out", "of?", "Outflow", "Outflow", ""},
    {3, "HH", "", "HerbigHaroObj", "Herbig-Haro Object", ""},

    // Main Sequence Stars
    {1, "MS*", "MS?", "MainSequence*", "Main Sequence Star", "Main Sequence"},
    {2, "Be*", "Be?", "Be*", "Be Star", ""},
    {2, "BS*", "BS?", "BlueStraggler", "Blue Straggler", ""},
    {3, "SX*", "", "SXPheV*", "SX Phe Variable", "See Pu* for candidate"},
    {2, "gD*", "", "gammaDorV*", "gamma Dor Variable", "See Pu* for candidate"},
    {2, "dS*", "", "delSctV*", "delta Sct Variable", "See Pu* for candidate"},

    // Evolved Stars
    {1, "Ev*", "Ev?", "Evolved*", "Evolved Star", "Post-Main Sequence"},
    {2, "RG*", "RB?", "RGB*", "Red Giant Branch star", ""},
    {2, "HS*", "HS?", "HotSubdwarf", "Hot Subdwarf", "SpT like sdB, sdO"},
    {2, "HB*", "HB?", "HorBranch*", "Horizontal Branch Star", ""},
    {3, "RR*", "RR?", "RRLyrae", "RR Lyrae Variable", ""},
    {2, "WV*", "WV?", "Type2Cep", "Type II Cepheid Variable", "Includes W Wir and BL Her subtypes"},
    {2, "Ce*", "Ce?", "Cepheid", "Cepheid Variable", "Includes anomalous Cepheids"},
    {3, "cC*", "", "ClassicalCep", "Classical Cepheid Variable", "delta Cep type"},
    {2, "C*", "C*?", "C*", "Carbon Star", "Mostly AGB or RGB. SpT like C,N,R C-N,C-R, or C-rich"},
    {2, "S*", "S?", "S*", "S Star", "Mostly AGB or RGB. SpT like S,SC,MS"},
    {2, "LP*", "LP?", "LongPeriodV*", "Long-Period Variable", "AB*, RG*, or s*r. Includes L, SR, and OSARG"},
    {2, "AB*", "AB?", "AGB*", "Asymptotic Giant Branch Star", ""},
    {3, "Mi*", "Mi?", "Mira", "Mira Variable", ""},
    {2, "OH*", "OH?", "OH/IR*", "OH/IR Star", "O-rich AGB or PAGB, doubled-peaked OH maser profile"},
    {2, "pA*", "pA?", "post-AGB*", "Post-AGB Star", ""},
    {2, "RV*", "RV?", "RVTauV*", "RV Tauri Variable", ""},
    {2, "PN", "PN?", "PlanetaryNeb", "Planetary Nebula", "Includes the progenitor Star when known"},
    {2, "WD*", "WD?", "WhiteDwarf", "White Dwarf", "SpT like D and PG1159"},

    // Chemically Peculiar Stars
    {1, "Pe*", "Pe?", "ChemPec", "Chemically Peculiar Star", "Includes metal-poor stars"},
    {2, "a2*", "a2?", "alf2CVnV*", "alpha2 CVn Variable", "Ap stars; magnetic"},
    {2, "RC*", "RC?", "RCrBV*", "R CrB Variable", ""},

    // Interacting Binaries and close Common Proper Motion Systems
    {1, "**", "**?", "**", "Double or Multiple Star", "Interacting binaries and close CPM systems"},
    {2, "EB*", "EB?", "EclBin", "Eclipsing Binary", ""},
    {2, "El*", "El?", "EllipVar", "Ellipsoidal Variable", ""},
    {2, "SB*", "SB?", "SB*", "Spectroscopic Binary", ""},
    {2, "RS*", "RS?", "RSCVnV*", "RS CVn Variable", ""},
    {2, "BY*", "BY?", "BYDraV*", "BY Dra Variable", ""},
    {2, "Sy*", "Sy?", "Symbiotic*", "Symbiotic Star", ""},
    {2, "XB*", "XB?", "XrayBin", "X-ray Binary", ""},
    {3, "LXB", "LX?", "LowMassXBin", "Low Mass X-ray Binary", ""},
    {3, "HXB", "HX?", "HighMassXBin", "High Mass X-ray Binary", ""},
    {2, "CV*", "CV?", "CataclyV", "Cataclysmic Binary", ""},
    {3, "No*", "No?", "Nova", "Classical Nova", ""},

    // SuperNovae
    {1, "SN*", "SN?", "Supernova", "SuperNova", ""},

    // Low mass Stars and substellar Objects
    {1, "LM*", "LM?", "Low-Mass*", "Low-mass Star", "M ~< 1 Mo"},
    {2, "BD*", "BD?", "BrownD*", "Brown Dwarf", "SpT L,T or Y"},
    {1, "Pl", "Pl?", "Planet", "Extra-solar Planet", ""},

    // Properties . variability. spectral. kinematic or environment
    {1, "V*", "V*?", "Variable*", "Variable Star", ""},
    {2, "Ir*", "", "IrregularV*", "Irregular Variable", ""},
    {2, "Er*", "Er?", "Eruptive*", "Eruptive Variable", ""},
    {2, "Ro*", "Ro?", "RotV", "Rotating Variable", ""},
    {2, "Pu*", "Pu?", "PulsV*", "Pulsating Variable", "Non LPV Pulsating Star"},

    // Spectral Properties
    {1, "Em*", "", "EmLine*", "Emission-line Star", "Can be sg*, YSO, Be*, AB*, Sy*, etc..."},

    // Kinematic and Environment Properties
    {1, "PM*", "", "HighPM*", "High Proper Motion Star", "Total proper motion >= 50 mas/yr"},
    {1, "HV*", "", "HighVel*", "High Velocity Star", ""},

    // SET OF STARS
    {0, "Cl*", "Cl?", "Cluster*", "Cluster of Stars", ""},
    {1, "GlC", "Gl?", "GlobCluster", "Globular Cluster", ""},
    {1, "OpC", "", "OpenCluster", "Open Cluster", ""},
    {0, "As*", "As?", "Association", "Association of Stars", ""},
    {1, "St*", "", "Stream", "Stellar Stream", ""},
    {1, "MGr", "", "MouvGroup", "Moving Group", ""},

    // INTERSTELLAR MEDIUM
    {0, "ISM", "", "ISM", "Interstellar Medium Object", ""},
    {1, "SFR", "", "StarFormingReg", "Star Forming Region", ""},
    {1, "HII", "", "HIIReg", "HII Region", ""},
    {1, "Cld", "", "Cloud", "Cloud", ""},
    {2, "GNe", "", "GalNeb", "Nebula", ""},
    {3, "RNe", "", "RefNeb", "Reflection Nebula", ""},
    {2, "MoC", "", "MolCld", "Molecular Cloud", ""},
    {2, "DNe", "", "DarkNeb", "Dark Cloud (nebula)", ""},
    {2, "glb", "", "Globule", "Globule (low-mass dark cloud)", ""},
    {2, "CGb", "", "ComGlob", "Cometary Globule / Pillar", ""},
    {2, "HVC", "", "HVCld", "High-velocity Cloud", "Vlsr > ~90 km/s"},
    {1, "cor", "", "denseCore", "Dense Core", "N(H) > ~1D23 at/cm2, or detection of NH3,HCN,CS,HCO,.."},
    {1, "bub", "", "Bubble", "Bubble", ""},
    {1, "SNR", "SR?", "SNRemnant", "SuperNova Remnant", ""},
    {1, "sh", "", "HIshell", "Interstellar Shell", ""},
    {1, "flt", "", "Filament", "Interstellar Filament", ""},

    // TAXONOMY OF GALAXIES
    {0, "G", "G?", "Galaxy", "Galaxy", ""},
    {1, "LSB", "", "LowSurfBrghtG", "Low Surface Brightness Galaxy", ""},
    {1, "bCG", "", "BlueCompactG", "Blue Compact Galaxy", ""},
    {1, "SBG", "", "StarburstG", "Starburst Galaxy", ""},
    {1, "H2G", "", "HIIG", "HII Galaxy", ""},
    {1, "EmG", "", "EmissionG", "Emission-line galaxy", ""},
    {1, "AGN", "AG?", "AGN", "Active Galaxy Nucleus", ""},
    {2, "SyG", "", "Seyfert", "Seyfert Galaxy", ""},
    {3, "Sy1", "", "Seyfert1", "Seyfert 1 Galaxy", ""},
    {3, "Sy2", "", "Seyfert2", "Seyfert 2 Galaxy", ""},
    {2, "rG", "", "RadioG", "Radio Galaxy", ""},
    {2, "LIN", "", "LINER", "LINER-type Active Galaxy Nucleus", ""},
    {2, "QSO", "Q?", "QSO", "Quasar", ""},
    {3, "Bla", "Bz?", "Blazar", "Blazar", ""},
    {4, "BLL", "BL?", "BLLac", "BL Lac", ""},
    {1, "GiP", "", "GinPair", "Galaxy in Pair of Galaxies", ""},
    {1, "GiG", "", "GtowardsGroup", "Galaxy towards a Group of Galaxies", ""},
    {1, "GiC", "", "GtowardsCl", "Galaxy towards a Cluster of Galaxies", ""},
    {2, "BiC", "", "BrightestCG", "Brightest Galaxy in a Cluster (of Galaxies (BCG)", ""},

    // SETS OF GALAXIES
    {0, "IG", "", "InteractingG", "Interacting Galaxies", ""},
    {0, "PaG", "", "PairG", "Pair of Galaxies", ""},
    {0, "GrG", "Gr?", "GroupG", "Group of Galaxies", ""},
    {1, "CGG", "", "Compact_Gr_G", "Compact Group of Galaxies", ""},
    {0, "ClG", "C?G", "ClG", "Cluster of Galaxies", ""},
    {0, "PCG", "PCG?", "protoClG", "Proto Cluster of Galaxies", ""},
    {0, "SCG", "SC?", "SuperClG", "Supercluster of Galaxies", ""},
    {0, "vid", "", "Void", "Underdense Region of the Universe", ""},

    // GRAVITATION
    {0, "grv", "", "Gravitation", "Gravitational Source", ""},
    {1, "Lev", "", "LensingEv", "(Micro)Lensing Event", ""},
    {1, "gLS", "LS?", "GravLensSystem", "Gravitational Lens System (lens+images)", ""},
    {2, "gLe", "Le?", "GravLens", "Gravitational Lens", ""},
    {2, "LeI", "LI?", "LensedImage", "Gravitationally Lensed Image", ""},
    {3, "LeG", "", "LensedG", "Gravitationally Lensed Image of a Galaxy", ""},
    {3, "LeQ", "", "LensedQ", "Gravitationally Lensed Image of a Quasar", ""},
    {1, "BH", "BH?", "BlackHole", "Black Hole", ""},
    {1, "GWE", "", "GravWaveEvent", "Gravitational Wave Event", ""},

    // GENERAL SPECTRAL PROPERTIES
    {0, "ev", "", "Transient", "Transient Event", ""},
    {0, "var", "", "Variable", "Variable source", ""},
    {0, "Rad", "", "Radio", "Radio Source", ""},
    {1, "mR", "", "metricRad", "Metric Radio Source", ""},
    {1, "cm", "", "cmRad", "Centimetric Radio Source", ""},
    {1, "mm", "", "mmRad", "Millimetric Radio Source", ""},
    {1, "smm", "", "smmRad", "Sub-Millimetric Source", ""},
    {1, "HI", "", "HI", "HI (21cm) Source", ""},
    {1, "rB", "", "radioBurst", "Radio Burst", ""},
    {1, "Mas", "", "Maser", "Maser", ""},
    {0, "IR", "", "Infrared", "Infra-Red Source", ""},
    {1, "FIR", "", "FarIR", "Far-IR source (λ >= 30 µm)", "e.g. IRAS 60/100, AKARI-FIS, Hershel"},
    {1, "MIR", "", "MidIR", "Mid-IR Source (3 to 30 µm)", "e.g. IRAS 12/25, AKARI-IRC, WISE, Spitzer-IRAC-MIPS"},
    {1, "NIR", "", "NearIR", "Near-IR Source (λ < 3 µm)", "JHK, 2MASS, UKIDSS, ..."},
    {0, "Opt", "", "Optical", "Optical Source", "Corresponding to BVRI range"},
    {1, "EmO", "", "EmObj", "Emission Object", "Mostly YSO, Em*, PNe, WR*, Sy*, Galaxies, AGN, QSO"},
    {1, "blu", "", "blue", "Blue Object", "Mostly WD*, HS*, blue HB*, QSO"},
    {0, "UV", "", "UV", "UV-emission Source", ""},
    {0, "X", "", "X", "X-ray Source", ""},
    {1, "ULX", "UX?", "ULX", "Ultra-luminous X-ray Source", ""},
    {0, "gam", "", "gamma", "Gamma-ray Source", ""},
    {1, "gB", "", "gammaBurst", "Gamma-ray Burst", ""},

    // BLENDS, ERRORS, NOT WELL DEFINED OBJECTS
    {0, "mul", "", "Blend", "Composite Object, Blend", ""},
    {0, "err", "", "Inexistent", "Not an Object (Error, Artefact, ...)", ""},
    {0, "PoC", "", "PartofCloud", "Part of Cloud", ""},
    {0, "PoG", "", "PartofG", "Part of a Galaxy", ""},
    {0, "?", "", "Unknown", "Object of Unknown Nature", ""},
    {0, "reg", "", "Region", "Region defined in the Sky", ""}
};
