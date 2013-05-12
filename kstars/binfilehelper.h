
/***************************************************************************
                    binfilehelper.h - K Desktop Planetarium
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


#ifndef BINFILEHELPER_H_
#define BINFILEHELPER_H_

#include <QString>
#include <QVector>

#include <cstdio>

class QString;

/**
 *@short   A structure describing a data field in the file
 */
typedef struct dataElement {
    char name[10];
    qint8 size;
    quint8 type;
    qint32 scale;
} dataElement;


/**
 *@class BinFileHelper
 *This class provides utility functions to handle binary data files in the format prescribed
 *by KStars. The file format is designed specifically to support data that has the form of an
 *array of structures. See data/README.fileformat for details.
 *The methods use primitive C file I/O routines defined in stdio.h to obtain efficiency
 *@short Implements an interface to handle binary data files used by KStars
 *@author Akarsh Simha
 *@version 1.0
 */

class BinFileHelper {

 public:

    /**
     *Constructor
     */
  
    BinFileHelper();
    
    /**
     *Destructor
     */
    
    ~BinFileHelper();

    /**
     *@short Checks if a file exists. WARNING: Might be incompatible in other locales
     */
    static bool testFileExists( const QString &fileName );
    
    /**
     *WARNING: This function may not be compatible in other locales, because it calls QString::toAscii
     *@short Open a Binary data file and set the handle
     *@param fileName  Reference to QString containing the name of the file
     *@return Handle to the file if successful, NULL if an error occurred, sets the error.
     */

    FILE *openFile(const QString &fileName);

    /**
     *@short  Read the header and index table from the file and fill up the QVector s with the entries
     *@return True if successful, false if an error occurred, sets the error.
     */

    bool readHeader();

    /**
     *@short  Close the binary data file
     */

    void closeFile();

    /**
     *@short   Get error number
     *@return  A number corresponding to the error
     */
    int getErrorNumber();

    /**
     *@short   Get error string
     *@return  A QString containing the error message to be displayed
     */
    QString getError();

    /**
     *@short   Get field by name
     *@param   fieldName  Name of the field
     *@return  A dataElement structure containing field data if the field exists, 
     *         containing junk values if the field doesn't exist
     */
    struct dataElement getField(const QString &fieldName);

    /**
     *@short  Check whether a field exists
     *@param  fieldName   Name of the field
     *@return True if the field exists, false if it does not or the FD hasn't been read yet
     */
    bool isField(const QString &FieldName);

    /**
     *@short  Check whether file properties are real or bogus
     *@return The value of private variable indexUpdated, which is true if data has been updated
     */
    inline bool propertiesUpdated() { return indexUpdated; }

    /**
     *@short  Get the file handle corresponding to the currently open file
     *@return The filehandle if a file is open, NULL if no file is open
     */
    inline FILE *getFileHandle() { return fileHandle; }

    /**
     *@short  Returns the offset in the file corresponding to the given index ID
     *@param  id  ID of the index entry whose offset is required
     *@return The offset corresponding to that index ID, 0 if the index hasn't been read
     */
    inline long getOffset(int id) { return (indexUpdated ? indexOffset.at( id ) : 0); }

    /**
     *@short  Returns the number of records under the given index ID
     *@param  id  ID of the index entry
     *@return The number of records under index that index ID, or 0 if the index has not been read
     */
    inline unsigned int getRecordCount(int id) { return (indexUpdated ? indexCount.at( id ) : 0); }

    /**
     *@short  Returns the total number of records in the file
     *@return The number of records in the file, or 0 if the index has not been read
     */
    inline unsigned long getRecordCount() { return (indexUpdated ? recordCount : 0); }

    /**
     *@short  Should we do byte swapping?
     *@note   To be called only after the header has been parsed
     *@return true if we must do byte swapping, false if not
     */
    inline bool getByteSwap() { return byteswap; }

    /**
     *@short  Return a guessed record size
     *@note   The record size returned is guessed from the field descriptor table and may
     *        not be correct in many cases
     *@return A guessed size of the record, or zero if the FD hasn't been read yet
     */
    inline int guessRecordSize() { return (RSUpdated ? recordSize : 0); }

    /**
     *@short  Override record size
     *@note   To be used to override the guess in case the guess is wrong. This method should be called
     *        before calling readHeader(). Use this only if you are sure of the recordSize or are sure
     *        that the guess will not work.
     *@param  The correct recordSize
     */
    inline void setRecordSize(int rs) { recordSize = rs; RSUpdated = true; }

    /**
     *@short  Returns the number of fields as suggested by the field descriptor in the header
     *@return Number of fields, or zero if the FD hasn't been read yet
     */
    inline int getFieldCount() { return (FDUpdated ? nfields : 0); }

    /**
     *@short  Returns the header text
     *@return QString containing the header text if header has been read, blank QString otherwise
     */
    inline QString getHeaderText() { return (preambleUpdated ? headerText : ""); }

    /**
     *@short  Returns the offset at which the data begins
     *@return The value of dataOffset if indexUpdated, else zero
     */
    inline long getDataOffset() { return (indexUpdated ? dataOffset : 0); }

    /**
     *@short  Returns the offset at which the index table begins
     *@return The value of itableOffset if FDUpdated, else zero
     */
    inline long getIndexTableOffset() { return (FDUpdated ? itableOffset : 0); }

    /**
     *@short Wrapper around fseek for large offsets
     *
     * fseek takes a signed long for the offset, since it can be
     * either positive or negative. If the argument is a 32-bit
     * unsigned integer, fseek will fail in most situations, since
     * that will be interpreted as a number of the opposite sign.
     * This wrapper circumvents that problem by repeatedly calling
     * fseek twice if the offset is too large. Useful when 64-bit
     * handling is really not necessary.
     *
     *@return Zero on success. Else, error code of the first failing fseek call.
     *@note Clearly, this method can move forward only. When we need
     * one that goes back, we'll implement that.
     */
    static int unsigned_KDE_fseek( FILE *stream, quint32 offset, int whence );

    /**
     *@short   An enum providing user-friendly names for errors encountered
     */
    enum Errors {
	ERR_NULL,             // No error occurred
	ERR_FILEOPEN,         // File could not be opened
	ERR_FD_TRUNC,         // Field descriptor table is truncated
	ERR_INDEX_TRUNC,      // File ends prematurely, before expected end of index table
	ERR_INDEX_BADID,      // Index table has an invalid ID entry 
	ERR_INDEX_IDMISMATCH, // Index table has a mismatched ID entry [ID found in the wrong place]
	ERR_INDEX_BADOFFSET,  // Offset / Record count specified in the Index table is bad
	ERR_BADSEEK           // Premature end of file / bad seek while reading index table
    };

    /**
     *@short   An enum providing user-friendly names for data types
     */
    
    enum dataType {
	DT_CHAR,              /* Character */
	DT_INT8,              /* 8-bit Integer */
	DT_UINT8,             /* 8-bit Unsigned Integer */
	DT_INT16,             /* 16-bit Integer */
	DT_UINT16,            /* 16-bit Unsigned Integer */
	DT_INT32,             /* 32-bit Integer */
	DT_UINT32,            /* 32-bit Unsigned Integer */
	DT_CHARV,             /* Fixed-length array of characters */
	DT_STR,               /* Variable length array of characters, either terminated by NULL or by the limit on field size */
	DT_SPCL = 128         /* Flag indicating that the field requires special treatment (eg: Different bits may mean different things) */
    };

 private:

    /**
     *@short  Backends for readHeader without cleanup on abort
     *@return Return the appropriate error code, or ERR_NULL if successful
     */
    enum Errors  __readHeader();

    /**
     *@short  Helper function that clears all field entries in the QVector fields
     */
    void clearFields();

    /**
     *@short  Helper function that sets all variables to their initial values
     */
    void init();

    FILE *fileHandle;                     // Handle to the file.
    QVector<unsigned long> indexOffset;   // Stores offsets corresponding to each index table entry
    QVector<unsigned int> indexCount;     // Stores number of records under each index table entry
    bool indexUpdated;                    // True if the data from the index, and associated properties have been updated
    bool FDUpdated;                       // True if the data from the Field Descriptor, and associated properties have been updated
    bool RSUpdated;                       // True if the recordSize parameter is set correctly, either manually or bye reading the FD
    bool preambleUpdated;                 // True if the data from the preamble and associated properties have been updated
    enum Errors errnum;                   // Stores the number corresponding to the previous error
    bool byteswap;                        // True if byteswapping should be done
    int recordSize;                       // Stores the size of a record in bytes for quick retrieval
    QVector<dataElement *> fields;        // Maintains a list of fields in the file, along with relevant details
    QString headerText;                   // Stores the file header text
    qint16 nfields;                       // Stores the number of fields as indicated by the field descriptor
    quint32 indexSize;                    // Stores the size of the index table in number of entries
    long itableOffset;                    // Stores the offset position of the first index table entry
    long dataOffset;                      // Stores the offset position of the start of data
    QString errorMessage;                 // Stores the most recent 'unread' error message
    unsigned long recordCount;            // Stores the total number of records in the file
    quint8 versionNumber;                 // Stores the version number of the file
};

#endif
