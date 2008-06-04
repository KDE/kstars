#include <kstandarddirs.h>

#include "binfilehelper.h"

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
    errno = ERR_NULL;
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
    const char *filepath = FilePath.toAscii().data();

    fileHandle = fopen(filepath, "rb");

    if(!fileHandle) {
	errno = ERR_FILEOPEN;
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
    ASCII_text[125] = '\0';
    headerText = ASCII_text;

    fread(&endian_id, 2, 1, fileHandle);
    if(endian_id != 0x4B53)
	byteswap = 1;
    else
	byteswap = 0;

    preambleUpdated = true;    
    // Read the field descriptor table
    fread(&nfields, 2, 1, fileHandle);

    for(i = 0; i < nfields; ++i) {
	de = new dataElement;
	if(!fread(de, sizeof(dataElement), 1, fileHandle)) {
	    return ERR_FD_TRUNC;
	}
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

    quint16 ID;
    quint32 offset;
    quint32 prev_offset;
    quint16 nrecs;
    quint16 prev_nrecs;

    itableOffset = ftell(fileHandle);

    prev_offset = 0;
    prev_nrecs = 0;
    recordCount = 0;

    for(i = 0; i < indexSize; ++i) {
	if(!fread(&ID, 2, 1, fileHandle)) {
	    errorMessage.sprintf("Table truncated before expected! Read i = %d index entries so far", i);
	    return ERR_INDEX_TRUNC;
	}
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
	if(!fread(&nrecs, 2, 1, fileHandle)) {
	    errorMessage.sprintf("Table truncated before expected! Read i = %d index entries so far", i);
	    return ERR_BADSEEK;
	}
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

    dataOffset = ftell(fileHandle);

    indexUpdated = true;

    return ERR_NULL;
}

bool BinFileHelper::readHeader() {
    switch( (errno = __readHeader()) ) {
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

bool BinFileHelper::closeFile() {
    fclose(fileHandle);
    fileHandle = NULL;
}

int BinFileHelper::getErrorNumber() {
    int err = errno;
    errno = ERR_NULL;
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

