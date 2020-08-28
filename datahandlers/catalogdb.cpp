/***************************************************************************
               catalogDB.cpp  -  K Desktop Planetarium
                            -------------------
   begin                : 2012/03/08
   copyright            : (C) 2012 by Rishab Arora
   email                : ra.rishab@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "catalogdb.h"

#include "catalogdata.h"
#include "catalogentrydata.h"
#include "kstars/version.h"
#include "../kstars/auxiliary/kspaths.h"
#include "starobject.h"
#include "deepskyobject.h"
#include "skycomponent.h"

#include <QSqlField>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlTableModel>

#include <catalog_debug.h>

bool CatalogDB::Initialize()
{
    skydb_         = QSqlDatabase::addDatabase("QSQLITE", "skydb");
    QString dbfile = KSPaths::locate(QStandardPaths::GenericDataLocation, QString("skycomponents.sqlite"));
    if (dbfile.isEmpty())
        dbfile = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + QString("skycomponents.sqlite");

    QFile testdb(dbfile);
    bool first_run = false;
    if (!testdb.exists())
    {
        qCWarning(KSTARS_CATALOG) << "DSO DB does not exist!";
        first_run = true;
    }
    skydb_.setDatabaseName(dbfile);
    if (!skydb_.open())
    {
        qCWarning(KSTARS_CATALOG) << "Unable to open DSO database file!";
        qCWarning(KSTARS_CATALOG) << LastError();
    }
    else
    {
        qCDebug(KSTARS_CATALOG) << "Opened the DSO Database. Ready!";
        if (first_run == true)
        {
            FirstRun();
        }
    }
    skydb_.close();
    return true;
}

void CatalogDB::FirstRun()
{
    qCWarning(KSTARS_CATALOG) << "Rebuilding Additional Sky Catalog Database";
    QVector<QString> tables;
    tables.append("CREATE TABLE Version ("
                  "Version CHAR DEFAULT NULL)");
    tables.append("INSERT INTO Version VALUES (\"" KSTARS_VERSION "\")");
    tables.append("CREATE TABLE ObjectDesignation ("
                  "id INTEGER NOT NULL  DEFAULT NULL PRIMARY KEY,"
                  "id_Catalog INTEGER DEFAULT NULL REFERENCES Catalog (id),"
                  "UID_DSO INTEGER DEFAULT NULL REFERENCES DSO (UID),"
                  "LongName MEDIUMTEXT DEFAULT NULL,"
                  "IDNumber INTEGER DEFAULT NULL,"
                  "Trixel INTEGER NULL)");
    // TODO(kstar):  `Trixel` int(11) NOT NULL COMMENT 'Trixel Number'
    //                  For Future safety

    tables.append("CREATE TABLE Catalog ("
                  "id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT,"
                  "Name CHAR NOT NULL  DEFAULT 'NULL',"
                  "Prefix CHAR DEFAULT 'NULL',"
                  "Color CHAR DEFAULT '#CC0000',"
                  "Epoch FLOAT DEFAULT 2000.0,"
                  "Author CHAR DEFAULT NULL,"
                  "License MEDIUMTEXT DEFAULT NULL,"
                  "FluxFreq CHAR DEFAULT 'NULL',"
                  "FluxUnit CHAR DEFAULT 'NULL')");

    tables.append("CREATE TABLE DSO ("
                  "UID INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT,"
                  "RA DOUBLE NOT NULL  DEFAULT 0.0,"
                  "Dec DOUBLE DEFAULT 0.0,"
                  //"RA CHAR NOT NULL DEFAULT 'NULL',"
                  //"Dec CHAR NOT NULL DEFAULT 'NULL',"
                  "Type INTEGER DEFAULT NULL,"
                  "Magnitude DECIMAL DEFAULT NULL,"
                  "PositionAngle INTEGER DEFAULT NULL,"
                  "MajorAxis FLOAT NOT NULL  DEFAULT NULL,"
                  "MinorAxis FLOAT DEFAULT NULL,"
                  "Flux FLOAT DEFAULT NULL,"
                  "Add1 VARCHAR DEFAULT NULL,"
                  "Add2 INTEGER DEFAULT NULL,"
                  "Add3 INTEGER DEFAULT NULL,"
                  "Add4 INTEGER DEFAULT NULL)");

    for (int i = 0; i < tables.count(); ++i)
    {
        QSqlQuery query(skydb_);
        if (!query.exec(tables[i]))
        {
            qCWarning(KSTARS_CATALOG) << query.lastError();
        }
    }
}

CatalogDB::~CatalogDB()
{
    skydb_.close();
}

QSqlError CatalogDB::LastError()
{
    // error description is in QSqlError::text()
    return skydb_.lastError();
}

QStringList *CatalogDB::Catalogs()
{
    RefreshCatalogList();
    return &catalog_list_;
}

void CatalogDB::RefreshCatalogList()
{
    catalog_list_.clear();
    skydb_.open();
    QSqlTableModel catalog(nullptr, skydb_);
    catalog.setTable("Catalog");
    catalog.setSort(0, Qt::AscendingOrder);
    catalog.select();

    for (int i = 0; i < catalog.rowCount(); ++i)
    {
        QSqlRecord record = catalog.record(i);
        QString name      = record.value("Name").toString();
        catalog_list_.append(name);
        //         QString author = record.value("Author").toString();
        //         QString license = record.value("License").toString();
        //         QString compiled_by = record.value("CompiledBy").toString();
        //         QString prefix = record.value("Prefix").toString();
    }

    catalog.clear();
    skydb_.close();
}

int CatalogDB::FindCatalog(const QString &catalog_name)
{
    skydb_.open();
    QSqlTableModel catalog(nullptr, skydb_);

    catalog.setTable("Catalog");
    catalog.setFilter("Name LIKE \'" + catalog_name + "\'");
    catalog.select();

    int catalog_count = catalog.rowCount();
    QSqlRecord record = catalog.record(0);

    int returnval = -1;
    if (catalog_count > 0)
        returnval = record.value("id").toInt();

    catalog.clear();
    skydb_.close();

    return returnval;
}

void CatalogDB::AddCatalog(const CatalogData &catalog_data)
{
    skydb_.open();
    QSqlTableModel cat_entry(nullptr, skydb_);
    cat_entry.setTable("Catalog");

    int row = 0;
    cat_entry.insertRows(row, 1);
    // row(0) is autoincerement ID
    cat_entry.setData(cat_entry.index(row, 1), catalog_data.catalog_name);
    cat_entry.setData(cat_entry.index(row, 2), catalog_data.prefix);
    cat_entry.setData(cat_entry.index(row, 3), catalog_data.color);
    cat_entry.setData(cat_entry.index(row, 4), catalog_data.epoch);
    cat_entry.setData(cat_entry.index(row, 5), catalog_data.author);
    cat_entry.setData(cat_entry.index(row, 6), catalog_data.license);
    cat_entry.setData(cat_entry.index(row, 7), catalog_data.fluxfreq);
    cat_entry.setData(cat_entry.index(row, 8), catalog_data.fluxunit);

    cat_entry.submitAll();

    cat_entry.clear();
    skydb_.close();
}

void CatalogDB::RemoveCatalog(const QString &catalog_name)
{
    // Part 1 Clear DSO Entries
    ClearDSOEntries(FindCatalog(catalog_name));

    skydb_.open();
    QSqlTableModel catalog(nullptr, skydb_);

    // Part 2 Clear Catalog Table
    catalog.setTable("Catalog");
    catalog.setFilter("Name LIKE \'" + catalog_name + "\'");
    catalog.select();

    catalog.removeRows(0, catalog.rowCount());
    catalog.submitAll();

    catalog.clear();
    skydb_.close();

    RefreshCatalogList();
}

void CatalogDB::ClearDSOEntries(int catalog_id)
{
    skydb_.open();

    QStringList del_query;

    // FIXME(spacetime): Only delete from DSO if removed from all Designations
    // del_query.append("DELETE FROM DSO WHERE UID IN (SELECT UID_DSO FROM "
    //                  "ObjectDesignation WHERE id_Catalog = " +
    //                  QString::number(catalog_id) + ")");

    del_query.append("DELETE FROM ObjectDesignation WHERE id_Catalog = " + QString::number(catalog_id));

    for (int i = 0; i < del_query.count(); ++i)
    {
        QSqlQuery query(skydb_);
        if (!query.exec(del_query[i]))
        {
            qCWarning(KSTARS_CATALOG) << query.lastError();
        }
    }

    skydb_.close();
}

int CatalogDB::FindFuzzyEntry(const double ra, const double dec, const double magnitude)
{
    /*
     * FIXME (spacetime): Match the incoming entry with the ones from the db
     * with certain fuzz. If found, store it in rowuid
     * This Fuzz has not been established after due discussion
    */
    //skydb_.open();
    QSqlTableModel dsoentries(nullptr, skydb_);

    QString filter = "((RA - " + QString().setNum(ra) +
                     ") between -0.0016 and 0.0016) and "
                     "((Dec - " +
                     QString().setNum(dec) +
                     ") between -0.0016 and 0.0016) and"
                     "((Magnitude - " +
                     QString().setNum(magnitude) + ") between -0.1 and 0.1)";
    //   qDebug() << filter;
    dsoentries.setTable("DSO");
    dsoentries.setFilter(filter);
    dsoentries.select();

    int entry_count   = dsoentries.rowCount();
    QSqlRecord record = dsoentries.record(0);

    int returnval = -1;
    if (entry_count > 0)
        returnval = record.value("UID").toInt();

    dsoentries.clear();
    //skydb_.close();
    //   qDebug() << returnval;
    return returnval;
}

bool CatalogDB::AddEntry(const CatalogEntryData &catalog_entry, int catid)
{
    if (!skydb_.open())
    {
        qCWarning(KSTARS_CATALOG) << "Failed to open database to add catalog entry!";
        qCWarning(KSTARS_CATALOG) << LastError();
        return false;
    }
    bool retVal = _AddEntry(catalog_entry, catid);
    skydb_.close();
    return retVal;
}

bool CatalogDB::_AddEntry(const CatalogEntryData &catalog_entry, int catid)
{
    // Verification step
    // If RA, Dec are Null, it denotes an invalid object and should not be written
    if (catid < 0)
    {
        qCWarning(KSTARS_CATALOG) << "Catalog ID " << catid << " is invalid! Cannot add object.";
        return false;
    }
    if (catalog_entry.ra == KSParser::EBROKEN_DOUBLE || catalog_entry.ra == 0.0 || std::isnan(catalog_entry.ra) ||
        catalog_entry.dec == KSParser::EBROKEN_DOUBLE || catalog_entry.dec == 0.0 || std::isnan(catalog_entry.dec))
    {
        qCWarning(KSTARS_CATALOG) << "Attempt to add incorrect ra & dec with ID:" << catalog_entry.ID
                 << " Long Name: " << catalog_entry.long_name;
        return false;
    }
    // Part 1: Adding in DSO table
    // I will not use QSQLTableModel as I need to execute a query to find
    // out the lastInsertId

    // Part 2: Fuzzy Match or Create New Entry
    int rowuid = FindFuzzyEntry(catalog_entry.ra, catalog_entry.dec, catalog_entry.magnitude);
    //skydb_.open();

    if (rowuid == -1) //i.e. No fuzzy match found. Proceed to add new entry
    {
        QSqlQuery add_query(skydb_);
        add_query.prepare("INSERT INTO DSO (RA, Dec, Type, Magnitude, PositionAngle,"
                          " MajorAxis, MinorAxis, Flux) VALUES (:RA, :Dec, :Type,"
                          " :Magnitude, :PositionAngle, :MajorAxis, :MinorAxis,"
                          " :Flux)");
        add_query.bindValue(":RA", catalog_entry.ra);
        add_query.bindValue(":Dec", catalog_entry.dec);
        add_query.bindValue(":Type", catalog_entry.type);
        add_query.bindValue(":Magnitude", catalog_entry.magnitude);
        add_query.bindValue(":PositionAngle", catalog_entry.position_angle);
        add_query.bindValue(":MajorAxis", catalog_entry.major_axis);
        add_query.bindValue(":MinorAxis", catalog_entry.minor_axis);
        add_query.bindValue(":Flux", catalog_entry.flux);
        if (!add_query.exec())
        {
            qCWarning(KSTARS_CATALOG) << "Custom Catalog Insert Query FAILED!";
            qCWarning(KSTARS_CATALOG) << add_query.lastQuery();
            qCWarning(KSTARS_CATALOG) << add_query.lastError();
        }

        // Find UID of the Row just added
        rowuid = add_query.lastInsertId().toInt();
        add_query.clear();
    }
    int ID = catalog_entry.ID;

    /* TODO(spacetime)
     * Possible Bugs in QSQL Db with SQLite
     * 1) Unless the db is closed and opened again, the next queries
     *    fail.
     * 2) unless I clear the resources, db close fails. The doc says
     *    this is to be rarely used.
     */

    // Find ID of catalog
    //skydb_.close();
    //catid = FindCatalog(catalog_entry.catalog_name);

    // Part 3: Add in Object Designation
    //skydb_.open();
    QSqlQuery add_od(skydb_);
    if (ID >= 0)
    {
        add_od.prepare("INSERT INTO ObjectDesignation (id_Catalog, UID_DSO, LongName"
                       ", IDNumber) VALUES (:catid, :rowuid, :longname, :id)");
        add_od.bindValue(":id", ID);
    }
    else
    {
        //qWarning() << "FIXME: This query has not been tested!!!!";
        add_od.prepare("INSERT INTO ObjectDesignation (id_Catalog, UID_DSO, LongName"
                       ", IDNumber) VALUES (:catid, :rowuid, :longname,"
                       "(SELECT MAX(ISNULL(IDNumber,1))+1 FROM ObjectDesignation WHERE id_Catalog = :catid) )");
    }
    add_od.bindValue(":catid", catid);
    add_od.bindValue(":rowuid", rowuid);
    add_od.bindValue(":longname", catalog_entry.long_name);
    bool retVal = true;
    if (!add_od.exec())
    {
        qWarning() << "Query exec failed:";
        qWarning() << add_od.lastQuery();
        qWarning() << skydb_.lastError();
        retVal = false;
    }
    add_od.clear();
    //skydb_.close();

    return retVal;
}

bool CatalogDB::CheckCustomEntry(const QString &entry_long_name, int catid)
{
    if (!skydb_.open())
    {
        qCWarning(KSTARS_CATALOG) << "Failed to open database to remove catalog entry!";
        qCWarning(KSTARS_CATALOG) << LastError();
        return false;
    }
    QSqlQuery query_od(skydb_);

    query_od.prepare("SELECT LongName FROM ObjectDesignation WHERE LongName = :long_name AND id_Catalog = :catid;");
    query_od.bindValue(":long_name", entry_long_name);
    query_od.bindValue(":catid", catid);
    if (!query_od.exec())
    {
        qWarning() << "Query exec failed:";
        qWarning() << query_od.lastQuery();
        qWarning() << skydb_.lastError();
        skydb_.close();
        return false;
    }
    query_od.next();
    QSqlRecord result = query_od.record();

    if (result.count() != 1 || result.value(0).toString().isEmpty())
    {
        skydb_.close();
        return false;
    }
    skydb_.close();
    return true;
}

bool CatalogDB::RemoveCustomEntry(const QString &entry_long_name, int catid)
{
    if (!skydb_.open())
    {
        qCWarning(KSTARS_CATALOG) << "Failed to open database to remove catalog entry!";
        qCWarning(KSTARS_CATALOG) << LastError();
        return false;
    }
    QSqlQuery remove_od(skydb_);

    remove_od.prepare("DELETE FROM ObjectDesignation WHERE LongName = :long_name AND id_Catalog = :catid;");
    remove_od.bindValue(":long_name", entry_long_name);
    remove_od.bindValue(":catid", catid);
    if (!remove_od.exec())
    {
        qWarning() << "Query exec failed:";
        qWarning() << remove_od.lastQuery();
        qWarning() << skydb_.lastError();
        skydb_.close();
        return false;
    }
    skydb_.close();
    return true;
}

QString CatalogDB::GetCatalogName(const QString &fname)
{
    QDir::setCurrent(QDir::homePath()); // for files with relative path
    QString filename = fname;
    // If the filename begins with "~", replace the "~" with the user's home
    // directory (otherwise, the file will not successfully open)
    if (filename.at(0) == '~')
        filename = QDir::homePath() + filename.mid(1, filename.length());

    QFile ccFile(filename);

    if (ccFile.open(QIODevice::ReadOnly))
    {
        QString catalog_name;
        QTextStream stream(&ccFile);
        QString line;

        for (int times = 10; times >= 0 && !stream.atEnd(); --times)
        {
            line      = stream.readLine();
            int iname = line.indexOf("# Name: ");
            if (iname == 0)
            {
                // line contains catalog name
                iname        = line.indexOf(":") + 2;
                catalog_name = line.mid(iname);
                return catalog_name;
            }
        }
    }

    return QString();
}

bool CatalogDB::AddCatalogContents(const QString &fname)
{
    QDir::setCurrent(QDir::homePath()); // for files with relative path
    QString filename = fname;
    // If the filename begins with "~", replace the "~" with the user's home
    // directory (otherwise, the file will not successfully open)
    if (filename.at(0) == '~')
        filename = QDir::homePath() + filename.mid(1, filename.length());

    QFile ccFile(filename);

    if (ccFile.open(QIODevice::ReadOnly))
    {
        QStringList columns; // list of data column descriptors in the header
        QString catalog_name;
        char delimiter;

        QTextStream stream(&ccFile);
        // TODO(spacetime) : Decide appropriate number of lines to be read
        QStringList lines;
        for (int times = 10; times >= 0 && !stream.atEnd(); --times)
            lines.append(stream.readLine());
        /*WAS
          * = stream.readAll().split('\n', QString::SkipEmptyParts);
          * Memory Hog!
          */

        if (lines.size() < 1 || !ParseCatalogInfoToDB(lines, columns, catalog_name, delimiter))
        {
            qWarning() << "Issue in catalog file header: " << filename;
            ccFile.close();
            return false;
        }
        ccFile.close();
        // The entry in the Catalog table is now ready!

        /*
          * Now 'Columns' should be a StringList of the Header contents
          * Hence, we 1) Convert the Columns to a KSParser compatible format
          *           2) Use KSParser to read stuff and store in DB
          */

        // Part 1) Conversion to KSParser compatible format
        QList<QPair<QString, KSParser::DataTypes>> sequence = buildParserSequence(columns);

        // Part 2) Read file and store into DB
        KSParser catalog_text_parser(filename, '#', sequence, delimiter);

        int catid = FindCatalog(catalog_name);

        skydb_.open();
        skydb_.transaction();

        QHash<QString, QVariant> row_content;
        while (catalog_text_parser.HasNextRow())
        {
            row_content = catalog_text_parser.ReadNextRow();

            CatalogEntryData catalog_entry;

            dms read_ra(row_content["RA"].toString(), false);
            dms read_dec(row_content["Dc"].toString(), true);
            //qDebug()<<row_content["Nm"].toString();
            catalog_entry.catalog_name   = catalog_name;
            catalog_entry.ID             = row_content["ID"].toInt();
            catalog_entry.long_name      = row_content["Nm"].toString();
            catalog_entry.ra             = read_ra.Degrees();
            catalog_entry.dec            = read_dec.Degrees();
            catalog_entry.type           = row_content["Tp"].toInt();
            catalog_entry.magnitude      = row_content["Mg"].toFloat();
            catalog_entry.position_angle = row_content["PA"].toFloat();
            catalog_entry.major_axis     = row_content["Mj"].toFloat();
            catalog_entry.minor_axis     = row_content["Mn"].toFloat();
            catalog_entry.flux           = row_content["Flux"].toFloat();

            _AddEntry(catalog_entry, catid);
        }

        skydb_.commit();
        skydb_.close();
    }
    return true;
}

bool CatalogDB::ParseCatalogInfoToDB(const QStringList &lines, QStringList &columns, QString &catalog_name,
                                     char &delimiter)
{
    /*
    * Most of the code here is by Thomas Kabelmann from customcatalogcomponent.cpp
    * (now catalogcomponent.cpp)
    *
    * I have modified the already existing code into this method
    * -- Rishab Arora (spacetime)
    */
    bool foundDataColumns = false; // set to true if description of data
    // columns found
    int ncol = 0;

    QStringList errs;
    QString catPrefix, catColor, catFluxFreq, catFluxUnit;
    float catEpoch;
    bool showerrs = false;

    catalog_name.clear();
    catPrefix.clear();
    catColor.clear();
    catFluxFreq.clear();
    catFluxUnit.clear();
    catEpoch  = 0.;
    delimiter = '\0';

    int i = 0;
    for (; i < lines.size(); ++i)
    {
        QString d(lines.at(i)); // current data line
        if (d.at(0) != '#')
            break; // no longer in header!

        int idelimiter = d.indexOf("# Delimiter: ");
        int iname      = d.indexOf("# Name: ");
        int iprefix    = d.indexOf("# Prefix: ");
        int icolor     = d.indexOf("# Color: ");
        int iepoch     = d.indexOf("# Epoch: ");
        int ifluxfreq  = d.indexOf("# Flux Frequency: ");
        int ifluxunit  = d.indexOf("# Flux Unit: ");

        if (idelimiter == 0) // line contains delimiter
        {
            idelimiter = d.indexOf(":") + 2;
            qWarning() << idelimiter << d;
            if (delimiter == '\0')
            {
                delimiter = d.mid(idelimiter).at(0).toLatin1();
            }
            else // duplicate name in header
            {
                if (showerrs)
                    errs.append(i18n("Parsing header: extra Delimiter field in header: %1."
                                                                "  Will be ignored",
                                                                d.mid(idelimiter)));
            }
        }
        else if (iname == 0) // line contains catalog name
        {
            iname = d.indexOf(":") + 2;
            if (catalog_name.isEmpty())
            {
                catalog_name = d.mid(iname);
            }
            else // duplicate name in header
            {
                if (showerrs)
                    errs.append(i18n("Parsing header: extra Name field in header: %1."
                                                                "  Will be ignored",
                                                                d.mid(iname)));
            }
        }
        else if (iprefix == 0) // line contains catalog prefix
        {
            iprefix = d.indexOf(":") + 2;
            if (catPrefix.isEmpty())
            {
                catPrefix = d.mid(iprefix);
            }
            else // duplicate prefix in header
            {
                if (showerrs)
                    errs.append(i18n("Parsing header: extra Prefix field in header: %1."
                                                                "  Will be ignored",
                                                                d.mid(iprefix)));
            }
        }
        else if (icolor == 0) // line contains catalog prefix
        {
            icolor = d.indexOf(":") + 2;
            if (catColor.isEmpty())
            {
                catColor = d.mid(icolor);
            }
            else // duplicate prefix in header
            {
                if (showerrs)
                    errs.append(i18n("Parsing header: extra Color field in header: %1."
                                                                "  Will be ignored",
                                                                d.mid(icolor)));
            }
        }
        else if (iepoch == 0) // line contains catalog epoch
        {
            iepoch = d.indexOf(":") + 2;
            if (catEpoch == 0.)
            {
                bool ok(false);
                catEpoch = d.midRef(iepoch).toFloat(&ok);
                if (!ok)
                {
                    if (showerrs)
                        errs.append(i18n("Parsing header: could not convert Epoch to float: "
                                                                    "%1.  Using 2000 instead",
                                                                    d.mid(iepoch)));
                    catEpoch = 2000.; // adopt default value
                }
            }
        }
        else if (ifluxfreq == 0) // line contains catalog flux frequnecy
        {
            ifluxfreq = d.indexOf(":") + 2;
            if (catFluxFreq.isEmpty())
            {
                catFluxFreq = d.mid(ifluxfreq);
            }
            else // duplicate prefix in header
            {
                if (showerrs)
                    errs.append(i18n("Parsing header: extra Flux Frequency field in header:"
                                                                " %1.  Will be ignored",
                                                                d.mid(ifluxfreq)));
            }
        }
        else if (ifluxunit == 0)
        {
            // line contains catalog flux unit
            ifluxunit = d.indexOf(":") + 2;

            if (catFluxUnit.isEmpty())
            {
                catFluxUnit = d.mid(ifluxunit);
            }
            else // duplicate prefix in header
            {
                if (showerrs)
                    errs.append(i18n("Parsing header: extra Flux Unit field in "
                                                                "header: %1.  Will be ignored",
                                                                d.mid(ifluxunit)));
            }
        }
        else if (!foundDataColumns) // don't try to parse data column
        {
            // descriptors if we already found them
            columns.clear();

            // Chomp off leading "#" character
            d = d.remove('#');

            // split on whitespace
            QStringList fields = d.split(' ', QString::SkipEmptyParts);

            // we need a copy of the master list of data fields, so we can
            // remove fields from it as they are encountered in the "fields" list.
            // this allows us to detect duplicate entries
            QStringList master(fields);

            for (int j = 0; j < fields.size(); ++j)
            {
                QString s(fields.at(j));
                if (master.contains(s))
                {
                    // add the data field
                    columns.append(s);

                    // remove the field from the master list and inc the
                    // count of "good" columns (unless field is "Ignore")
                    if (s != "Ig")
                    {
                        master.removeAt(master.indexOf(s));
                        ncol++;
                    }
                }
                else if (fields.contains(s)) // duplicate field
                {
                    fields.append("Ig"); // ignore the duplicate column
                    if (showerrs)
                        errs.append(i18n("Parsing header: duplicate data field descriptor "
                                                                    "\"%1\".  Will be ignored",
                                                                    s));
                }
                else // Invalid field
                {
                    fields.append("Ig"); // ignore the invalid column
                    if (showerrs)
                        errs.append(i18n("Parsing header: invalid data field descriptor "
                                                                    "\"%1\".  Will be ignored",
                                                                    s));
                }
            }

            if (ncol)
                foundDataColumns = true;
        }
    }

    // For backward compatibility of old catalogs. No delimiter -> assume space
    if (delimiter == '\0')
        delimiter = ' ';

    if (!foundDataColumns)
    {
        if (showerrs)
            errs.append(i18n("Parsing header: no valid column descriptors found.  Exiting"));
        return false;
    }

    if (i == lines.size())
    {
        if (showerrs)
            errs.append(i18n("Parsing header: no data lines found after"
                                                        " header.  Exiting."));
        return false;
    }
    else
    {
        // Make sure Name, Prefix, Color and Epoch were set
        if (catalog_name.isEmpty())
        {
            if (showerrs)
                errs.append(i18n("Parsing header: no Catalog Name specified;"
                                                            " setting to \"Custom\""));
            catalog_name = i18n("Custom");
        }
        if (catPrefix.isEmpty())
        {
            if (showerrs)
                errs.append(i18n("Parsing header: no Catalog Prefix specified"
                                                            "; setting to \"CC\""));
            catPrefix = "CC";
        }
        if (catColor.isEmpty())
        {
            if (showerrs)
                errs.append(i18n("Parsing header: no Catalog Color specified"
                                                            "; setting to Red"));
            catColor = "#CC0000";
        }
        if (catEpoch == 0.)
        {
            if (showerrs)
                errs.append(i18n("Parsing header: no Catalog Epoch specified"
                                                            "; assuming 2000"));
            catEpoch = 2000.;
        }
#if !defined(ANDROID)
        // Detect a duplicate catalog name
        if (FindCatalog(catalog_name) != -1)
        {
            if (KMessageBox::warningYesNo(nullptr,
                                          i18n("A catalog of the same name already exists. "
                                               "Overwrite contents? If you press yes, the"
                                               " new catalog will erase the old one!"),
                                          i18n("Overwrite Existing Catalog")) == KMessageBox::No)
            {
                return false;
            }
            else
            {
                RemoveCatalog(catalog_name);
            }
        }
#endif

        // Everything OK. Make a new Catalog entry in DB
        CatalogData new_catalog;

        new_catalog.catalog_name = catalog_name;
        new_catalog.prefix       = catPrefix;
        new_catalog.color        = catColor;
        new_catalog.epoch        = catEpoch;
        new_catalog.fluxfreq     = catFluxFreq;
        new_catalog.fluxunit     = catFluxUnit;

        AddCatalog(new_catalog);

        return true;
    }
}

void CatalogDB::GetCatalogData(const QString &catalog_name, CatalogData &load_catalog)
{
    skydb_.open();
    QSqlTableModel catalog(nullptr, skydb_);
    catalog.setTable("Catalog");
    catalog.setFilter("Name LIKE \'" + catalog_name + "\'");
    catalog.select();

    QSqlRecord record     = catalog.record(0);
    load_catalog.prefix   = record.value("Prefix").toString();
    load_catalog.color    = record.value("Color").toString();
    load_catalog.fluxfreq = record.value("FluxFreq").toString();
    load_catalog.fluxunit = record.value("FluxUnit").toString();
    load_catalog.epoch    = record.value("Epoch").toFloat();

    catalog.clear();
    skydb_.close();
}

void CatalogDB::GetAllObjects(const QString &catalog, QList<SkyObject *> &sky_list,
                              QList<QPair<int, QString>> &object_names, CatalogComponent *catalog_ptr,
                              bool includeCatalogDesignation)
{
    qDeleteAll(sky_list);
    sky_list.clear();
    QString selected_catalog = QString::number(FindCatalog(catalog));

    skydb_.open();
    QSqlQuery get_query(skydb_);
    get_query.prepare("SELECT Epoch, Type, RA, Dec, Magnitude, Prefix, "
                      "IDNumber, LongName, MajorAxis, MinorAxis, "
                      "PositionAngle, Flux FROM ObjectDesignation JOIN DSO "
                      "JOIN Catalog WHERE Catalog.id = :catID AND "
                      "ObjectDesignation.id_Catalog = Catalog.id AND "
                      "ObjectDesignation.UID_DSO = DSO.UID");
    get_query.bindValue(":catID", selected_catalog);

    //     qWarning() << get_query.lastQuery();
    //     qWarning() << get_query.lastError();
    //     qWarning() << FindCatalog(catalog);

    if (!get_query.exec())
    {
        qWarning() << get_query.lastQuery();
        qWarning() << get_query.lastError();
    }

    while (get_query.next())
    {
        int cat_epoch       = get_query.value(0).toInt();
        unsigned char iType = get_query.value(1).toInt();
        dms RA(get_query.value(2).toDouble());
        dms Dec(get_query.value(3).toDouble());
        float mag                = get_query.value(4).toFloat();
        QString catPrefix        = get_query.value(5).toString();
        int id_number_in_catalog = get_query.value(6).toInt();
        QString lname            = get_query.value(7).toString();
        float a                  = get_query.value(8).toFloat();
        float b                  = get_query.value(9).toFloat();
        float PA                 = get_query.value(10).toFloat();
        float flux               = get_query.value(11).toFloat();
        QString name;

        if (!includeCatalogDesignation && !lname.isEmpty())
        {
            name  = lname;
            lname = QString();
        }
        else
            name = catPrefix + ' ' + QString::number(id_number_in_catalog);

        SkyPoint t;
        t.set(RA, Dec);

        if (cat_epoch == 1950)
        {
            // Assume B1950 epoch
            t.B1950ToJ2000(); // t.ra() and t.dec() are now J2000.0
            // coordinates
        }
        else if (cat_epoch == 2000)
        {
            // Do nothing
            {
            }
        }
        else
        {
            // FIXME: What should we do?
            // FIXME: This warning will be printed for each line in the
            //        catalog rather than once for the entire catalog
            qWarning() << "Unknown epoch while dealing with custom "
                          "catalog. Will ignore the epoch and assume"
                          " J2000.0";
        }

        RA  = t.ra();
        Dec = t.dec();

        // FIXME: It is a bad idea to create objects in one class
        // (using new) and delete them in another! The objects created
        // here are usually deleted by CatalogComponent! See
        // CatalogComponent::loadData for more information!

        if (iType == 0) // Add a star
        {
            StarObject *o = new StarObject(RA, Dec, mag, lname);

            sky_list.append(o);
        }
        else // Add a deep-sky object
        {
            DeepSkyObject *o = new DeepSkyObject(iType, RA, Dec, mag, name, QString(), lname, catPrefix, a, b, -PA);

            o->setFlux(flux);
            o->setCustomCatalog(catalog_ptr);

            sky_list.append(o);

            // Add name to the list of object names
            if (!name.isEmpty())
            {
                object_names.append(qMakePair<int, QString>(iType, name));
            }
        }

        if (!lname.isEmpty() && lname != name)
        {
            object_names.append(qMakePair<int, QString>(iType, lname));
        }
    }

    get_query.clear();
    skydb_.close();
}

QList<QPair<QString, KSParser::DataTypes>> CatalogDB::buildParserSequence(const QStringList &Columns)
{
    QList<QPair<QString, KSParser::DataTypes>> sequence;
    QStringList::const_iterator iter = Columns.begin();

    while (iter != Columns.end())
    {
        // Available Types: ID RA Dc Tp Nm Mg Flux Mj Mn PA Ig
        KSParser::DataTypes current_type;

        if (*iter == QString("ID"))
            current_type = KSParser::D_QSTRING;
        else if (*iter == QString("RA"))
            current_type = KSParser::D_QSTRING;
        else if (*iter == QString("Dc"))
            current_type = KSParser::D_QSTRING;
        else if (*iter == QString("Tp"))
            current_type = KSParser::D_INT;
        else if (*iter == QString("Nm"))
            current_type = KSParser::D_QSTRING;
        else if (*iter == QString("Mg"))
            current_type = KSParser::D_FLOAT;
        else if (*iter == QString("Flux"))
            current_type = KSParser::D_FLOAT;
        else if (*iter == QString("Mj"))
            current_type = KSParser::D_FLOAT;
        else if (*iter == QString("Mn"))
            current_type = KSParser::D_FLOAT;
        else if (*iter == QString("PA"))
            current_type = KSParser::D_FLOAT;
        else if (*iter == QString("Ig"))
            current_type = KSParser::D_SKIP;

        sequence.append(qMakePair(*iter, current_type));
        ++iter;
    }

    return sequence;
}
