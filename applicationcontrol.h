#ifndef APPLICATIONCONTROL_H
#define APPLICATIONCONTROL_H

#include <QObject>
#include <QQmlEngine>

QT_BEGIN_NAMESPACE
class QUdpSocket;
QT_END_NAMESPACE

#include "macros.h"
#include <QHostAddress>
#include <QWebSocket>
#include <QUdpSocket>
#include <QThread>
#include <QMutex>
#include <QQueue>

class AssetImporter : public QThread
{
    Q_OBJECT

public:
    QByteArray messageToProcess;
    QString errorString;

    // TODO: refactor this one
    QString mWritePath;

    QString projectDir;
    QString folderChangeMessage;

    // QThread interface
protected:
    virtual void run() override;

    bool deleteDirectory(const QString& pDirectory);

    QMutex mutex;
};


class ApplicationControl: public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString currentFile READ currentFile WRITE setCurrentFile NOTIFY currentFileChanged)
    Q_PROPERTY(QString currentFolder READ currentFolder WRITE setCurrentFolder NOTIFY currentFolderChanged)
    PROPERTY(bool, isProcessing, setIsProcessing)
    PROPERTY(QString, status, setStatus)

    READONLY_PROPERTY(QVariantList, hosts, setHosts)
    PROPERTY(QString, activeServerIp, setActiveServerIp)


public:
    explicit ApplicationControl(QObject *parent = nullptr);
    ~ApplicationControl();

    Q_INVOKABLE bool createFolder(QString pPath, QString pFolderName);
    Q_INVOKABLE bool createFile(QString pPath, QString pFileName, QString pFileContent);

    Q_INVOKABLE QString readFileContents(const QString& pFilePath);
    Q_INVOKABLE bool writeFileContents(const QString& pFilePath, const QString& pFileContents);

    // TODO
    Q_INVOKABLE void addContextProperty(const QString& pKey, QVariant pData);
    Q_INVOKABLE void onTextMessageReceived(const QString& pMessage);
    Q_INVOKABLE void onBinaryMessageReceived(const QByteArray& pMessage);

    Q_INVOKABLE void clearComponentCache();

    QString currentFile() const;
    QString currentFolder() const;

    QQmlEngine* engine() const;
    void setEngine(QQmlEngine *engine);

    Q_INVOKABLE QString messageContent(const QString& message,
                                       const QString& tag,
                                       int fromIndex = 0);

    QStringList availableAddresses() const;
    Q_INVOKABLE QString idFromIp(const QString& pIp);

signals:
    void currentFileChanged(QString currentFile);
    void currentFolderChanged(QString currentFolder);

    void startedProcessing(QString message);
    void endedProcessing(QString message);

    void jsonMessage(QString message);

    void availableAddressesChanged(QStringList availableAddresses);

public slots:
    void setCurrentFile(QString currentFile);
    void setCurrentFolder(QString currentFolder);

protected:
    void handleFolderChangeMessage(const QString &pMessage);
    void handleFileChangeMessage(const QString &pMessage);
    void handleCurrentFileChangeMessage(const QString &pMessage);
    void handleAssetImportResults();

    QString localFilePathFromRemoteFilePath(const QString& pRemoteFile);

protected slots:
    void processPendingDatagrams();

private:
    QString m_currentFile;
    QString m_currentFolder;
    QString mWritePath;
    QString mCurrentProjectPath;
    QQmlEngine *mEngine = nullptr;

    QMap<QString, QString> mServers;

    QWebSocket* socket = nullptr;

    QUdpSocket udpSocket4;
    QUdpSocket udpSocket6;
    QHostAddress groupAddress4;
    QHostAddress groupAddress6;

    QQueue<QByteArray> mBinaryMessageQueue;
    QQueue<QString> mTextMessageQueue;

    AssetImporter mAssetImporter;
};

#endif // APPLICATIONCONTROL_H
