/*
    SPDX-FileCopyrightText: 2012 Rishab Arora <ra.rishab@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QHash>
#include <QList>
#include <QVariant>

#include "ksfilereader.h"

/**
 * @brief Generic class for text file parsers used in KStars.
 * Read rows using ReadCSVRow() regardless of the type of parser.
 * Usage:
 * 1) initialize KSParser
 * 2) while (KSParserObject.HasNextRow()) {
 *      QHash < Qstring, QVariant > read_stuff = KSParserObject.ReadNextRow();
 *      ...
 *      do what you need to do...
 *      ...
 *    }
 *
 * Debugging Information:
 * In case of read errors, the parsers emit a warning.
 * In case of conversion errors, the warnings are toggled by setting
 * const bool KSParser::parser_debug_mode_ = true; in ksparser.cpp
 *
 * In case of failure, the parser returns a Dummy Row. So if you see the
 * string "Null" in the returned QHash, it signifies the parserencountered an
 * unexpected error.
 **/
class KSParser
{
  public:
    /**
     * These are the values used in case of error in conversion
     **/
    static const double EBROKEN_DOUBLE;
    static const float EBROKEN_FLOAT;
    static const int EBROKEN_INT;
    static const QString EBROKEN_QSTRING;

    /**
     * @brief DataTypes for building sequence
     * D_QSTRING QString Type
     * D_INT Integer  Type
     * D_FLOAT Floating Point Type
     * D_DOUBLE Double PRecision Type
     * D_SKIP Unused Field. This string is not converted from QString
     **/
    enum DataTypes
    {
        D_QSTRING,
        D_INT,
        D_FLOAT,
        D_DOUBLE,
        D_SKIP
    };

    /**
     * @brief Returns a CSV parsing instance of a KSParser type object.
     *
     * Behavior:
     * 1) In case of attempt to read a non existent file, will return
     *    dummy row (not empty)
     * 2) In case of incomplete row, the whole row is ignored
     * 3) In case of missing values, parser will return empty string,
     *   or 0 or 0.0
     * 4) If you keep reading ignoring the HasNextRow you get dummy rows
     *
     * @param filename Full Path (Dir + Filename) of source file
     * @param comment_char Character signifying a comment line
     * @param sequence QList of QPairs of the form "field name,data type"
     * @param delimiter separate on which character. default ','
     **/
    KSParser(const QString &filename, const char comment_char, const QList<QPair<QString, DataTypes>> &sequence,
             const char delimiter = ',');

    /**
     * @brief Returns a Fixed Width parsing instance of a KSParser type object.
     *
     * Usage:
     * Important! The last value in width sequence is taken till the end of
     * line by default. This is done as the last line may or may not be padded
     * on the right.
     * Important! For backward compatibility, all string outputs are not
     * trimmed. Hence reading "hello  " will return "hello   " _not_ "hello"
     * If you need trimmed values like "hello" , use QString.trimmed()
     *
     * Behavior:
     * 1) In case of attempt to read a non existent file, will return
     *    dummy row (not empty!)
     * 2) In case of missing fields at the end, the line length is smaller
     *    than expected so it is skipped.
     * 3) If an integer or floating point value is empty (e.g. "    ")
     *    it is replaced by 0 or 0.0
     * 4) If you keep reading the file ignoring the HasNextRow(), you get
     *    Dummy Rows
     * @param filename Full Path (Dir + Filename) of source file
     * @param comment_char Character signifying a comment line
     * @param sequence QList of QPairs of the form "field name,data type"
     * @param widths width sequence. Last value is line.length() by default
     *               Hence, sequence.length() should be (width.length()+1)
     **/
    KSParser(const QString &filename, const char comment_char, const QList<QPair<QString, DataTypes>> &sequence,
             const QList<int> &widths);

    /**
     * @brief Generic function used to read the next row of a text file.
     * The constructor changes the function pointer to the appropriate function.
     * Returns the row as <"column name", value>
     *
     * @return QHash< QString, QVariant >
     **/
    QHash<QString, QVariant> ReadNextRow();

    /**
     * @brief Returns True if there are more rows to be read
     *
     * @return bool
     **/
    bool HasNextRow();
    // Too many warnings when const: datahandlers/ksparser.h:131:27: warning:
    // type qualifiers ignored on function return type [-Wignored-qualifiers]

    /**
     * @brief Wrapper function for KSFileReader setProgress
     *
     * @param msg What message to display
     * @param total_lines Total number of lines in file
     * @param step_size Size of step in emitting progress
     * @return void
     **/
    void SetProgress(QString msg, int total_lines, int step_size);

    /**
     * @brief Wrapper function for KSFileReader showProgress
     *
     * @return void
     **/
    void ShowProgress();

  private:
    /**
     * @brief Function Pointer used by ReadNextRow
     * to call the appropriate function among ReadCSVRow and ReadFixedWidthRow
     *
     * @return QHash< QString, QVariant >
     **/
    QHash<QString, QVariant> (KSParser::*readFunctionPtr)();

    /**
     * @brief Returns a single row from CSV.
     * If HasNextRow is false, returns a row with default values.
     *
     * @return QHash< QString, QVariant >
     **/
    QHash<QString, QVariant> ReadCSVRow();

    /**
     * @brief Returns a single row from Fixed Width File.
     * If HasNextRow is false, returns a row with default values.
     *
     * @return QHash< QString, QVariant >
     **/
    QHash<QString, QVariant> ReadFixedWidthRow();

    /**
     * @brief Returns a default value row.
     * Values are according to the current assigned sequence.
     *
     * @return QHash< QString, QVariant >
     **/
    QHash<QString, QVariant> DummyRow();

    /**
     * @brief This function combines the separated QString in case of quotes
     * The function can not handle stray quote marks.
     * eg. hello,"",world is acceptable
     *     hello"",world is not
     *
     * @param separated a list of QStrings separated at every delimiter
     * @return QList< QString >
     **/
    QList<QString> CombineQuoteParts(QList<QString> &separated);

    /**
     * @brief Function to return a QVariant of selected data type
     *
     * @param input_string QString of what the object should contain
     * @param data_type Data Type of input_string
     * @return QVariant
     **/
    QVariant ConvertToQVariant(const QString &input_string, const DataTypes &data_type, bool &ok);

    static const bool parser_debug_mode_;

    KSFileReader file_reader_;
    QString filename_;
    char comment_char_;

    QList<QPair<QString, DataTypes>> name_type_sequence_;
    QList<int> width_sequence_;
    char delimiter_ { 0 };
};
