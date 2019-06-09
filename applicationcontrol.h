#ifndef APPLICATIONCONTROL_H
#define APPLICATIONCONTROL_H

#include <QObject>
#include <QQmlEngine>

#include "macros.h"

class ApplicationControl: public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString currentFile READ currentFile WRITE setCurrentFile NOTIFY currentFileChanged)
    Q_PROPERTY(QString currentFolder READ currentFolder WRITE setCurrentFolder NOTIFY currentFolderChanged)
    PROPERTY(bool, isProcessing, setIsProcessing)
    PROPERTY(QString, status, setStatus)

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

    QString currentFile() const;
    QString currentFolder() const;

    QQmlEngine* engine() const;
    void setEngine(QQmlEngine *engine);

    Q_INVOKABLE QString messageContent(const QString& message,
                                       const QString& tag,
                                       int fromIndex = 0);

signals:
    void currentFileChanged(QString currentFile);
    void currentFolderChanged(QString currentFolder);

    void startedProcessing(QString message);
    void endedProcessing(QString message);

public slots:
    void setCurrentFile(QString currentFile);
    void setCurrentFolder(QString currentFolder);

protected:
    void handleFolderChangeMessage(const QString &pMessage);
    void handleFileChangeMessage(const QString &pMessage);
    void handleCurrentFileChangeMessage(const QString &pMessage);

    QString localFilePathFromRemoteFilePath(const QString& pRemoteFile);

    bool uncompressFile(const QString& pZipFile, const QString& pDirectory);
    bool deleteDirectory(const QString& pDirectory);

private:
    QString m_currentFile;
    QString m_currentFolder;
    QString mWritePath;
    QString mCurrentProjectPath;
    QQmlEngine *mEngine = nullptr;
};

#endif // APPLICATIONCONTROL_H
