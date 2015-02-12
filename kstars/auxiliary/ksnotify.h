#ifndef KSNOTIFY_H
#define KSNOTIFY_H

/**
 * @namespace KSNotify
 *
 * Provides capability to play notification sounds for certain types of events such are OK, Error, and File Received.
 */
namespace KSNotify
{

    /**
    * Supported notification types.
    */
    typedef enum { NOTIFY_OK,           /**< Operation completed OK */
                   NOTIFY_ERROR,        /**< Operation completed with error */
                   NOTIFY_FILE_RECEIVED /**< File downloaded successfuly from device or internet */
                 } Type;

    /**
     * @brief play a notification
     * @param type type of notification
     */
    void play(Type type);

}

#endif // KSNOTIFY_H
