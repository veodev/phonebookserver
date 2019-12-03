#ifndef SERVER_H
#define SERVER_H

#include <QObject>
#include <QTcpSocket>
#include <QTcpServer>
#include <QtXml>
#include <enums.h>

class Server : public QObject
{
    Q_OBJECT

public:
    explicit Server(QObject* parent = nullptr);

private:
    void initTcpServer();
    void newClientConnection();
    void writeConsoleMessage(QString message);
    void readFromXmlFile();
    void writeToXmlFile();
    void readMessageFromBuffer();
    void updateClientData();
    void sendMessage(QByteArray& message);
    void parseContact(QByteArray& data);

private slots:
    void closeConnection();
    void readyRead();

private:
    QTcpServer* _tcpServer;
    QTcpSocket* _tcpSocket;
    struct Item
    {
        QString secondName;
        QString firstName;
        QString patronym;
        QString sex;
        QString phone;
        void reset()
        {
            secondName.clear();
            firstName.clear();
            patronym.clear();
            sex.clear();
            phone.clear();
        }
    };
    QList<Item> _contacts;
    QByteArray _messagesBuffer;
    Item _bufferItem;
    int _itemIndex;
};

#endif  // SERVER_H
