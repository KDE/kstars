#ifndef KSNEWSTUFF_H
#define KSNEWSTUFF_H

#include <klocale.h>
#include <kdebug.h>
#include <qobject.h>

#include <knewstuff/knewstuff.h>

class KDirWatch;
class KStars;

class KSNewStuff : public QObject, public KNewStuff
{
	Q_OBJECT
	public:
		KSNewStuff( QWidget *parent = 0 );
		bool install( const QString &fileName );

// not yet...
		bool createUploadFile( const QString & /*fileName*/ ) {
			kdDebug() << i18n( "Uploading data is not possible yet!" );
			return false;
		}

	public slots:
		void updateData( const QString &newFile );

 private:
	KDirWatch *kdw;
	KStars *ks;
};

#endif
