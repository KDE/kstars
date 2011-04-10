/***************************************************************************
                          kstarsdb.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Wed May 12 2010
    copyright            : (C) 2010 by Victor Carbune
    email                : victor.carbune@kdemail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "kstarsdb.h"
#include <klocale.h>
#include "kdebug.h"
#include "ksfilereader.h"
#include "kstarsdata.h"
#include "kstarsdb.h"
#include "dms.h"
#include "skyobjects/deepskyobject.h"

KStarsDB::KStarsDB()
{
    db = QSqlDatabase::addDatabase("QSQLITE");
    kDebug() << "WTF" << objectHash.contains(1);
}

KStarsDB* KStarsDB::pinstance = 0;

bool KStarsDB::loadDatabase(QString dbfile = KSTARS_DBFILE)
{
    db.setDatabaseName(dbfile);
    return db.open();
}

bool KStarsDB::createDefaultDatabase(QString dbfile)
{
    db.setDatabaseName(dbfile);
    return createDefaultDatabase();
}

bool KStarsDB::createDefaultDatabase()
{
    if (db.open() == false) {
           kDebug() << i18n("Unable to open database file!");

           return false;
    }

    QSqlQuery query(db);

    // Enable Foreign Keys -- just to ensure integrity of the table!
    if (!query.exec("PRAGMA foreign_keys = ON")) {
        qDebug() << query.lastError();
    }

    // Disable synchronous for speed up
    if (!query.exec("PRAGMA synchronous = OFF")) {
        qDebug() << query.lastError();
    }

    // Create the Catalog entity
    if (!query.exec(QString("CREATE TABLE ctg (name TEXT, source TEXT, description TEXT)"))) {
        qDebug() << query.lastError();
    }
    
    // Create the Constellation entity
    if (!query.exec(QString("CREATE TABLE cnst (shortname TEXT, latinname TEXT, boundary TEXT, description TEXT)"))) {
        qDebug() << query.lastError();
    }

    /*
    // Create the Deep Sky Object entity
    if (!query.exec(QString("CREATE TABLE dso (rah INTEGER, ram INTEGER, ras FLOAT, ") + 
                    QString("sgn INTEGER, decd INTEGER, decm INTEGER, decs INTEGER, ") +
                    QString("bmag FLOAT, type INTEGER, pa FLOAT, minor FLOAT, major FLOAT, longname TEXT)"))) {
        qDebug() << query.lastError();
    }
    */

    
    // Create the Deep Sky Object entity
    if (!query.exec(QString("CREATE TABLE dso (ra TEXT, dec TEXT, ") +
                    QString("sgn INTEGER, ") +
                    QString("bmag FLOAT, type INTEGER, pa FLOAT, minor FLOAT, major FLOAT, longname TEXT)"))) {
        kDebug() << query.lastError();
    }

    
    // Create the Object Designation entity
    if (!query.exec(QString("CREATE TABLE od (designation TEXT, idCTG INTEGER NOT NULL, idDSO INTEGER NOT NULL)"))) {
        kDebug() << query.lastError();
    }

    // Creating index on idCTG
    // Justification: Searches based on the catalog id are done frequently (when loading)
    if (!query.exec("CREATE INDEX idCTG ON od(idCTG)")) {
        kDebug() << query.lastError();
    }

    // Creating index on idDSO
    // Justification: Searches based on the deep sky objects are done when loading the objects
    if (!query.exec("CREATE INDEX idDSO ON od(idDSO)")) {
        kDebug() << query.lastError();
    }

    // Creating index on designation
    // Searching: Sometimes, we want to retrieve an object by it's designation, (M 23) for example
    if (!query.exec("CREATE INDEX designation ON od(designation)")) {
        kDebug() << query.lastError();
    }

    // Create the URL Table (was data/image_url.dat and data/info_url.dat)
    // For all DSO the information is now kept in this table
    if (!query.exec(QString("CREATE TABLE dso_url (idDSO INTEGER NOT NULL, title TEXT, url TEXT, type INTEGER)"))) {
        kDebug() << query.lastError();
    }

    // Creating index on idDSO in dso_url
    // Justification: When loading deep sky objects, a search based on idDSO is made
    if (!query.exec("CREATE INDEX idDSOURL ON dso_url(idDSO)")) {
        kDebug() << query.lastError();
    }

    // Insert the NGC catalog
    if (!query.exec(QString("INSERT INTO ctg VALUES(\"NGC\", \"ngcic.dat\", \"New General Catalogue\")"))) {
        kDebug() << query.lastError();
    }
    
    // Insert the IC catalog
    if (!query.exec(QString("INSERT INTO ctg VALUES(\"IC\", \"ngcic.dat\", \"Index Catalogue of Nebulae and Clusters of Stars\")"))) {
        kDebug() << query.lastError();
    }
    
    // Insert the Messier catalog
    if (!query.exec(QString("INSERT INTO ctg VALUES(\"M\", \"ngcic.dat\", \"Messier Catalogue\")"))) {
        kDebug() << query.lastError();
    }

    // Insert the PGC catalog
    if (!query.exec(QString("INSERT INTO ctg VALUES(\"PGC\", \"ngcic.dat\", \"Principal Galaxies Catalogue\")"))) {
        kDebug() << query.lastError();
    }
    
    // Insert the UGC catalog
    if (!query.exec(QString("INSERT INTO ctg VALUES(\"UGC\", \"ngcic.dat\", \"Uppsala General Catalogue\")"))) {
        kDebug() << query.lastError();
    }

    return true;
}

void KStarsDB::migrateData(QString filename)
{
    QSqlQuery query(db), dsoquery(db), tmpq(db);
    KSFileReader fileReader;

    query.exec("SELECT rowid, * FROM dso");
    QString s, ra, dec;
    int k = 0;

    // Just some test query to show up the first 10 objects in the db
    while (query.next()) {
        s = "";

        for (int i = 0; i < 14; i++) {
            s += query.value(i).toString() + " ";
        }

        kDebug() << s;
        s = "Catalog object: ";

        dsoquery.prepare("SELECT designation, idCTG FROM od WHERE idDSO = :iddso");
        dsoquery.bindValue(":iddso", query.value(0).toString().toInt());
        dsoquery.exec();

        while (dsoquery.next()) {
            tmpq.prepare("SELECT name FROM ctg WHERE rowid = :idctg");
            tmpq.bindValue(":idctg", dsoquery.value(1).toString().toInt());
            tmpq.exec();
            if (tmpq.next()) {
                s += tmpq.value(0).toString() + " " + dsoquery.value(0).toString();
            }
        }

        kDebug() << s;
        if ( k++ == 10 )
            break;
    }

    if (!fileReader.open(filename)) return; 
    fileReader.setProgress(i18n("Migrating NGC/IC objects from ngcic.dat to kstars.db"), 13444, 10 );

    while (fileReader.hasMoreLines()) {
        QString line, con, ss, name, name2, longname;
        QString cat, cat2, sgn;
        float mag(1000.0), a, b;
        int type, ingc, imess(-1), pa; // , rah, ram, dd, dm, ds;
        int pgc, ugc;
        QChar iflag;

        line = fileReader.readLine();

        // Prepare the insert statement of the current object
        query.prepare("INSERT INTO dso VALUES(:ra, :dec, :sign, :bmag, :type, :pa, :minor, :major, :name)");

        //Ignore comment lines
        while ( line.at(0) == '#' && fileReader.hasMoreLines() ) line = fileReader.readLine();

        //Ignore lines with no coordinate values
        while ( line.mid(6,8).trimmed().isEmpty() && fileReader.hasMoreLines() ) {
            line = fileReader.readLine();
        }

        iflag = line.at( 0 ); //check for NGC/IC catalog flag
        if ( iflag == 'I' ) cat = "IC";
        else if ( iflag == 'N' ) cat = "NGC";

        ingc = line.mid( 1, 4 ).toInt();  // NGC/IC catalog number
        if ( ingc==0 ) cat.clear(); //object is not in NGC or IC catalogs

        /*
        // read the right ascension coordinates
        rah = line.mid( 6, 2 ).toInt();
        ram = line.mid( 8, 2 ).toInt();
        ras = line.mid( 10, 4 ).toFloat();
        */

        ra = line.mid( 6, 8 );

        /*
        // bind the values for right ascension params
        query.bindValue(":rah", rah);
        query.bindValue(":ram", ram);
        query.bindValue(":ras", ras);
        */

        query.bindValue(":ra", ra);

        // read the declination coordinates
        sgn = line.mid( 15, 1 ); //don't use at(), because it crashes on invalid index

        // bine the sign in the db query (maybe will not be used anymore)
        query.bindValue(":sign", (sgn == "-") ? -1:1);

        /*
        dd = line.mid( 16, 2 ).toInt();
        dm = line.mid( 18, 2 ).toInt();
        ds = line.mid( 20, 2 ).toInt();
        
        // bind the values for declination
        query.bindValue(":decd", dd);
        query.bindValue(":decm", dm);
        query.bindValue(":decs", ds);
        */

        dec = line.mid(16, 6);

        query.bindValue(":dec", dec);

        //B magnitude
        ss = line.mid( 23, 4 );
        if (ss == "    " ) { mag = 99.9f; } else { mag = ss.toFloat(); }

        query.bindValue(":bmag", mag);

        //object type
        type = line.mid( 28, 2 ).toInt();

        query.bindValue(":type", type);

        //major and minor axes
        ss = line.mid( 31, 5 );
        if (ss == "      " ) { a = 0.0; } else { a = ss.toFloat(); }
        ss = line.mid( 37, 5 );
        if (ss == "     " ) { b = 0.0; } else { b = ss.toFloat(); }

        query.bindValue(":minor", b);
        query.bindValue(":major", a);

        //position angle.  The catalog PA is zero when the Major axis
        //is horizontal.  But we want the angle measured from North, so
        //we set PA = 90 - pa.
        ss = line.mid( 43, 3 );
        if (ss == "   " ) { pa = 90; } else { pa = 90 - ss.toInt(); }

        query.bindValue(":pa", pa);

        //PGC number
        ss = line.mid( 47, 6 );
        if (ss == "      " ) { pgc = 0; } else { pgc = ss.toInt(); }

        //UGC number
        if ( line.mid( 54, 3 ) == "UGC" ) {
            ugc = line.mid( 58, 5 ).toInt();
        } else {
            ugc = 0;
        }

        //Messier number
        if ( line.mid( 70,1 ) == "M" ) {
            cat2 = cat;
            if ( ingc==0 ) cat2.clear();
            cat = 'M';
            imess = line.mid( 72, 3 ).toInt();
        }

        longname = line.mid( 76, line.length() ).trimmed();

        // Bind the name value
        query.bindValue(":name", longname);

        // Add the DSO Object to the database
        query.exec();


        /*
        // Get the rowid of the object newly inserted
        query.prepare("SELECT rowid FROM dso WHERE rah = :rah AND ram = :ram AND ras = :ras AND sgn = :sgn AND decd = :decd AND decm = :decm AND decs = :decs");
        */
        query.prepare("SELECT rowid, ra, dec FROM dso WHERE ra = :ra AND sgn = :sgn AND dec = :dec");

        /*
        query.bindValue(":rah", rah);
        query.bindValue(":ram", ram);
        query.bindValue(":ras", ras);
        query.bindValue(":sgn", (sgn == "-") ? (-1):1);
        query.bindValue(":decd", dd);
        query.bindValue(":decm", dm);
        query.bindValue(":decs", ds);
        */

        query.bindValue(":ra", ra);
        query.bindValue(":sgn", (sgn == "-") ? (-1):1);
        query.bindValue(":dec", dec);

        query.exec();

        int dsoRowID = -1;

        // Retrieve the row id of the newly inserted object
        while (query.next()) {
       //   kDebug() << query.value(0).toString() << query.value(1).toString() << query.value(2).toString();
            dsoRowID = query.value(0).toString().toInt();
        }
/*
        dms r;
        r.setH( rah, ram, int(ras) );
        dms d( dd, dm, ds );

        if ( sgn == "-" ) { d.setD( -1.0*d.Degrees() ); }
*/
        bool hasName = true;
        QString snum;


        /* Normal detection of the catalogs id (use it to first determine what ids are assigned to
         * catalogs, if you have not made them by yourself 
         * 
         * As for the default migration (using createDefaultDatabase method) the static numbers
         * defined below are sufficient and assigning them manually speeds up the process
         */

        int ngcRowID = 1, icRowID = 2, mRowID = 3, ugcRowID = 5, pgcRowID = 4;
        
        /*
        query.exec("SELECT rowid FROM ctg WHERE name = 'IC'");
        if (query.next()) {
            icRowID = query.value(0).toString().toInt();
        } else {
            qDebug() << "Unable to get IC catalog row id!";
        }
        qDebug() << "IC: " << icRowID;

        query.exec("SELECT rowid FROM ctg WHERE name = 'NGC'");
        if (query.next()) {
            ngcRowID = query.value(0).toString().toInt();
        } else {
            qDebug() << "Unable to get NGC catalog row id!";
        }
        qDebug() << "NGC: " << ngcRowID;
        
        query.exec("SELECT rowid FROM ctg WHERE name = 'M'");
        if (query.next())
            mRowID = query.value(0).toString().toInt();
        else {
            qDebug() << "Unable to retrive Messier catalog row id!";
        }
        qDebug() << "Messier: " << mRowID;
        */

        if ( cat=="IC" || cat=="NGC" ) {
            snum.setNum( ingc );
            name = cat + ' ' + snum;

            dsoquery.prepare("INSERT INTO od VALUES (:des, :idctg, :iddso)");

            if (cat == "IC") {
                dsoquery.bindValue(":idctg", icRowID);
            } else if (cat == "NGC") {
                dsoquery.bindValue(":idctg", ngcRowID);
            }

            dsoquery.bindValue(":des", ingc);
            dsoquery.bindValue(":iddso", dsoRowID);
            dsoquery.exec();

//          qDebug() << "NGC / IC error: " << dsoquery.lastError();
        } else if ( cat=="M" ) {
            snum.setNum( imess );
            name = cat + ' ' + snum;

            dsoquery.prepare("INSERT INTO od VALUES (:des, :idctg, :iddso)");

            dsoquery.bindValue(":des", imess);
            dsoquery.bindValue(":idctg", mRowID);
            dsoquery.bindValue(":iddso", dsoRowID);

            dsoquery.exec();
//          qDebug() << "Messier error: " << dsoquery.lastError();

            if ( cat2=="NGC" ) {
                snum.setNum( ingc );
                name2 = cat2 + ' ' + snum;

                dsoquery.prepare("INSERT INTO od VALUES (:des, :idctg, :iddso)");
                dsoquery.bindValue(":des", ingc);
                dsoquery.bindValue(":idctg", ngcRowID);
                dsoquery.bindValue(":iddso", dsoRowID);
                dsoquery.exec();
//              qDebug() << "NGC error: " << dsoquery.lastError();

            } else if ( cat2=="IC" ) {
                snum.setNum( ingc );
                name2 = cat2 + ' ' + snum;

                dsoquery.prepare("INSERT INTO od VALUES (:des, :idctg, :iddso)");
                dsoquery.bindValue(":des", ingc);
                dsoquery.bindValue(":idctg", icRowID);
                dsoquery.bindValue(":iddso", dsoRowID);
                dsoquery.exec();

//              qDebug() << "IC error: " << dsoquery.lastError();

            } else {
                name2.clear();
            }
        }
        else {
//          qDebug() << "No catalog assigned!";
            if ( ! longname.isEmpty() ) name = longname;
            else {
                hasName = false;
                name = i18n( "Unnamed Object" );
            }
        }

        if (ugc != 0) {
            dsoquery.prepare("INSERT INTO od VALUES (:des, :idctg, :iddso)");

            dsoquery.bindValue(":idctg", ugcRowID);

            dsoquery.bindValue(":des", ugc);
            dsoquery.bindValue(":iddso", dsoRowID);

            if (!dsoquery.exec())
                qDebug() << "UGC error: " << dsoquery.lastError();
        }

        if (pgc != 0) {
            dsoquery.prepare("INSERT INTO od VALUES (:des, :idctg, :iddso)");

            dsoquery.bindValue(":idctg", pgcRowID);

            dsoquery.bindValue(":des", pgc);
            dsoquery.bindValue(":iddso", dsoRowID);
            if (!dsoquery.exec())
                qDebug() << "PGC error: " << dsoquery.lastError();
        }
    }
}

void KStarsDB::migrateURLData(const QString &urlfile, int type) {
    QFile file;
    KStarsData *data = KStarsData::Instance();

    if (!data->openUrlFile(urlfile, file)) {
        return;
    }

    QTextStream stream(&file);

    QSqlQuery query(db);
    QString ctgname, objname;

    while ( !stream.atEnd() ) {
        QString line = stream.readLine();

        //ignore comment lines
        if ( !line.startsWith('#') ) {
            int idx = line.indexOf(':');
            QString name = line.left( idx );
            QString sub = line.mid( idx + 1 );
            idx = sub.indexOf(':');
            QString title = sub.left( idx );
            QString url = sub.mid( idx + 1 );

            idx = name.indexOf(' ');
            ctgname = name.left(idx);
            objname = name.mid(idx + 1);

            query.prepare("SELECT rowid FROM ctg WHERE name = :ctgname");
            query.bindValue(":ctgname", ctgname);
            query.exec();

            if (query.next()) {
                // get catalog id
                idx = query.value(0).toInt();

                query.prepare("SELECT idDSO FROM od WHERE idCTG = :ctgid AND designation = :design");
                query.bindValue(":ctgid", idx);
                query.bindValue(":design", objname);

                query.exec();

                if (query.next()) {
                    // Get the DSO id
                    idx = query.value(0).toInt();

                    // Insert the image into the DB
                    query.prepare("INSERT INTO dso_url VALUES(:idDSO, :title, :url, :type)");
                    query.bindValue(":idDSO", idx);
                    query.bindValue(":title", title);
                    query.bindValue(":url", url);
                    query.bindValue(":type", type);

                    if (!query.exec()) {
                        qDebug() << "Encountered error on insert the image information";
                    }
                }
            }
        }
    }

    file.close();
}

KStarsDB* KStarsDB::Create()
{
    delete pinstance;
    pinstance = new KStarsDB();
    return pinstance;
}

KStarsDB* KStarsDB::Instance()
{
    return pinstance;
}

SkyObject * KStarsDB::getObject(qlonglong id)
{
    kDebug() << "Works!" << id;

    if (objectHash.contains(id)) {
        kDebug() << "Found a hashed object!";
        return objectHash[id];
    }

    kDebug() << "Definfajdiwa";

    KStarsData *data = KStarsData::Instance();
    SkyObject *o = NULL;

    // Variables needed to load all the data
    QString line, con, ss, name[10], longname;
    QString cat[10];

    // Magnitude, Right Ascension (seconds), Semimajor and Semiminor axis
    float mag, ras, a, b;

    // RA Hours, Minutes, DSO Type, NGC Index, Messier Index
    int rah, ram, type;

    // Dec Degrees, Minutes, Seconds, Position Angle, Sign
    int dd, dm, ds, pa, sgn;

    // PGC Index, UGC Index, Catalogs number
    int pgc = 0, ugc = 0, k = 0;

    // Index flag, nameflag, counter
    QChar iflag; bool hasName;

    QSqlQuery query, dsoquery;

    QString queryStatement =  QString("SELECT o.ra, o.dec, ") +
                                QString("o.sgn, ") +
                                QString("o.bmag, o.type, o.pa, o.minor, o.major, ") +
                                QString("o.longname, o.rowid FROM dso AS o ") +
                                QString("WHERE o.rowid = ") + QString::number(id);

    if (!query.exec(queryStatement)) {
        kDebug() << "Deep Sky select statement error: " << query.lastError();
    }

    while ( query.next() ) {
        // Right Ascension
        ras = query.value(0).toString().mid(0, 2).toInt();
        ram = query.value(0).toString().mid(2, 2).toInt();
        rah = query.value(0).toString().mid(4, 4).toFloat();

        // Declination
        dd = query.value(1).toString().mid(0, 2).toInt();
        dm = query.value(1).toString().mid(2, 2).toInt();
        ds = query.value(1).toString().mid(4, 2).toInt();

        // Position Angle, Magnitude, Semimajor axis
        pa = query.value(5).toInt(); mag = query.value(3).toFloat(); a = query.value(7).toFloat(); b = query.value(6).toFloat();

        // Object type, SGN
        type = query.value(4).toInt(); sgn = query.value(2).toInt();


        // Inner join to retrieve all the catalogs in which the object appears
        dsoquery.prepare("SELECT od.designation, ctg.name FROM od INNER JOIN ctg ON od.idCTG = ctg.rowid WHERE od.idDSO = :iddso");
        dsoquery.bindValue(":iddso", query.value(9).toInt());

        if (!dsoquery.exec()) {
            qDebug() << "Error on retrieving the catalog list for an object: " << dsoquery.lastError();
        }

        // Parsing catalog information
        k = 0;

        while (dsoquery.next()) {
            name[k] = dsoquery.value(1).toString() + ' ' + dsoquery.value(0).toString();
            cat[k] = dsoquery.value(1).toString();
            k++;
        }

        hasName = true;
        longname = query.value(8).toString();

        if (!longname.isEmpty()) {
            if (name[0] == "") {
                name[0] = longname;
            }
        } else if (name[0] == "") {
            hasName = false;
            name[0] = "Unnamed Object";
        }

        dms r;
        r.setH( rah, ram, int(ras) );
        dms d( dd, dm, ds );

        if ( sgn == -1 ) { d.setD( -1.0*d.Degrees() ); }

        // Create new Deep Sky Object Instance
        if ( type==0 ) type = 1; //Make sure we use CATALOG_STAR, not STAR

        o = new DeepSkyObject( type, r, d, mag, name[0], name[1], longname, cat[0], a, b, pa, pgc, ugc );
        o->EquatorialToHorizontal( data->lst(), data->geo()->lat() );

        /*
        // Add the name(s) to the nameHash for fast lookup -jbb
        if (hasName == true) {
            // hash the default name
            nameHash[name[0].toLower()] = o;
            // hash the longname
            if (!longname.isEmpty())
                nameHash[longname.toLower()] = o;

            // hash all the other names (catalog designations) of the object
            for (i = 1; i < k; i++)
                if (!name[i].isEmpty())
                    nameHash[name[i].toLower()] = o;
        }
        */

        /*
        Trixel trixel = m_skyMesh->index( (SkyPoint*) o );

        // Assign object to general DeepSkyObjects list,
        // and a secondary list based on its catalog.
        m_DeepSkyList.append( o );
        appendIndex( o, &m_DeepSkyIndex, trixel );
        */
        /*
        if ( o->isCatalogM()) {
            m_MessierList.append( o );
            appendIndex( o, &m_MessierIndex, trixel );
        }
        else if (o->isCatalogNGC() ) {
            m_NGCList.append( o );
            appendIndex( o, &m_NGCIndex, trixel );
        }
        else if ( o->isCatalogIC() ) {
            m_ICList.append( o );
            appendIndex( o, &m_ICIndex, trixel );
        }
        else {
            m_OtherList.append( o );
            appendIndex( o, &m_OtherIndex, trixel );
        }

        // Add name to the list of object names
        if (!name[0].isEmpty())
            objectNames(type).append(name[0]);

        // Add longname to the list of object names
        if (!longname.isEmpty() && longname != name)
            objectNames(type).append(longname);
        */

        // Load the images associated to the deep sky object (this was done in KStarsData::readUrlData)
        dsoquery.prepare("SELECT url, title, type FROM dso_url WHERE idDSO = :iddso");
        dsoquery.bindValue(":iddso", query.value(9).toInt());

        if (!dsoquery.exec()) {
            qDebug() << "URL query error: " << dsoquery.lastError();
        } else {
            while (dsoquery.next()) {
                switch (dsoquery.value(2).toInt()) {
                    case IMG_URL:
                        o->ImageList().append(dsoquery.value(0).toString());
                        o->ImageTitle().append(dsoquery.value(1).toString());
                        break;

                    case INFO_URL:
                        o->InfoList().append(dsoquery.value(0).toString());
                        o->InfoTitle().append(dsoquery.value(1).toString());
                        break;
                }
            }
        }

        kDebug() << "GETTTING HERE!";
        kDebug() << query.lastError() << dsoquery.lastError();
        objectHash[id] = o;
        kDebug() << "GeTJIS";
        return o;
    }

    return 0;
}

qlonglong KStarsDB::getSkyObjectIDByName(QString searchedName)
{
    QSqlQuery db;

    // By default, just display all the objects, mixing all the catalogs
    QString subQuery = "(SELECT group_concat(ctg.name || \" \" || od.designation) FROM od " + 
       QString("INNER JOIN ctg ON od.idCTG = ctg.rowid WHERE od.idDSO = dso.rowid) AS Designations");
    
    QString searchQuery = "SELECT " + subQuery + ", dso.longname AS Name, dso.rowid FROM dso " + 
       QString("WHERE Designations LIKE \'%" + searchedName + "%\' OR Name LIKE \'%" + searchedName + "%\') ");

    db.exec(searchQuery);

    while (db.next()) {
        return db.value(2).toLongLong();
    }

    return -1;
}
