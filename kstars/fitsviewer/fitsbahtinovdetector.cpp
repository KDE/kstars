/*
    SPDX-FileCopyrightText: 2020 Patrick Molenaar <pr_molenaar@hotmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later

    Some code fragments were adapted from Peter Kirchgessner's FITS plugin
    SPDX-FileCopyrightText: Peter Kirchgessner <http://members.aol.com/pkirchg>
*/

#include <math.h>
#include <cmath>

#include "fits_debug.h"
#include "fitsbahtinovdetector.h"
#include "hough/houghline.h"

#include <QElapsedTimer>
#include <QtConcurrent>

//void FITSBahtinovDetector::configure(const QString &setting, const QVariant &value)
//{
//    if (!setting.compare("NUMBER_OF_AVERAGE_ROWS", Qt::CaseInsensitive))
//    {
//        if (value.canConvert <int> ())
//        {
//            NUMBER_OF_AVERAGE_ROWS = value.value <int> ();

//            // Validate number of average rows value
//            if (NUMBER_OF_AVERAGE_ROWS % 2 == 0)
//            {
//                NUMBER_OF_AVERAGE_ROWS--;
//                qCInfo(KSTARS_FITS) << "Warning, number of rows must be an odd number, correcting number of rows to "
//                                    << NUMBER_OF_AVERAGE_ROWS;
//            }
//            // Rows must be a positive number!
//            if (NUMBER_OF_AVERAGE_ROWS < 1)
//            {
//                NUMBER_OF_AVERAGE_ROWS = 1;
//                qCInfo(KSTARS_FITS) << "Warning, number of rows must be positive correcting number of rows to "
//                                    << NUMBER_OF_AVERAGE_ROWS;
//            }
//        }
//    }
//}

QFuture<bool> FITSBahtinovDetector::findSources(QRect const &boundary)
{
    FITSImage::Statistic const &stats = m_ImageData->getStatistics();
    switch (stats.dataType)
    {
        case TSHORT:
            return QtConcurrent::run(this, &FITSBahtinovDetector::findBahtinovStar<int16_t>, boundary);

        case TUSHORT:
            return QtConcurrent::run(this, &FITSBahtinovDetector::findBahtinovStar<uint16_t>, boundary);

        case TLONG:
            return QtConcurrent::run(this, &FITSBahtinovDetector::findBahtinovStar<int32_t>, boundary);

        case TULONG:
            return QtConcurrent::run(this, &FITSBahtinovDetector::findBahtinovStar<uint32_t>, boundary);

        case TFLOAT:
            return QtConcurrent::run(this, &FITSBahtinovDetector::findBahtinovStar<float>, boundary);

        case TLONGLONG:
            return QtConcurrent::run(this, &FITSBahtinovDetector::findBahtinovStar<int64_t>, boundary);

        case TDOUBLE:
            return QtConcurrent::run(this, &FITSBahtinovDetector::findBahtinovStar<double>, boundary);

        default:
        case TBYTE:
            return QtConcurrent::run(this, &FITSBahtinovDetector::findBahtinovStar<uint8_t>, boundary);

    }
}

template <typename T>
bool FITSBahtinovDetector::findBahtinovStar(const QRect &boundary)
{
    if (boundary.isEmpty())
        return false;

    QList<Edge*> starCenters;
    int subX = qMax(0, boundary.isNull() ? 0 : boundary.x());
    int subY = qMax(0, boundary.isNull() ? 0 : boundary.y());
    int subW = (boundary.isNull() ? m_ImageData->width() : boundary.width());
    int subH = (boundary.isNull() ? m_ImageData->height() : boundary.height());

    int BBP = m_ImageData->getBytesPerPixel();
    uint16_t dataWidth = m_ImageData->width();

    // #1 Find offsets
    uint32_t size   = subW * subH;
    uint32_t offset = subX + subY * dataWidth;

    // #2 Create new buffer
    auto * buffer = new uint8_t[size * BBP];
    // If there is no offset, copy whole buffer in one go
    if (offset == 0)
    {
        memcpy(buffer, m_ImageData->getImageBuffer(), size * BBP);
    }
    else
    {
        uint8_t * dataPtr = buffer;
        const uint8_t * origDataPtr = m_ImageData->getImageBuffer();
        // Copy data line by line
        for (int height = subY; height < (subY + subH); height++)
        {
            uint32_t lineOffset = (subX + height * dataWidth) * BBP;
            memcpy(dataPtr, origDataPtr + lineOffset, subW * BBP);
            dataPtr += (subW * BBP);
        }
    }

    // #3 Create new FITSData to hold it
    FITSImage::Statistic stats;
    stats.width = subW;
    stats.height = subH;
    stats.dataType = m_ImageData->getStatistics().dataType;
    stats.bytesPerPixel = BBP;
    stats.samples_per_channel = size;
    FITSData* boundedImage = new FITSData();
    boundedImage->restoreStatistics(stats);
    boundedImage->setProperty("dataType", stats.dataType);

    // #4 Set image buffer
    boundedImage->setImageBuffer(buffer);

    boundedImage->calculateStats(true);

    QElapsedTimer timer1;

    // Rotate image 180 degrees in steps of 1 degree
    QMap<int, BahtinovLineAverage> lineAveragesPerAngle;
    const int steps = 180;
    double radPerStep = M_PI / steps;

    timer1.start();

    for (int angle = 0; angle < steps; angle++)
    {
        // TODO Apply multi threading to speed up calculation
        BahtinovLineAverage lineAverage = calculateMaxAverage<T>(boundedImage, angle);
        // Store line average in map
        lineAveragesPerAngle.insert(angle, lineAverage);
    }

    qCDebug(KSTARS_FITS) << "Getting max average for all 180 rotations took" << timer1.elapsed() << "milliseconds";

    // Not needed anymore
    delete boundedImage;

    // Calculate Bahtinov angles
    QVector<HoughLine*> bahtinov_angles;

    // For all three Bahtinov angles
    for (int index1 = 0; index1 < 3; index1++)
    {
        double maxAverage = 0.0;
        double maxAngle = 0.0;
        int maxAverageOffset = 0;
        for (int angle = 0; angle < steps; angle++)
        {
            BahtinovLineAverage lineAverage = lineAveragesPerAngle[angle];
            if (lineAverage.average > maxAverage)
            {
                maxAverage = lineAverage.average;
                maxAverageOffset = lineAverage.offset;
                maxAngle = angle;
            }
        }
        HoughLine* pHoughLine = new HoughLine(maxAngle * radPerStep, maxAverageOffset, subW, subH, maxAverage);
        if (pHoughLine != nullptr)
        {
            bahtinov_angles.append(pHoughLine);
        }

        // Remove data around peak to prevent it from being detected again
        int minBahtinovAngleOffset = 18;
        for (int subAngle = maxAngle - minBahtinovAngleOffset; subAngle < maxAngle + minBahtinovAngleOffset; subAngle++)
        {
            int angleInRange = subAngle;
            if (angleInRange < 0)
            {
                angleInRange += 180;
            }
            if (angleInRange >= 180)
            {
                angleInRange -= 180;
            }
            lineAveragesPerAngle.remove(angleInRange);
        }
    }

    // Proceed with focus offset calculation, but only when at least 3 lines have been detected
    QVector<HoughLine*> top3Lines;
    if (bahtinov_angles.size() >= 3)
    {
        HoughLine::getSortedTopThreeLines(bahtinov_angles, top3Lines);

        // Debug output
        qCDebug(KSTARS_FITS) << "Sorted bahtinov angles:";
        foreach (HoughLine* ln, top3Lines)
        {
            ln->printHoughLine();
        }

        // Determine intersection between outer lines
        HoughLine* oneLine = top3Lines[0];
        HoughLine* otherLine = top3Lines[2];
        QPointF intersection;
        HoughLine::IntersectResult result = oneLine->Intersect(*otherLine, intersection);
        if (result == HoughLine::INTERESECTING)
        {

            qCDebug(KSTARS_FITS) << "Intersection: " << intersection.x() << ", " << intersection.y();

            // Determine offset between intersection and middle line
            HoughLine* midLine = top3Lines[1];
            QPointF intersectionOnMidLine;
            double distance;
            if (midLine->DistancePointLine(intersection, intersectionOnMidLine, distance))
            {
                qCDebug(KSTARS_FITS) << "Distance between intersection and midline is " << distance
                                     << " at mid line point " << intersectionOnMidLine.x() << ", "
                                     << intersectionOnMidLine.y();

                // Add star center to selected stars
                // Maximum Radius
                int maxR = qMin(subW - 1, subH - 1) / 2;
                BahtinovEdge* center  = new BahtinovEdge();
                center->width = maxR / 3;
                center->x     = subX + intersection.x();
                center->y     = subY + intersection.y();
                // Set distance value in HFR
                center->HFR   = distance;

                center->offset.setX(subX + intersectionOnMidLine.x());
                center->offset.setY(subY + intersectionOnMidLine.y());
                oneLine->Offset(subX, subY);
                midLine->Offset(subX, subY);
                otherLine->Offset(subX, subY);
                center->line.append(*oneLine);
                center->line.append(*midLine);
                center->line.append(*otherLine);
                starCenters.append(center);
            }
            else
            {
                qCWarning(KSTARS_FITS) << "Closest point does not fall within the line segment.";
            }
        }
        else
        {
            qCWarning(KSTARS_FITS) << "Lines are not intersecting (result: " << result << ")";
        }
    }

    // Clean up Bahtinov line array (of pointers) as they are no longer needed
    for (int index = 0; index < bahtinov_angles.size(); index++)
    {
        HoughLine* pLineAverage = bahtinov_angles[index];
        if (pLineAverage != nullptr)
        {
            delete pLineAverage;
        }
    }
    bahtinov_angles.clear();

    // Clean up line averages array as they are no longer needed
    lineAveragesPerAngle.clear();

    top3Lines.clear();

    m_ImageData->setStarCenters(starCenters);

    return true;
}

template <typename T>
BahtinovLineAverage FITSBahtinovDetector::calculateMaxAverage(const FITSData *data, int angle)
{
    int BBP = data->getBytesPerPixel();
    int size = data->getStatistics().samples_per_channel;
    int width = data->width();
    int height = data->height();
    int numChannels = data->channels();

    BahtinovLineAverage lineAverage;
    auto * rotimage = new T[size * BBP];

    rotateImage(data, angle, rotimage);

    // Calculate average pixel value for each row
    auto * rotBuffer = reinterpret_cast<T *>(rotimage);

    //    printf("Angle;%d;Width;%d;Height;%d;Rows;%d;;RowSum;", angle, width, height, NUMBER_OF_AVERAGE_ROWS);

    int NUMBER_OF_AVERAGE_ROWS = getValue("NUMBER_OF_AVERAGE_ROWS", 1).toInt();
    if (NUMBER_OF_AVERAGE_ROWS % 2 == 0)
    {
        NUMBER_OF_AVERAGE_ROWS--;
        qCWarning(KSTARS_FITS) << "Warning, number of rows must be an odd number, correcting number of rows to "
                               << NUMBER_OF_AVERAGE_ROWS;
    }
    // Rows must be a positive number!
    if (NUMBER_OF_AVERAGE_ROWS < 1)
    {
        NUMBER_OF_AVERAGE_ROWS = 1;
        qCWarning(KSTARS_FITS) << "Warning, number of rows must be positive correcting number of rows to "
                               << NUMBER_OF_AVERAGE_ROWS;
    }

    for (int y = 0; y < height; y++)
    {
        int yMin = y - ((NUMBER_OF_AVERAGE_ROWS - 1) / 2);
        int yMax = y + ((NUMBER_OF_AVERAGE_ROWS - 1) / 2);

        unsigned long multiRowSum = 0;
        // Calculate average over multiple rows
        for (int y1 = yMin; y1 <= yMax; y1++)
        {
            int y2 = y1;
            if (y2 < 0)
            {
                y2 += height;
            }
            if (y2 >= height)
            {
                y2 -= height;
            }
            if (y2 < 0 || y2 >= height)
            {
                qCWarning(KSTARS_FITS) << "Y still out of bounds: 0 <=" << y2 << "<" << height;
            }

            for (int x1 = 0; x1 < width; x1++)
            {
                int index = y2 * width + x1;
                unsigned long channelAverage = 0;
                for (int i = 0; i < numChannels; i++)
                {
                    int offset = size * i;
                    channelAverage += rotBuffer[index + offset];
                }
                multiRowSum += qRound(channelAverage / static_cast<double>(numChannels));
            }
        }
        //        printf("%lu;", multiRowSum);

        double average = multiRowSum / static_cast<double>(width * NUMBER_OF_AVERAGE_ROWS);
        if (average > lineAverage.average)
        {
            lineAverage.average = average;
            lineAverage.offset = y;
        }
    }
    //    printf(";;MaxAverage;%.3f;MaxAverageIndex;%lu\r\n", lineAverage.average, lineAverage.offset);
    //    fflush(stdout);

    rotBuffer = nullptr;
    delete[] rotimage;
    rotimage = nullptr;

    return lineAverage;
}

/** Rotate an image by angle degrees.
 * verbose generates extra info on stdout.
 * return true if successful and rotated image.
 * @param angle The angle over which the image needs to be rotated
 * @param rotImage The image that needs to be rotated, also the rotated image return value
 */
template <typename T>
bool FITSBahtinovDetector::rotateImage(const FITSData *data, int angle, T * rotImage)
{
    int BBP = data->getBytesPerPixel();
    int size = data->getStatistics().samples_per_channel;
    int width = data->width();
    int height = data->height();
    int numChannels = data->channels();

    int hx, hy;
    size_t bufferSize;

    /* Check allocation buffer for rotated image */
    if (rotImage == nullptr)
    {
        qWarning() << "No memory allocated for rotated image buffer!";
        return false;
    }

    while (angle < 0)
    {
        angle = angle + 360;
    }
    while (angle >= 360)
    {
        angle = angle - 360;
    }

    hx = qFloor((width + 1) / 2.0);
    hy = qFloor((height + 1) / 2.0);

    bufferSize = size * numChannels * BBP;
    memset(rotImage, 0, bufferSize);

    auto * rotBuffer = reinterpret_cast<T *>(rotImage);
    auto * buffer = reinterpret_cast<const T *>(data->getImageBuffer());

    double innerCircleRadius = (0.5 * qSqrt(2.0) * qMin(hx, hy));
    double angleInRad = angle * M_PI / 180.0;
    double sinAngle = qSin(angleInRad);
    double cosAngle = qCos(angleInRad);
    int leftEdge = qCeil(hx - innerCircleRadius);
    int rightEdge = qFloor(hx + innerCircleRadius);
    int topEdge = qCeil(hy - innerCircleRadius);
    int bottomEdge = qFloor(hy + innerCircleRadius);

    for (int i = 0; i < numChannels; i++)
    {
        int offset = size * i;
        for (int x1 = leftEdge; x1 < rightEdge; x1++)
        {
            for (int y1 = topEdge; y1 < bottomEdge; y1++)
            {
                // translate point back to origin:
                double x2 = x1 - hx;
                double y2 = y1 - hy;

                // rotate point
                double xnew = x2 * cosAngle - y2 * sinAngle;
                double ynew = x2 * sinAngle + y2 * cosAngle;

                // translate point back:
                x2 = xnew + hx;
                y2 = ynew + hy;

                int orgIndex = y1 * height + x1;
                int newIndex = qRound(y2) * height + qRound(x2);

                if (newIndex >= 0 && newIndex < static_cast<int>(bufferSize))
                {
                    rotBuffer[newIndex + offset] = buffer[orgIndex + offset];
                } // else index out of bounds, do not update pixel
            }
        }
    }

    return true;
}
