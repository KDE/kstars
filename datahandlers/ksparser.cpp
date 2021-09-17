/*
    SPDX-FileCopyrightText: 2012 Rishab Arora <ra.rishab@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ksparser.h"

#include <QDebug>

const int KSParser::EBROKEN_INT         = 0;
const double KSParser::EBROKEN_DOUBLE   = 0.0;
const float KSParser::EBROKEN_FLOAT     = 0.0;
const QString KSParser::EBROKEN_QSTRING = "Null";
const bool KSParser::parser_debug_mode_ = false;

KSParser::KSParser(const QString &filename, const char comment_char, const QList<QPair<QString, DataTypes>> &sequence,
                   const char delimiter)
    : filename_(filename), comment_char_(comment_char), name_type_sequence_(sequence), delimiter_(delimiter)
{
    if (!file_reader_.openFullPath(filename_))
    {
        qWarning() << "Unable to open file: " << filename;
        readFunctionPtr = &KSParser::DummyRow;
    }
    else
    {
        readFunctionPtr = &KSParser::ReadCSVRow;
        qDebug() << "File opened: " << filename;
    }
}

KSParser::KSParser(const QString &filename, const char comment_char, const QList<QPair<QString, DataTypes>> &sequence,
                   const QList<int> &widths)
    : filename_(filename), comment_char_(comment_char), name_type_sequence_(sequence), width_sequence_(widths)
{
    if (!file_reader_.openFullPath(filename_))
    {
        qWarning() << "Unable to open file: " << filename;
        readFunctionPtr = &KSParser::DummyRow;
    }
    else
    {
        readFunctionPtr = &KSParser::ReadFixedWidthRow;
        qDebug() << "File opened: " << filename;
    }
}

QHash<QString, QVariant> KSParser::ReadNextRow()
{
    return (this->*readFunctionPtr)();
}

QHash<QString, QVariant> KSParser::ReadCSVRow()
{
    /**
     * @brief read_success(bool) signifies if a row has been successfully read.
     * If any problem (eg incomplete row) is encountered. The row is discarded
     * and the while loop continues till it finds a good row or the file ends.
     **/
    bool read_success = false;
    QString next_line;
    QStringList separated;
    QHash<QString, QVariant> newRow;

    while (file_reader_.hasMoreLines() && read_success == false)
    {
        next_line = file_reader_.readLine();
        if (next_line.mid(0, 1)[0] == comment_char_)
            continue;
        separated = next_line.split(delimiter_);
        /*
            * 1) split along delimiter eg. comma (,)
            * 2) check first and last characters.
            *    if the first letter is  '"',
            *    then combine the nexto ones in it till
            *    till you come across the next word which
            *    has the last character as '"'
            *    (CombineQuoteParts
            *
        */
        if (separated.length() == 1)
            continue; // Length will be 1 if there
        // is no delimiter

        separated = CombineQuoteParts(separated); // At this point, the
        // string has been split
        // taking the quote marks into account

        // Check if the generated list has correct size
        // If not, continue to next row. (i.e SKIP INCOMPLETE ROW)
        if (separated.length() != name_type_sequence_.length())
            continue;

        for (int i = 0; i < name_type_sequence_.length(); i++)
        {
            bool ok;
            newRow[name_type_sequence_[i].first] = ConvertToQVariant(separated[i], name_type_sequence_[i].second, ok);
            if (!ok && parser_debug_mode_)
            {
                qDebug() << name_type_sequence_[i].second << "Failed at field: " << name_type_sequence_[i].first
                         << " & next_line : " << next_line;
            }
        }
        read_success = true;
    }
    /*
     * This signifies that someone tried to read a row
     * without checking if HasNextRow is true.
     * OR
     * The file was truncated OR the file ends with one or more '\n'
     */
    if (file_reader_.hasMoreLines() == false && newRow.size() <= 1)
        newRow = DummyRow();
    return newRow;
}

QHash<QString, QVariant> KSParser::ReadFixedWidthRow()
{
    if (name_type_sequence_.length() != (width_sequence_.length() + 1))
    {
        // line length is appendeded to width_sequence_ by default.
        // Hence, the length of width_sequence_ is one less than
        // name_type_sequence_
        qWarning() << "Unequal fields and widths! Returning dummy row!";
        Q_ASSERT(false); // Make sure that in Debug mode, this condition generates an abort.
        return DummyRow();
    }

    /**
    * @brief read_success (bool) signifies if a row has been successfully read.
    * If any problem (eg incomplete row) is encountered. The row is discarded
    * and the while loop continues till it finds a good row or the file ends.
    **/
    bool read_success = false;
    QString next_line;
    QStringList separated;
    QHash<QString, QVariant> newRow;
    int total_min_length = 0;

    foreach (const int width_value, width_sequence_)
    {
        total_min_length += width_value;
    }
    while (file_reader_.hasMoreLines() && read_success == false)
    {
        /*
         * Steps:
         * 1) Read Line
         * 2) If it is a comment, loop again
         * 3) If it is too small, loop again
         * 4) Else, a) Break it down according to widths
         *          b) Convert each broken down unit to appropriate value
         *          c) set read_success to True denoting we have a valid
         *             conversion
        */
        next_line = file_reader_.readLine();
        if (next_line.mid(0, 1)[0] == comment_char_)
            continue;
        if (next_line.length() < total_min_length)
            continue;

        int curr_width = 0;
        for (int split : width_sequence_)
        {
            // Build separated stringlist. Then assign it afterwards.
            QString temp_split;

            temp_split = next_line.mid(curr_width, split);
            // Don't use at(), because it crashes on invalid index
            curr_width += split;
            separated.append(temp_split.trimmed());
        }
        separated.append(next_line.mid(curr_width).trimmed()); // Append last segment

        // Conversions
        for (int i = 0; i < name_type_sequence_.length(); ++i)
        {
            bool ok;
            newRow[name_type_sequence_[i].first] = ConvertToQVariant(separated[i], name_type_sequence_[i].second, ok);
            if (!ok && parser_debug_mode_)
            {
                qDebug() << name_type_sequence_[i].second << "Failed at field: " << name_type_sequence_[i].first
                         << " & next_line : " << next_line;
            }
        }
        read_success = true;
    }
    /*
     * This signifies that someone tried to read a row
     * without checking if HasNextRow is true.
     * OR
     * The file was truncated OR the file ends with one or more '\n'
     */
    if (file_reader_.hasMoreLines() == false && newRow.size() <= 1)
        newRow = DummyRow();
    return newRow;
}

QHash<QString, QVariant> KSParser::DummyRow()
{
    // qWarning() << "File named " << filename_ << " encountered an error while reading";
    QHash<QString, QVariant> newRow;

    for (auto &item : name_type_sequence_)
    {
        switch (item.second)
        {
            case D_QSTRING:
                newRow[item.first] = EBROKEN_QSTRING;
                break;
            case D_DOUBLE:
                newRow[item.first] = EBROKEN_DOUBLE;
                break;
            case D_INT:
                newRow[item.first] = EBROKEN_INT;
                break;
            case D_FLOAT:
                newRow[item.first] = EBROKEN_FLOAT;
                break;
            case D_SKIP:
            default:
                break;
        }
    }
    return newRow;
}

bool KSParser::HasNextRow()
{
    return file_reader_.hasMoreLines();
}

void KSParser::SetProgress(QString msg, int total_lines, int step_size)
{
    file_reader_.setProgress(msg, total_lines, step_size);
}

void KSParser::ShowProgress()
{
    file_reader_.showProgress();
}

QList<QString> KSParser::CombineQuoteParts(QList<QString> &separated)
{
    QString iter_string;
    QList<QString> quoteCombined;
    QStringList::const_iterator iter;

    if (separated.length() == 0)
    {
        qDebug() << "Cannot Combine empty list";
    }
    else
    {
        /* Algorithm:
         * In the following steps, 'word' implies a unit from 'separated'.
         * i.e. separated[0], separated[1] etc are 'words'
         *
         * 1) Read a word
         * 2) If word does not start with \" add to final expression. Goto 1)
         * 3) If word starts with \", push to queue
         * 4) If word ends with \", empty queue and join each with delimiter.
         *    Add this to final expression. Go to 6)
         * 5) Read next word. Goto 3) until end of list of words is reached
         * 6) Goto 1) until end of list of words is reached
        */
        iter = separated.constBegin();

        while (iter != separated.constEnd())
        {
            QList<QString> queue;
            iter_string = *iter;

            if (iter_string.indexOf("\"") == 0) // if (quote mark is the first character)
            {
                iter_string = (iter_string).remove(0, 1); // remove the quote at the start
                while (iter_string.lastIndexOf('\"') != (iter_string.length() - 1) &&
                       iter != separated.constEnd()) // handle stuff between parent quotes
                {
                    queue.append((iter_string));
                    ++iter;
                    iter_string = *iter;
                }
                iter_string.chop(1); // remove the quote at the end
                queue.append(iter_string);
            }
            else
            {
                queue.append(iter_string);
            }

            QString col_result;
            foreach (const QString &join, queue)
                col_result += (join + delimiter_);
            col_result.chop(1); // remove extra delimiter
            quoteCombined.append(col_result);
            ++iter;
        }
    }
    return quoteCombined;
}

QVariant KSParser::ConvertToQVariant(const QString &input_string, const KSParser::DataTypes &data_type, bool &ok)
{
    ok = true;
    QVariant converted_object;
    switch (data_type)
    {
        case D_QSTRING:
        case D_SKIP:
            converted_object = input_string;
            break;
        case D_DOUBLE:
            converted_object = input_string.trimmed().toDouble(&ok);
            if (!ok)
                converted_object = EBROKEN_DOUBLE;
            break;
        case D_INT:
            converted_object = input_string.trimmed().toInt(&ok);
            if (!ok)
                converted_object = EBROKEN_INT;
            break;
        case D_FLOAT:
            converted_object = input_string.trimmed().toFloat(&ok);
            if (!ok)
                converted_object = EBROKEN_FLOAT;
            break;
    }
    return converted_object;
}
