/*
    SPDX-FileCopyrightText: 2021 Valentin Boettcher <hiro at protagon.space; @hiro98:tchncs.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "import_skycomp.h"

QString db_path()
{
    return QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation))
           .filePath("skycomponents.sqlite");
}

std::pair<bool, QSqlDatabase> SkyComponentsImport::get_skycomp_db(const QString &path)
{
    if (!QFileInfo(path).exists())
        return { false, {} };

    auto db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(path);

    if (!db.open())
        return { false, {} };

    return { true, db };
}

std::pair<bool, QSqlDatabase> SkyComponentsImport::get_skycomp_db()
{
    return get_skycomp_db(db_path());
}

std::tuple<bool, QString, CatalogsDB::CatalogObjectVector>
SkyComponentsImport::get_objects(QSqlDatabase db, const std::list<int> &ids)
{
    QVector<QString> const placeholders(ids.size(), "?");
    if (!db.open())
    {
        qDebug() << "here";
        return { false, {}, {} };
    }

    QSqlQuery query{ db };
    query.prepare(
        QString("SELECT UID, RA, Dec, Type, Magnitude, PositionAngle, MajorAxis, "
                "MinorAxis, "
                "Flux, LongName, id_Catalog FROM DSO INNER JOIN ObjectDesignation ON "
                "DSO.UID "
                "= "
                "ObjectDesignation.UID_DSO WHERE id_Catalog IN (%1)")
        .arg(QStringList::fromVector(placeholders).join(", ")));

    for (auto const &i : ids)
        query.addBindValue(i);

    query.setForwardOnly(true);

    if (!query.exec())
        return { false, query.lastError().text(), {} };

    CatalogsDB::CatalogObjectVector objs{};
    while (query.next())
    {
        const SkyObject::TYPE type = static_cast<SkyObject::TYPE>(query.value(3).toInt());

        const double ra         = query.value(1).toDouble();
        const double dec        = query.value(2).toDouble();
        const float mag         = query.isNull(4) ? NaN::f : query.value(4).toFloat();
        const QString name      = query.value(9).toString();
        const QString long_name = "";
        const QString catalog_identifier = "";
        const float major                = query.value(6).toFloat();
        const float minor                = query.value(7).toFloat();
        const double position_angle      = query.value(5).toDouble();
        const float flux                 = query.value(8).toFloat();
        const int catalog_id             = 0;

        objs.emplace_back(CatalogObject::oid{}, type, dms(ra), dms(dec), mag, name,
                          long_name, catalog_identifier, catalog_id, major, minor,
                          position_angle, flux, "");
    }

    db.close();
    return { true, {}, objs };
};
