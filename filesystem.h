#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <QObject>
#include <QFileInfo>
#include <QVector>
#include <QTimer>

#include <QAbstractListModel>
#include <QSortFilterProxyModel>

#include <QFileSystemWatcher>

#include "macros.h"

class FsProxyModel;

// ---------------------------------------------------------------
// Fs
// ---------------------------------------------------------------

class FsEntry: public QObject
{
    Q_OBJECT

    PROPERTY(QString, path, setPath)
    PROPERTY(QString, name, setName)
    PROPERTY(FsEntry*, parent, setParent)
    PROPERTY(bool, expandable, setExpandable)
    PROPERTY(bool, expanded, setExpanded)

public:
    FsEntry();
    FsEntry(const FsEntry& other);
    FsEntry(const QFileInfo& fileInfo, FsEntry* parent = nullptr);

    virtual ~FsEntry(){}

    int row() const;

    QVector<FsEntry *> children() const;

    Q_INVOKABLE QVariantList childrenAsVariantList();
    Q_INVOKABLE int childrenCount();
    Q_INVOKABLE FsEntry* childAt(int index);

private:
    QVector<FsEntry*> mChildren;
};

Q_DECLARE_METATYPE(FsEntry);

class FsEntryModel: public QAbstractItemModel
{
    Q_OBJECT

public:
    explicit FsEntryModel(QObject* parent = nullptr);

    enum Roles
    {
        NameRole = Qt::UserRole + 1,
        PathRole,
        IsExpandableRole,
        IsExpandedRole,
        IsHiddenRole,
        ChildrenRole,
        ChildrenCountRole,
        EntryRole
    };
    Q_ENUM(Roles)

    QHash<int,QByteArray> roleNames() const override
    {
        QHash<int, QByteArray> result;
        result.insert(FsEntryModel::NameRole, "fsName");
        result.insert(FsEntryModel::PathRole, "fsPath");
        result.insert(FsEntryModel::IsExpandableRole, "isExpandable");
        result.insert(FsEntryModel::IsExpandedRole, "isExpanded");
        result.insert(FsEntryModel::IsHiddenRole, "isHidden");
        result.insert(FsEntryModel::ChildrenRole, "fsChildren");
        result.insert(FsEntryModel::ChildrenCountRole, "childrenCount");
        result.insert(FsEntryModel::EntryRole, "entry");
        return result;
    }
    Q_INVOKABLE int roleFromString(QString roleName);

    // QAbstractItemModel interface
    virtual QModelIndex index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const override;
    virtual QModelIndex parent(const QModelIndex &child) const override;
    virtual int rowCount(const QModelIndex &parent= QModelIndex()) const override;
    virtual int columnCount(const QModelIndex &parent= QModelIndex()) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;

    QString path() const;
    void setPath(const QString &path);

    bool containsDir(const QString& path);

    void expandAll();
    void collapseAll();

    FsEntry* root() const;

signals:
    void fileSystemChange();

protected:
    void loadEntries();
    void _loadEntries();

private:
    FsEntry* rootItem = nullptr;
    QString mPath;
    QFileSystemWatcher mWatcher;
    QTimer mChangeTimer;
};

class FsProxyModel: public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(QString path READ path WRITE setPath)
public:
    FsProxyModel(QObject* parent = nullptr);
    virtual ~FsProxyModel() override;

    QString path() const;
    void setPath(const QString &path);

    QString filterText() const;
    void setFilterText(QString filterText);

    bool containsDir(const QString& path);
    Q_INVOKABLE FsEntry* root() const;
    Q_INVOKABLE int roleFromString(QString roleName);

    void expandAll();
    void collapseAll();

signals:
    void filterTextChanged(QString filterText);
    void fileSystemChange();

protected:
    // QSortFilterProxyModel interface
    virtual bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

private:
    QString m_filterText;

};



#endif // FILESYSTEM_H
