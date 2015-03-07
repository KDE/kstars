#include <QDebug>
#include <QUrl>
#include <QStandardPaths>
#include <QtMultimedia/QSoundEffect>

#include "ksnotify.h"


namespace KSNotify
{

    namespace
    {
        QSoundEffect effect;
        int lastType = -1;

    }

void play(Type type)
{
    if (lastType != type)
    {
        switch (type)
        {
            case NOTIFY_OK:
                effect.setSource(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::DataLocation, "ekos-ok.ogg" )));
                break;

            case NOTIFY_FILE_RECEIVED:
                effect.setSource(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::DataLocation, "ekos-fits.ogg" )));
                break;

            case NOTIFY_ERROR:
                 effect.setSource(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::DataLocation, "ekos-error.ogg" )));
                 break;
        }

        lastType = type;
    }

    effect.play();
}

}
