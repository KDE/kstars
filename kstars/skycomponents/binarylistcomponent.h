/*
    SPDX-FileCopyrightText: 2018 Valentin Boettcher <valentin@boettcher.cf>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDataStream>

#include "listcomponent.h"
#include "binarylistcomponent.h"
#include "auxiliary/kspaths.h"

//TODO: Error Handling - SERIOUSLY

/**
 * @class BinaryListComponent
 * @short provides functionality for loading the component data from Binary
 * @author Valentin Boettcher
 * @version 1.0
 *
 * This class is an abstract Template which requires that the type `T` is some child of
 * `SkyObject` and the type `Component` is some child of `ListComponent`. The class `T`
 * must provide a static `TYPE` property of the type `SkyObject::TYPE`. This is required
 * because access to the `type()` method is inconvenient here!
 *
 * The derived class must provide a `void loadFromText()` method, which loads the component
 * via `addListObject` or similar. (This method implements parsing etc, and cannot be
 * abstracted by this class.)
 *
 * Finally, one has to add this template as a friend class upon deriving it.
 * This is a concession to the already present architecture.
 *
 * File paths are determent by the means of KSPaths::writableLocation.
 */
template <class T, typename Component>
class BinaryListComponent
{
public:
    /**
     * @brief BinaryListComponent
     * @param parent a reference to the inheriting child
     * @param basename the base filename for the binary
     *
     * Sets the file extensions to `dat` for text and `bin` for binary.
     */
    BinaryListComponent(Component* parent, QString basename);

    /**
     * @brief BinaryListComponent
     * @param parent a reference to the inheriting child
     * @param basename the base filename for the binary
     * @param txtExt text data file extension
     * @param binExt binary data file extension
     */
    BinaryListComponent(Component* parent, QString basename, QString txtExt, QString binExt);

protected:
    /**
     * @brief loadData
     * @short Calls `loadData(false)`
     */
    virtual void loadData();

    /**
     * @brief loadData
     * @short Load the component data from binary (if available) or from text
     * @param dropBinaryFile whether to drop the current binary (and to recreate it)
     *
     * Tip: If you want to reload the data and recreate the binfile, just call
     * loadData(true).
     */
    virtual void loadData(bool dropBinaryFile);

    /**
     * @brief loadDataFromBinary
     * @short Opens the default binfile and calls `loadDataFromBinary([FILE])`
     */
    virtual void loadDataFromBinary();

    /**
     * @brief loadDataFromBinary
     * @param binfile the binary file
     * @short Loads the component data from the given binary.
     */
    virtual void loadDataFromBinary(QFile &binfile);

    /**
     * @brief writeBinary
     * @short Opens the default binfile and calls `writeBinary([FILE])`
     */
    virtual void writeBinary();

    /**
     * @brief writeBinary
     * @param binfile
     * @short Writes the component data to the specified binary. (Destructive)
     */
    virtual void writeBinary(QFile &binfile);

    /**
     * @brief loadDataFromText
     * @short Load the component data from text.
     *
     * This method shall be implemented by those who derive this class.
     *
     * This method should load the component data from text by the use of
     * `addListObject`  or similar.
     */
    virtual void loadDataFromText() = 0;

    // TODO: Rename, and integrate it into a wrapper
    //virtual void updateDataFile() = 0; // legacy from current implementation!

    /**
     * @brief dropBinary
     * @short Removes the binary file.
     * @return True if operation succeeds
     */
    virtual bool dropBinary();

    /**
     * @brief dropText
     * @short Removes the text file.
     * @return True if operation succeeds
     */
    virtual bool dropText();

    /**
     * @brief clearData
     * @short Removes the current component data where necessary.
     */
    virtual void clearData();

    QString filepath_txt;
    QString filepath_bin;

// Don't allow the children to mess with the Binary Version!
private:
    QDataStream::Version binversion = QDataStream::Qt_5_5;
    Component* parent;
};

template<class T, typename Component>
 BinaryListComponent<T, Component>::BinaryListComponent(Component *parent, QString basename) :  BinaryListComponent<T, Component>(parent, basename, "dat", "bin") {}

template<class T, typename Component>
 BinaryListComponent<T, Component>::BinaryListComponent(Component *parent, QString basename, QString txtExt, QString binExt) : parent { parent }
{
     filepath_bin = QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(basename + '.' + binExt);
     filepath_txt = QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(basename + '.' + txtExt);
}

template<class T, typename Component>
void  BinaryListComponent<T, Component>::loadData()
{
    loadData(false);
}

template<class T, typename Component>
void  BinaryListComponent<T, Component>::loadData(bool dropBinaryFile)
{
    // Clear old Stuff (in case of reload)
    clearData();

    // Drop Binary file for a fresh reload
    if(dropBinaryFile)
        dropBinary();

    QFile binfile(filepath_bin);
    if (binfile.exists()) {
        loadDataFromBinary(binfile);
    } else {
        loadDataFromText();
        writeBinary(binfile);
    }
}

template<class T, typename Component>
void  BinaryListComponent<T, Component>::loadDataFromBinary()
{
    QFile binfile(filepath_bin);
    loadDataFromBinary(binfile);
}

template<class T, typename Component>
void  BinaryListComponent<T, Component>::loadDataFromBinary(QFile &binfile)
{
    // Open our binary file and create a Stream
    if (binfile.open(QIODevice::ReadOnly))
    {
        QDataStream in(&binfile);

        // Use the specified binary version
        // TODO: Place this into the config
        in.setVersion(binversion);
        in.setFloatingPointPrecision(QDataStream::DoublePrecision);

        while(!in.atEnd()){
            T *new_object = nullptr;
            in >> new_object;

            parent->appendListObject(new_object);
            // Add name to the list of object names
            parent->objectNames(T::TYPE).append(new_object->name());
            parent->objectLists(T::TYPE).append(QPair<QString, const SkyObject *>(new_object->name(), new_object));
        }
        binfile.close();
    }
    else qWarning() << "Failed loading binary data from" << binfile.fileName();
}

template<class T, typename Component>
void  BinaryListComponent<T, Component>::writeBinary()
{
    QFile binfile(filepath_bin);
    writeBinary(binfile);
}

template<class T, typename Component>
void  BinaryListComponent<T, Component>::writeBinary(QFile &binfile)
{
    // Open our file and create a stream
    binfile.open(QIODevice::WriteOnly | QIODevice::Truncate);
    QDataStream out(&binfile);
    out.setVersion(binversion);
    out.setFloatingPointPrecision(QDataStream::DoublePrecision);

    // Now just dump out everything
    for(auto object : parent->m_ObjectList){
         out << *((T*)object);
    }

    binfile.close();
}

template<class T, typename Component>
bool  BinaryListComponent<T, Component>::dropBinary()
{
    return QFile::remove(filepath_bin);
}

template<class T, typename Component>
bool  BinaryListComponent<T, Component>::dropText()
{
    return QFile::remove(filepath_txt);
}

template<class T, typename Component>
void  BinaryListComponent<T, Component>::clearData()
{
    // Clear lists
    qDeleteAll(parent->m_ObjectList);
    parent->m_ObjectList.clear();
    parent->m_ObjectHash.clear();

    parent->objectLists(T::TYPE).clear();
    parent->objectNames(T::TYPE).clear();
}
