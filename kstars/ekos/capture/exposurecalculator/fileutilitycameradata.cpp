/*
    SPDX-FileCopyrightText: 2023 Joseph McGee <joseph.mcgee@sbcglobal.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <stdio.h>
#include <string.h>
#include <QLoggingCategory>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QStandardPaths>
#include <QUrl>
#include <QTimer>
#include <QIODevice>
#include <QDirIterator>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include "fileutilitycameradata.h"
#include "imagingcameradata.h"
#include "cameragainreadnoise.h"
#include "exposurecalculatordialog.h"
#include <ekos_capture_debug.h>
#include <sstream>
#include <kspaths.h>

/*
 * Direct access to files on KDE/kstars git repository:
 */
QString const OptimalExposure::FileUtilityCameraData::cameraDataRemoteRepositoryList
    = "https://api.github.com/repos/KDE/kstars/git/trees/master:kstars%2Fdata%2Fcameradata";

QString const OptimalExposure::FileUtilityCameraData::cameraDataRemoteRepository
    = "https://raw.githubusercontent.com/KDE/kstars/master/kstars/data/cameradata/";

QString const OptimalExposure::FileUtilityCameraData::cameraApplicationDataRepository
    = KSPaths::locate(QStandardPaths::AppDataLocation, QString("cameradata/"), QStandardPaths::LocateDirectory);

QString const OptimalExposure::FileUtilityCameraData::cameraLocalDataRepository
    = QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("kstars/cameradata/");

QStringList OptimalExposure::FileUtilityCameraData::getAvailableCameraFilesList()
{
    QStringList cameraDataFiles;
    QStringList dirs;


    dirs << KSPaths::locateAll(QStandardPaths::GenericDataLocation,
                               QString::fromLatin1("kstars/cameradata"),
                               QStandardPaths::LocateDirectory);

    Q_FOREACH (const QString &dir, dirs)
    {
        QDirIterator it(dir, QStringList() << QLatin1String("*.xml"));

        while (it.hasNext())
        {
            cameraDataFiles.append(it.next());
        }
    }

    return(cameraDataFiles);
}


QString OptimalExposure::FileUtilityCameraData::cameraIdToCameraDataFileName(QString cameraId)
{

    // Replace spaces with "_" in the file name
    QString aCameraDataFile = cameraId.replace(" ", "_");

    if(!aCameraDataFile.endsWith(".xml"))
    {
        aCameraDataFile += ".xml";
    }

    return(aCameraDataFile);
}

QString OptimalExposure::FileUtilityCameraData::cameraDataFileNameToCameraId(QString cameraDataFileName)
{
    QFileInfo cameraDataFileInfo(cameraDataFileName);

    // QString aCameraId = cameraDataFileName;

    QString aCameraId = cameraDataFileInfo.baseName();

    if(aCameraId.endsWith(".xml"))
        aCameraId.truncate(aCameraId.length() - 4);

    aCameraId = aCameraId.replace("_", " ");
    return(aCameraId);
}


void OptimalExposure::FileUtilityCameraData::downloadRepositoryCameraDataFileList(
    QDialog *aDialog)

{

    qCInfo(KSTARS_EKOS_CAPTURE) << "cameraDataRemoteRepositoryList: " <<
                                OptimalExposure::FileUtilityCameraData::cameraDataRemoteRepositoryList;
    qCInfo(KSTARS_EKOS_CAPTURE) << "cameraDataRemoteRepository: " <<
                                OptimalExposure::FileUtilityCameraData::cameraDataRemoteRepository;
    // qCInfo(KSTARS_EKOS_CAPTURE) << "cameraApplicationDataRepository: " << OptimalExposure::FileUtilityCameraData::cameraApplicationDataRepository;
    qCInfo(KSTARS_EKOS_CAPTURE) << "cameraLocalDataRepository: " <<
                                OptimalExposure::FileUtilityCameraData::cameraLocalDataRepository;

    // Using git tree to access camera file list, this approach requires parsing the tree
    // string collect the xml (camera data) file names.

    FileUtilityCameraDataDialog *activeDialog = static_cast<FileUtilityCameraDataDialog *> (aDialog);
    activeDialog->setANetworkAccessManager(new QNetworkAccessManager(activeDialog));

    QString aCameraDataRemoteFolderURL = OptimalExposure::FileUtilityCameraData::cameraDataRemoteRepositoryList;
    qCInfo(KSTARS_EKOS_CAPTURE) << "Attempting access of camera data file repository " << aCameraDataRemoteFolderURL;

    QNetworkRequest activeRequest = QNetworkRequest(QUrl(aCameraDataRemoteFolderURL));
    activeDialog->setRequest(&activeRequest);
    activeDialog->setReply(activeDialog->getANetworkAccessManager()->get(activeRequest));

    QNetworkReply *activeReply = activeDialog->getReply();
    QTimer::singleShot(60000, activeReply, [activeReply]   //Time-out is 60 seconds
    {
        activeReply->abort();
        activeReply->deleteLater();
        qCCritical(KSTARS_EKOS_CAPTURE) << "CameraDataFile Download Timed out.";
    });

    QVector<QString> *availableCameraDataFiles = new QVector<QString>();

    activeDialog->connect(activeReply, &QNetworkReply::finished,
                          activeDialog,
                          [availableCameraDataFiles, activeReply, activeDialog]
    {
        // qCInfo(KSTARS_EKOS_CAPTURE) << "The Camera Data File List download is finished";
        activeReply->deleteLater();
        if (activeReply->error() != QNetworkReply::NoError)
        {
            qCCritical(KSTARS_EKOS_CAPTURE) << "An error occurred in the Camera Data File List download";
            return;
        }

        // Parse out the filenames from the tree
        std::string responseDataStr = activeReply->readAll().toStdString();

        // qCInfo(KSTARS_EKOS_CAPTURE) << "Camera Data File Github Tree:\n" << QString::fromStdString(responseDataStr);

        std::string startdelimiter = "[";
        std::string stopdelimiter = "]";
        std::string treeStr = responseDataStr.substr(responseDataStr.find(startdelimiter) + 1, responseDataStr.find(stopdelimiter) - responseDataStr.find(startdelimiter));

        std::vector <std::string> tokens;
        std::stringstream responseDataStream(treeStr);

        std::string intermediate;

        while(getline(responseDataStream, intermediate, ','))
        {
            tokens.push_back(intermediate);
        }

        // Find the camera file name
        for(std::size_t i = 0; i < tokens.size(); i++)
        {
            if(tokens[i].find("path") != std::string::npos)
            {

                std::string aCameraDataFileStr = tokens[i].substr(9,  tokens[i].length() - 14);
                // std::cout << aCameraDataFileStr << '\n';

                QString aCameraDataFile = QString::fromStdString(aCameraDataFileStr);

                availableCameraDataFiles->append(aCameraDataFile);

            }
        }
        activeDialog->setAvailableCameraDataFiles(*availableCameraDataFiles);
        activeDialog->refreshCameraList();
    });

    activeDialog->connect(activeReply, &QNetworkReply::downloadProgress,
                          activeDialog, []
    {
        qCInfo(KSTARS_EKOS_CAPTURE) << "The Camera Data File List download is in progress";
    });
    activeDialog->connect(activeReply, &QNetworkReply::errorOccurred,
                          activeDialog, []
    {
        qCCritical(KSTARS_EKOS_CAPTURE) << "The Camera Data File List download had an error";
    });
    activeDialog->connect(activeReply, &QNetworkReply::destroyed,
                          activeDialog, []
    {
        qCInfo(KSTARS_EKOS_CAPTURE) << "The Camera Data File List connection was destroyed";
    });

    qCInfo(KSTARS_EKOS_CAPTURE) << "The Camera Data File List length is " << availableCameraDataFiles->length();


}

void OptimalExposure::FileUtilityCameraData::downloadCameraDataFile(
    QString cameraId,
    QDialog *aDialog)
{

    FileUtilityCameraDataDialog *activeDialog = static_cast<FileUtilityCameraDataDialog *> (aDialog);

    activeDialog->setANetworkAccessManager(new QNetworkAccessManager(activeDialog));

    QDir filePath = OptimalExposure::FileUtilityCameraData::cameraLocalDataRepository;
    filePath.mkpath(".");

    QString cameraDataFileName = cameraIdToCameraDataFileName(cameraId);

    // File in remote repository
    QString aCameraDataDownloadURL = OptimalExposure::FileUtilityCameraData::cameraDataRemoteRepository
                                     + cameraDataFileName;

    // File for local storage
    QString aCameraDataLocalFileName = OptimalExposure::FileUtilityCameraData::cameraLocalDataRepository
                                       + cameraDataFileName;

    //QString aCameraDataLocalFileName = KSPaths::locate(QStandardPaths::AppLocalDataLocation,
    //                                   QString("cameradata/%1").arg(cameraDataFileName));



    qCInfo(KSTARS_EKOS_CAPTURE) << "Attempting Download Camera Data from "
                                << aCameraDataDownloadURL << "\n\tto " << aCameraDataLocalFileName;

    QNetworkRequest activeRequest = QNetworkRequest(QUrl(aCameraDataDownloadURL));
    activeDialog->setRequest(&activeRequest);

    activeDialog->setReply(activeDialog->getANetworkAccessManager()->get(activeRequest));
    QNetworkReply *activeReply = activeDialog->getReply();
    QTimer::singleShot(60000, activeReply, [activeReply]   //Time-out is 60 seconds
    {
        activeReply->abort();
        activeReply->deleteLater();
        qCCritical(KSTARS_EKOS_CAPTURE) << Q_FUNC_INFO << "CameraDataFile Download Timed out.";
    });
    activeDialog->connect(activeReply, &QNetworkReply::finished,
                          activeDialog,
                          [aCameraDataLocalFileName, activeReply, activeDialog]
    {
        activeReply->deleteLater();
        if (activeReply->error() != QNetworkReply::NoError)
        {
            qCCritical(KSTARS_EKOS_CAPTURE)
                    << "An error occurred in the Camera Data File download of "
                    << aCameraDataLocalFileName;
            return;
        }
        QFile aCameraDataFile(aCameraDataLocalFileName);
        if(aCameraDataFile.open(QIODevice::ReadWrite | QIODevice::Truncate))
        {
            QByteArray responseData = activeReply->readAll();
            if (aCameraDataFile.write(responseData))
            {
                aCameraDataFile.close();
                qCInfo(KSTARS_EKOS_CAPTURE) << "The Camera Data File download of "
                                            << aCameraDataLocalFileName << " is finished";
                activeDialog->decrementDownloadFileCounter();
            }
            else
            {
                qCCritical(KSTARS_EKOS_CAPTURE) << "The Camera Data File download of "
                                                << aCameraDataLocalFileName << " was not completed";
            }
        }
    });
    activeDialog->connect(activeReply, &QNetworkReply::downloadProgress,
                          activeDialog, [aCameraDataLocalFileName]
    {
        qCInfo(KSTARS_EKOS_CAPTURE) << "The Camera Data File download of "
                                    << aCameraDataLocalFileName << " is in progress";
    });
    activeDialog->connect(activeReply, &QNetworkReply::errorOccurred,
                          activeDialog, [aCameraDataLocalFileName]
    {
        qCInfo(KSTARS_EKOS_CAPTURE) << "The Camera Data File download of "
                                    << aCameraDataLocalFileName << " had an error";
    });
    activeDialog->connect(activeReply, &QNetworkReply::destroyed,
                          activeDialog, [aCameraDataLocalFileName]
    {
        qCInfo(KSTARS_EKOS_CAPTURE) << "The Camera Data File connection for " << aCameraDataLocalFileName << " was destroyed";
    });

}


int OptimalExposure::FileUtilityCameraData::readCameraDataFile(QString aCameraDataFile,
        OptimalExposure::ImagingCameraData *anImagingCameraData)
{
    //    QString aCameraDataFile = OptimalExposure::FileUtilityCameraData::cameraApplicationDataRepository +
    //                              cameraIdToCameraDataFileName(cameraId);

    // qCInfo(KSTARS_EKOS_CAPTURE) << "Opening... " + aCameraDataFile;

    QFile file(aCameraDataFile);
    if(file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        // qCInfo(KSTARS_EKOS_CAPTURE) << "Reading... " + aCameraDataFile;
        QXmlStreamReader xmlReader(&file);

        if (xmlReader.readNextStartElement())
        {
            // qCInfo(KSTARS_EKOS_CAPTURE) << "xmlReader start element name: " << xmlReader.name();
            if (xmlReader.name().toString() == "ExposureCalculatorCameraData")
            {
                // qCInfo(KSTARS_EKOS_CAPTURE) << "Found aCamera Element...";

                QVector<OptimalExposure::CameraGainReadMode> *aCameraGainReadModeVector
                    = new QVector<OptimalExposure::CameraGainReadMode>();

                while(xmlReader.readNextStartElement())
                {
                    // qCInfo(KSTARS_EKOS_CAPTURE) << "xmlReader start element name: " << xmlReader.name();
                    if (xmlReader.name().toString() == "CameraDataClassVersion")
                    {
                        int aDataClassVersion = xmlReader.readElementText().toInt();
                        anImagingCameraData->setDataClassVersion(aDataClassVersion);
                    }

                    if (xmlReader.name().toString() == "CameraId")
                    {
                        // qCInfo(KSTARS_EKOS_CAPTURE) << "Found aCameraId Element...";
                        QString aCameraIdString = xmlReader.readElementText();
                        // qCInfo(KSTARS_EKOS_CAPTURE) << "Read element text..." + aCameraIdString;
                        anImagingCameraData->setCameraId(aCameraIdString);
                    }

                    if (xmlReader.name().toString() == "SensorType")
                    {
                        // qCInfo(KSTARS_EKOS_CAPTURE) << "Found SensorType Element...";
                        QString aSensorType = xmlReader.readElementText();
                        // qCInfo(KSTARS_EKOS_CAPTURE) << "Read element text..." + aSensorType;
                        if(aSensorType == "COLOR")
                            anImagingCameraData->setSensorType(OptimalExposure::SENSORTYPE_COLOR);

                        if(aSensorType == "MONOCHROME")
                            anImagingCameraData->setSensorType(OptimalExposure::SENSORTYPE_MONOCHROME);
                    }

                    if (xmlReader.name().toString() == "GainSelectionType")
                    {
                        // qCInfo(KSTARS_EKOS_CAPTURE) << "Found GainSelectionType Element...";
                        QString aGainSelectionType = xmlReader.readElementText();
                        // qCInfo(KSTARS_EKOS_CAPTURE) << "Read element text..." + aGainSelectionType;
                        if(aGainSelectionType == "NORMAL")
                            anImagingCameraData->setGainSelectionType(OptimalExposure::GAIN_SELECTION_TYPE_NORMAL);
                        if(aGainSelectionType == "ISO_DISCRETE")
                            anImagingCameraData->setGainSelectionType(OptimalExposure::GAIN_SELECTION_TYPE_ISO_DISCRETE);
                        if(aGainSelectionType == "FIXED")
                            anImagingCameraData->setGainSelectionType(OptimalExposure::GAIN_SELECTION_TYPE_FIXED);
                    }


                    // GainSelection collection  // For GAIN_SELECTION_TYPE_NORMAL the collection represents min and max,
                    // for GAIN_SELECTION_TYPE_ISO_DISCRETE, the collection is the discrete list of of gain/iso values.
                    // Note that the data available for GainReadNoiseValue may not match a camera discrete gain/iso values,
                    // so this collection resolves that.

                    if (xmlReader.name().toString() == "CameraGainSelections")
                    {
                        QVector<int> *aGainSelectionVector = new QVector<int>();

                        int aGainSelection = 0;
                        while(xmlReader.readNext() && !(xmlReader.name() == QString("CameraGainSelections") && xmlReader.isEndElement()))
                        {
                            // qCInfo(KSTARS_EKOS_CAPTURE) << "xmlReader.name() = " <<  xmlReader.name();
                            if (xmlReader.name().toString() == "GainSelection")
                            {
                                // qCInfo(KSTARS_EKOS_CAPTURE) << "xmlReader.name() = " <<  xmlReader.name();
                                QString aGainSelectionString = xmlReader.readElementText();
                                aGainSelection = aGainSelectionString.toInt();
                                // qCInfo(KSTARS_EKOS_CAPTURE) << "a Found GainSelection text: " << aGainSelection;

                                // qCInfo(KSTARS_EKOS_CAPTURE) << "Adding GainSelection to Vector " << QString::number(aGainSelection);

                                aGainSelectionVector->push_back(aGainSelection);
                            }
                        }
                        anImagingCameraData->setGainSelectionRange(*aGainSelectionVector);
                    }

                    if (xmlReader.name().toString() == "CameraGainReadMode")
                    {
                        OptimalExposure::CameraGainReadMode *aCameraGainReadMode = new OptimalExposure::CameraGainReadMode();

                        while(xmlReader.readNext() && !(xmlReader.name().toString() == "CameraGainReadMode" && xmlReader.isEndElement()))
                        {
                            // qCInfo(KSTARS_EKOS_CAPTURE) << "xmlReader.name() = " <<  xmlReader.name();
                            if (xmlReader.name().toString() == "GainReadModeNumber")
                            {
                                QString aGainReadModeNumberString = xmlReader.readElementText();
                                aCameraGainReadMode->setCameraGainReadModeNumber(aGainReadModeNumberString.toInt());
                            }
                            if (xmlReader.name().toString() == "GainReadModeName")
                            {
                                aCameraGainReadMode->setCameraGainReadModeName(xmlReader.readElementText());
                            }

                            // CameraGainReadNoise collection
                            if (xmlReader.name().toString() == "CameraGainReadNoise")
                            {
                                // qCInfo(KSTARS_EKOS_CAPTURE) << "Found CameraGainReadNoise Element...";
                                // QString aCameraGainReadNoise = xmlReader.readElementText();
                                // // qCInfo(KSTARS_EKOS_CAPTURE) << "Read element ..." + aCameraGainReadNoise;

                                QVector<OptimalExposure::CameraGainReadNoise> *aCameraGainReadNoiseVector
                                    = new QVector<OptimalExposure::CameraGainReadNoise>();


                                // Iterate for Gain Read-Noise data
                                int aGain = 0;
                                double aReadNoise = 0.0;
                                while(xmlReader.readNext() && !(xmlReader.name().toString() == "CameraGainReadNoise" && xmlReader.isEndElement()) )
                                {
                                    // qCInfo(KSTARS_EKOS_CAPTURE) << "xmlReader.name() = " <<  xmlReader.name();

                                    if (xmlReader.isEndElement())
                                        // qCInfo(KSTARS_EKOS_CAPTURE) << "At end of  = " <<  xmlReader.name();


                                        if (xmlReader.name().toString() == "GainReadNoiseValue")
                                        {
                                            // qCInfo(KSTARS_EKOS_CAPTURE) << "Found GainReadNoiseValue Element...";
                                        }

                                    if (xmlReader.name().toString() == "Gain")
                                    {
                                        // qCInfo(KSTARS_EKOS_CAPTURE) << "Found Gain Element...";
                                        QString aGainString = xmlReader.readElementText();
                                        aGain = aGainString.toInt();
                                        // qCInfo(KSTARS_EKOS_CAPTURE) << "a Found Gain text: " << aGain;
                                    }

                                    if (xmlReader.name().toString() == "ReadNoise")
                                    {
                                        // qCInfo(KSTARS_EKOS_CAPTURE) << "Found ReadNoise Element...";
                                        QString aReadNoiseString = xmlReader.readElementText();
                                        aReadNoise = aReadNoiseString.toDouble();
                                        // qCInfo(KSTARS_EKOS_CAPTURE) << "a Found ReadNoise text: " << aReadNoise;

                                        // Add this to a vector
                                        // qCInfo(KSTARS_EKOS_CAPTURE) << "Adding Gain Read-Noise to Vector " << QString::number(aGain) << " " << QString::number(aReadNoise) ;
                                        aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(aGain, aReadNoise)));
                                    }
                                }
                                aCameraGainReadMode->setCameraGainReadNoiseVector(*aCameraGainReadNoiseVector);
                                aCameraGainReadModeVector->push_back(*aCameraGainReadMode);
                            }
                            anImagingCameraData->setCameraGainReadModeVector(*aCameraGainReadModeVector);
                        }
                        // qCInfo(KSTARS_EKOS_CAPTURE) << "Finished reading mode ";
                    }
                    if (xmlReader.name().toString() == "CameraPixelSize")
                    {
						double aCameraPixelSize = xmlReader.readElementText().toDouble();
						anImagingCameraData->setCameraPixelSize(aCameraPixelSize);
					}
					if (xmlReader.name().toString() == "CameraQuantumEfficiency")
                    {
						double aCameraQuantumEfficiency = xmlReader.readElementText().toDouble();
						anImagingCameraData->setCameraQuantumEfficiency(aCameraQuantumEfficiency);
					}

                }
                // qCInfo(KSTARS_EKOS_CAPTURE) << "Read xml data for " + anImagingCameraData->getCameraId();

            }
            else
            {
                qCCritical(KSTARS_EKOS_CAPTURE) << "Read Failed";
                xmlReader.raiseError(QObject::tr("Incorrect file"));
            }
        }
        else
        {
            qCCritical(KSTARS_EKOS_CAPTURE) << "Read Initial Element Failed,";
        }
    }
    else
    {
        qCCritical(KSTARS_EKOS_CAPTURE)
                << "Cannot open file for reading " << file.errorString();
    }

    file.close();
    return 0;
}

int OptimalExposure::FileUtilityCameraData::writeCameraDataFile(OptimalExposure::ImagingCameraData *anImagingCameraData)
{

    if(QDir().mkpath(cameraLocalDataRepository))
    {
        // Replace spaces with "_" in the file name
        QString aCameraDataFile = cameraLocalDataRepository + cameraIdToCameraDataFileName(anImagingCameraData->getCameraId());

        QFile file(aCameraDataFile);
        if(file.open(QIODevice::ReadWrite | QIODevice::Truncate | QIODevice::Text))
        {

            QXmlStreamWriter xmlWriter(&file);
            xmlWriter.setAutoFormatting(true);
            xmlWriter.writeStartDocument();

            xmlWriter.writeStartElement("ExposureCalculatorCameraData");

            xmlWriter.writeTextElement("CameraDataClassVersion", QString::number(anImagingCameraData->getDataClassVersion()));

            xmlWriter.writeTextElement("CameraId", anImagingCameraData->getCameraId() );

            switch(anImagingCameraData->getSensorType())
            {
                case OptimalExposure::SENSORTYPE_MONOCHROME:
                    xmlWriter.writeTextElement("SensorType", QString("MONOCHROME"));
                    break;
                case OptimalExposure::SENSORTYPE_COLOR:
                    xmlWriter.writeTextElement("SensorType", QString("COLOR"));
                    break;
            }

            switch(anImagingCameraData->getGainSelectionType())
            {
                case OptimalExposure::GAIN_SELECTION_TYPE_NORMAL:
                    xmlWriter.writeTextElement("GainSelectionType", QString("NORMAL"));
                    break;
                case OptimalExposure::GAIN_SELECTION_TYPE_ISO_DISCRETE:
                    xmlWriter.writeTextElement("GainSelectionType", QString("ISO_DISCRETE"));
                    break;
                case OptimalExposure::GAIN_SELECTION_TYPE_FIXED:
                    xmlWriter.writeTextElement("GainSelectionType", QString("FIXED"));
                    break;
            }

            xmlWriter.writeStartElement("CameraGainSelections");
            QVector<int> aGainSelectionRange = anImagingCameraData->getGainSelectionRange();
            for(int gs = 0; gs < aGainSelectionRange.count(); gs++)
            {
                // xmlWriter.writeStartElement("GainSelection");
                xmlWriter.writeTextElement("GainSelection", QString::number(anImagingCameraData->getGainSelectionRange()[gs]));
                // xmlWriter.writeEndElement();
            }
            xmlWriter.writeEndElement();

            // Iterate through values of anImagingCameraData->CameraGainReadModeVector

            xmlWriter.writeStartElement("CameraGainReadMode");
            QVector<OptimalExposure::CameraGainReadMode> aCameraGainReadModeVector =
                anImagingCameraData->getCameraGainReadModeVector();
            for(QVector<OptimalExposure::CameraGainReadMode>::iterator readMode = aCameraGainReadModeVector.begin();
                    readMode != aCameraGainReadModeVector.end(); ++readMode)
            {
                xmlWriter.writeTextElement("GainReadModeNumber", QString::number(readMode->getCameraGainReadModeNumber()));
                xmlWriter.writeTextElement("GainReadModeName", readMode->getCameraGainReadModeName());
                //            xmlWriter.writeStartElement("GainReadModeNumber");
                //            xmlWriter.writeEndElement();
                //            xmlWriter.writeStartElement("GainReadModeName");
                //            xmlWriter.writeEndElement();
                // Iterate through values of readMode->CameraGainReadNoiseVector
                xmlWriter.writeStartElement("CameraGainReadNoise");
                QVector<OptimalExposure::CameraGainReadNoise> aCameraGainReadNoiseVector =
                    readMode->getCameraGainReadNoiseVector();

                for(QVector<OptimalExposure::CameraGainReadNoise>::iterator readNoisePair = aCameraGainReadNoiseVector.begin();
                        readNoisePair != aCameraGainReadNoiseVector.end(); ++readNoisePair)
                {
                    xmlWriter.writeStartElement("GainReadNoiseValue");
                    xmlWriter.writeTextElement("Gain", QString::number(readNoisePair->getGain()));
                    xmlWriter.writeTextElement("ReadNoise", QString::number(readNoisePair->getReadNoise()));
                    xmlWriter.writeEndElement();
                }
                xmlWriter.writeEndElement();

            }
            xmlWriter.writeEndElement();
            xmlWriter.writeEndElement();
            xmlWriter.writeEndDocument();
            file.close();

        }
        else
        {
            qCCritical(KSTARS_EKOS_CAPTURE)
                    << "Cannot open camera data file for writing " << file.errorString();
        }
    }
    return 0;


}


// Make example camera data files
//void OptimalExposure::FileUtilityCameraData::buildCameraDataFile()
//{
//    /*
//     * camera naming should try match the Camera device name (cameraId) present in KStars.
//     * A cameraId in KStars appears to consist of a device label (or driver id) and the camera model.
//     *
//     * For example:
//     *   a "ZWO ASI-071 MC Pro"
//     *
//     *   The usb id is "ZWO ASI071MC Pro", but KStars uses a driver prefix ("ZWO CCD") in the cameraId.
//     *   cameraId = "ZWO CCD ASI071MC Pro";
//     *
//     *   The file utility code will replace spaces with underscore on writes, and vice-versa on reads.
//     *   So the xml file would be "ZWO_CCD_ASI071MC_Pro.xml"
//     *
//     *   But for many of these examples the kstars cameraId was not available (I do not own all these cameras).
//     *   So guesses were made for the cameraId of most of these cameras.
//     *
//     *
//     *
//     * Manufacturers:
//        cat indidrivers.xml | grep -i driver | grep -i ccd
//            <driver name="CCD Simulator">indi_simulator_ccd</driver>
//            <driver name="V4L2 CCD">indi_v4l2_ccd</driver>
//            <driver name="Apogee CCD">indi_apogee_ccd</driver>
//            <driver name="ZWO CCD">indi_asi_ccd</driver>
//            <driver name="ZWO Camera">indi_asi_single_ccd</driver>
//            <driver name="Atik">indi_atik_ccd</driver>
//            <driver name="Cam90 CCD">indi_cam90_ccd</driver>
//            <driver name="DSI">indi_dsi_ccd</driver>
//            <driver name="FireFly MV">indi_ffmv_ccd</driver>
//            <driver name="Starfish CCD">indi_fishcamp_ccd</driver>
//            <driver name="FLI Kepler">indi_kepler_ccd</driver>
//            <driver name="FLI CCD">indi_fli_ccd</driver>
//            <driver name="Canon DSLR">indi_canon_ccd</driver>
//            <driver name="Nikon DSLR">indi_nikon_ccd</driver>
//            <driver name="Pentax DSLR">indi_pentax_ccd</driver>
//            <driver name="Sony DSLR">indi_sony_ccd</driver>
//            <driver name="Fuji DSLR">indi_fuji_ccd</driver>
//            <driver name="GPhoto CCD">indi_gphoto_ccd</driver>
//            <driver name="iNova PLX">indi_inovaplx_ccd</driver>
//            <driver name="MI CCD">indi_mi_ccd_usb</driver>
//            <driver name="MI CCD">indi_mi_ccd_eth</driver>
//            <driver name="Nightscape CCD">indi_nightscape_ccd</driver>
//            <driver name="Orion SSG3">indi_orion_ssg3_ccd</driver>
//            <driver name="PlayerOne CCD">indi_playerone_ccd</driver>
//            <driver name="QHY CCD">indi_qhy_ccd</driver>
//            <driver name="QSI CCD">indi_qsi_ccd</driver>
//            <driver name="SBIG CCD">indi_sbig_ccd</driver>
//            <driver name="SBIG CCD">indi_sbig_ccd</driver>
//            <driver name="SVBONY CCD">indi_svbony_ccd</driver>
//            <driver name="SX CCD">indi_sx_ccd</driver>
//            <driver name="Toupcam">indi_toupcam_ccd</driver>
//            <driver name="Altair">indi_altair_ccd</driver>
//            <driver name="Bressercam">indi_bressercam_ccd</driver>
//            <driver name="Mallincam">indi_mallincam_ccd</driver>
//            <driver name="Nncam">indi_nncam_ccd</driver>
//            <driver name="Ogmacam">indi_ogmacam_ccd</driver>
//            <driver name="OmegonPro">indi_omegonprocam_ccd</driver>
//            <driver name="StartshootG">indi_starshootg_ccd</driver>
//            <driver name="Tscam">indi_tscam_ccd</driver>
//            <driver name="Nncam">indi_nncam_ccd</driver>
//            <driver name="INDI Webcam">indi_webcam_ccd</driver>

//        grep -iR "device label" | grep -i " CCD"
//        indidrivers.xml:        <device label="DMK CCD" manufacturer="DMK">
//        indidrivers.xml:        <device label="V4L2 CCD" manufacturer="Web Cameras">
//        indidrivers.xml:        <device label="Apogee CCD" manufacturer="Andor">
//        indidrivers.xml:        <device label="ZWO CCD" mdpd="true" manufacturer="ZWO">
//        indidrivers.xml:        <device label="Atik CCD" mdpd="true" manufacturer="Atik">
//        indidrivers.xml:        <device label="Cam90 CCD" mdpd="true">
//        indidrivers.xml:        <device label="Starfish CCD">
//        indidrivers.xml:        <device label="FLI CCD" manufacturer="Finger Lakes Instruments">
//        indidrivers.xml:        <device label="GPhoto CCD" manufacturer="DSLRs">
//        indidrivers.xml:        <device label="MI CCD (USB)" manufacturer="Moravian Instruments">
//        indidrivers.xml:        <device label="MI CCD (ETH)" manufacturer="Moravian Instruments">
//        indidrivers.xml:        <device label="Nightscape 8300 CCD" mdpd="true" manufacturer="Celestron">
//        indidrivers.xml:        <device label="PlayerOne CCD" mdpd="true" manufacturer="PlayerOne">
//        indidrivers.xml:        <device label="QHY CCD" mdpd="true" manufacturer="QHY">
//        indidrivers.xml:        <device label="QSI CCD" manufacturer="QSI">
//        indidrivers.xml:        <device label="SBIG CCD" manufacturer="Diffraction Limited">
//        indidrivers.xml:        <device label="SVBONY CCD" mdpd="true" manufacturer="SVBONY">
//        indidrivers.xml:        <device label="SX CCD" mdpd="true" manufacturer="Starlight XPress">

//     */

//    // Define and re-use for each camera instance
//    QString cameraId;
//    int readModeNumber = 0;
//    QString readModeName;
//    QVector<OptimalExposure::CameraGainReadNoise> *aCameraGainReadNoiseVector;
//    QVector<OptimalExposure::CameraGainReadMode> *aCameraGainReadModeVector;

//    QVector<int> *gainSelection;
//    OptimalExposure::ImagingCameraData *anImagingCameraData;

//    // This QHY camera has selectable read modes, with distinct read-noise data.
//    // The indi driver may provide a "Read Mode" selectoin on the indi panel.
//    // The xml data files are revised to handle multiple read-modes for a camera
//    // in a single file.

//    // #0 Photographic, #1 High Gain, #2 Extended Full Well and #3 Extended Full Well 2CMSIT.

//    // Research: Is the same read-noise is used in both Mono and Color versions?

//    cameraId = "QHY CCD 268M";
//    aCameraGainReadModeVector = new QVector<OptimalExposure::CameraGainReadMode>();

//    aCameraGainReadNoiseVector = new QVector<OptimalExposure::CameraGainReadNoise>();
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(0, 7.25)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(5, 7.1)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(10, 6.78)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(15, 6.74)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(20, 6.74)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(25, 2.8)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(30, 2.63)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(35, 2.7)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(40, 2.57)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(45, 2.55)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(50, 2.52)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(55, 2.35)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(60, 2.07)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(65, 2.05)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(70, 2.04)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(75, 2.06)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(80, 2.03)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(85, 2.01)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(90, 2)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(95, 1.99)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(100, 2)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(105, 1.97)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(110, 1.96)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(115, 1.95)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(120, 1.94)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(125, 1.93)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(130, 1.92)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(135, 1.89)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(140, 1.9)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(145, 1.89)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(150, 1.85)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(155, 1.82)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(160, 1.81)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(165, 1.8)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(170, 1.79)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(175, 1.75)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(180, 1.74)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(185, 1.7)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(190, 1.68)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(195, 1.67)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(200, 1.62)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(205, 1.59)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(210, 1.56)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(215, 1.53)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(220, 1.48)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(225, 1.44)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(230, 1.41)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(235, 1.36)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(240, 1.32)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(245, 1.26)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(250, 1.17)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(255, 1.12)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(260, 1.03)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(265, 0.92)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(270, 0.79)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(275, 0.62)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(280, 0.6)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(285, 0.7)));

//    readModeNumber = 0;
//    readModeName = "Photographic";

//    aCameraGainReadModeVector->push_back(*(new OptimalExposure::CameraGainReadMode(readModeNumber, readModeName,
//                                           *aCameraGainReadNoiseVector)));

//    aCameraGainReadNoiseVector = new QVector<OptimalExposure::CameraGainReadNoise>();

//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(5, 3.6)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(10, 3.58)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(15, 3.64)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(20, 3.63)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(25, 3.63)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(30, 3.58)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(35, 3.49)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(40, 3.39)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(45, 3.43)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(50, 3.53)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(55, 3.43)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(60, 1.69)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(65, 1.69)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(70, 1.67)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(75, 1.67)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(80, 1.63)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(85, 1.6)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(90, 1.57)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(95, 1.53)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(100, 1.25)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(105, 1.22)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(110, 1.22)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(115, 1.21)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(120, 1.2)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(125, 1.2)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(130, 1.19)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(135, 1.18)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(140, 1.17)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(145, 1.15)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(150, 1.14)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(155, 1.14)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(160, 1.13)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(165, 1.12)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(170, 1.11)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(175, 1.09)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(180, 1.09)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(185, 1.08)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(190, 1.06)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(195, 1.06)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(200, 1.03)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(205, 1.01)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(210, 0.98)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(215, 0.99)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(220, 0.96)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(225, 0.97)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(230, 0.92)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(235, 0.93)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(240, 0.87)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(245, 0.87)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(250, 0.8)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(255, 0.79)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(260, 0.8)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(265, 0.7)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(270, 0.71)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(275, 0.7)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(280, 0.69)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(285, 0.69)));

//    readModeNumber = 1;
//    readModeName = "High Gain";

//    aCameraGainReadModeVector->push_back(*(new OptimalExposure::CameraGainReadMode(readModeNumber, readModeName,
//                                           *aCameraGainReadNoiseVector)));

//    aCameraGainReadNoiseVector = new QVector<OptimalExposure::CameraGainReadNoise>();
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(0, 7.56)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(5, 7.48)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(10, 7.41)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(15, 7.34)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(20, 7.22)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(25, 7.15)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(30, 7.01)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(35, 6.83)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(40, 6.69)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(45, 6.6)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(50, 6.64)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(55, 6.72)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(60, 6.86)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(65, 6.93)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(70, 6.73)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(75, 6.44)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(80, 6.55)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(85, 6.06)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(90, 6.16)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(95, 5.75)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(100, 5.36)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(105, 5.34)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(110, 5.34)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(115, 5.35)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(120, 5.34)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(125, 5.3)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(130, 5.22)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(135, 5.18)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(140, 5.09)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(145, 5.04)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(150, 4.96)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(155, 4.9)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(160, 4.85)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(165, 4.77)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(170, 4.71)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(175, 4.62)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(180, 4.57)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(185, 4.46)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(190, 4.4)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(195, 4.32)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(200, 4.2)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(205, 4.1)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(210, 4.02)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(215, 3.95)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(220, 3.79)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(225, 3.71)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(230, 3.58)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(235, 3.53)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(240, 3.41)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(245, 3.26)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(250, 3.14)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(255, 3.04)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(260, 2.87)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(265, 2.8)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(270, 2.62)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(275, 2.5)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(280, 2.34)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(285, 2.18)));

//    readModeNumber = 2;
//    readModeName = "Extended Full Well";

//    aCameraGainReadModeVector->push_back(*(new OptimalExposure::CameraGainReadMode(readModeNumber, readModeName,
//                                           *aCameraGainReadNoiseVector)));

//    aCameraGainReadNoiseVector = new QVector<OptimalExposure::CameraGainReadNoise>();
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(0, 5.89)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(5, 5.82)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(10, 5.82)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(15, 5.73)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(20, 5.66)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(25, 5.58)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(30, 5.44)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(35, 5.39)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(40, 5.26)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(45, 5.17)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(50, 5.17)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(55, 5.25)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(60, 5.34)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(65, 5.43)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(70, 5.33)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(75, 5.08)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(80, 5.05)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(85, 4.81)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(90, 4.85)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(95, 4.55)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(100, 4.26)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(105, 4.25)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(110, 4.23)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(115, 4.26)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(120, 4.26)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(125, 4.23)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(130, 4.18)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(135, 4.15)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(140, 4.08)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(145, 4.09)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(150, 4.05)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(155, 4.01)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(160, 3.98)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(165, 3.96)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(170, 3.91)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(175, 3.87)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(180, 3.83)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(185, 3.8)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(190, 3.76)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(195, 3.68)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(200, 3.64)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(205, 3.57)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(210, 3.51)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(215, 3.44)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(220, 3.39)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(225, 3.33)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(230, 3.25)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(235, 3.2)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(240, 3.13)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(245, 3.03)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(250, 2.99)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(255, 2.87)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(260, 2.77)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(265, 2.72)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(270, 2.58)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(275, 2.5)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(280, 2.37)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(285, 2.22)));

//    readModeNumber = 3;
//    readModeName = "Extended Full Well 2CMSIT";
//    aCameraGainReadModeVector->push_back(*(new OptimalExposure::CameraGainReadMode(readModeNumber, readModeName,
//                                           *aCameraGainReadNoiseVector)));

//    gainSelection = new QVector<int>( { 0, 285 } );
//    anImagingCameraData = new OptimalExposure::ImagingCameraData(cameraId, OptimalExposure::SENSORTYPE_MONOCHROME,
//            OptimalExposure::GAIN_SELECTION_TYPE_NORMAL, *gainSelection, *aCameraGainReadModeVector);
//    anImagingCameraData->setDataClassVersion(1);
//    OptimalExposure::FileUtilityCameraData::writeCameraDataFile(anImagingCameraData);

//    cameraId = "ZWO CCD ASI071MC Pro";
//    aCameraGainReadNoiseVector = new QVector<OptimalExposure::CameraGainReadNoise>();
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(0, 3.28)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(50, 2.78)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(100, 2.58)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(150, 2.39)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(200, 2.29)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(250, 2.27)));

//    readModeNumber = 0;
//    readModeName = "Standard";
//    aCameraGainReadModeVector = new QVector<OptimalExposure::CameraGainReadMode>();
//    aCameraGainReadModeVector->push_back(*(new OptimalExposure::CameraGainReadMode(readModeNumber, readModeName,
//                                           *aCameraGainReadNoiseVector)));

//    gainSelection = new QVector<int>( { 0, 250 } );
//    anImagingCameraData = new OptimalExposure::ImagingCameraData(cameraId, OptimalExposure::SENSORTYPE_COLOR,
//            OptimalExposure::GAIN_SELECTION_TYPE_NORMAL, *gainSelection, *aCameraGainReadModeVector);

//    anImagingCameraData->setDataClassVersion(1);
//    OptimalExposure::FileUtilityCameraData::writeCameraDataFile(anImagingCameraData);




//    // usb id is "ZWO ASI178MC"
//    cameraId = "ZWO CCD ASI178MC";
//    aCameraGainReadNoiseVector = new QVector<OptimalExposure::CameraGainReadNoise>();
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(0, 2.23)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(50, 1.92)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(100, 1.74)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(150, 1.58)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(200, 1.45)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(250, 1.39)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(300, 1.38)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(400, 1.35)));

//    readModeNumber = 0;
//    readModeName = "Standard";
//    aCameraGainReadModeVector = new QVector<OptimalExposure::CameraGainReadMode>();
//    aCameraGainReadModeVector->push_back(*(new OptimalExposure::CameraGainReadMode(readModeNumber, readModeName,
//                                           *aCameraGainReadNoiseVector)));


//    gainSelection = new QVector<int>( { 0, 400 } );
//    anImagingCameraData = new OptimalExposure::ImagingCameraData(cameraId, OptimalExposure::SENSORTYPE_COLOR,
//            OptimalExposure::GAIN_SELECTION_TYPE_NORMAL, *gainSelection, *aCameraGainReadModeVector);

//    anImagingCameraData->setDataClassVersion(1);
//    OptimalExposure::FileUtilityCameraData::writeCameraDataFile(anImagingCameraData);


//    cameraId = "ZWO CCD ASI6200MM";
//    aCameraGainReadNoiseVector = new QVector<OptimalExposure::CameraGainReadNoise>();

//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(0, 3.6)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(50, 3.5)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(99, 3.38)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(100, 1.55)));  // pronounced step
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(150, 1.5)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(200, 1.45)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(250, 1.42)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(300, 1.40)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(350, 1.41)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(400, 1.32)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(450, 1.39)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(470, 1.25)));

//    readModeNumber = 0;
//    readModeName = "Standard";
//    aCameraGainReadModeVector = new QVector<OptimalExposure::CameraGainReadMode>();
//    aCameraGainReadModeVector->push_back(*(new OptimalExposure::CameraGainReadMode(readModeNumber, readModeName,
//                                           *aCameraGainReadNoiseVector)));

//    gainSelection = new QVector<int>( { 0, 470 } );

//    // qCInfo(KSTARS_EKOS_CAPTURE) << "aCameraGainReadNoiseVector size: " << aCameraGainReadNoiseVector->size();
//    anImagingCameraData = new OptimalExposure::ImagingCameraData(cameraId, OptimalExposure::SENSORTYPE_MONOCHROME,
//            OptimalExposure::GAIN_SELECTION_TYPE_NORMAL, *gainSelection, *aCameraGainReadModeVector);
//    anImagingCameraData->setDataClassVersion(1);
//    OptimalExposure::FileUtilityCameraData::writeCameraDataFile(anImagingCameraData);

//    cameraId = "ZWO CCD ASI1600MM";
//    aCameraGainReadNoiseVector = new QVector<OptimalExposure::CameraGainReadNoise>();
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(0, 3.62)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(50, 2.5)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(100, 1.85)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(150, 1.72)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(200, 1.40)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(250, 1.32)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(300, 1.30)));

//    readModeNumber = 0;
//    readModeName = "Standard";
//    aCameraGainReadModeVector = new QVector<OptimalExposure::CameraGainReadMode>();
//    aCameraGainReadModeVector->push_back(*(new OptimalExposure::CameraGainReadMode(readModeNumber, readModeName,
//                                           *aCameraGainReadNoiseVector)));


//    gainSelection = new QVector<int>( { 0, 300 } );
//    anImagingCameraData = new OptimalExposure::ImagingCameraData(cameraId, OptimalExposure::SENSORTYPE_MONOCHROME,
//            OptimalExposure::GAIN_SELECTION_TYPE_NORMAL, *gainSelection, *aCameraGainReadModeVector);
//    anImagingCameraData->setDataClassVersion(1);
//    OptimalExposure::FileUtilityCameraData::writeCameraDataFile(anImagingCameraData);


//    cameraId = "ZWO CCD ASI224MC";
//    aCameraGainReadNoiseVector = new QVector<OptimalExposure::CameraGainReadNoise>();
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(0, 3.09)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(20, 2.71)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(59, 2.26)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(60, 1.6)));    // pronounced step
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(70, 1.54)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(100, 1.34)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(150, 1.14)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(250, 0.93)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(300, 0.86)));

//    readModeNumber = 0;
//    readModeName = "Standard";
//    aCameraGainReadModeVector = new QVector<OptimalExposure::CameraGainReadMode>();
//    aCameraGainReadModeVector->push_back(*(new OptimalExposure::CameraGainReadMode(readModeNumber, readModeName,
//                                           *aCameraGainReadNoiseVector)));


//    gainSelection = new QVector<int>( { 0, 300 } );
//    anImagingCameraData = new OptimalExposure::ImagingCameraData(cameraId, OptimalExposure::SENSORTYPE_COLOR,
//            OptimalExposure::GAIN_SELECTION_TYPE_NORMAL, *gainSelection, *aCameraGainReadModeVector);
//    anImagingCameraData->setDataClassVersion(1);
//    OptimalExposure::FileUtilityCameraData::writeCameraDataFile(anImagingCameraData);

//    cameraId = "ZWO CCD ASI120MC";  // formerly "ZWOASI-120MC" but now assuming a standard naming convention from ZWO
//    aCameraGainReadNoiseVector = new QVector<OptimalExposure::CameraGainReadNoise>();
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(0, 6.35)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(10, 6.41)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(15, 6.5)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(16, 4.63))); // pronounced step
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(20, 4.67))); // a rise after the step down
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(31, 4.7)));  // another step
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(32, 3.87)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(47, 3.95))); // rise after the step down
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(48, 3.7)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(63, 3.68))); // another step
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(64, 3.54)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(75, 3.66))); // rise after a step down
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(87, 3.62)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(100, 3.62)));

//    readModeNumber = 0;
//    readModeName = "Standard";

//    aCameraGainReadModeVector = new QVector<OptimalExposure::CameraGainReadMode>();
//    aCameraGainReadModeVector->push_back(*(new OptimalExposure::CameraGainReadMode(readModeNumber, readModeName,
//                                           *aCameraGainReadNoiseVector)));


//    gainSelection = new QVector<int>( { 0, 100 } );
//    anImagingCameraData = new OptimalExposure::ImagingCameraData(cameraId, OptimalExposure::SENSORTYPE_COLOR,
//            OptimalExposure::GAIN_SELECTION_TYPE_NORMAL, *gainSelection, *aCameraGainReadModeVector);
//    anImagingCameraData->setDataClassVersion(1);
//    OptimalExposure::FileUtilityCameraData::writeCameraDataFile(anImagingCameraData);

//    cameraId = "ZWO CCD ASI183MC Pro";  // (The same read-noise is used in both Mono and Color)
//    aCameraGainReadNoiseVector = new QVector<OptimalExposure::CameraGainReadNoise>();
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(0, 3.00)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(50, 2.6)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(100, 2.22)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(150, 2.02)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(200, 1.84)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(250, 1.75)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(270, 1.68)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(300, 1.58)));

//    readModeNumber = 0;
//    readModeName = "Standard";

//    aCameraGainReadModeVector = new QVector<OptimalExposure::CameraGainReadMode>();
//    aCameraGainReadModeVector->push_back(*(new OptimalExposure::CameraGainReadMode(readModeNumber, readModeName,
//                                           *aCameraGainReadNoiseVector)));


//    gainSelection = new QVector<int>( { 0, 300 } );
//    anImagingCameraData = new OptimalExposure::ImagingCameraData(cameraId, OptimalExposure::SENSORTYPE_COLOR,
//            OptimalExposure::GAIN_SELECTION_TYPE_NORMAL, *gainSelection, *aCameraGainReadModeVector);
//    anImagingCameraData->setDataClassVersion(1);
//    OptimalExposure::FileUtilityCameraData::writeCameraDataFile(anImagingCameraData);

//    cameraId = "ZWO CCD ASI183MM Pro";  // The same read-noise is used in both Mono and Color
//    aCameraGainReadNoiseVector = new QVector<OptimalExposure::CameraGainReadNoise>();
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(0, 3.00)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(50, 2.6)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(100, 2.22)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(150, 2.02)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(200, 1.84)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(250, 1.75)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(270, 1.68)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(300, 1.58)));

//    readModeNumber = 0;
//    readModeName = "Standard";

//    aCameraGainReadModeVector = new QVector<OptimalExposure::CameraGainReadMode>();
//    aCameraGainReadModeVector->push_back(*(new OptimalExposure::CameraGainReadMode(readModeNumber, readModeName,
//                                           *aCameraGainReadNoiseVector)));


//    gainSelection = new QVector<int>( { 0, 300 } );
//    anImagingCameraData = new OptimalExposure::ImagingCameraData(cameraId, OptimalExposure::SENSORTYPE_MONOCHROME,
//            OptimalExposure::GAIN_SELECTION_TYPE_NORMAL, *gainSelection, *aCameraGainReadModeVector);
//    anImagingCameraData->setDataClassVersion(1);
//    OptimalExposure::FileUtilityCameraData::writeCameraDataFile(anImagingCameraData);



//    cameraId = "ZWO CCD ASI2600MM Pro"; // formerly "ZWOASI-2600MM" but now assuming a standard naming convention from ZWO
//    aCameraGainReadNoiseVector = new QVector<OptimalExposure::CameraGainReadNoise>();
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(0, 3.28)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(50, 3.06)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(99, 2.88)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(100, 1.46)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(150, 1.42)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(200, 1.42)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(250, 1.35)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(300, 1.35)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(350, 1.25)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(400, 1.17)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(450, 1.11)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(460, 1.03)));

//    readModeNumber = 0;
//    readModeName = "Standard";
//    aCameraGainReadModeVector = new QVector<OptimalExposure::CameraGainReadMode>();
//    aCameraGainReadModeVector->push_back(*(new OptimalExposure::CameraGainReadMode(readModeNumber, readModeName,
//                                           *aCameraGainReadNoiseVector)));


//    gainSelection = new QVector<int>( { 0, 460 } );
//    anImagingCameraData = new OptimalExposure::ImagingCameraData(cameraId, OptimalExposure::SENSORTYPE_MONOCHROME,
//            OptimalExposure::GAIN_SELECTION_TYPE_NORMAL, *gainSelection, *aCameraGainReadModeVector);
//    anImagingCameraData->setDataClassVersion(1);
//    OptimalExposure::FileUtilityCameraData::writeCameraDataFile(anImagingCameraData);



//    cameraId = "ZWO CCD ASI290MM Mini"; //
//    aCameraGainReadNoiseVector = new QVector<OptimalExposure::CameraGainReadNoise>();

//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(0, 3.23)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(50, 2.66)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(59, 2.62)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(60, 1.66)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(100, 1.45)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(150, 1.29)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(200, 1.13)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(250, 1.11)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(300, 1.01)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(350, 0.96)));

//    readModeNumber = 0;
//    readModeName = "Standard";
//    aCameraGainReadModeVector = new QVector<OptimalExposure::CameraGainReadMode>();
//    aCameraGainReadModeVector->push_back(*(new OptimalExposure::CameraGainReadMode(readModeNumber, readModeName,
//                                           *aCameraGainReadNoiseVector)));


//    gainSelection = new QVector<int>( { 0, 350 } );
//    anImagingCameraData = new OptimalExposure::ImagingCameraData(cameraId, OptimalExposure::SENSORTYPE_MONOCHROME,
//            OptimalExposure::GAIN_SELECTION_TYPE_NORMAL, *gainSelection, *aCameraGainReadModeVector);
//    anImagingCameraData->setDataClassVersion(1);
//    OptimalExposure::FileUtilityCameraData::writeCameraDataFile(anImagingCameraData);








//    cameraId = "QHY CCD 533M";  // Assuming the QHY cameraId appear similar to ZWO
//    aCameraGainReadNoiseVector = new QVector<OptimalExposure::CameraGainReadNoise>();
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(0, 3.34)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(5, 3.3)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(10, 3.32)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(15, 3.4)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(20, 3.3)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(25, 3.22)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(30, 3.19)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(35, 3.09)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(40, 3.18)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(45, 3.12)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(50, 3.05)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(55, 2.93)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(60, 1.66))); // pronounced step
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(65, 1.6)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(70, 1.56)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(75, 1.54)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(80, 1.51)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(85, 1.45)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(90, 1.4)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(95, 1.34)));

//    readModeNumber = 0;
//    readModeName = "Standard";

//    aCameraGainReadModeVector = new QVector<OptimalExposure::CameraGainReadMode>();
//    aCameraGainReadModeVector->push_back(*(new OptimalExposure::CameraGainReadMode(readModeNumber, readModeName,
//                                           *aCameraGainReadNoiseVector)));


//    gainSelection = new QVector<int>( { 0, 95 } );
//    anImagingCameraData = new OptimalExposure::ImagingCameraData(cameraId, OptimalExposure::SENSORTYPE_MONOCHROME,
//            OptimalExposure::GAIN_SELECTION_TYPE_NORMAL, *gainSelection, *aCameraGainReadModeVector);
//    anImagingCameraData->setDataClassVersion(1);
//    OptimalExposure::FileUtilityCameraData::writeCameraDataFile(anImagingCameraData);



//    cameraId = "MI CCD C3"; // Not sure how Moravian cameras appear on USB or KStars, need research.
//    aCameraGainReadNoiseVector = new QVector<OptimalExposure::CameraGainReadNoise>();
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(0, 3.51)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(2749, 3.15)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(2750, 1.46)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(4030, 1.39)));

//    readModeNumber = 0;
//    readModeName = "Standard";

//    aCameraGainReadModeVector = new QVector<OptimalExposure::CameraGainReadMode>();
//    aCameraGainReadModeVector->push_back(*(new OptimalExposure::CameraGainReadMode(readModeNumber, readModeName,
//                                           *aCameraGainReadNoiseVector)));

//    gainSelection = new QVector<int>( { 0, 4030 } );
//    anImagingCameraData = new OptimalExposure::ImagingCameraData(cameraId, OptimalExposure::SENSORTYPE_MONOCHROME,
//            OptimalExposure::GAIN_SELECTION_TYPE_NORMAL, *gainSelection, *aCameraGainReadModeVector);
//    anImagingCameraData->setDataClassVersion(1);
//    OptimalExposure::FileUtilityCameraData::writeCameraDataFile(anImagingCameraData);


//    // DSLR Read-noise for ISO values data found at:   https://www.photonstophotos.net/Charts/RN_e.htm

//    // For a Nikon D5000 usb id reads "Nikon Corp. D5000", Kstars shows "Nikon DSLR DSC D5000 (PTP mode)"
//    cameraId = "Nikon DSLR DSC D5000 (PTP mode)";
//    aCameraGainReadNoiseVector = new QVector<OptimalExposure::CameraGainReadNoise>();
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(100, 7.727)));  // log2 value 2.95
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(126, 7.674)));  // log2 value 2.94
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(159, 7.727)));  // log2 value 2.95
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(200, 6.320)));  // log2 value 2.36)
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(251, 5.134)));  // log2 value 2.36)
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(318, 5.169)));  // log2 value 2.37
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(400, 4.532)));  // log2 value 2.18
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(503, 4.79)));  // log2 value 2.26
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(636, 5.169)));  // log2 value 2.37
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(800, 4.925)));  // log2 value 2.3
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(1006, 4.891)));  // log2 value 2.29
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(1273, 4.724)));  // log2 value 2.24
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(1600, 4.5)));  // log2 value 2.17
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(2011, 4.028)));  // log2 value 2.01
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(2546, 3.34)));  // log2 value 1.74
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(3200, 2.969)));  // log2 value 1.57
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(4022, 2.828)));  // log2 value 1.5
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(5091, 2.789)));  // log2 value 1.48
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(6400, 2.732)));  // log2 value 1.45

//    readModeNumber = 0;
//    readModeName = "Standard";
//    aCameraGainReadModeVector = new QVector<OptimalExposure::CameraGainReadMode>();
//    aCameraGainReadModeVector->push_back(*(new OptimalExposure::CameraGainReadMode(readModeNumber, readModeName,
//                                           *aCameraGainReadNoiseVector)));


//    //  Unfortunately this data from www.photonstophotos.net does not align precisely with the discrete values for the ISO in the camera.
//    //  So, the proof of concept code, which needs a list of iso values will use an array from the file.
//    //  The following array would likely come from Ekos with getActiveChip()->getISOList() ?

//    gainSelection = new QVector<int>( { 100, 125, 160, 250, 320, 400, 500, 640, 800, 1000, 1250, 1600, 2000, 2500, 3200, 4000, 5000, 6400 } );
//    anImagingCameraData = new OptimalExposure::ImagingCameraData(cameraId, OptimalExposure::SENSORTYPE_COLOR,
//            OptimalExposure::GAIN_SELECTION_TYPE_ISO_DISCRETE, *gainSelection, *aCameraGainReadModeVector);
//    anImagingCameraData->setDataClassVersion(1);
//    OptimalExposure::FileUtilityCameraData::writeCameraDataFile(anImagingCameraData);


//    // Driver id is "Canon DSLR"
//    cameraId = "Canon DSLR EOS Ra";  // Not sure how Canon cameras appear on USB, or KStars need research.

//    aCameraGainReadNoiseVector = new QVector<OptimalExposure::CameraGainReadNoise>();
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(100, 9.063))); // log2 value 3.18
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(126, 10.2)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(159, 5.58)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(200, 4.96)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(251, 5.46)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(318, 3.14)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(400, 3.18)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(503, 3.34)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(636, 2.41)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(800, 2.41)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(1006, 2.48)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(1273, 1.92)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(1600, 1.91)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(2011, 1.91)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(2546, 1.65)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(3200, 1.67)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(4022, 1.68)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(5091, 1.4)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(6400, 1.38)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(8045, 1.36)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(10183, 1.38)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(12800, 1.38)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(16090, 1.38)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(20366, 1.34)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(25600, 1.33)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(32180, 1.35)));
//    aCameraGainReadNoiseVector->push_back(*(new OptimalExposure::CameraGainReadNoise(40731, 1.34)));


//    readModeNumber = 0;
//    readModeName = "Standard";

//    aCameraGainReadModeVector = new QVector<OptimalExposure::CameraGainReadMode>();
//    aCameraGainReadModeVector->push_back(*(new OptimalExposure::CameraGainReadMode(readModeNumber, readModeName,
//                                           *aCameraGainReadNoiseVector)));

//    gainSelection = new QVector<int>( { 100, 125, 160, 200, 250, 320, 400, 500, 640, 800, 1000, 1250, 1600, 2000, 2500, 3200, 4000, 5000, 6400, 8000, 10200, 12800, 16000, 20400, 25600, 32000, 40000  } );

//    anImagingCameraData = new OptimalExposure::ImagingCameraData(cameraId, OptimalExposure::SENSORTYPE_COLOR,
//            OptimalExposure::GAIN_SELECTION_TYPE_ISO_DISCRETE, *gainSelection, *aCameraGainReadModeVector);
//    anImagingCameraData->setDataClassVersion(1);
//    OptimalExposure::FileUtilityCameraData::writeCameraDataFile(anImagingCameraData);



//}

