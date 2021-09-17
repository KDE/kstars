/*
    SPDX-FileCopyrightText: 2008 Akarsh Simha <akarshsimha@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "binfilehelper.h"

#include "byteorder.h"
#include "auxiliary/kspaths.h"

#include <QStandardPaths>

class BinFileHelper;

BinFileHelper::BinFileHelper()
{
    init();
}

BinFileHelper::~BinFileHelper()
{
    qDeleteAll(fields);
    fields.clear();
    if (fileHandle)
        closeFile();
}

void BinFileHelper::init()
{
    if (fileHandle)
        fclose(fileHandle);

    fileHandle      = nullptr;
    indexUpdated    = false;
    FDUpdated       = false;
    RSUpdated       = false;
    preambleUpdated = false;
    byteswap        = false;
    errnum          = ERR_NULL;
    recordCount     = 0;
    recordSize      = 0;
    nfields         = 0;
    indexSize       = 0;
    itableOffset    = 0;
    dataOffset      = 0;
    recordCount     = 0;
    versionNumber   = 0;
}

void BinFileHelper::clearFields()
{
    qDeleteAll(fields);
    fields.clear();
}

bool BinFileHelper::testFileExists(const QString &fileName)
{
    QString FilePath     = KSPaths::locate(QStandardPaths::AppDataLocation, fileName);
    QByteArray b         = FilePath.toLatin1();
    const char *filepath = b.data();
    FILE *f              = fopen(filepath, "rb");
    if (f)
    {
        fclose(f);
        return true;
    }
    else
        return false;
}

FILE *BinFileHelper::openFile(const QString &fileName)
{
    QString FilePath = KSPaths::locate(QStandardPaths::AppDataLocation, fileName);
    init();
    QByteArray b         = FilePath.toLatin1();
    const char *filepath = b.data();

    fileHandle = fopen(filepath, "rb");

    if (!fileHandle)
    {
        errnum = ERR_FILEOPEN;
        return nullptr;
    }
    return fileHandle;
}

enum BinFileHelper::Errors BinFileHelper::__readHeader()
{
    qint16 endian_id, i;
    char ASCII_text[125];
    int ret = 0;

    // Read the preamble
    if (!fileHandle)
        return ERR_FILEOPEN;

    rewind(fileHandle);

    // Read the first 124 bytes of the binary file which contains a general text about the binary data.
    // e.g. "KStars Star Data v1.0. To be read using the 32-bit StarData structure only"
    ret = fread(ASCII_text, 124, 1, fileHandle); // cppcheck-suppress redundantAssignment
    ASCII_text[124] = '\0';
    headerText      = ASCII_text;

    // Find out endianess from reading "KS" 0x4B53 in the binary file which was encoded on a little endian machine
    // Therefore, in the binary file it is written as 53 4B (little endian as least significant byte is stored first),
    // and when read on a little endian machine then it results in 0x4B53 (least significant byte is stored first in memory),
    // whereas a big endian machine would read it as 0x534B (most significant byte is stored first in memory).
    ret = fread(&endian_id, 2, 1, fileHandle); // cppcheck-suppress redundantAssignment
    if (endian_id != 0x4B53)
        byteswap = 1;
    else
        byteswap = 0;

    ret = fread(&versionNumber, 1, 1, fileHandle); // cppcheck-suppress redundantAssignment

    preambleUpdated = true;
    // Read the field descriptor table
    ret = fread(&nfields, 2, 1, fileHandle); // cppcheck-suppress redundantAssignment
    if (byteswap)
        nfields = bswap_16(nfields);

    qDeleteAll(fields);
    fields.clear();
    for (i = 0; i < nfields; ++i)
    {
        dataElement *de = new dataElement;

        // Read 16 byte dataElement that describe each field (name[8], size[1], type[1], scale[4])
        if (!fread(de, sizeof(dataElement), 1, fileHandle))
        {
            delete de;
            qDeleteAll(fields);
            fields.clear();
            return ERR_FD_TRUNC;
        }
        if (byteswap)
            de->scale = bswap_32(de->scale);
        fields.append(de);
    }

    if (!RSUpdated)
    {
        recordSize = 0;
        for (i = 0; i < fields.size(); ++i)
            recordSize += fields[i]->size;
        RSUpdated = true;
    }

    FDUpdated = true;

    // Read the index table
    ret = fread(&indexSize, 4, 1, fileHandle); // cppcheck-suppress redundantAssignment
    if (byteswap)
        indexSize = bswap_32(indexSize);

    quint32 j;
    quint32 ID;
    quint32 offset;
    quint32 prev_offset;
    quint32 nrecs;
    quint32 prev_nrecs;

    // Find out current offset so far in the binary file (how many bytes we read so far)
    itableOffset = ftell(fileHandle);

    prev_offset = 0;
    prev_nrecs  = 0;
    recordCount = 0;

    indexCount.clear();
    indexOffset.clear();

    if (indexSize == 0)
    {
        errorMessage = QStringLiteral("Zero index size!");
        return ERR_INDEX_TRUNC;
    }

    // We read each 12-byte index entry (ID[4], Offset[4] within file in bytes, nrec[4] # of Records).
    // After reading all the indexes, we are (itableOffset + indexSize * 12) bytes within the file
    // indexSize is usually the size of the HTM level (eg. HTM level 3 --> 512)
    for (j = 0; j < indexSize; ++j)
    {
        if (!fread(&ID, 4, 1, fileHandle))
        {
            errorMessage = QStringLiteral("Table truncated before expected! Read i = %1 index entries so far").arg(QString::number(j));
            return ERR_INDEX_TRUNC;
        }

        if (byteswap)
            ID = bswap_32(ID);

        if (ID >= indexSize)
        {
            errorMessage = QString::asprintf("ID %u is greater than the expected number of expected entries (%u)", ID, indexSize);
            return ERR_INDEX_BADID;
        }

        if (ID != j)
        {
            errorMessage = QString::asprintf("Found ID %u, at the location where ID %u was expected", ID, j);
            return ERR_INDEX_IDMISMATCH;
        }

        if (!fread(&offset, 4, 1, fileHandle))
        {
            errorMessage = QString::asprintf("Table truncated before expected! Read i = %d index entries so far", j);
            return ERR_BADSEEK;
        }

        if (byteswap)
            offset = bswap_32(offset);

        if (!fread(&nrecs, 4, 1, fileHandle))
        {
            errorMessage = QString::asprintf("Table truncated before expected! Read i = %d index entries so far", j);
            return ERR_BADSEEK;
        }

        if (byteswap)
            nrecs = bswap_32(nrecs);

        //if (prev_offset != 0 && prev_nrecs != (-prev_offset + offset) / recordSize)
        if (prev_offset != 0 && prev_nrecs != (offset - prev_offset) / recordSize)
        {
            errorMessage = QString::asprintf("Expected %u  = (%X - %x) / %x records, but found %u, in index entry %u",
                                 (offset - prev_offset) / recordSize, offset, prev_offset, recordSize, prev_nrecs,
                                 j - 1);
            return ERR_INDEX_BADOFFSET;
        }

        indexOffset.append(offset);
        indexCount.append(nrecs);

        recordCount += nrecs;
        prev_offset = offset;
        prev_nrecs  = nrecs;
    }

    dataOffset = ftell(fileHandle);

    indexUpdated = true;

    return ERR_NULL;
}

bool BinFileHelper::readHeader()
{
    switch ((errnum = __readHeader()))
    {
        case ERR_NULL:
            return true;
            break;
        case ERR_FILEOPEN:
            return false;
            break;
        case ERR_FD_TRUNC:
            clearFields();
            break;
        case ERR_INDEX_TRUNC:
        case ERR_INDEX_BADID:
        case ERR_INDEX_IDMISMATCH:
        case ERR_BADSEEK:
        case ERR_INDEX_BADOFFSET:
        {
            indexOffset.clear();
            indexCount.clear();
        }
    }
    return false;
}

void BinFileHelper::closeFile()
{
    fclose(fileHandle);
    fileHandle = nullptr;
}

int BinFileHelper::getErrorNumber()
{
    int err = errnum;
    errnum  = ERR_NULL;
    return err;
}

QString BinFileHelper::getError()
{
    QString erm  = errorMessage;
    errorMessage = "";
    return erm;
}

struct dataElement BinFileHelper::getField(const QString &fieldName) const
{
    dataElement de;

    for (auto &field : fields)
    {
        if (field->name == fieldName)
        {
            de = *field;
            return de;
        }
    }
    return de; // Returns junk!
}

bool BinFileHelper::isField(const QString &fieldName) const
{
    for (auto &field : fields)
    {
        if (field->name == fieldName)
            return true;
    }
    return false;
}

int BinFileHelper::unsigned_KDE_fseek(FILE *stream, quint32 offset, int whence)
{
    Q_ASSERT(stream);
    int ret = 0;
    if (offset <= ((quint32)1 << 31) - 1)
    {
        ret = fseek(stream, offset, whence);
    }
    else
    {
        // Do the fseek in two steps
        ret = fseek(stream, ((quint32)1 << 31) - 1, whence);
        if (!ret)
            ret = fseek(stream, offset - ((quint32)1 << 31) + 1, SEEK_CUR);
    }
    return ret;
}
