#ifndef XMLVALIDATOR_H
#define XMLVALIDATOR_H

class QIODevice;
class QString;

class XmlValidator
{
public:
    static bool validate(QIODevice *xml, QIODevice *schema, QString &errorMsg);
};

#endif // XMLVALIDATOR_H
