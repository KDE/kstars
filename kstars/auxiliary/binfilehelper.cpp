
/***************************************************************************
                   binfilehelper.cpp - K Desktop Planetarium
                             -------------------
    begin                : Sat May 31 2008
    copyright            : (C) 2008 by Akarsh Simha
    email                : akarshsimha@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "binfilehelper.h"

#include <QStandardPaths>
#include "byteorder.h"
#include "auxiliary/kspaths.h"

class BinFileHelper;

BinFileHelper::BinFileHelper() {
    fileHandle = NULL;
    init();
}

BinFileHelper::~BinFileHelper() {
    qDeleteAll( fields );
    if( fileHandle )
        closeFile();
}

void BinFileHelper::init() {
    if(fileHandle)
        fclose(fileHandle);
    fileHandle = NULL;
    indexUpdated = false;
    FDUpdated = false;
    RSUpdated = false;
    preambleUpdated = false;
    byteswap = false;
    errnum = ERR_NULL;
    recordCount = 0;
}

void BinFileHelper::clearFields() {
    qDeleteAll( fields );
    fields.clear();
}

bool BinFileHelper::testFileExists( const QString &fileName ) {
    QString FilePath = KSPaths::locate(QStandardPaths::GenericDataLocation, fileName );
    QByteArray b = FilePath.toLatin1();
    const char *filepath = b.data();
    FILE *f  = fopen(filepath, "rb");
    if( f ) {
        fclose( f );
        return true;
    }
    else
        return false;
}

FILE *BinFileHelper::openFile(const QString &fileName) {
    QString FilePath = KSPaths::locate(QStandardPaths::GenericDataLocation, fileName );
    init();
    QByteArray b = FilePath.toLatin1();
    const char *filepath = b.data();

    fileHandle = fopen(filepath, "rb");

    if(!fileHandle) {
        errnum = ERR_FILEOPEN;
        return NULL;
    }
    return fileHandle;
}

enum BinFileHelper::Errors BinFileHelper::__readHeader() {
    qint16 endian_id, i;
    char ASCII_text[125];
    dataElement *de;

    // Read the preamble
    if(!fileHandle) 
        return ERR_FILEOPEN;

    rewind(fileHandle);

    fread(ASCII_text, 124, 1, fileHandle);
    ASCII_text[124] = '\0';
    headerText = ASCII_text;

    fread(&endian_id, 2, 1, fileHandle);
    if(endian_id != 0x4B53)
        byteswap = 1;
    else
        byteswap = 0;

    fread( &versionNumber, 1, 1, fileHandle );

    preambleUpdated = true;    
    // Read the field descriptor table
    fread(&nfields, 2, 1, fileHandle);
    if( byteswap ) nfields = bswap_16( nfields );
    fields.clear();
    for(i = 0; i < nfields; ++i) {
        de = new dataElement;
        if(!fread(de, sizeof(dataElement), 1, fileHandle)) {
            delete de;
            return ERR_FD_TRUNC;
        }
        if( byteswap ) de->scale = bswap_32( de->scale );
        fields.append( de );
    }

    if(!RSUpdated) {
        recordSize = 0;
        for(i = 0; i < fields.size(); ++i)
            recordSize += fields[i] -> size;
        RSUpdated = true;
    }

    FDUpdated = true;

    // Read the index table
    fread(&indexSize, 4, 1, fileHandle);
    if( byteswap ) indexSize = bswap_32( indexSize );

    quint32 j;
    quint32 ID;
    quint32 offset;
    quint32 prev_offset;
    quint32 nrecs;
    quint32 prev_nrecs;

    itableOffset = ftell(fileHandle);

    prev_offset = 0;
    prev_nrecs = 0;
    recordCount = 0;

    indexCount.clear();
    indexOffset.clear();

    if( indexSize == 0 ) {
        errorMessage.sprintf( "Zero index size!" );
        return ERR_INDEX_TRUNC;
    }
    for(j = 0; j < indexSize; ++j) {
        if(!fread(&ID, 4, 1, fileHandle)) {
            errorMessage.sprintf("Table truncated before expected! Read i = %d index entries so far", j);
            return ERR_INDEX_TRUNC;
        }
        if( byteswap ) ID = bswap_32( ID );
        if(ID >= indexSize) {
            errorMessage.sprintf("ID %u is greater than the expected number of expected entries (%u)", ID, indexSize);
            return ERR_INDEX_BADID;
        }
        if(ID != j) {
            errorMessage.sprintf("Found ID %u, at the location where ID %u was expected", ID, j);
            return ERR_INDEX_IDMISMATCH;
        }
        if(!fread(&offset, 4, 1, fileHandle)) {
            errorMessage.sprintf("Table truncated before expected! Read i = %d index entries so far", j);
            return ERR_BADSEEK;
        }
        if( byteswap ) offset = bswap_32( offset );
        if(!fread(&nrecs, 4, 1, fileHandle)) {
            errorMessage.sprintf("Table truncated before expected! Read i = %d index entries so far", j);
            return ERR_BADSEEK;
        }
        if( byteswap ) nrecs = bswap_32( nrecs );
        if(prev_offset != 0 && prev_nrecs != (-prev_offset + offset)/recordSize) { 
            errorMessage.sprintf("Expected %u  = (%X - %x) / %x records, but found %u, in index entry %u", 
                                    (offset - prev_offset) / recordSize, offset, prev_offset, recordSize, prev_nrecs, j - 1);
            return ERR_INDEX_BADOFFSET;
        }

        indexOffset.append( offset );
        indexCount.append( nrecs );

        recordCount += nrecs;
        prev_offset = offset;
        prev_nrecs = nrecs;
    }

    dataOffset = ftell(fileHandle);

    indexUpdated = true;

    return ERR_NULL;
}

bool BinFileHelper::readHeader() {
    switch( (errnum = __readHeader()) ) {
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
    case ERR_INDEX_BADOFFSET: {
        indexOffset.clear();
        indexCount.clear();
    }
    }
    return false;
}

void BinFileHelper::closeFile() {
    fclose(fileHandle);
    fileHandle = NULL;
}

int BinFileHelper::getErrorNumber() {
    int err = errnum;
    errnum = ERR_NULL;
    return err;
}

QString BinFileHelper::getError() {
    QString erm = errorMessage;
    errorMessage = "";
    return erm;
}

struct dataElement BinFileHelper::getField(const QString &fieldName) const {
    dataElement de;
    for(int i = 0; i < fields.size(); ++i) {
        if(fields[i] -> name == fieldName) {
            de = *fields[i];
            return de;
        }
    }
    return de; // Returns junk!
}

bool BinFileHelper::isField(const QString &fieldName) const {
    for(int i = 0; i < fields.size(); ++i) {
        if(fields[i] -> name == fieldName)
            return true;
    }
    return false;
}

int BinFileHelper::unsigned_KDE_fseek( FILE *stream, quint32 offset, int whence ) {
    Q_ASSERT( stream );
    int ret = 0;
    if( offset <= ((quint32)1 << 31) - 1 ) {
        ret = fseek( stream, offset, whence );
    }
    else {
        // Do the fseek in two steps
        ret = fseek( stream, ((quint32)1 << 31) - 1, whence );
        if( !ret )
            ret = fseek( stream, offset - ((quint32)1 << 31) + 1, SEEK_CUR );
    }
    return ret;
}

