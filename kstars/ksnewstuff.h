#ifndef KSNEWSTUFF_H
#define KSNEWSTUFF_H

#include <klocale.h>
#include <kdebug.h>

#include <knewstuff/knewstuff.h>
#include "kstars.h"

class KSNewStuff : public KNewStuff
{
	public:
		KSNewStuff( QWidget *parent = 0 );
		bool install( const QString &fileName );

// not yet...
		bool createUploadFile( const QString &fileName ) {
			kdDebug() << i18n( "Uploading data is not possible yet!" );
			return false;
		}
};

#endif
