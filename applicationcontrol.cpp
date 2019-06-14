#include "applicationcontrol.h"

#include <QDebug>
#include <QQmlContext>
#include <QDirIterator>
#include <QProcess>
#include <QStandardPaths>
#include <QDataStream>
#include <private/qzipreader_p.h>
#include <private/qzipwriter_p.h>
#include <QHostInfo>
#include <QNetworkDatagram>
#include <QTimer>
#include <QUdpSocket>

inline QString quoted(const QString& pToQuote) { return "\"" + pToQuote + "\""; }

inline QString beginTag(const QString& tag)
{
    return "<" + tag + ">";
}

inline QString endTag(const QString& tag)
{
    return "</" + tag + ">";
}

QString ApplicationControl::messageContent(const QString& message,
                                           const QString& tag,
                                           int fromIndex)
{
    QString bTag = beginTag(tag);
    QString eTag = endTag(tag);

    int beginIndex = message.indexOf(bTag, fromIndex);
    int endIndex = message.indexOf(eTag, fromIndex);

    if (endIndex <= beginIndex)
        return QString(); // null qstring

    return message.mid(beginIndex + bTag.length(), endIndex - beginIndex - bTag.length());
}

QStringList ApplicationControl::availableAddresses() const
{
    return mServers.keys();
}

QString ApplicationControl::idFromIp(const QString &pIp)
{
    return mServers[pIp];
}

ApplicationControl::ApplicationControl(QObject *parent)
    : QObject(parent),
      m_currentFile(""),
      groupAddress4(QStringLiteral("239.255.255.250")),
      groupAddress6(QStringLiteral("ff12::2115"))
{
    mWritePath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/qmlplaygroundclient_cache";
    setStatus("");
    setIsProcessing(false);

    // Prep socket for receiving
    socket = new QWebSocket();
    socket->setParent(this);
    connect(this, &ApplicationControl::activeServerIpChanged, [=]()
    {
        socket->open(QUrl(QString("ws://%1").arg(m_activeServerIp)));
    });
    connect(socket, &QWebSocket::textMessageReceived, this, &ApplicationControl::onTextMessageReceived);
    connect(socket, &QWebSocket::binaryMessageReceived, this, &ApplicationControl::onBinaryMessageReceived);

    // Prep network elements for discovery
    if (!udpSocket4.bind(QHostAddress::AnyIPv4, 45454, QUdpSocket::ShareAddress))
        qCritical() << "Could not bind ipv4";
    else if (!udpSocket4.joinMulticastGroup(groupAddress4))
        qCritical() << "Could not join ipv4 multicast group";


    if (!udpSocket6.bind(QHostAddress::AnyIPv6, 45454, QUdpSocket::ShareAddress) ||
        !udpSocket6.joinMulticastGroup(groupAddress6))
        qDebug() << tr("Listening for multicast messages on IPv4 only");

    connect(&udpSocket4, SIGNAL(readyRead()), this, SLOT(processPendingDatagrams()));
    connect(&udpSocket6, &QUdpSocket::readyRead, this, &ApplicationControl::processPendingDatagrams);

    // Asset import
    connect(&mAssetImporter, &QThread::finished, this, &ApplicationControl::handleAssetImportResults);
}

ApplicationControl::~ApplicationControl()
{
}

bool ApplicationControl::createFolder(QString pPath, QString pFolderName)
{
    QString lPath = pPath.replace("file:///", "");
    QString lFilePath = lPath + "/" + pFolderName;

    QDir dir(lPath);
    if (!dir.mkpath(lFilePath))
    {
        qDebug() << "Failed to create folder " << lFilePath;
        return false;
    }
    return true;
}

bool ApplicationControl::createFile(QString pPath, QString pFileName, QString pFileContent)
{
    QString lPath = pPath.replace("file:///", "");
    lPath += "/" + pFileName;

    // Ensure target directory exists
    QString targetDir = lPath.mid(0, lPath.lastIndexOf("/"));
    QDir().mkpath(targetDir);

    QFile file(lPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream textStream(&file);
        textStream << pFileContent;
    }
    else
    {
        qDebug() << QString("Unable to create file \"%1\"").arg(lPath);
        return false;
    }
    return true;
}

QString ApplicationControl::readFileContents(const QString &pFilePath)
{
    QString filePath = pFilePath;
    filePath = filePath.replace("file:///", "");

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return QString();

    QTextStream stream(&file);
    return stream.readAll();
}

bool ApplicationControl::writeFileContents(const QString &pFilePath, const QString &pFileContents)
{
    QString filePath = pFilePath;
    filePath = filePath.replace("file:///", "");

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    QTextStream stream(&file);
    stream << pFileContents;

    return true;
}

void ApplicationControl::addContextProperty(const QString& pKey, QVariant pData)
{
    mEngine->rootContext()->setContextProperty(pKey, pData);
}

void ApplicationControl::onTextMessageReceived(const QString &pMessage)
{
    if (this->isProcessing())
    {
        mTextMessageQueue.enqueue(pMessage);
        return;
    }

    // Handle message type
    QString messageType = messageContent(pMessage, "messagetype");

    if (messageType == "folderchange")
    {
//        mEngine->trimComponentCache();
//        mEngine->clearComponentCache();
        handleFolderChangeMessage(pMessage);
    }
    else if (messageType == "filechange")
    {
//        mEngine->trimComponentCache();
//        mEngine->clearComponentCache();
        handleFileChangeMessage(pMessage);
    }
    else if (messageType == "data")
    {
        // hand the data message over to the qml
        QString json = messageContent(pMessage, "json");
        emit this->jsonMessage(json);
    }
}

void AssetImporter::run()
{
    mutex.lock();

    errorString.clear();
    QString result;

    // Prepare a stream to get fields from the message
    QByteArray message = messageToProcess;
    QDataStream stream(&message, QIODevice::ReadOnly);

    // Prepare fields that will be read
    QString readProjectName;
    QByteArray payload;
    qint32 payloadSize;
    stream >> readProjectName
           >> payloadSize
           >> folderChangeMessage;

    // Read the payload (zip file)
    payload.resize(payloadSize);
    stream.readRawData(payload.data(), payloadSize);
    qDebug() << "read project name: " << readProjectName
             << "read payload size: " << payloadSize
             << "read folderchangemessage: " << folderChangeMessage;

//    QString filePath = "C:/Users/vincent.ponchaut/Desktop/perso/testzipresult_client/zaza.zip";
    QString zipFilePath = mWritePath + QString("/projects/%1/%1.zip").arg(readProjectName);
    QFileInfo fileInfo(zipFilePath);
    projectDir = fileInfo.absolutePath();

    // Ensure resulting directory exists
    if (!QDir().mkpath(projectDir))
    {
        errorString = "Error creating " + projectDir;
        return;
    }

    if (QDir().exists(projectDir) && !deleteDirectory(projectDir))
    {
        qDebug() << "Could not cleanup " + projectDir;
    }

    // Remove previous file if it exists
    if (fileInfo.exists() && !QFile::remove(zipFilePath))
    {
        errorString = "Error removing " + zipFilePath;
        qDebug() << errorString;
        return;
    }

    // Create the resulting file
    QFile file(zipFilePath);
    if (!file.open(QIODevice::ReadWrite))
    {
        errorString = "Error: could not open " + zipFilePath;
        qDebug() << errorString;
        return;
    }

    // write to it
    file.write(payload);
    file.close(); // Important: close before attempting a read

    // Now uncompress the data
    QZipReader zipReader(zipFilePath);
    if (zipReader.status() != QZipReader::NoError ||
       (!zipReader.extractAll(projectDir)))
    {
        errorString = "Error: could not extract " + zipFilePath;
        qDebug() << errorString;
        return;
    }

    // Remove the zip file
    if (!file.remove())
    {
        qDebug() << "Could not remove " << file.fileName();
    }

    mutex.unlock();
}

bool AssetImporter::deleteDirectory(const QString &pDirectory)
{
    bool success = true;

    QDirIterator it(pDirectory, QStringList() << "*", QDir::NoFilter, QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        QString itPath = it.next();

        QFile f(itPath);
        if (!f.open(QIODevice::ReadWrite))
            continue;

        success &= f.remove();
        f.close();
    }

    return success;
}

void ApplicationControl::onBinaryMessageReceived(const QByteArray &pMessage)
{
    if (this->isProcessing())
    {
        mBinaryMessageQueue.enqueue(pMessage);
        return;
    }

    setStatus("Loading assets...");
    setIsProcessing(true);

    if (mAssetImporter.isRunning())
    {
        return;
    }

    mAssetImporter.messageToProcess = pMessage;
    mAssetImporter.mWritePath = mWritePath;
    mAssetImporter.start();
}

void ApplicationControl::clearComponentCache()
{
    //    mEngine->clearComponentCache();
}

void ApplicationControl::handleAssetImportResults()
{
    if (mAssetImporter.errorString.isEmpty())
    {
        setStatus("Assets loaded.");
        mAssetImporter.wait(500); // ensure the thread finishes and drops file handles

        mCurrentProjectPath = mAssetImporter.projectDir;
        if (!mAssetImporter.folderChangeMessage.isEmpty())
            handleFolderChangeMessage(mAssetImporter.folderChangeMessage);
    }
    else
    {
        setStatus(mAssetImporter.errorString);
    }
    setIsProcessing(false);

    // Nevertheless, dequeue messages
    while (!mTextMessageQueue.isEmpty())
    {
        onTextMessageReceived(mTextMessageQueue.dequeue());
    }
    // For the moment, we'll try not dequeuing binary messages (they may be spammed by server)
//    while (!mBinaryMessageQueue.isEmpty())
//    {
//        onBinaryMessageReceived(mBinaryMessageQueue.dequeue());
//    }
}

void ApplicationControl::handleFolderChangeMessage(const QString &pMessage)
{
//    qDebug () << "handleFolderChangeMessage" << pMessage;

    // Retrieve distant folder name
    QString folderName = messageContent(pMessage, "folder").remove("\n");
    folderName.remove("file:///");
    setCurrentFolder(folderName);
    qDebug() << "FOLDER: " << folderName;

    // Ensure destination folder exists
    QString projectName = folderName.mid(folderName.lastIndexOf("/") + 1);
//    QDir().mkpath(mWritePath + "/projects/" + projectName);
    QString projectPath = mWritePath + "/projects/" + projectName;
    if (mCurrentProjectPath != projectPath)
        mCurrentProjectPath = projectPath; // TODO: emit ?
    QDir().mkpath(mCurrentProjectPath);

    // Refresh file contents
    int lastFileIndex = 0;

    QString currentFileName = messageContent(pMessage, "file");
    QString currentFileContent = messageContent(pMessage, "content");

    while (!currentFileName.isEmpty())
    {
        QString localFileName = currentFileName.remove(folderName);
        localFileName = localFileName.startsWith("/") ? localFileName.remove(0,1) : localFileName;
        qDebug() << "localFileName: " << localFileName;

        createFile(mCurrentProjectPath,
                   localFileName,
                   currentFileContent);

        lastFileIndex = pMessage.indexOf(endTag("content"), lastFileIndex) + endTag("content").length();
        currentFileName = messageContent(pMessage, "file", lastFileIndex);
        currentFileContent = messageContent(pMessage, "content", lastFileIndex);
    }

    // Clear component cache
//    ->trimComponentCache();
//    mEngine->clearComponentCache();

    // Check for a current file change
    handleCurrentFileChangeMessage(pMessage);
}

void ApplicationControl::handleFileChangeMessage(const QString &pMessage)
{
    // Find the corresponding local file
    QString currentFileName = messageContent(pMessage, "file");
    if (currentFileName.isEmpty())
        return;

    QString currentFileContent = messageContent(pMessage, "content");

    QString currentFileNameLocal = localFilePathFromRemoteFilePath(currentFileName);

    // Replace contents
    createFile(mCurrentProjectPath, currentFileNameLocal, currentFileContent);

    // Check for a current file change
    handleCurrentFileChangeMessage(pMessage);
}

void ApplicationControl::handleCurrentFileChangeMessage(const QString &pMessage)
{
    // Extract current file from message
    QString currentFileDistant = messageContent(pMessage, "currentfile");
    if (currentFileDistant.isEmpty())
        return;

    // TODO: fix urls such as C:\Users\user\folder\file:///C:\Users\user\folder\main.qml
    QString currentFileLocal = localFilePathFromRemoteFilePath(currentFileDistant);
    setCurrentFile(""); // Necessary to force a reload of loaders, even after having cleared the qmlengine cache
//    mEngine->clearComponentCache(); // do not do that here, otherwise the websocket is recreated...
    setCurrentFile(currentFileLocal);
}

QString ApplicationControl::localFilePathFromRemoteFilePath(const QString &pRemoteFile)
{
    QString localFile = pRemoteFile;
    localFile.remove(this->currentFolder());
    localFile.remove("file:///");
    if (localFile.startsWith("/"))
        localFile.remove(0,1);

    localFile = "file:///" + mCurrentProjectPath + "/" + localFile;

    return localFile;
}

void ApplicationControl::processPendingDatagrams()
{
    bool shouldNotify = false;

    QByteArray datagram;
    QHostAddress hostAddress;
    quint16 port;

    std::vector<QUdpSocket*> udpSockets { &udpSocket4, &udpSocket6 };

    for (QUdpSocket* udpSocket: udpSockets)
    {
        while (udpSocket->hasPendingDatagrams())
        {
            datagram.resize(int(udpSocket->pendingDatagramSize()));
            udpSocket->readDatagram(datagram.data(), datagram.size(), &hostAddress, &port);
        }
        if (datagram.startsWith("qmlplayground"))
        {
            QString hostId = messageContent(QString(datagram), "id");
            QString hostIp = hostAddress.toString().remove("::ffff:") + ":" + QString::number(12345, 10);

            if (!mServers.contains(hostIp) || mServers[hostIp] != hostId)
            {
                mServers[hostIp] = hostId;
                shouldNotify = true;
                m_hosts.push_back(QVariantMap
                {
                  { "address", hostIp },
                  { "id", hostId }
                });
            }
        }
    }

    if (shouldNotify)
    {
        emit hostsChanged(m_hosts);
//        emit availableAddressesChanged(mServers.keys());
    }
#if 0
    while (udpSocket4.hasPendingDatagrams())
    {
        datagram.resize(int(udpSocket4.pendingDatagramSize()));
        udpSocket4.readDatagram(datagram.data(), datagram.size(), &hostAddress, &port);
//        qDebug() << "ipv4 received: " << datagram << "from" << hostAddress.toString() << "@" << port;
    }
    if (datagram.startsWith("qmlplayground"))
    {
        QString hostId = messageContent(QString(datagram), "id");
        QString hostIp = hostAddress.toString().remove("::ffff:") + ":" + QString::number(12345, 10);

        RemoteHostData hostData;
        hostData.setId(hostId);
        hostData.setAddress(hostIp);

        // Check if an update is needed
        if (!m_availableHosts.contains(hostData))
        {
            m_availableHosts.append(hostData);
            shouldNotify = true;
        }
    }

    while (udpSocket6.hasPendingDatagrams())
    {
        datagram.resize(int(udpSocket6.pendingDatagramSize()));
        udpSocket6.readDatagram(datagram.data(), datagram.size(), &hostAddress, &port);
//        qDebug() << "ipv6 received: " << datagram << "from" << hostAddress.toString() << "@" << port;
    }
    if (datagram.startsWith("qmlplayground"))
    {
        QString hostId = messageContent(QString(datagram), "id");
        QString hostIp = hostAddress.toString().remove("::ffff:") + ":" + QString::number(12345, 10);

        RemoteHostData hostData;
        hostData.setId(hostId);
        hostData.setAddress(hostIp);

        // Check if an update is needed
        if (!m_availableHosts.contains(hostData))
        {
            m_availableHosts.append(hostData);
            shouldNotify = true;
        }
    }

    if (shouldNotify)
        setAvailableServerIds(m_avai)
#endif
}


QQmlEngine *ApplicationControl::engine() const
{
    return mEngine;
}

void ApplicationControl::setEngine(QQmlEngine *engine)
{
    mEngine = engine;
}

QString ApplicationControl::currentFile() const
{
    return m_currentFile;
}

QString ApplicationControl::currentFolder() const
{
    return m_currentFolder;
}

void ApplicationControl::setCurrentFile(QString currentFile)
{
    if (m_currentFile == currentFile)
        return;

    m_currentFile = currentFile;
    emit currentFileChanged(m_currentFile);

    qDebug() << "Current file changed " << currentFile;
}

void ApplicationControl::setCurrentFolder(QString currentFolder)
{
    if (m_currentFolder == currentFolder)
        return;

    m_currentFolder = currentFolder;
    emit currentFolderChanged(m_currentFolder);

    qDebug() << "Current folder changed " << currentFolder;
}


