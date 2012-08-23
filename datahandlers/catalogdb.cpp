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

#include "datahandlers/catalogdb.h"
#include <kstars/Options.h>


bool CatalogDB::Initialize() {
  // TODO(spacetime): Shift db to user directory
  skydb_ = QSqlDatabase::addDatabase("QSQLITE", "skydb");
  QString dbfile = KStandardDirs::locateLocal("appdata",
                                           QString("skycomponents.db"));
  QFile testdb(dbfile);
  bool first_run = false;
  if (!testdb.exists()) {
      kDebug()<< i18n("DSO DB does not exist!");
      first_run = true;
  }
  skydb_.setDatabaseName(dbfile);
  if (!skydb_.open()) {
          kWarning() << i18n("Unable to open DSO database file!");
          kWarning() << LastError();
  } else {
      kDebug() << i18n("Opened the DSO Database. Ready!");
      if (first_run == true) {
          FirstRun();
      }
  }
  skydb_.close();
  return true;
}

void CatalogDB::FirstRun() {
    kWarning() << i18n("Rebuilding User Database");
    QVector<QString> tables;
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
                  // FIXME(spacetime): Major refactoring required to enable
                  // handling double values
                  // "RA DOUBLE NOT NULL  DEFAULT 0.0,"
                  // "Dec DOUBLE DEFAULT 0.0,"
                  "RA CHAR NOT NULL DEFAULT 'NULL',"
                  "Dec CHAR NOT NULL DEFAULT 'NULL',"
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

    for (int i = 0; i < tables.count(); ++i) {
        QSqlQuery query(skydb_);
        if (!query.exec(tables[i])) {
            kDebug() << query.lastError();
        }
    }
    return;
}


CatalogDB::~CatalogDB() {
  skydb_.close();
  QSqlDatabase::removeDatabase("skydb");
}


QSqlError CatalogDB::LastError() {
  // error description is in QSqlError::text()
  return skydb_.lastError();
}


QStringList* CatalogDB::Catalogs() {
  RefreshCatalogList();
  return &catalog_list_;
}


void CatalogDB::RefreshCatalogList() {
  catalog_list_.clear();
  skydb_.open();
  QSqlTableModel catalog(0, skydb_);
  catalog.setTable("Catalog");
  catalog.select();

  for (int i = 0; i < catalog.rowCount(); ++i) {
      QSqlRecord record = catalog.record(i);
      // TODO(spacetime): complete list!
      QString name = record.value("Name").toString();
      catalog_list_.append(name);
//         QString author = record.value("Author").toString();
//         QString license = record.value("License").toString();
//         QString compiled_by = record.value("CompiledBy").toString();
//         QString prefix = record.value("Prefix").toString();
  }

  catalog.clear();
  skydb_.close();
}


int CatalogDB::FindCatalog(const QString &name) {
  skydb_.open();
  QSqlTableModel catalog(0, skydb_);

  catalog.setTable("Catalog");
  catalog.setFilter("Name LIKE \'" + name + "\'");
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


void CatalogDB::AddCatalog(const QString& catalog_name, const QString& prefix,
                           const QString& color, const float epoch,
                           const QString& fluxfreq, const QString& fluxunit,
                           const QString& author, const QString& license) {
  skydb_.open();
  QSqlTableModel cat_entry(0, skydb_);
  cat_entry.setTable("Catalog");

  int row = 0;
  cat_entry.insertRows(row, 1);
  // row(0) is autoincerement ID
  cat_entry.setData(cat_entry.index(row, 1), catalog_name);
  cat_entry.setData(cat_entry.index(row, 2), prefix);
  cat_entry.setData(cat_entry.index(row, 3), color);
  cat_entry.setData(cat_entry.index(row, 4), epoch);
  cat_entry.setData(cat_entry.index(row, 5), author);
  cat_entry.setData(cat_entry.index(row, 6), license);
  cat_entry.setData(cat_entry.index(row, 7), fluxfreq);
  cat_entry.setData(cat_entry.index(row, 8), fluxunit);

  cat_entry.submitAll();

  cat_entry.clear();
  skydb_.close();
}

void CatalogDB::RemoveCatalog(const QString& catalog_name) {
    // Remove from Options if visible
    QList<QString> checkedlist = Options::showCatalogNames();
    if (checkedlist.contains(catalog_name)) {
        checkedlist.removeAll(catalog_name);
        Options::setShowCatalogNames(checkedlist);
    }

    // Part 1 Clear DSO Entries
    ClearDSOEntries(FindCatalog(catalog_name));

    skydb_.open();
    QSqlTableModel catalog(0, skydb_);

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

void CatalogDB::ClearDSOEntries(int catalog_id) {
    skydb_.open();

    QStringList del_query;
    del_query.append("DELETE FROM DSO WHERE UID IN (SELECT UID_DSO FROM "
                     "ObjectDesignation WHERE id_Catalog = " +
                     QString::number(catalog_id) + ")");
    del_query.append("DELETE FROM ObjectDesignation WHERE id_Catalog = " +
                      QString::number(catalog_id));

    for (int i = 0; i < del_query.count(); ++i) {
        QSqlQuery query(skydb_);
        if (!query.exec(del_query[i])) {
            kDebug() << query.lastError();
        }
    }
    skydb_.close();
}


void CatalogDB::AddEntry(const QString& catalog_name, const int ID,
                         const QString& long_name, const QString& ra,
                         const QString& dec, const int type,
                         const float magnitude, const int position_angle,
                         const float major_axis, const float minor_axis,
                         const float flux) {
  skydb_.open();
  // Part 1: Adding in DSO table
  // I will not use QSQLTableModel as I need to execute a query to find
  // out the lastInsertId

  /*
   * TODO (spacetime): Match the incoming entry with the ones from the db
   * with certain fuzz. If found, store it in rowuid
  */
  int rowuid;
  int catid;
  QSqlQuery add_query(skydb_);
  add_query.prepare("INSERT INTO DSO (RA, Dec, Type, Magnitude, PositionAngle,"
                    " MajorAxis, MinorAxis, Flux) VALUES (:RA, :Dec, :Type,"
                    " :Magnitude, :PositionAngle, :MajorAxis, :MinorAxis,"
                    " :Flux)");
  add_query.bindValue("RA", ra);
  add_query.bindValue("Dec", dec);
  add_query.bindValue("Type", type);
  add_query.bindValue("Magnitude", magnitude);
  add_query.bindValue("PositionAngle", position_angle);
  add_query.bindValue("MajorAxis", major_axis);
  add_query.bindValue("MinorAxis", minor_axis);
  add_query.bindValue("Flux", flux);
  if (!add_query.exec())
    kWarning() << add_query.lastQuery();

  // Find UID of the Row just added
  rowuid = add_query.lastInsertId().toInt();
  add_query.clear();

  /* TODO(spacetime)
   * Possible Bugs in QSQL Db with SQLite
   * 1) Unless the db is closed and opened again, the next queries
   *    fail.
   * 2) unless I clear the resources, db close fails. The doc says
   *    this is to be rarely used.
   */

  // Find ID of catalog
  catid = FindCatalog(catalog_name);
  skydb_.close();

  // Part 2: Add in Object Designation
  skydb_.open();
  QSqlQuery add_od(skydb_);
  add_od.prepare("INSERT INTO ObjectDesignation (id_Catalog, UID_DSO, LongName,"
                 " IDNumber) VALUES (:catid, :rowuid, :longname, :id)");
  add_od.bindValue("catid", catid);
  add_od.bindValue("rowuid", rowuid);
  add_od.bindValue("longname", long_name);
  add_od.bindValue("id", ID);
  if (!add_od.exec()) {
    kWarning() << add_od.lastQuery();
    kWarning() << skydb_.lastError();
  }
  add_od.clear();

  skydb_.close();
}


bool CatalogDB::AddCatalogContents(const QString& fname) {
  QDir::setCurrent(QDir::homePath());  // for files with relative path
  QString filename = fname;
  // If the filename begins with "~", replace the "~" with the user's home
  // directory (otherwise, the file will not successfully open)
  if (filename.at(0) == '~')
      filename = QDir::homePath() + filename.mid(1, filename.length());

  QFile ccFile(filename);

  if (ccFile.open(QIODevice::ReadOnly)) {
      QStringList columns;  // list of data column descriptors in the header
      QString catalog_name;

      QTextStream stream(&ccFile);
      // TODO(spacetime) : Decide appropriate number of lines to be read
      QStringList lines;
      for (int times = 10; times >= 0 && !stream.atEnd(); --times)
        lines.append(stream.readLine());
      /*WAS
        * = stream.readAll().split('\n', QString::SkipEmptyParts);
        * Memory Hog!
        */

      if (lines.size() < 1 ||
          !ParseCatalogInfoToDB(lines, columns, catalog_name)) {
          kWarning() << "Issue in catalog file header: " << filename;
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
      QList< QPair<QString, KSParser::DataTypes> > sequence =
                                          buildParserSequence(columns);

      // Part 2) Read file and store into DB
      KSParser catalog_text_parser(filename, '#', sequence, ' ');

      QHash<QString, QVariant> row_content;
      while (catalog_text_parser.HasNextRow()) {
        row_content = catalog_text_parser.ReadNextRow();
        AddEntry(catalog_name, row_content["ID"].toInt(),
                 row_content["Nm"].toString(), row_content["RA"].toString(),
                 row_content["Dc"].toString(), row_content["Tp"].toInt(),
                 row_content["Mg"].toFloat(), row_content["PA"].toFloat(),
                 row_content["Mj"].toFloat(), row_content["Mn"].toFloat(),
                 row_content["Flux"].toFloat());
      }
  }
  return true;
}


bool CatalogDB::ParseCatalogInfoToDB(const QStringList &lines,
                                     QStringList &columns,
                                     QString &catalog_name) {
/*
* Most of the code here is by Thomas Kabelmann from customcatalogcomponent.cpp 
* (now catalogcomponent.cpp)
* 
* I have modified the already existing code into this method
* -- Rishab Arora (spacetime)
*/
  bool foundDataColumns = false;  // set to true if description of data
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
  catEpoch = 0.;

  int i = 0;
  for (; i < lines.size(); ++i) {
      QString d(lines.at(i));  // current data line
      if (d.left(1) != "#") break;  // no longer in header!

      int iname      = d.indexOf("# Name: ");
      int iprefix    = d.indexOf("# Prefix: ");
      int icolor     = d.indexOf("# Color: ");
      int iepoch     = d.indexOf("# Epoch: ");
      int ifluxfreq  = d.indexOf("# Flux Frequency: ");
      int ifluxunit  = d.indexOf("# Flux Unit: ");

      if (iname == 0) {  // line contains catalog name
          iname = d.indexOf(":")+2;
          if (catalog_name.isEmpty()) {
              catalog_name = d.mid(iname);
          } else {  // duplicate name in header
              if (showerrs)
                  errs.append(i18n("Parsing header: ") +
                                i18n("Extra Name field in header: %1."
                                      "  Will be ignored", d.mid(iname)));
          }
      } else if (iprefix == 0) {  // line contains catalog prefix
          iprefix = d.indexOf(":")+2;
          if (catPrefix.isEmpty()) {
              catPrefix = d.mid(iprefix);
          } else {  // duplicate prefix in header
              if (showerrs)
                  errs.append(i18n("Parsing header: ") +
                                i18n("Extra Prefix field in header: %1."
                                      "  Will be ignored", d.mid(iprefix)));
          }
      } else if (icolor == 0) {  // line contains catalog prefix
          icolor = d.indexOf(":")+2;
          if (catColor.isEmpty()) {
                catColor = d.mid(icolor);
          } else {  // duplicate prefix in header
              if (showerrs)
                  errs.append(i18n("Parsing header: ") +
                                i18n("Extra Color field in header: %1."
                                      "  Will be ignored", d.mid(icolor)));
          }
      } else if (iepoch == 0) {  // line contains catalog epoch
          iepoch = d.indexOf(":")+2;
          if (catEpoch == 0.) {
              bool ok(false);
                catEpoch = d.mid(iepoch).toFloat(&ok);
              if (!ok) {
                  if (showerrs)
                      errs.append(i18n("Parsing header: ") +
                                    i18n("Could not convert Epoch to float: "
                                          "%1.  Using 2000. instead",
                                          d.mid(iepoch)));
                  catEpoch = 2000.;  // adopt default value
              }
          }
      } else if (ifluxfreq == 0) {  // line contains catalog flux frequnecy
                ifluxfreq = d.indexOf(":")+2;
                if (catFluxFreq.isEmpty()) {
                  catFluxFreq = d.mid(ifluxfreq);
                } else {  // duplicate prefix in header
                  if (showerrs)
                    errs.append(i18n("Parsing header: ") +
                                i18n("Extra Flux Frequency field in header:"
                                      " %1.  Will be ignored",
                                      d.mid(ifluxfreq)));
          }
      } else if (ifluxunit == 0) {
                      // line contains catalog flux unit
                      ifluxunit = d.indexOf(":") + 2;

                      if (catFluxUnit.isEmpty()) {
                          catFluxUnit = d.mid(ifluxunit);
                      } else {  // duplicate prefix in header
                          if (showerrs)
                              errs.append(i18n("Parsing header: ") +
                                           i18n("Extra Flux Unit field in "
                                           "header: %1.  Will be ignored",
                                           d.mid(ifluxunit)));
            }
      } else if (!foundDataColumns) {  // don't try to parse data column
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

          for (int j = 0; j < fields.size(); ++j) {
              QString s(fields.at(j));
              if (master.contains(s)) {
                  // add the data field
                  columns.append(s);

                  // remove the field from the master list and inc the
                  // count of "good" columns (unless field is "Ignore")
                  if (s != "Ig") {
                      master.removeAt(master.indexOf(s));
                      ncol++;
                  }
              } else if (fields.contains(s)) {  // duplicate field
                  fields.append("Ig");  // ignore the duplicate column
                  if (showerrs)
                      errs.append(i18n("Parsing header: ") +
                                    i18n("Duplicate data field descriptor "
                                    "\"%1\" will be ignored", s));
              } else {  // Invalid field
                  fields.append("Ig");  // ignore the invalid column
                  if (showerrs)
                      errs.append(i18n("Parsing header: ") +
                                    i18n("Invalid data field descriptor "
                                    "\"%1\" will be ignored", s));
              }
          }

          if (ncol) foundDataColumns = true;
      }
  }

  if (!foundDataColumns) {
      if (showerrs)
          errs.append(i18n("Parsing header: ") +
                        i18n("No valid column descriptors found.  Exiting"));
      return false;
  }

  if (i == lines.size()) {
      if (showerrs) errs.append(i18n("Parsing header: ") +
                                        i18n("No data lines found after"
                                        " header.  Exiting."));
      return false;
  } else {
      // Make sure Name, Prefix, Color and Epoch were set
      if (catalog_name.isEmpty()) {
          if (showerrs) errs.append(i18n("Parsing header: ") +
                                            i18n("No Catalog Name specified;"
                                            " setting to \"Custom\""));
          catalog_name = i18n("Custom");
      }
      if (catPrefix.isEmpty()) {
          if (showerrs) errs.append(i18n("Parsing header: ") +
                                            i18n("No Catalog Prefix specified"
                                            "; setting to \"CC\""));
          catPrefix = "CC";
      }
      if (catColor.isEmpty()) {
          if (showerrs) errs.append(i18n("Parsing header: ") +
                                            i18n("No Catalog Color specified"
                                            "; setting to Red"));
          catColor = "#CC0000";
      }
      if (catEpoch == 0.) {
          if (showerrs) errs.append(i18n("Parsing header: ") +
                                            i18n("No Catalog Epoch specified"
                                            "; assuming 2000."));
          catEpoch = 2000.;
      }

      // Detect a duplicate catalog name
      if (FindCatalog(catalog_name) != -1) {
        if (KMessageBox::warningYesNo(0,
                          i18n("A catalog of the same name already exists. "
                                "Overwrite contents? If you press yes, the"
                                " new catalog will erase the old one!"),
                          i18n("Overwrite Existing Catalog"))
            == KMessageBox::No) {
            KMessageBox::information(0, "Catalog addition cancelled.");
            return false;
        } else {
            RemoveCatalog(catalog_name);
        }
      }

      // Everything OK. Make a new Catalog entry in DB
      AddCatalog(catalog_name, catPrefix, catColor, catEpoch,
                 catFluxFreq, catFluxUnit);

      return true;
  }
}

void CatalogDB::GetCatalogData(const QString& catalog_name, QString& prefix,
                               QString& color, QString& fluxfreq,
                               QString& fluxunit, float& epoch) {
    skydb_.open();
    QSqlTableModel catalog(0, skydb_);
    catalog.setTable("Catalog");
    catalog.setFilter("Name LIKE \'" + catalog_name + "\'");
    catalog.select();

    QSqlRecord record = catalog.record(0);
    prefix = record.value("Prefix").toString();
    color = record.value("Color").toString();
    fluxfreq = record.value("FluxFreq").toString();
    fluxunit = record.value("FluxUnit").toString();
    epoch = record.value("Epoch").toFloat();

    catalog.clear();
    skydb_.close();
}


void CatalogDB::GetAllObjects(const QString &catalog,
                              QList< SkyObject* > &sky_list,
                              QMap<int, QString> &names,
                              CatalogComponent *catalog_ptr) {
    sky_list.clear();

    skydb_.open();
    QSqlQuery get_query(skydb_);
    get_query.prepare("SELECT Epoch, Type, RA, Dec, Magnitude, Prefix, "
                      "IDNumber, LongName, MajorAxis, MinorAxis, "
                      "PositionAngle, Flux FROM ObjectDesignation NATURAL "
                      "JOIN DSO NATURAL JOIN Catalog WHERE Catalog.id = "
                      ":catID");
    get_query.bindValue("catID", QString::number(FindCatalog(catalog)));

    kWarning() << get_query.lastQuery();
    kWarning() << get_query.lastError();
    kWarning() << FindCatalog(catalog);

    if (!get_query.exec()) {
        kWarning() << get_query.lastQuery();
        kWarning() << get_query.lastError();
    }

    while (get_query.next()) {
        dms RA, Dec;

        int cat_epoch = get_query.value(0).toInt();
        unsigned char iType = get_query.value(1).toInt();
        RA.setFromString(get_query.value(2).toString(), false);
        Dec.setFromString(get_query.value(3).toString(), true);
        float mag = get_query.value(4).toFloat();
        QString catPrefix = get_query.value(5).toString();
        int id_number_in_catalog = get_query.value(6).toInt();
        QString lname = get_query.value(7).toString();
        float a = get_query.value(8).toFloat();
        float b = get_query.value(9).toFloat();
        float PA = get_query.value(10).toFloat();
        float flux = get_query.value(11).toFloat();

        QString name = catPrefix + ' ' + QString::number(id_number_in_catalog);
        SkyPoint t;
        t.set(RA, Dec);

        if (cat_epoch == 1950) {
            // Assume B1950 epoch
            t.B1950ToJ2000();  // t.ra() and t.dec() are now J2000.0
                               // coordinates
        } else if (cat_epoch == 2000) {
            // Do nothing
                 { }
               } else {
                 // FIXME: What should we do?
                 // FIXME: This warning will be printed for each line in the
                 //        catalog rather than once for the entire catalog
                 kWarning() << "Unknown epoch while dealing with custom "
                               "catalog. Will ignore the epoch and assume"
                               " J2000.0";
        }

        RA = t.ra();
        Dec = t.dec();

        if (iType == 0) {  // Add a star
            StarObject *o = new StarObject(RA, Dec, mag, lname);
            sky_list.append(o);
        } else {  // Add a deep-sky object
            DeepSkyObject *o = new DeepSkyObject(iType, RA, Dec, mag,
                                                 name, QString(), lname,
                                                 catPrefix, a, b, PA);
            o->setFlux(flux);
            o->setCustomCatalog(catalog_ptr);

            sky_list.append(o);

            // Add name to the list of object names
            if (!name.isEmpty()) {
                names.insert(iType, name);
            }
        }

        if (!lname.isEmpty() && lname != name) {
            names.insert(iType, lname);
        }
    }

    get_query.clear();
    skydb_.close();
}


QList< QPair<QString, KSParser::DataTypes> > CatalogDB::
                            buildParserSequence(const QStringList& Columns) {
  QList< QPair<QString, KSParser::DataTypes> > sequence;
  QStringList::const_iterator iter = Columns.begin();

  while (iter != Columns.end()) {
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
