#include <QDebug>
#include <QUrl>
#include <QStandardPaths>
#include <QtMultimedia/QMediaPlayer>

#include "ksnotify.h"


namespace KSNotify
{

    namespace
    {
        QMediaPlayer player;
        int lastType = -1;

    }

void play(Type type)
{
    if (lastType != type)
    {
        switch (type)
        {
            case NOTIFY_OK:
                player.setMedia(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::DataLocation, "ekos-ok.ogg" )));
                break;

            case NOTIFY_FILE_RECEIVED:
                player.setMedia(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::DataLocation, "ekos-fits.ogg" )));
                break;

            case NOTIFY_ERROR:
                 player.setMedia(QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::DataLocation, "ekos-error.ogg" )));
                 break;
        }

        lastType = type;
    }

    player.stop();
    player.play();
}

}
