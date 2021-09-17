/*
    SPDX-FileCopyrightText: 2001 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "locationdialog.h"

#include "kspaths.h"
#include "kstarsdata.h"
#include "Options.h"
#include "ksnotification.h"
#include "kstars_debug.h"
#include "ksutils.h"

#include <QSqlQuery>

#ifdef HAVE_GEOCLUE2
#include <QGeoPositionInfoSource>
#endif
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkConfigurationManager>
#include <QNetworkReply>
#include <QNetworkSession>
#include <QQmlContext>
#include <QUrlQuery>
#include <QPlainTextEdit>


LocationDialogUI::LocationDialogUI(QWidget *parent) : QFrame(parent)
{
    setupUi(this);
}

LocationDialog::LocationDialog(QWidget *parent) : QDialog(parent), timer(nullptr)
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
    KStarsData *data = KStarsData::Instance();

    SelectedCity = nullptr;
    ld           = new LocationDialogUI(this);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(ld);
    setLayout(mainLayout);

    ld->MapView->setLocationDialog(this);

    setWindowTitle(i18nc("@title:window", "Set Geographic Location"));

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(slotOk()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    for (int i = 0; i < 25; ++i)
        ld->TZBox->addItem(QLocale().toString(static_cast<double>(i - 12)));

    //Populate DSTRuleBox
    foreach (const QString &key, data->getRulebook().keys())
    {
        if (!key.isEmpty())
            ld->DSTRuleBox->addItem(key);
    }

    connect(ld->CityFilter, SIGNAL(textChanged(QString)), this, SLOT(enqueueFilterCity()));
    connect(ld->ProvinceFilter, SIGNAL(textChanged(QString)), this, SLOT(enqueueFilterCity()));
    connect(ld->CountryFilter, SIGNAL(textChanged(QString)), this, SLOT(enqueueFilterCity()));
    connect(ld->NewCityName, SIGNAL(textChanged(QString)), this, SLOT(nameChanged()));
    connect(ld->NewProvinceName, SIGNAL(textChanged(QString)), this, SLOT(nameChanged()));
    connect(ld->NewCountryName, SIGNAL(textChanged(QString)), this, SLOT(nameChanged()));
    connect(ld->NewLong, SIGNAL(textChanged(QString)), this, SLOT(dataChanged()));
    connect(ld->NewLat, SIGNAL(textChanged(QString)), this, SLOT(dataChanged()));
    connect(ld->NewElev, SIGNAL(valueChanged(double)), this, SLOT(dataChanged()));

    connect(ld->TZBox, SIGNAL(activated(int)), this, SLOT(dataChanged()));
    connect(ld->DSTRuleBox, SIGNAL(activated(int)), this, SLOT(dataChanged()));
    connect(ld->GeoBox, SIGNAL(itemSelectionChanged()), this, SLOT(changeCity()));
    connect(ld->AddCityButton, SIGNAL(clicked()), this, SLOT(addCity()));
    connect(ld->ClearFieldsButton, SIGNAL(clicked()), this, SLOT(clearFields()));
    connect(ld->RemoveButton, SIGNAL(clicked()), this, SLOT(removeCity()));
    connect(ld->UpdateButton, SIGNAL(clicked()), this, SLOT(updateCity()));

    // FIXME Disable this until Qt5 works with Geoclue2
#ifdef HAVE_GEOCLUE_2
    source = QGeoPositionInfoSource::createDefaultSource(this);
    source->setPreferredPositioningMethods(QGeoPositionInfoSource::SatellitePositioningMethods);
    qDebug() << "Last known position" << source->lastKnownPosition().coordinate();

    connect(source, SIGNAL(positionUpdated(QGeoPositionInfo)), this, SLOT(positionUpdated(QGeoPositionInfo)));
    connect(source, SIGNAL(error(QGeoPositionInfoSource::Error)), this, SLOT(positionUpdateError(QGeoPositionInfoSource::Error)));
    connect(source, SIGNAL(updateTimeout()), this, SLOT(positionUpdateTimeout()));

    connect(ld->GetLocationButton, SIGNAL(clicked()), this, SLOT(requestUpdate()));
#endif

    ld->DSTLabel->setText("<a href=\"showrules\">" + i18n("DST rule:") + "</a>");
    connect(ld->DSTLabel, SIGNAL(linkActivated(QString)), this, SLOT(showTZRules()));

    dataModified = false;
    nameModified = false;
    ld->AddCityButton->setEnabled(false);

    ld->errorLabel->setText(QString());

    // FIXME Disable this until Qt5 works with Geoclue2
#ifdef HAVE_GEOCLUE_2
    nam = new QNetworkAccessManager(this);
    connect(nam, SIGNAL(finished(QNetworkReply*)), this, SLOT(processLocationNameData(QNetworkReply*)));
#endif

    initCityList();
    resize(640, 480);
}

void LocationDialog::initCityList()
{
    KStarsData *data = KStarsData::Instance();
    foreach (GeoLocation *loc, data->getGeoList())
    {
        ld->GeoBox->addItem(loc->fullName());
        filteredCityList.append(loc);

        //If TZ is not an even integer value, add it to listbox
        if (loc->TZ0() - int(loc->TZ0()) && ld->TZBox->findText(QLocale().toString(loc->TZ0())) != -1)
        {
            for (int i = 0; i < ld->TZBox->count(); ++i)
            {
                if (ld->TZBox->itemText(i).toDouble() > loc->TZ0())
                {
                    ld->TZBox->addItem(QLocale().toString(loc->TZ0()), i - 1);
                    break;
                }
            }
        }
    }

    //Sort the list of Cities alphabetically...note that filteredCityList may now have a different ordering!
    ld->GeoBox->sortItems();

    ld->CountLabel->setText(
        i18np("One city matches search criteria", "%1 cities match search criteria", ld->GeoBox->count()));

    // attempt to highlight the current kstars location in the GeoBox
    ld->GeoBox->setCurrentItem(nullptr);
    for (int i = 0; i < ld->GeoBox->count(); i++)
    {
        if (ld->GeoBox->item(i)->text() == data->geo()->fullName())
        {
            ld->GeoBox->setCurrentRow(i);
            break;
        }
    }
}

void LocationDialog::enqueueFilterCity()
{
    if (timer)
        timer->stop();
    else
    {
        timer = new QTimer(this);
        timer->setSingleShot(true);
        connect(timer, SIGNAL(timeout()), this, SLOT(filterCity()));
    }
    timer->start(500);
}

void LocationDialog::filterCity()
{
    KStarsData *data = KStarsData::Instance();
    ld->GeoBox->clear();
    //Do NOT delete members of filteredCityList!
    while (!filteredCityList.isEmpty())
        filteredCityList.takeFirst();

    nameModified = false;
    dataModified = false;
    ld->AddCityButton->setEnabled(false);
    ld->UpdateButton->setEnabled(false);

    foreach (GeoLocation *loc, data->getGeoList())
    {
        QString sc(loc->translatedName());
        QString ss(loc->translatedCountry());
        QString sp = "";
        if (!loc->province().isEmpty())
            sp = loc->translatedProvince();

        if (sc.startsWith(ld->CityFilter->text(), Qt::CaseInsensitive) &&
                sp.startsWith(ld->ProvinceFilter->text(), Qt::CaseInsensitive) &&
                ss.startsWith(ld->CountryFilter->text(), Qt::CaseInsensitive))
        {
            ld->GeoBox->addItem(loc->fullName());
            filteredCityList.append(loc);
        }
    }

    ld->GeoBox->sortItems();

    ld->CountLabel->setText(
        i18np("One city matches search criteria", "%1 cities match search criteria", ld->GeoBox->count()));

    if (ld->GeoBox->count() > 0) // set first item in list as selected
        ld->GeoBox->setCurrentItem(ld->GeoBox->item(0));

    ld->MapView->repaint();
}

void LocationDialog::changeCity()
{
    KStarsData *data = KStarsData::Instance();

    //when the selected city changes, set newCity, and redraw map
    SelectedCity = nullptr;
    if (ld->GeoBox->currentItem())
    {
        for (auto &loc : filteredCityList)
        {
            if (loc->fullName() == ld->GeoBox->currentItem()->text())
            {
                SelectedCity = loc;
                break;
            }
        }
    }

    ld->MapView->repaint();

    //Fill the fields at the bottom of the window with the selected city's data.
    if (SelectedCity)
    {
        ld->NewCityName->setText(SelectedCity->translatedName());
        if (SelectedCity->province().isEmpty())
            ld->NewProvinceName->setText(QString());
        else
            ld->NewProvinceName->setText(SelectedCity->translatedProvince());

        ld->NewCountryName->setText(SelectedCity->translatedCountry());
        ld->NewLong->showInDegrees(SelectedCity->lng());
        ld->NewLat->showInDegrees(SelectedCity->lat());
        ld->TZBox->setEditText(QLocale().toString(SelectedCity->TZ0()));
        ld->NewElev->setValue(SelectedCity->elevation());

        //Pick the City's rule from the rulebook
        for (int i = 0; i < ld->DSTRuleBox->count(); ++i)
        {
            TimeZoneRule tzr = data->getRulebook().value(ld->DSTRuleBox->itemText(i));
            if (tzr.equals(SelectedCity->tzrule()))
            {
                ld->DSTRuleBox->setCurrentIndex(i);
                break;
            }
        }

        ld->RemoveButton->setEnabled(SelectedCity->isReadOnly() == false);
    }

    nameModified = false;
    dataModified = false;
    ld->AddCityButton->setEnabled(false);
    ld->UpdateButton->setEnabled(false);
}

bool LocationDialog::addCity()
{
    return updateCity(CITY_ADD);
}

bool LocationDialog::updateCity()
{
    if (SelectedCity == nullptr)
        return false;

    return updateCity(CITY_UPDATE);
}

bool LocationDialog::removeCity()
{
    if (SelectedCity == nullptr)
        return false;

    return updateCity(CITY_REMOVE);
}

bool LocationDialog::updateCity(CityOperation operation)
{
    if (operation == CITY_REMOVE)
    {
        QString message = i18n("Are you sure you want to remove %1?", selectedCityName());
        if (KMessageBox::questionYesNo(nullptr, message, i18n("Remove City?")) == KMessageBox::No)
            return false; //user answered No.
    }
    else if (!nameModified && !dataModified)
    {
        QString message = i18n("This city already exists in the database.");
        KSNotification::sorry(message, i18n("Error: Duplicate Entry"));
        return false;
    }

    bool latOk(false), lngOk(false), tzOk(false);
    dms lat                = ld->NewLat->createDms(true, &latOk);
    dms lng                = ld->NewLong->createDms(true, &lngOk);
    QString TimeZoneString = ld->TZBox->lineEdit()->text();
    TimeZoneString.replace(QLocale().decimalPoint(), ".");
    double TZ = TimeZoneString.toDouble(&tzOk);
    double height          = ld->NewElev->value();

    if (ld->NewCityName->text().isEmpty() || ld->NewCountryName->text().isEmpty())
    {
        QString message = i18n("All fields (except province) must be filled to add this location.");
        KSNotification::sorry(message, i18n("Fields are Empty"));
        return false;
    }
    else if (!latOk || !lngOk)
    {
        QString message = i18n("Could not parse the Latitude/Longitude.");
        KSNotification::sorry(message, i18n("Bad Coordinates"));
        return false;
    }
    else if (!tzOk)
    {
        QString message = i18n("UTC Offset must be selected.");
        KSNotification::sorry(message, i18n("UTC Offset"));
        return false;
    }

    // If name is still the same then it's an update operation
    if (operation == CITY_ADD && !nameModified)
        operation = CITY_UPDATE;

    /*if ( !nameModified )
    {
        QString message = i18n( "Really override original data for this city?" );
        if ( KMessageBox::questionYesNo( 0, message, i18n( "Override Existing Data?" ), KGuiItem(i18n("Override Data")), KGuiItem(i18n("Do Not Override"))) == KMessageBox::No )
            return false; //user answered No.
    }*/

    QSqlDatabase mycitydb = QSqlDatabase::database("mycitydb");
    QString dbfile        = QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("mycitydb.sqlite");

    // If it doesn't exist, create it
    if (QFile::exists(dbfile) == false)
    {
        mycitydb.setDatabaseName(dbfile);
        mycitydb.open();
        QSqlQuery create_query(mycitydb);
        QString query("CREATE TABLE city ( "
                      "id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, "
                      "Name TEXT DEFAULT NULL, "
                      "Province TEXT DEFAULT NULL, "
                      "Country TEXT DEFAULT NULL, "
                      "Latitude TEXT DEFAULT NULL, "
                      "Longitude TEXT DEFAULT NULL, "
                      "TZ REAL DEFAULT NULL, "
                      "TZRule TEXT DEFAULT NULL,"
                      "Elevation REAL NOT NULL DEFAULT -10 )");

        if (create_query.exec(query) == false)
        {
            qCWarning(KSTARS) << create_query.lastError();
            return false;
        }
    }
    else if (mycitydb.open() == false)
    {
        qCWarning(KSTARS) << mycitydb.lastError();
        return false;
    }

    //Strip off white space
    QString name     = ld->NewCityName->text().trimmed();
    QString province = ld->NewProvinceName->text().trimmed();
    QString country  = ld->NewCountryName->text().trimmed();
    QString TZrule   = ld->DSTRuleBox->currentText();
    double Elevation = ld->NewElev->value();
    GeoLocation *g   = nullptr;

    switch (operation)
    {
        case CITY_ADD:
        {
            QSqlQuery add_query(mycitydb);
            add_query.prepare("INSERT INTO city(Name, Province, Country, Latitude, Longitude, TZ, TZRule, Elevation) "
                              "VALUES(:Name, :Province, :Country, :Latitude, :Longitude, :TZ, :TZRule, :Elevation)");
            add_query.bindValue(":Name", name);
            add_query.bindValue(":Province", province);
            add_query.bindValue(":Country", country);
            add_query.bindValue(":Latitude", lat.toDMSString());
            add_query.bindValue(":Longitude", lng.toDMSString());
            add_query.bindValue(":TZ", TZ);
            add_query.bindValue(":TZRule", TZrule);
            add_query.bindValue(":Elevation", Elevation);
            if (add_query.exec() == false)
            {
                qCWarning(KSTARS) << add_query.lastError();
                return false;
            }

            //Add city to geoList...don't need to insert it alphabetically, since we always sort GeoList
            g = new GeoLocation(lng, lat, name, province, country, TZ, &KStarsData::Instance()->Rulebook[TZrule], Elevation);
            KStarsData::Instance()->getGeoList().append(g);
        }
        break;

        case CITY_UPDATE:
        {
            g = SelectedCity;

            QSqlQuery update_query(mycitydb);
            update_query.prepare("UPDATE city SET Name = :newName, Province = :newProvince, Country = :newCountry, "
                                 "Latitude = :Latitude, Longitude = :Longitude, TZ = :TZ, TZRule = :TZRule, Elevation = :Elevation WHERE "
                                 "Name = :Name AND Province = :Province AND Country = :Country");
            update_query.bindValue(":newName", name);
            update_query.bindValue(":newProvince", province);
            update_query.bindValue(":newCountry", country);
            update_query.bindValue(":Name", SelectedCity->name());
            update_query.bindValue(":Province", SelectedCity->province());
            update_query.bindValue(":Country", SelectedCity->country());
            update_query.bindValue(":Latitude", lat.toDMSString());
            update_query.bindValue(":Longitude", lng.toDMSString());
            update_query.bindValue(":TZ", TZ);
            update_query.bindValue(":TZRule", TZrule);
            update_query.bindValue(":Elevation", Elevation);
            if (update_query.exec() == false)
            {
                qCWarning(KSTARS) << update_query.lastError();
                return false;
            }

            g->setName(name);
            g->setProvince(province);
            g->setCountry(country);
            g->setLat(lat);
            g->setLong(lng);
            g->setTZ0(TZ);
            g->setTZRule(&KStarsData::Instance()->Rulebook[TZrule]);
            g->setElevation(height);

        }
        break;

        case CITY_REMOVE:
        {
            g = SelectedCity;
            QSqlQuery delete_query(mycitydb);
            delete_query.prepare("DELETE FROM city WHERE Name = :Name AND Province = :Province AND Country = :Country");
            delete_query.bindValue(":Name", name);
            delete_query.bindValue(":Province", province);
            delete_query.bindValue(":Country", country);
            if (delete_query.exec() == false)
            {
                qCWarning(KSTARS) << delete_query.lastError();
                return false;
            }

            filteredCityList.removeOne(g);
            KStarsData::Instance()->getGeoList().removeOne(g);
            delete g;
            g = nullptr;
        }
        break;
    }

    //(possibly) insert new city into GeoBox by running filterCity()
    filterCity();

    //Attempt to highlight new city in list
    ld->GeoBox->setCurrentItem(nullptr);
    if (g && ld->GeoBox->count())
    {
        for (int i = 0; i < ld->GeoBox->count(); i++)
        {
            if (ld->GeoBox->item(i)->text() == g->fullName())
            {
                ld->GeoBox->setCurrentRow(i);
                break;
            }
        }
    }

    mycitydb.commit();
    mycitydb.close();

    return true;
}

void LocationDialog::findCitiesNear(int lng, int lat)
{
    KStarsData *data = KStarsData::Instance();
    //find all cities within 3 degrees of (lng, lat); list them in GeoBox
    ld->GeoBox->clear();
    //Remember, do NOT delete members of filteredCityList
    while (!filteredCityList.isEmpty())
        filteredCityList.takeFirst();

    foreach (GeoLocation *loc, data->getGeoList())
    {
        if ((abs(lng - int(loc->lng()->Degrees())) < 3) && (abs(lat - int(loc->lat()->Degrees())) < 3))
        {
            ld->GeoBox->addItem(loc->fullName());
            filteredCityList.append(loc);
        }
    }

    ld->GeoBox->sortItems();
    ld->CountLabel->setText(
        i18np("One city matches search criteria", "%1 cities match search criteria", ld->GeoBox->count()));

    if (ld->GeoBox->count() > 0) // set first item in list as selected
        ld->GeoBox->setCurrentItem(ld->GeoBox->item(0));

    repaint();
}

bool LocationDialog::checkLongLat()
{
    if (ld->NewLong->text().isEmpty() || ld->NewLat->text().isEmpty())
        return false;

    bool ok;
    double lng = ld->NewLong->createDms(true, &ok).Degrees();
    if (!ok)
        return false;
    double lat = ld->NewLat->createDms(true, &ok).Degrees();
    if (!ok)
        return false;

    if (fabs(lng) > 180 || fabs(lat) > 90)
        return false;

    return true;
}

void LocationDialog::clearFields()
{
    ld->CityFilter->clear();
    ld->ProvinceFilter->clear();
    ld->CountryFilter->clear();
    ld->NewCityName->clear();
    ld->NewProvinceName->clear();
    ld->NewCountryName->clear();
    ld->NewLong->clearFields();
    ld->NewLat->clearFields();
    ld->NewElev->setValue(-10);
    ld->TZBox->setCurrentIndex(-1);
    // JM 2017-09-16: No, let's not assume it is 0. User have to explicitly set TZ so avoid mistakes.
    //ld->TZBox->lineEdit()->setText(QLocale().toString(0.0));
    ld->DSTRuleBox->setCurrentIndex(0);
    nameModified = true;
    dataModified = false;

    ld->AddCityButton->setEnabled(false);
    ld->UpdateButton->setEnabled(false);
    ld->NewCityName->setFocus();
}

void LocationDialog::showTZRules()
{
    QFile file;

    if (KSUtils::openDataFile(file, "TZrules.dat") == false)
        return;

    QTextStream stream(&file);

    QString message = i18n("Daylight Saving Time Rules");

    QPointer<QDialog> tzd = new QDialog(this);
    tzd->setWindowTitle(message);

    QPlainTextEdit *textEdit = new QPlainTextEdit(tzd);
    textEdit->setReadOnly(true);
    while (stream.atEnd() == false)
    {
        QString line = stream.readLine();
        if (line.startsWith('#'))
            textEdit->appendPlainText(line);
    }
    textEdit->moveCursor(QTextCursor::Start);
    textEdit->ensureCursorVisible();

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(textEdit);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(rejected()), tzd, SLOT(reject()));

    tzd->setLayout(mainLayout);

    tzd->exec();

    delete tzd;
}

void LocationDialog::nameChanged()
{
    nameModified = true;
    dataChanged();
}

//do not enable Add button until all data are present and valid.
void LocationDialog::dataChanged()
{
    dataModified = true;
    ld->AddCityButton->setEnabled(nameModified && !ld->NewCityName->text().isEmpty() &&
                                  !ld->NewCountryName->text().isEmpty() && checkLongLat() && ld->TZBox->currentIndex() != -1);
    if (SelectedCity)
        ld->UpdateButton->setEnabled(SelectedCity->isReadOnly() == false && !ld->NewCityName->text().isEmpty() &&
                                     !ld->NewCountryName->text().isEmpty() && checkLongLat() && ld->TZBox->currentIndex() != -1);

    if (ld->AddCityButton->isEnabled() == false && ld->UpdateButton->isEnabled() == false)
    {
        if (ld->NewCityName->text().isEmpty())
        {
            ld->errorLabel->setText(i18n("Cannot add new location -- city name blank"));
        }
        else if (ld->NewCountryName->text().isEmpty())
        {
            ld->errorLabel->setText(i18n("Cannot add new location -- country name blank"));
        }
        else if (!checkLongLat())
        {
            ld->errorLabel->setText(i18n("Cannot add new location -- invalid latitude / longitude"));
        }
        else if (ld->TZBox->currentIndex() == -1)
        {
            ld->errorLabel->setText(i18n("Cannot add new location -- missing UTC Offset"));
        }
        else if (SelectedCity && SelectedCity->isReadOnly())
        {
            ld->errorLabel->setText(i18n("City is Read Only. Change name to add new city."));
        }
    }
    else
    {
        ld->errorLabel->setText(QString());
    }
}

void LocationDialog::slotOk()
{
    if (ld->AddCityButton->isEnabled())
    {
        if (addCity())
            accept();
    }
    else
        accept();
}

// FIXME Disable this until Qt5 works with Geoclue2
#ifdef HAVE_GEOCLUE_2
void LocationDialog::getNameFromCoordinates(double latitude, double longitude)
{
    QString lat = QString::number(latitude);
    QString lon = QString::number(longitude);
    QString latlng(lat + ", " + lon);

    QUrl url("http://maps.googleapis.com/maps/api/geocode/json");
    QUrlQuery query;
    query.addQueryItem("latlng", latlng);
    url.setQuery(query);
    qDebug() << "submitting request";

    nam->get(QNetworkRequest(url));
    connect(nam, SIGNAL(finished(QNetworkReply*)), this, SLOT(processLocationNameData(QNetworkReply*)));
}

void LocationDialog::processLocationNameData(QNetworkReply *networkReply)
{
    if (!networkReply)
        return;

    if (!networkReply->error())
    {
        QJsonDocument document = QJsonDocument::fromJson(networkReply->readAll());

        if (document.isObject())
        {
            QJsonObject obj = document.object();
            QJsonValue val;

            if (obj.contains(QStringLiteral("results")))
            {
                val = obj["results"];

                QString city =
                    val.toArray()[0].toObject()["address_components"].toArray()[2].toObject()["long_name"].toString();
                QString region =
                    val.toArray()[0].toObject()["address_components"].toArray()[3].toObject()["long_name"].toString();
                QString country =
                    val.toArray()[0].toObject()["address_components"].toArray()[4].toObject()["long_name"].toString();

                //emit newNameFromCoordinates(city, region, country);
            }
            else
            {
            }
        }
    }
    networkReply->deleteLater();
}

void LocationDialog::requestUpdate()
{
    source->requestUpdate(15000);
}

void LocationDialog::positionUpdated(const QGeoPositionInfo &info)
{
    qDebug() << "Position updated:" << info;
}

void LocationDialog::positionUpdateError(QGeoPositionInfoSource::Error error)
{
    qDebug() << "Position update error: " << error;
}

void LocationDialog::positionUpdateTimeout()
{
    qDebug() << "Timed out!";
    qDebug() << source->error();
}
#endif
