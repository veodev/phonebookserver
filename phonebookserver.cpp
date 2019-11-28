#include "phonebookserver.h"

#include <QDebug>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QtEndian>

const QString PHONEBOOK_FILENAME = QDir::homePath() + "/" + "phonebook.xml";

Server::Server(QObject* parent)
    : QObject(parent)
    , _tcpServer(nullptr)
    , _tcpSocket(nullptr)
    , _itemIndex(-1)
{
    initTcpServer();
    readFromXmlFile();
}

void Server::initTcpServer()
{
    _tcpServer = new QTcpServer(this);
    connect(_tcpServer, &QTcpServer::newConnection, this, &Server::newClientConnection);
    _tcpServer->listen(QHostAddress::Any, 50002) ? writeConsoleMessage("Start server: OK") : writeConsoleMessage("Start server: ERROR");
}

void Server::newClientConnection()
{
    if (_tcpSocket == nullptr) {
        _tcpSocket = _tcpServer->nextPendingConnection();
        connect(_tcpSocket, &QTcpSocket::disconnected, this, &Server::closeConnection);
        connect(_tcpSocket, &QTcpSocket::readyRead, this, &Server::readyRead);
        writeConsoleMessage("Client connected");
        updateClientData();
    }
    else {
        _tcpServer->nextPendingConnection()->close();
    }
}

void Server::writeConsoleMessage(QString message)
{
    qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << message;
}

void Server::readFromXmlFile()
{
    QFile phoneBookFile(PHONEBOOK_FILENAME);
    phoneBookFile.open(QFile::ReadWrite);
    QXmlStreamReader xmlReader(&phoneBookFile);
    bool isContactFilled = false;
    while (!xmlReader.atEnd()) {
        if (xmlReader.isStartElement()) {
            if (xmlReader.name() == "item") {
                _itemIndex = xmlReader.attributes().at(0).value().toInt();
                _bufferItem.reset();
                isContactFilled = false;
            }
            if (xmlReader.isStartElement() && xmlReader.name() == "secondname" && !isContactFilled) {
                _bufferItem.secondName = xmlReader.readElementText();
            }
            if (xmlReader.isStartElement() && xmlReader.name() == "firstname" && !isContactFilled) {
                _bufferItem.firstName = xmlReader.readElementText();
            }
            if (xmlReader.isStartElement() && xmlReader.name() == "patronym" && !isContactFilled) {
                _bufferItem.patronym = xmlReader.readElementText();
            }
            if (xmlReader.isStartElement() && xmlReader.name() == "sex" && !isContactFilled) {
                _bufferItem.sex = xmlReader.readElementText();
            }
            if (xmlReader.isStartElement() && xmlReader.name() == "phone" && !isContactFilled) {
                _bufferItem.phone = xmlReader.readElementText();
                isContactFilled = true;
            }
        }

        if (isContactFilled) {
            _contacts.push_back(_bufferItem);
            isContactFilled = false;
        }
        xmlReader.readNext();
    }
    phoneBookFile.close();
}

void Server::writeToXmlFile()
{
    if (_contacts.empty()) {
        return;
    }

    QFile phoneBookFile(PHONEBOOK_FILENAME);
    phoneBookFile.open(QFile::ReadWrite | QFile::Truncate);
    QXmlStreamWriter xmlWriter(&phoneBookFile);
    xmlWriter.setAutoFormatting(true);
    xmlWriter.writeStartDocument();
    xmlWriter.writeStartElement("phonebook");
    for (int i = 0; i < _contacts.size(); ++i) {
        xmlWriter.writeStartElement("item");
        xmlWriter.writeAttribute("id", QString("%1").arg(i));

        xmlWriter.writeStartElement("secondname");
        xmlWriter.writeCharacters(_contacts[i].secondName);
        xmlWriter.writeEndElement();

        xmlWriter.writeStartElement("firstname");
        xmlWriter.writeCharacters(_contacts[i].firstName);
        xmlWriter.writeEndElement();


        xmlWriter.writeStartElement("patronym");
        xmlWriter.writeCharacters(_contacts[i].patronym);
        xmlWriter.writeEndElement();

        xmlWriter.writeStartElement("sex");
        xmlWriter.writeCharacters(_contacts[i].sex);
        xmlWriter.writeEndElement();

        xmlWriter.writeStartElement("phone");
        xmlWriter.writeCharacters(_contacts[i].phone);
        xmlWriter.writeEndElement();

        xmlWriter.writeEndElement();
    }
    xmlWriter.writeEndElement();
    xmlWriter.writeEndDocument();
    phoneBookFile.close();
}

void Server::readMessageFromBuffer()
{
    Headers header = Unknown;
    while (true) {
        if (_messagesBuffer.size() >= static_cast<int>(sizeof(qint16))) {
            quint16 size = qFromLittleEndian<quint16>(reinterpret_cast<const uchar*>(_messagesBuffer.left(sizeof(quint16)).data()));
            if (_messagesBuffer.size() >= size) {
                _messagesBuffer.remove(0, sizeof(quint16));
                header = static_cast<Headers>(_messagesBuffer.at(0));
                _messagesBuffer.remove(0, sizeof(Headers));
                switch (header) {
                case Headers::ClearContacts:
                    _contacts.clear();
                    _itemIndex = 0;
                    break;
                case Headers::SaveContacts:
                    writeToXmlFile();
                    break;
                case Headers::StartSingleContact:
                    _bufferItem.reset();
                    break;
                case Headers::EndSingleContact:
                    _contacts.insert(_itemIndex, _bufferItem);
                    ++_itemIndex;
                    break;
                case Headers::SecondName: {
                    _bufferItem.secondName = QString::fromUtf8(_messagesBuffer.left(size - sizeof(Headers)));
                    _messagesBuffer.remove(0, size - sizeof(Headers));
                    break;
                }
                case Headers::FirstName: {
                    _bufferItem.firstName = QString::fromUtf8(_messagesBuffer.left(size - sizeof(Headers)));
                    _messagesBuffer.remove(0, size - sizeof(Headers));
                    break;
                }
                case Headers::Patronym: {
                    _bufferItem.patronym = QString::fromUtf8(_messagesBuffer.left(size - sizeof(Headers)));
                    _messagesBuffer.remove(0, size - sizeof(Headers));
                    break;
                }
                case Headers::Sex: {
                    _bufferItem.sex = QString::fromUtf8(_messagesBuffer.left(size - sizeof(Headers)));
                    _messagesBuffer.remove(0, size - sizeof(Headers));
                    break;
                }
                case Headers::Phone: {
                    _bufferItem.phone = QString::fromUtf8(_messagesBuffer.left(size - sizeof(Headers)));
                    _messagesBuffer.remove(0, size - sizeof(Headers));
                    break;
                }
                default:
                    break;
                }
            }
            else {
                break;
            }
        }
        else {
            break;
        }
    }
}

void Server::updateClientData()
{
    if (_contacts.isEmpty() == false) {
        QByteArray message;
        message.append(Headers::ClearContacts);
        sendMessage(message);

        for (auto& contact : _contacts) {
            QByteArray message;
            message.append(Headers::StartSingleContact);
            sendMessage(message);

            message.clear();
            message.append(Headers::SecondName);
            message.append(contact.secondName.toUtf8());
            sendMessage(message);

            message.clear();
            message.append(Headers::FirstName);
            message.append(contact.firstName.toUtf8());
            sendMessage(message);

            message.clear();
            message.append(Headers::Patronym);
            message.append(contact.patronym.toUtf8());
            sendMessage(message);

            message.clear();
            message.append(Headers::Sex);
            message.append(contact.sex.toUtf8());
            sendMessage(message);

            message.clear();
            message.append(Headers::Phone);
            message.append(contact.phone.toUtf8());
            sendMessage(message);

            message.clear();
            message.append(Headers::EndSingleContact);
            sendMessage(message);
        }

        message.clear();
        message.append(Headers::SaveContacts);
        sendMessage(message);
    }
}

void Server::sendMessage(QByteArray& message)
{
    if (_tcpSocket != nullptr) {
        quint16 size = static_cast<quint16>(message.size());
        _tcpSocket->write(reinterpret_cast<char*>(&size), sizeof(quint16));
        _tcpSocket->write(message);
        _tcpSocket->flush();
    }
}

void Server::closeConnection()
{
    disconnect(_tcpSocket, &QTcpSocket::disconnected, this, &Server::closeConnection);
    disconnect(_tcpSocket, &QTcpSocket::readyRead, this, &Server::readyRead);
    _tcpSocket->deleteLater();
    _tcpSocket = nullptr;
    writeConsoleMessage("Client disconnected!");
}

void Server::readyRead()
{
    while (_tcpSocket->bytesAvailable()) {
        _messagesBuffer.append(_tcpSocket->readAll());
    }
    readMessageFromBuffer();
}
