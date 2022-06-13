/*
    SPDX-FileCopyrightText: 2021 Valentin Boettcher <hiro at protagon.space; @hiro98:tchncs.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "addcatalogobject.h"
#include "ui_addcatalogobject.h"
#include "kstars_debug.h"

#include <QInputDialog>
#include <QPushButton>

AddCatalogObject::AddCatalogObject(QWidget *parent, const CatalogObject &obj)
    : QDialog(parent), ui(new Ui::AddCatalogObject), m_object{ obj }
{
    ui->setupUi(this);
    ui->ra->setDegType(false);
    fill_form_from_object();

    connect(ui->name, &QLineEdit::textChanged,
            [&](const auto &name) { m_object.setName(name); });

    connect(ui->long_name, &QLineEdit::textChanged,
            [&](const auto &name) { m_object.setLongName(name); });

    connect(ui->catalog_identifier, &QLineEdit::textChanged,
            [&](const auto &ident) { m_object.setCatalogIdentifier(ident); });

    connect(ui->type, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [&](const auto index) {
                if (index > SkyObject::TYPE_UNKNOWN)
                    m_object.setType(SkyObject::TYPE_UNKNOWN);

                m_object.setType(index);

                refresh_flux();
            });

    auto validateAndStoreCoordinates = [&]() {
        bool raOk(false), decOk(false);
        auto ra = ui->ra->createDms(&raOk);
        auto dec = ui->dec->createDms(&decOk);
        auto* okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
        Q_ASSERT(!!okButton);
        okButton->setEnabled(raOk && decOk);
        if (raOk && decOk) {
            m_object.setRA0(ra);
            m_object.setDec0(dec);
        }
    };

    connect(ui->ra, &dmsBox::textChanged, validateAndStoreCoordinates);
    connect(ui->dec, &dmsBox::textChanged, validateAndStoreCoordinates);

    auto updateMag = [&]()
    {
        m_object.setMag(
            ui->magUnknown->isChecked() ? NaN::f : static_cast<float>(ui->mag->value()));
    };
    connect(ui->mag, QOverload<double>::of(&QDoubleSpinBox::valueChanged), updateMag);
    connect(ui->magUnknown, &QCheckBox::stateChanged, updateMag);

    connect(ui->maj, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [&](const auto value) { m_object.setMaj(static_cast<float>(value)); });

    connect(ui->min, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [&](const auto value) { m_object.setMin(static_cast<float>(value)); });

    connect(ui->flux, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [&](const auto value) { m_object.setFlux(static_cast<float>(value)); });

    connect(ui->position_angle, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            [&](const auto value) { m_object.setPA(value); });

    connect(ui->guessFromTextButton, &QPushButton::clicked, [this]() { this->guess_form_contents_from_text(); });
}

AddCatalogObject::~AddCatalogObject()
{
    delete ui;
}

void AddCatalogObject::fill_form_from_object()
{
    if (m_object.hasName()) // N.B. Avoid filling name fields with "unnamed"
        ui->name->setText(m_object.name());
    if (m_object.hasLongName())
        ui->long_name->setText(m_object.longname());
    ui->catalog_identifier->setText(m_object.catalogIdentifier());

    for (int k = 0; k < SkyObject::NUMBER_OF_KNOWN_TYPES; ++k)
    {
        ui->type->addItem(SkyObject::typeName(k));
    }
    ui->type->addItem(SkyObject::typeName(SkyObject::TYPE_UNKNOWN));
    ui->type->setCurrentIndex((int)(m_object.type()));

    dms ra0 = m_object.ra0();
    dms dec0 = m_object.dec0(); // Make a copy to avoid overwriting by signal-slot connection
    ui->ra->show(ra0);
    ui->dec->show(dec0);
    if (std::isnan(m_object.mag()))
    {
        ui->magUnknown->setChecked(true);
    }
    else
    {
        ui->mag->setValue(m_object.mag());
        ui->magUnknown->setChecked(false);
    }
    ui->flux->setValue(m_object.flux());
    ui->position_angle->setValue(m_object.pa());
    ui->maj->setValue(m_object.a());
    ui->min->setValue(m_object.b());
    refresh_flux();
}

void AddCatalogObject::refresh_flux()
{
    ui->flux->setEnabled(m_object.type() == SkyObject::RADIO_SOURCE);

    if (!ui->flux->isEnabled())
        ui->flux->setValue(0);
}

void AddCatalogObject::guess_form_contents_from_text()
{
    bool accepted = false;
    QString text = QInputDialog::getMultiLineText(
        this,
        i18n("Guess object data from text"),
        i18n("Copy-paste a text blurb with data on the object, and KStars will try to guess the contents of the fields from the text. The result is just a guess, so please verify the coordinates and the other information."),
        QString(),
        &accepted);
    if (accepted && !text.isEmpty())
    {
        guess_form_contents_from_text(text);
    }
}

void AddCatalogObject::guess_form_contents_from_text(QString text)
{
    // Parse text to fill in the entries in the form, using regexes to
    // guess the field values

    // TODO: Guess type from catalog name if the type match failed

    QRegularExpression matchJ2000Line("^(.*)(?:J2000|ICRS|FK5|\\(2000(?:\\.0)?\\))(.*)$");
    matchJ2000Line.setPatternOptions(QRegularExpression::MultilineOption);
    QRegularExpression matchCoords("(?:^|[^-\\d])([-+]?\\d\\d?)(?:h ?|d ?|[^\\d]?° ?|:| )(\\d\\d)(?:m ?|\' ?|’ ?|′ "
                                   "?|:| )(\\d\\d(?:\\.\\d+)?)?(?:s|\"|\'\'|”|″)?\\b");
    QRegularExpression matchCoords2("J?\\d{6,6}[-+]\\d{6,6}");
    QRegularExpression findMag1(
        "(?:[mM]ag(?:nitudes?)?\\s*(?:\\([vV]\\))?|V(?=\\b))(?:\\s*=|:)?\\s*(-?\\d{1,2}(?:\\.\\d{1,3})?)");
    QRegularExpression findMag2("\\b(-?\\d{1,2}\\.\\d{1,3})?\\s*(?:[mM]ag|V|B)\\b");
    QRegularExpression findSize1("\\b(\\d{1,3}(?:\\.\\d{1,2})?)\\s*(°|\'|\"|\'\')?\\s*[xX×]\\s*(\\d{1,3}(?:\\.\\d{1,2})"
                                 "?)\\s*(°|\'|\"|\'\')?\\b");
    QRegularExpression findSize2("\\b(?:[Ss]ize|[Dd]imensions?|[Dd]iameter)[: "
                                 "](?:\\([vV]\\))?\\s*(\\d{1,3}(?:\\.\\d{1,2})?)\\s*(°|\'|\"|\'\')?\\b");
    QRegularExpression findMajorAxis("\\b[Mm]ajor\\s*[Aa]xis:?\\s*(\\d{1,3}(?:\\.\\d{1,2})?)\\s*(°|\'|\"|\'\')?\\b");
    QRegularExpression findMinorAxis("\\b[Mm]inor\\s*[Aa]xis:?\\s*(\\d{1,3}(?:\\.\\d{1,2})?)\\s*(°|\'|\"|\'\')?\\b");
    QRegularExpression findPA(
        "\\b(?:[Pp]osition *[Aa]ngle|PA|[pP]\\.[aA]\\.):?\\s*(\\d{1,3}(\\.\\d{1,2})?)(?:°|[Ddeg])?\\b");
    QRegularExpression findName1(
        "\\b(?:(?:[nN]ames?|NAMES?)[: ]|[iI]dent(?:ifier)?:|[dD]esignation:)\\h*\"?([-+\'A-Za-z0-9 ]*)\"?\\b");
    QStringList catalogNames;
    catalogNames << "NGC"
                 << "IC"
                 << "M"
                 << "PGC"
                 << "UGC"
                 << "UGCA"
                 << "MCG"
                 << "ESO"
                 << "SDSS"
                 << "LEDA"
                 << "IRAS"
                 << "PNG"
                 << "Abell"
                 << "ACO"
                 << "HCG"
                 << "CGCG"
                 << "[IV]+ ?Zw"
                 << "Hickson"
                 << "AGC"
                 << "2MASS"
                 << "RCS2"
                 << "Terzan"
                 << "PN [A-Z0-9]"
                 << "VV"
                 << "PK"
                 << "GSC2"
                 << "LBN"
                 << "LDN"
                 << "Caldwell"
                 << "HIP"
                 << "AM"
                 << "vdB"
                 << "Barnard"
                 << "Shkh?"
                 << "KTG"
                 << "Palomar"
                 << "KPG"
                 << "CGPG"
                 << "TYC"
                 << "Arp"
                 << "Minkowski"
                 << "KUG"
                 << "DDO"
        ;
    QList<QPair<QString, SkyObject::TYPE>> objectTypes;
    objectTypes.append({"[Op]pen ?[Cc]luster|OCL|\\*Cl|Cl\\*|OpC|Cl\\?|Open Cl\\.", SkyObject::OPEN_CLUSTER});
    objectTypes.append({"Globular ?Cluster|GlC|GCl|Glob\\. Cl\\.|Globular|Gl\\?", SkyObject::GLOBULAR_CLUSTER});
    objectTypes.append({"(?:[Gg]as(?:eous)?|Diff(?:\\.|use)?|Emission|Ref(?:\\.|lection)?) ?[Nn]eb(?:ula|\\.)?|Neb|RfN|HII|GNe", SkyObject::GASEOUS_NEBULA});
    objectTypes.append({"PN|PNe|Pl\\.? ?[Nn]eb\\.?|(?:[Pp]re[- ]?)[Pp]lanetary|PPNe|PPN|pA\\*|post[- ]?AGB", SkyObject::PLANETARY_NEBULA});
    objectTypes.append({"SNR|[Ss]upernova ?[Rr]em(?:\\.|nant)?|SNRem(?:\\.|nant)?", SkyObject::SUPERNOVA_REMNANT});
    objectTypes.append({"Gxy?|HIIG|H2G|[bB]CG|SBG|AGN|EmG|LINER|LIRG|GiG|GinP", SkyObject::GALAXY});
    objectTypes.append({"Ast\\.?|Asterism", SkyObject::ASTERISM});
    objectTypes.append({"ClG|GrG|CGG|[Gg](?:ala)?xy ?(?:[Gg]roup|[Cc]luster|[Pp]air|[Tt]rio|[Tt]riple)|GClus|GGrp|GGroup|GClstr|GPair|GTrpl|GTrio", SkyObject::GALAXY_CLUSTER});
    objectTypes.append({"ISM|DNeb?|[dD](?:k\\.?|ark) ?[Nn]eb(?:ula|\\.)?", SkyObject::DARK_NEBULA});
    objectTypes.append({"QSO|[qQ]uasar", SkyObject::QUASAR});
    objectTypes.append({"(?:[Dd]ouble|[Dd]bl\\.?|[Tt]riple|[Mm]ult(?:iple|\\.)? ?[Ss]tar)|\\*\\*|Mult\\.? ?\\*", SkyObject::MULT_STAR});
    objectTypes.append({"[nN]ebula", SkyObject::GASEOUS_NEBULA}); // Catch all nebula
    objectTypes.append({"[gG]alaxy", SkyObject::GALAXY}); // Catch all galaxy

    QRegularExpression findName2("\\b(" + catalogNames.join("|") + ")\\s+(J?[-+0-9\\.]+[A-Da-h]?)\\b");
    QRegularExpression findName3("\\b([A-Za-z]+[0-9]?)\\s+(J?[-+0-9]+[A-Da-h]?)\\b");

    // FIXME: This code will clean up by a lot if std::optional<> can be used
    QString coordText;
    bool coordsFound = false,
        magFound = false,
        sizeFound = false,
        nameFound = false,
        positionAngleFound = false,
        catalogDetermined = false,
        typeFound = false;

    dms RA, Dec;
    float mag           = NaN::f;
    float majorAxis     = NaN::f;
    float minorAxis     = NaN::f;
    float positionAngle = 0;
    QString name;
    QString catalogName;
    QString catalogIdentifier;
    SkyObject::TYPE type = SkyObject::TYPE_UNKNOWN;

    // Note: The following method is a proxy to support older versions of Qt.
    // In Qt 5.5 and above, the QString::indexOf(const QRegularExpression &re, int from, QRegularExpressionMatch *rmatch) method obviates the need for the following.
    auto indexOf = [](const QString & s, const QRegularExpression & regExp, int from, QRegularExpressionMatch * m) -> int
    {
        *m = regExp.match(s, from);
        return m->capturedStart(0);
    };

    auto countNonOverlappingMatches = [indexOf](const QString & string, const QRegularExpression & regExp,
                                      QStringList *list = nullptr) -> int
    {
        int count           = 0;
        int matchIndex      = -1;
        int lastMatchLength = 1;
        QRegularExpressionMatch rmatch;
        while ((matchIndex = indexOf(string, regExp, matchIndex + lastMatchLength, &rmatch)) >= 0)
        {
            ++count;
            lastMatchLength = rmatch.captured(0).length();
            if (list)
                list->append(rmatch.captured(0));
        }
        return count;
    };

    QRegularExpressionMatch rmatch;
    int nonOverlappingMatchCount;
    std::size_t coordTextIndex = 0;
    if ((nonOverlappingMatchCount = countNonOverlappingMatches(text, matchCoords)) == 2)
    {
        coordText = text;
    }
    else if (nonOverlappingMatchCount > 2)
    {
        qCDebug(KSTARS) << "Found more than 2 coordinate matches. Trying to match J2000 line.";
        if ((coordTextIndex = indexOf(text, matchJ2000Line, 0, &rmatch)) >= 0)
        {
            coordText = rmatch.captured(1) + rmatch.captured(2);
            qCDebug(KSTARS) << "Found a J2000 line match: " << coordText;
        }
    }

    if (!coordText.isEmpty())
    {
        int coord1 = indexOf(coordText, matchCoords, 0, &rmatch);
        if (coord1 >= 0) {
            std::size_t length1 = rmatch.captured(0).length();
            RA         = dms(rmatch.captured(1) + ' ' + rmatch.captured(2) + ' ' + rmatch.captured(3), false);
            int coord2 = indexOf(coordText, matchCoords, coord1 + length1, &rmatch);
            if (coord2 >= 0) {
                Dec = dms(rmatch.captured(1) + ' ' + rmatch.captured(2) + ' ' + rmatch.captured(3), true);
                qCDebug(KSTARS) << "Extracted coordinates: " << RA.toHMSString() << " " << Dec.toDMSString();
                coordsFound = true;

                // Remove the coordinates from the original string so subsequent tasks don't confuse it
                std::size_t length2 = rmatch.captured(0).length();
                qCDebug(KSTARS) << "Eliminating text: " << text.midRef(coordTextIndex + coord1, length1) << " and " << text.midRef(coordTextIndex + coord2, length2);
                text.replace(coordTextIndex + coord1, length1, "\n");
                text.replace(coordTextIndex + coord2 - length1 + 1, length2, "\n");
                qCDebug(KSTARS) << "Text now: " << text;
            }
        }
    }
    else
    {
        if (text.contains(matchCoords2, &rmatch))
        {
            QString matchString = rmatch.captured(0);
            QRegularExpression extractCoords2("(\\d\\d)(\\d\\d)(\\d\\d)([-+]\\d\\d)(\\d\\d)(\\d\\d)");
            Q_ASSERT(matchString.contains(extractCoords2, &rmatch));
            RA          = dms(rmatch.captured(1) + ' ' + rmatch.captured(2) + ' ' + rmatch.captured(3), false);
            Dec         = dms(rmatch.captured(4) + ' ' + rmatch.captured(5) + ' ' + rmatch.captured(6), true);
            coordsFound = true;

            // Remove coordinates to avoid downstream confusion with it
            qCDebug(KSTARS) << "Eliminating text: " << text.midRef(rmatch.capturedStart(), rmatch.captured(0).length());
            text.replace(rmatch.capturedStart(), rmatch.captured(0).length(), "\n");
                qCDebug(KSTARS) << "Text now: " << text;
        }
        else
        {
            QStringList matches;
            qCDebug(KSTARS) << "Could not extract RA/Dec. Found " << countNonOverlappingMatches(text, matchCoords, &matches)
                     << " coordinate matches:";
            qCDebug(KSTARS) << matches;
        }
    }

    // Type determination: Support full names of types, or SIMBAD/NED shorthands
    for (const auto& p : objectTypes)
    {
        QRegularExpression findType("\\b(?:" + p.first + ")\\b");
        if (text.contains(findType, &rmatch)) {
            type = p.second;
            typeFound = true;
            qCDebug(KSTARS) << "Found Type: " << SkyObject::typeName(p.second);
            qCDebug(KSTARS) << "Eliminating text: " << text.midRef(rmatch.capturedStart(), rmatch.captured(0).length());
            text.replace(rmatch.capturedStart(), rmatch.captured(0).length(), "\n"); // Remove to avoid downstream confusion
            qCDebug(KSTARS) << "Text now: " << text;
            break;
        }
    }
    if (!typeFound) {
        qCDebug(KSTARS) << "Type not found";
    }

    nameFound = true;
    catalogDetermined = true; // Transition to std::optional with C++17
    if (text.contains(findName1, &rmatch)) // Explicit name search
    {
        qCDebug(KSTARS) << "Found explicit name field: " << rmatch.captured(1) << " in text " << rmatch.captured(0);
        name = rmatch.captured(1);
        catalogDetermined = false;
    }
    else if (text.contains(findName2, &rmatch))
    {
        catalogDetermined = true;
        catalogName = rmatch.captured(1);
        catalogIdentifier = rmatch.captured(2);
        name = catalogName + ' ' + catalogIdentifier;
        qCDebug(KSTARS) << "Found known catalog field: " << name
                        << " in text " << rmatch.captured(0);
    }
    else if (text.contains(findName3, &rmatch))
    {
        // N.B. This case is not strong enough to assume catalog name was found correctly
        name = rmatch.captured(1) + ' ' + rmatch.captured(2);
        qCDebug(KSTARS) << "Found something that looks like a catalog designation: "
                        << name << " in text " << rmatch.captured(0);
    }
    else
    {
        qCDebug(KSTARS) << "Could not find name.";
        nameFound = false;
        catalogDetermined = false;
    }

    magFound = true;
    if (text.contains(findMag1, &rmatch))
    {
        qCDebug(KSTARS) << "Found magnitude: " << rmatch.captured(1) << " in text " << rmatch.captured(0);
        mag = rmatch.captured(1).toFloat();
    }
    else if (text.contains(findMag2, &rmatch))
    {
        qCDebug(KSTARS) << "Found magnitude: " << rmatch.captured(1) << " in text " << rmatch.captured(0);
        mag = rmatch.captured(1).toFloat();
    }
    else
    {
        qCDebug(KSTARS) << "Could not find magnitude.";
        magFound = false;
    }

    sizeFound = true;
    if (text.contains(findSize1, &rmatch))
    {
        qCDebug(KSTARS) << "Found size: " << rmatch.captured(1) << " x " << rmatch.captured(3) << " with units "
                 << rmatch.captured(4) << " in text " << rmatch.captured(0);
        majorAxis = rmatch.captured(1).toFloat();
        QString unitText2;
        if (rmatch.captured(2).isEmpty())
        {
            unitText2 = rmatch.captured(4);
        }
        else
        {
            unitText2 = rmatch.captured(2);
        }

        if (unitText2.contains("°"))
            majorAxis *= 60;
        else if (unitText2.contains("\"") || unitText2.contains("\'\'"))
            majorAxis /= 60;

        minorAxis = rmatch.captured(3).toFloat();
        if (rmatch.captured(4).contains("°"))
            minorAxis *= 60;
        else if (rmatch.captured(4).contains("\"") || rmatch.captured(4).contains("\'\'"))
            minorAxis /= 60;
        qCDebug(KSTARS) << "Major axis = " << majorAxis << "; minor axis = " << minorAxis << " in arcmin";
    }
    else if (text.contains(findSize2, &rmatch))
    {
        majorAxis = rmatch.captured(1).toFloat();
        if (rmatch.captured(2).contains("°"))
            majorAxis *= 60;
        else if (rmatch.captured(2).contains("\"") || rmatch.captured(2).contains("\'\'"))
            majorAxis /= 60;
        minorAxis = majorAxis;
    }
    else if (text.contains(findMajorAxis, &rmatch))
    {
        majorAxis = rmatch.captured(1).toFloat();
        if (rmatch.captured(2).contains("°"))
            majorAxis *= 60;
        else if (rmatch.captured(2).contains("\"") || rmatch.captured(2).contains("\'\'"))
            majorAxis /= 60;
        minorAxis = majorAxis;
        if (text.contains(findMinorAxis, &rmatch))
        {
            minorAxis = rmatch.captured(1).toFloat();
            if (rmatch.captured(2).contains("°"))
                minorAxis *= 60;
            else if (rmatch.captured(2).contains("\"") || rmatch.captured(2).contains("\'\'"))
                minorAxis /= 60;
        }
    }

    else
    {
        qCDebug(KSTARS)
                << "Could not find size."; // FIXME: Improve to include separate major and minor axis matches, and size matches for round objects.
        sizeFound = false;
    }

    positionAngleFound = true;
    if (text.contains(findPA, &rmatch))
    {
        qCDebug(KSTARS) << "Found position angle: " << rmatch.captured(1) << " in text " << rmatch.captured(0);
        positionAngle = rmatch.captured(1).toFloat();
    }
    else
    {
        qCDebug(KSTARS) << "Could not find position angle.";
        positionAngleFound = false;
    }

    if (typeFound)
        ui->type->setCurrentIndex((int)type);
    if (nameFound)
        ui->name->setText(name);
    if (magFound)
    {
        ui->mag->setValue(mag);
        ui->magUnknown->setChecked(false);
    } else
    {
        ui->magUnknown->setChecked(true);
    }
    if (coordsFound)
    {
        ui->ra->show(RA);
        ui->dec->show(Dec);
    }
    if (positionAngleFound)
        ui->position_angle->setValue(positionAngle);
    if (sizeFound)
    {
        ui->maj->setValue(majorAxis);
        ui->min->setValue(minorAxis);
    }
    if (catalogDetermined)
    {
        ui->catalog_identifier->setText(catalogIdentifier);
    }
    refresh_flux();
}
