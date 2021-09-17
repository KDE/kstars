/*
    SPDX-FileCopyrightText: 2021 Valentin Boettcher <hiro at protagon.space; @hiro98:tchncs.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef CATALOGCSVIMPORT_H
#define CATALOGCSVIMPORT_H

#include <QDialog>
#include <QString>
#include <QComboBox>
#include <QFile>
#include <unordered_map>
#include <rapidcsv.h>
#include "catalogobject.h"
#include "skyobject.h"
#include "catalogobjectlistmodel.h"
#include "qstring_hash.h"

namespace Ui
{
class CatalogCSVImport;
}

/**
 * Custom Conversion Logic
 *
 * This requires some template magic to give the parser information
 * wether to use degrees or hours.
 */
enum coord_unit
{
    deg   = 0,
    hours = 1,
};

template <coord_unit unit_p>
struct typed_dms
{
    dms data;
};

namespace rapidcsv
{
template <>
inline void
Converter<typed_dms<coord_unit::deg>>::ToVal(const std::string &pStr,
                                             typed_dms<coord_unit::deg> &pVal) const
{
    if (!pVal.data.setFromString(pStr.c_str(), true))
        throw std::exception();
}

template <>
inline void
Converter<typed_dms<coord_unit::hours>>::ToVal(const std::string &pStr,
                                               typed_dms<coord_unit::hours> &pVal) const
{
    if (!pVal.data.setFromString(pStr.c_str(), false))
        throw std::exception();
}

template <>
inline void Converter<QString>::ToVal(const std::string &pStr, QString &pVal) const
{
    pVal = QString(pStr.c_str());
}
} // namespace rapidcsv

class CatalogCSVImport : public QDialog
{
    Q_OBJECT

  public:
    explicit CatalogCSVImport(QWidget *parent = nullptr);
    ~CatalogCSVImport();

    /**
     * Maps a string to a `SkyObject::TYPE`.
     */
    using type_map = std::unordered_map<std::string, SkyObject::TYPE>;

    /**
     * Maps a field in the Catalog object to a column in the CSV.
     *
     * The first element of the tuple can take on the following values:
     *     - -2 :: Default initialize this value.
     *     - -1 :: Take the value in the second part of the string for every row.
     *     - everything else :: Read from the column indicated by the integer value.
     */
    using column_pair = std::pair<int, QString>;
    using column_map  = std::unordered_map<QString, column_pair>;

    inline const std::vector<CatalogObject> &get_objects() const { return m_objects; };
  private slots:
    /** Selects a CSV file and opens it. Calls `init_mapping_selectors`. */
    void select_file();

    /** Reads the header of the CSV and initializes the mapping selectors. */
    void init_mapping_selectors();

    /** Add a row to the type table. */
    void type_table_add_map();

    /** Remove the selected row from the type table. */
    void type_table_remove_map();

    /** Read all the objects from the csv */
    inline void read_objects() { read_n_objects(); };

  private:
    void init_column_mapping();
    void init_type_table();
    type_map get_type_mapping();
    column_map get_column_mapping();
    void read_n_objects(size_t n = std::numeric_limits<int>::max());

    // Parsing
    SkyObject::TYPE parse_type(const std::string &type, const type_map &type_map);

    template <typename T>
    T cell_or_default(const column_pair &config, const size_t row, const T &default_val)
    {
        if (config.first < 0)
            return default_val;

        return m_doc.GetCell<T>(config.first, row);
    };

    template <typename T>
    T get_default(const column_pair &config, const T &default_val)
    {
        if (config.first != -1)
            return default_val;

        rapidcsv::Converter<T> converter(rapidcsv::ConverterParams{});

        T res{};
        converter.ToVal(config.second.toStdString(), res);

        return res;
    }

    Ui::CatalogCSVImport *ui;

    /** Disables the mapping selection and resets everything. */
    void reset_mapping();

    /** Maps the fields to a selector widget. */
    std::map<QString, QComboBox *> m_selectors{};

    /** Rapidcsv Document */
    rapidcsv::Document m_doc{};

    /** The Parsed Objects */
    std::vector<CatalogObject> m_objects;

    /** The model to preview the import */
    CatalogObjectListModel m_preview_model;

    static const char default_separator = ',';
    static const char default_comment     = '#';
    static const int default_preview_size = 10;
};

#endif // CATALOGCSVIMPORT_H
