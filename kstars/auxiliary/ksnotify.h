#ifndef KSNOTIFY_H
#define KSNOTIFY_H

namespace KSNotify
{

    typedef enum { NOTIFY_OK, NOTIFY_ERROR, NOTIFY_FILE_RECEIVED} Type;

    void play(Type type);

}

#endif // KSNOTIFY_H
