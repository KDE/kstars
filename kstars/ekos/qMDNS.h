/*
Copyright (c) 2016 Alex Spataru

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <QObject>

class QHostInfo;
class QUdpSocket;
class QHostAddress;

/**
 * \brief Implements a simple mDNS responder using Qt
 *
 * This implementation is able perform mDNS queries and mDNS responses in any
 * operating system supported by the Qt network module.
 *
 * You can obtain the IP of an mDNS device using the \c lookup() function,
 * the \c hostFound() signal will be emitted whenever this class interprets
 * a mDNS response packet and obtains valid information about a remote host.
 *
 * You can change the name that the local computer uses to identify itself
 * in the mDNS network using the \c setHostName() function.
 *
 * \todo Implement NSEC block code generation in the \c sendResponse() packet
 */
class qMDNS : public QObject {
    Q_OBJECT

  signals:
    void hostFound (const QHostInfo& info);

  public:
    static qMDNS* getInstance();

    QString hostName() const;
    QString getAddress (const QString& string);

  protected:
    explicit qMDNS();
    ~qMDNS();

  public slots:
    void setTTL (const quint32 ttl);
    void lookup (const QString& name);
    void setHostName (const QString& name);

  private slots:
    void onReadyRead();
    void readQuery (const QByteArray& data);
    void sendPacket (const QByteArray& data);
    void readResponse (const QByteArray& data);
    void sendResponse (const quint16 query_id);

  private:
    QString getHostNameFromResponse (const QByteArray& data);
    QString getIPv4FromResponse (const QByteArray& data, const QString& host);
    QStringList getIPv6FromResponse (const QByteArray& data, const QString& host);
    QList<QHostAddress> getAddressesFromResponse (const QByteArray& data,
                                                  const QString& host);

  private:
    quint32 m_ttl;
    QString m_hostName;
    QString m_serviceName;
    QUdpSocket* m_IPv4Socket;
    QUdpSocket* m_IPv6Socket;
};
