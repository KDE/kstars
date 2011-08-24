#include "xmlvalidator.h"
#include "QAbstractMessageHandler"
#include "QXmlSchema"
#include "QXmlSchemaValidator"


class MessageHandler : public QAbstractMessageHandler
{
public:
    MessageHandler()
        : QAbstractMessageHandler()
    {
    }

    QString statusMessage() const
    {
        return mDescription;
    }

    int line() const
    {
        return mSourceLoc.line();
    }

    int column() const
    {
        return mSourceLoc.column();
    }

protected:
    virtual void handleMessage(QtMsgType type, const QString &description,
                               const QUrl &identifier, const QSourceLocation &sourceLocation)
    {
        Q_UNUSED(type);
        Q_UNUSED(identifier);

        mMessageType = type;
        mDescription = description;
        mSourceLoc = sourceLocation;
    }

private:
    QtMsgType mMessageType;
    QString mDescription;
    QSourceLocation mSourceLoc;
};

bool XmlValidator::validate(QIODevice *xml, QIODevice *schema, QString &errorMsg)
{
    QXmlSchema xsd;
    MessageHandler msgHandler;
    xsd.setMessageHandler(&msgHandler);

    xsd.load(schema);

    bool error = false;
    if(!xsd.isValid()) {
        error = true;
    } else {
        QXmlSchemaValidator validator(xsd);
        if(!validator.validate(xml)) {
            error = true;
        }
    }

    if(error) {
        errorMsg = msgHandler.statusMessage();
        return false;
    } else {
        return true;
    }
}
