#include "applicationcontrol.h"

#include <QDebug>
#include <QQmlContext>
#include <QDirIterator>
#include <QProcess>
#include <QStandardPaths>
#include <QDataStream>
#include <private/qzipreader_p.h>
#include <private/qzipwriter_p.h>

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

ApplicationControl::ApplicationControl(QObject *parent)
    : QObject(parent)
{
    mWritePath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/qmlplaygroundclient_cache";
    setStatus("");
    setIsProcessing(false);
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
    // Handle message type
    QString messageType = messageContent(pMessage, "messagetype");

    if (messageType == "folderchange")
    {
        mEngine->trimComponentCache();
        mEngine->clearComponentCache();
        handleFolderChangeMessage(pMessage);
    }
    else if (messageType == "filechange")
    {
        mEngine->trimComponentCache();
        mEngine->clearComponentCache();
        handleFileChangeMessage(pMessage);
    }
}

void ApplicationControl::onBinaryMessageReceived(const QByteArray &pMessage)
{
    setStatus("Loading assets...");
    setIsProcessing(true);

    // Prepare a stream to get fields from the message
    QByteArray message = pMessage;
    QDataStream stream(&message, QIODevice::ReadOnly);

    // Prepare fields that will be read
    QString readProjectName;
    QByteArray payload;
    qint32 payloadSize;
    stream >> readProjectName >> payloadSize;

    // Read the payload (zip file)
    payload.resize(payloadSize);
    stream.readRawData(payload.data(), payloadSize);
    qDebug() << "read project name: " << readProjectName << "read payload size: " << payloadSize;

//    QString filePath = "C:/Users/vincent.ponchaut/Desktop/perso/testzipresult_client/zaza.zip";
    QString zipFilePath = mWritePath + QString("/projects/%1/%1.zip").arg(readProjectName);
    QFileInfo fileInfo(zipFilePath);
    QString projectDir = fileInfo.absolutePath();

    // Ensure resulting directory exists
    if (!QDir().mkpath(projectDir))
    {
        setStatus("Error creating " + projectDir);
        setIsProcessing(false);
        return;
    }

    if (QDir().exists(projectDir) && !deleteDirectory(projectDir))
    {
        qDebug() << "Could not cleanup " + projectDir;
    }

    // Remove previous file if it exists
    if (fileInfo.exists() && !QFile::remove(zipFilePath))
    {
        qDebug() << "Error removing " + zipFilePath;
        setStatus("Error removing " + zipFilePath);
        setIsProcessing(false);
        return;
    }

    // Create the resulting file
    QFile file(zipFilePath);
    if (!file.open(QIODevice::ReadWrite))
    {
        qDebug() << "Error: could not open " + zipFilePath;
        setStatus("Error: could not open " + zipFilePath);
        setIsProcessing(false);
        return;
    }

    // write to it
    file.write(payload);
    file.close(); // Important: close before attempting a read

    // Now uncompress the data
    QZipReader zipReader(zipFilePath);
    if (!zipReader.status() == QZipReader::NoError ||
        !zipReader.extractAll(projectDir))
    {
        qDebug() << "Error: could not extract " + zipFilePath;
        setStatus("Error: could not extract " + zipFilePath);
        setIsProcessing(false);
        return;
    }

    // Remove the zip file
    file.remove();

    mCurrentProjectPath = projectDir;
    setStatus("Assets loaded.");
    setIsProcessing(false);
}

void ApplicationControl::handleFolderChangeMessage(const QString &pMessage)
{
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

    // Build file list
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

    QString currentFileLocal = localFilePathFromRemoteFilePath(currentFileDistant);
    setCurrentFile(""); // Necessary to force a reload of loaders, even after having cleared the qmlengine cache
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

bool ApplicationControl::uncompressFile(const QString &pZipFile, const QString &pDirectory)
{
    QZipReader zipReader(pZipFile);
    for (QZipReader::FileInfo item : zipReader.fileInfoList())
    {
        qDebug() << item.filePath;
    }
    return true;
}

bool ApplicationControl::deleteDirectory(const QString &pDirectory)
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
