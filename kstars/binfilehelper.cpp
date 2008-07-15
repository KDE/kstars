
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

#include <kstandarddirs.h>
#include <kde_file.h>
#include "byteswap.h"

class BinFileHelper;

BinFileHelper::BinFileHelper() {
    fileHandle = NULL;
    init();
}

BinFileHelper::~BinFileHelper() {

    for(int i = 0; i < fields.size(); ++i)
	delete fields[i];

    if(fileHandle)
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
    for(int i = 0; i < fields.size(); ++i)
	delete fields[i];
    fields.clear();
}

FILE *BinFileHelper::openFile(const QString &fileName) {
    QString FilePath = KStandardDirs::locate( "appdata", fileName );
    init();
    QByteArray b = FilePath.toAscii();
    const char *filepath = b.data();

    fileHandle = KDE_fopen(filepath, "rb");

    if(!fileHandle) {
	errnum = ERR_FILEOPEN;
	return NULL;
    }
    return fileHandle;
}

enum BinFileHelper::Errors BinFileHelper::__readHeader() {
    int i;
    qint16 endian_id;
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

    preambleUpdated = true;    
    // Read the field descriptor table
    fread(&nfields, 2, 1, fileHandle);
    if( byteswap ) bswap_16( nfields );
    fields.clear();
    for(i = 0; i < nfields; ++i) {
	de = new dataElement;
	if(!fread(de, sizeof(dataElement), 1, fileHandle)) {
	    return ERR_FD_TRUNC;
	}
        bswap_32( de->scale );
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
    fread(&indexSize, 2, 1, fileHandle);
    bswap_16( indexSize );

    quint16 ID;
    quint32 offset;
    quint32 prev_offset;
    quint16 nrecs;
    quint16 prev_nrecs;

    itableOffset = KDE_ftell(fileHandle);

    prev_offset = 0;
    prev_nrecs = 0;
    recordCount = 0;

    indexCount.clear();
    indexOffset.clear();
    for(i = 0; i < indexSize; ++i) {
	if(!fread(&ID, 2, 1, fileHandle)) {
	    errorMessage.sprintf("Table truncated before expected! Read i = %d index entries so far", i);
	    return ERR_INDEX_TRUNC;
	}
        bswap_16( ID );
	if(ID >= indexSize) {
	    errorMessage.sprintf("ID %u is greater than the expected number of expected entries (%u)", ID, indexSize);
	    return ERR_INDEX_BADID;
	}
	if(ID != i) {
	    errorMessage.sprintf("Found ID %u, at the location where ID %u was expected", ID, i);
	    return ERR_INDEX_IDMISMATCH;
	}
	if(!fread(&offset, 4, 1, fileHandle)) {
	    errorMessage.sprintf("Table truncated before expected! Read i = %d index entries so far", i);
	    return ERR_BADSEEK;
	}
        bswap_32( offset );
	if(!fread(&nrecs, 2, 1, fileHandle)) {
	    errorMessage.sprintf("Table truncated before expected! Read i = %d index entries so far", i);
	    return ERR_BADSEEK;
	}
        bswap_16( nrecs );
	if(prev_offset != 0 && prev_nrecs != (-prev_offset + offset)/recordSize) { 
	    errorMessage.sprintf("Expected %u  = (%X - %x) / %x records, but found %u, in index entry %u", 
				    (offset - prev_offset) / recordSize, offset, prev_offset, recordSize, prev_nrecs, i - 1);
	    return ERR_INDEX_BADOFFSET;
	}

	indexOffset.append( offset );
	indexCount.append( nrecs );

	recordCount += nrecs;
	prev_offset = offset;
	prev_nrecs = nrecs;
    }

    dataOffset = KDE_ftell(fileHandle);

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

struct dataElement BinFileHelper::getField(const QString &fieldName) {
    dataElement de;
    for(int i = 0; i < fields.size(); ++i) {
	if(fields[i] -> name == fieldName) {
	    de = *fields[i];
	    return de;
	}
    }
    return de; // Returns junk!
}

bool BinFileHelper::isField(const QString &fieldName) {
    for(int i = 0; i < fields.size(); ++i) {
	if(fields[i] -> name == fieldName)
	    return true;
    }
    return false;
}

