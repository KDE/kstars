#include "constellationsart.h"
#include "kstars/auxiliary/ksfilereader.h"

ConstellationsArt::ConstellationsArt(int serial)
{
    //artDisplayed(0);

    // Find constellation art file and open it. If it doesn't exist, output an error message.

    KSFileReader fileReader;
    if ( ! fileReader.open("constellationsart.txt" ) )
    {
        qDebug() << "No constellationsart.txt file found for sky culture";
        return;
    }

    while ( fileReader.hasMoreLines() ) {
        QString line;

        line = fileReader.readLine();
        if( line.isEmpty() )
            continue;
        QChar mode = line.at( 0 );
        //ignore lines beginning with "#":
        if( mode == '#' )
            continue;

        int rank=line.mid(0,2).trimmed().toInt(); //reads the first column from constellationart.txt

        //Read pixel coordinates and HD number of star 1
        x1 = line.mid( 3, 3 ).trimmed().toInt();
        y1 = line.mid( 7, 3 ).trimmed().toInt();
        hd1 = line.mid( 11, 6 ).trimmed().toInt();


        //Read pixel coordinates and HD number of star 2
        x2 = line.mid( 18, 3 ).trimmed().toInt();
        y2 = line.mid( 22, 3 ).trimmed().toInt();
        hd2 = line.mid( 26, 6 ).trimmed().toInt();


        //Read pixel coordinates and HD number of star
        x3 = line.mid( 33, 3 ).trimmed().toInt();
        y3 = line.mid( 37, 3 ).trimmed().toInt();
        hd3 = line.mid( 41, 6 ).trimmed().toInt();

        abbrev = line.mid( 48, 3 );
        imageFileName  = line.mid( 52 ).trimmed();

        qDebug()<< "Serial number:"<<rank<<"x1:"<<x1<<"y1:"<<y1<<"HD1:"<<hd1<<"x2:"<<x2<<"y2:"<<y2<<"HD2:"<<hd2<<"x3:"<<x3<<"y3:"<<y3<<"HD3:"<<hd3<<"abbreviation:"<<abbrev<<"name"<<imageFileName;
        //testing to see if the file opens and outputs the data

    }

}

ConstellationsArt::~ConstellationsArt()
{
}
