#include "filesystem.h"

#include <QDir>
#include <QQmlEngine>
#include <QDebug>

// ---------------------------------------------------------------
// FsEntry
// ---------------------------------------------------------------

FsEntry::FsEntry()
{

}

FsEntry::FsEntry(const FsEntry &other)
{
    setName(other.name());
    setPath(other.path());
    setParent(other.parent());
    setExpanded(other.expanded());
    setExpandable(other.expandable());
    mChildren = other.mChildren;
}

FsEntry::FsEntry(const QFileInfo &fileInfo, FsEntry *parent)
    : QObject(parent)
{
    assert(fileInfo.exists());

    setPath(fileInfo.absoluteFilePath());
    setName(fileInfo.fileName());
    setExpandable(fileInfo.isDir() || fileInfo.isSymLink());
    setExpanded(expandable()); // TODO: read from settings
    setParent(parent);

//    qDebug() << "Creating entry for " << path();

    if (this->expandable())
    {
        QDir dir(fileInfo.absoluteFilePath());
        QFileInfoList subdirs = dir.entryInfoList({"*.qml"}, QDir::Files | QDir::AllDirs | QDir::NoDotAndDotDot); // no filter on dirs
        // TODO QDirIterator::subdirectories

        for (auto& subdir: subdirs)
        {
            this->mChildren.append(new FsEntry(subdir, this));
        }
    }
}

int FsEntry::row() const
{
    if (!parent())
        return 0;
    return parent()->mChildren.indexOf(const_cast<FsEntry*>(this));
}

QVector<FsEntry *> FsEntry::children() const
{
    return mChildren;
}

QVariantList FsEntry::childrenAsVariantList()
{
    QVariantList result;
    for (auto c: mChildren)
    {
        result << QVariant::fromValue(c);
    }
    return result;
    //    ?return QVariantList::fromVector(mChildren);
}

int FsEntry::childrenCount()
{
    return mChildren.size();
}

FsEntry *FsEntry::childAt(int index)
{
    if (index < 0 || index >= mChildren.size())
        return nullptr;
    return mChildren.at(index);
}


// ---------------------------------------------------------------
// FsEntryModel Utils
// ---------------------------------------------------------------

inline bool recursiveMatch(FsEntry* root, QString val, int role = FsEntryModel::PathRole)
{
    if (!root)
        return false;

    QString entryVal;
    if (role == FsEntryModel::PathRole)
    {
        entryVal = root->path();
    }
    else if (role == FsEntryModel::NameRole)
    {
        entryVal = root->name();
    }

    if (entryVal.contains(val, Qt::CaseInsensitive))
        return true;
    else
    {
        for (auto&& c: root->children())
        {
            if (recursiveMatch(c, val, role))
                return true;
        }
    }
    return false;
}
inline void recursiveCallback(FsEntry* root, const std::function<void(FsEntry*)>& callback)
{
    if (!root)
        return;

    callback(root);

    for (auto&& c: root->children())
    {
        recursiveCallback(c, callback);
    }
}
inline bool fuzzymatch(QString str, QString filter)
{
    bool allFound = true;
    for (auto&& s: filter.split(" "))
    {
        allFound &= str.contains(s, Qt::CaseInsensitive);
    }
    return allFound;
}

inline void recursiveAddToWatcher(const FsEntry* root, QFileSystemWatcher& watcher)
{
    watcher.addPath(root->path());
    for (const FsEntry* c: root->children())
    {
        recursiveAddToWatcher(c, watcher);
    }
}

// ---------------------------------------------------------------
// FsEntryModel
// ---------------------------------------------------------------

FsEntryModel::FsEntryModel(QObject *parent)
    : QAbstractItemModel(parent)
{
    mChangeTimer.setSingleShot(true);
    mChangeTimer.setInterval(16);

    auto handleFileSystemChange = [=]()
    {
        qDebug() << "file system change";
        loadEntries();
        emit this->fileSystemChange();

        // TODO: find only the relevant indexes
//        emit this->dataChanged(index(0), index(rowCount()));
//        emit this->layoutChanged();
//        beginResetModel();
//        endResetModel();
    };
    auto startChangeTimer = [=]()
    {
        // If the timer is already running, it will be stopped and restarted.
        mChangeTimer.start();
    };
    connect(&mChangeTimer, &QTimer::timeout, handleFileSystemChange);

    connect(&mWatcher, &QFileSystemWatcher::directoryChanged, startChangeTimer);
    connect(&mWatcher, &QFileSystemWatcher::fileChanged, startChangeTimer);
}

int FsEntryModel::roleFromString(QString roleName)
{
    auto rn = roleNames();
    QHashIterator<int, QByteArray> it(rn);
    while (it.hasNext())
    {
        it.next();
        if (it.value() == roleName)
            return it.key();
    }
    return -1;
}

QModelIndex FsEntryModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    FsEntry* parentItem;

    if (!parent.isValid())
    {
//        return createIndex(0, 0, rootItem);
        parentItem = rootItem;
    }
    else
    {
        parentItem = static_cast<FsEntry*>(parent.internalPointer());
    }

    FsEntry *childItem = parentItem->children().at(row);
    if (childItem)
        return createIndex(row, column, childItem);
    return QModelIndex();
}

QModelIndex FsEntryModel::parent(const QModelIndex &child) const
{
    if (!child.isValid())
        return QModelIndex();

    FsEntry *childItem = static_cast<FsEntry*>(child.internalPointer());
    FsEntry *parentItem = childItem->parent();

    if (!parentItem)
        return QModelIndex();

    if (parentItem == rootItem)
    {
        return QModelIndex();
//        return createIndex(0,0, rootItem);
    }

    return createIndex(parentItem->row(), 0, parentItem);
}

int FsEntryModel::rowCount(const QModelIndex &parent) const
{
    FsEntry* parentItem;

    if (!parent.isValid())
    {
        parentItem = rootItem;
//        return 1; // rootItem
    }
    else
    {
        parentItem = static_cast<FsEntry*>(parent.internalPointer());
    }

    return parentItem->children().size();
}

int FsEntryModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 1;
}

QVariant FsEntryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    FsEntry* fs = static_cast<FsEntry*>(index.internalPointer());
    if (!fs)
        return QVariant();

    switch (role)
    {
    case NameRole:
    {
        return fs->name();
    }
    case PathRole:
    {
        return fs->path();
    }
    case IsExpandableRole:
    {
        return fs->expandable();
    }
    case IsExpandedRole:
    {
        return fs->expanded();
    }
    case ChildrenRole:
    {
//        return QVariant::fromValue(fs->children);
        QVariantList list;
        for (auto&& c: fs->children())
            list.append(QVariant::fromValue(c));
        return list;
        //return QVariantList(fs->children);
    }
    case ChildrenCountRole:
    {
        return fs->children().length();
    }
    case EntryRole:
    {
//        QVariant v = QVariant::fromValue(fs);
        QQmlEngine::setObjectOwnership(fs, QQmlEngine::CppOwnership);
        return QVariant::fromValue(fs);
    }
    }

    return QVariant();
}

QString FsEntryModel::path() const
{
    return mPath;
}

void FsEntryModel::setPath(const QString &path)
{
    mPath = path;
    if (!mPath.isEmpty())
        loadEntries();
}

bool FsEntryModel::containsDir(const QString &path)
{
    if (path.isEmpty())
        return false;

    for (auto&& dir: mWatcher.directories())
    {
        if (dir.contains(path, Qt::CaseInsensitive))
            return true;
    }
    return false;
//    if (path.isEmpty() || !rootItem)
//        return false;

//    return recursiveMatch(rootItem, path, FsEntryModel::PathRole);
}

void FsEntryModel::expandAll()
{
    recursiveCallback(rootItem,
    [](FsEntry* entry) {
        if (entry->expandable())
            entry->setExpanded(true);
    });
}

void FsEntryModel::collapseAll()
{
    recursiveCallback(rootItem,
    [](FsEntry* entry) {
        if (entry->expandable())
            entry->setExpanded(false);
    });
}

void FsEntryModel::loadEntries()
{
    if (!mWatcher.directories().empty())
        mWatcher.removePaths(mWatcher.directories());
    if (!mWatcher.files().empty())
        mWatcher.removePaths(mWatcher.files());

    _loadEntries();

    recursiveAddToWatcher(rootItem, mWatcher);
}

void FsEntryModel::_loadEntries()
{
    beginResetModel();

    if (rootItem)
        rootItem->deleteLater();

    QFileInfo rootInfo(mPath);
    assert(rootInfo.exists());

    rootItem = new FsEntry(rootInfo);
    endResetModel();
}

FsEntry *FsEntryModel::root() const
{
    return rootItem;
}

// ---------------------------------------------------------------
// FsProxyModel
// ---------------------------------------------------------------

FsProxyModel::FsProxyModel(QObject *parent)
    : QSortFilterProxyModel (parent)
{
    setRecursiveFilteringEnabled(true);
}

FsProxyModel::~FsProxyModel(){}

QString FsProxyModel::path() const
{
    auto fsModel = qobject_cast<FsEntryModel*>(sourceModel());
    assert(fsModel);

    return fsModel->path();
}

void FsProxyModel::setPath(const QString &path)
{
    auto fsModel = qobject_cast<FsEntryModel*>(sourceModel());
    if (fsModel)
    {
        fsModel->setPath(path);
    }
    else
    {
        fsModel = new FsEntryModel(this);
        fsModel->setPath(path);

        connect(fsModel, &FsEntryModel::fileSystemChange, this, &FsProxyModel::fileSystemChange);

        /*
        connect(fsModel, &FsEntryModel::modelReset, [=]()
        {
            beginResetModel();
            invalidateFilter();
            endResetModel();

            emit this->dataChanged(index(0,0), index(rowCount(),0));
            emit this->layoutChanged();
        });
        */

        setSourceModel(fsModel);
    }
}

QString FsProxyModel::filterText() const
{
    return m_filterText;
}

void FsProxyModel::setFilterText(QString filterText)
{
    if (m_filterText == filterText)
        return;

    m_filterText = filterText;

    beginResetModel();
//    layoutAboutToBeChanged();
    invalidateFilter();
//    layoutChanged();
    endResetModel();

    emit filterTextChanged(m_filterText);
}

bool FsProxyModel::containsDir(const QString &path)
{
    auto fsModel = qobject_cast<FsEntryModel*>(sourceModel());
    return fsModel->containsDir(path);
}

FsEntry *FsProxyModel::root() const
{
    auto fsModel = qobject_cast<FsEntryModel*>(sourceModel());
    return fsModel->root();
}

int FsProxyModel::roleFromString(QString roleName)
{
    auto fsModel = qobject_cast<FsEntryModel*>(sourceModel());
    return fsModel->roleFromString(roleName);
}

void FsProxyModel::expandAll()
{
    auto fsModel = qobject_cast<FsEntryModel*>(sourceModel());
    fsModel->expandAll();
}

void FsProxyModel::collapseAll()
{
    auto fsModel = qobject_cast<FsEntryModel*>(sourceModel());
    fsModel->collapseAll();
}

bool FsProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    const QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
    if (!index.isValid())
        return false;

    const FsEntry* entry = static_cast<const FsEntry*>(index.internalPointer());
    if (!entry)
        return false;

    if (entry->expandable())
        return false;

    return fuzzymatch(entry->name(), m_filterText);
}
