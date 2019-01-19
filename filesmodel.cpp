#include "filesmodel.h"


#include <QFile>
#include <QDebug>

FilesModel::FilesModel() :
    QAbstractItemModel(nullptr),
    worker_thread(),
    unique_group(new Model("Unique files", -1)),
    total_files(0),
    rehashing_files(0),
    notify_ended(false),
    timer()
{
    unique_group->isFile = false;
    worker = new HashWorker();
    worker->moveToThread(&worker_thread);
    connect(this, &FilesModel::scan_directory, worker, &HashWorker::process);
    connect(this, &FilesModel::calculate_hash, worker, &HashWorker::get_hash);
    connect(worker, &HashWorker::file_processed, this, &FilesModel::add_file);
    connect(worker, &HashWorker::end_scan, this, &FilesModel::no_more_files);
    worker_thread.start();
}

FilesModel::~FilesModel() {
    delete unique_group;
    for (auto ptr : grouped_files) {
        delete ptr;
    }
}

QVariant FilesModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid())
        return QVariant();

    if (role != Qt::DisplayRole)
        return QVariant();

    auto ptr = static_cast<Model*>(index.internalPointer());
    if (ptr == unique_group) {
        return QString::number(ptr->lists.size()) + " unique files";
    } else if (ptr->isFile) {
        return ptr->name;
    } else {
        return QString::number(ptr->lists.size()) + " same files";
    }
}

QVariant FilesModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    if (orientation != Qt::Horizontal) {
        return QVariant();
    }

    if (section == 0) {
        return "Name";
    } else {
        return QVariant();
    }
}

QModelIndex FilesModel::index(int row, int column, const QModelIndex &parent) const {
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    if (!parent.isValid()) {
        if (row == grouped_files.size()) {
            return createIndex(row, column, unique_group);
        } else {
            return createIndex(row, column, grouped_files[row]);
        }
    }

    auto parent_ptr = static_cast<Model*>(parent.internalPointer());
    return createIndex(row, column, parent_ptr->lists[row]);
}

QModelIndex FilesModel::parent(const QModelIndex &index) const {
    auto ptr = static_cast<Model*>(index.internalPointer());
    auto parent_ptr = ptr->root;

    if (parent_ptr == nullptr) {
        return QModelIndex();
    }

    if (parent_ptr == unique_group) {
        return createIndex(grouped_files.size(), 0, unique_group);
    } else {
        int parents_row = hash_to_index[parent_ptr->hash];
        return createIndex(parents_row, 0, grouped_files[parents_row]);
    }
}

int FilesModel::rowCount(const QModelIndex &parent) const {
    if (!parent.isValid()) {
        return grouped_files.size() + 1;
    }
    return static_cast<Model*>(parent.internalPointer())->lists.size();
}

int FilesModel::columnCount(const QModelIndex &parent) const {
    return 1;
}

Model* FilesModel::get_and_remove_file_from_unique(const QMap<QByteArray, int>::iterator &it) {
    // move file from unique to group
    // some files in front of us could be deleted, need to recalculate position
    int old_file_pos = it.value();
    if (old_file_pos >= unique_group->lists.size()) { old_file_pos = unique_group->lists.size() - 1; }
    while (unique_group->lists[old_file_pos]->hash != it.key()) { old_file_pos--; }

    Model* old_file = unique_group->lists[old_file_pos];
    beginRemoveRows(index(grouped_files.size(), 0, QModelIndex()), old_file_pos, old_file_pos);
    unique_group->lists.erase(unique_group->lists.begin() + old_file_pos);
    unique_id.erase(it);
    endRemoveRows();

    return old_file;
}

void FilesModel::add_file_to_group(Model* file, Model* group, int parent_pos) {
    file->root = group;
    auto parent_index = index(parent_pos, 0, QModelIndex());
    beginInsertRows(parent_index, group->lists.size(), group->lists.size());
    group->lists.push_back(file);
    endInsertRows();

    emit dataChanged(parent_index, parent_index); // need to update group title
}

void FilesModel::add_file(Model* file) {
    if (file->hashed) {
        rehashing_files--;
        auto pos = hash_to_index.find(file->hash);
        int parent_pos;
        Model* group;

        if (pos == hash_to_index.end()) {
            auto unique_pos = unique_id.find(file->hash);
            if (unique_pos == unique_id.end()) {
                group = unique_group;
                parent_pos = grouped_files.size();

                unique_id[file->hash] = unique_group->lists.size();
            } else {
                group = new Model(QString(), file->size);
                group->isFile = false;
                group->hash = file->hash;
                group->hashed = true;

                parent_pos = grouped_files.size();
                hash_to_index[file->hash] = parent_pos;

                beginInsertRows(QModelIndex(), parent_pos, parent_pos);
                grouped_files.push_back(group);
                endInsertRows();

                auto old_file = get_and_remove_file_from_unique(unique_pos);
                add_file_to_group(old_file, group, parent_pos);
            }
        } else {
            parent_pos = pos.value();
            group = grouped_files[parent_pos];
        }

        add_file_to_group(file, group, parent_pos);

        total_files++;
        emit scan_update(total_files);

        if (notify_ended && rehashing_files == 0) {
            emit scan_ended(total_files);
            qDebug() << timer.elapsed();
        }
    } else {
        auto size_it = size_to_ptr.find(file->size);
        if (size_it == size_to_ptr.end()) {
            size_to_ptr[file->size] = file;

            auto group = unique_group;
            auto parent_pos = grouped_files.size();

            unique_id[file->hash] = unique_group->lists.size();
            add_file_to_group(file, group, parent_pos);

            total_files++;
            emit scan_update(total_files);
        } else {
            emit calculate_hash(file);
            rehashing_files++;

            auto ptr = size_it.value();
            if (ptr != nullptr) {
                auto unique_pos = unique_id.find(ptr->hash);
                get_and_remove_file_from_unique(unique_pos);

                emit calculate_hash(ptr);
                total_files--;
                rehashing_files++;

                size_to_ptr[ptr->size] = nullptr;
            }
        }
    }

}

void FilesModel::no_more_files() {
    if (rehashing_files == 0) {
        emit scan_ended(total_files);
        qDebug() << timer.elapsed();
    } else {
        notify_ended = true;
    }
}

void FilesModel::start_scan(QString const& directory) {
    // cleanup
    beginResetModel();
    for (auto ptr: unique_group->lists) {
        delete ptr;
    }
    unique_group->lists.clear();
    unique_id.clear();

    for (auto ptr: grouped_files) {
        delete ptr;
    }
    grouped_files.clear();
    hash_to_index.clear();
    size_to_ptr.clear();

    total_files = 0;
    rehashing_files = 0;
    notify_ended = false;
    endResetModel();

    timer.restart();
    emit scan_directory(directory);
}

void FilesModel::stop_scan() {
    worker->stop();
    qDebug() << timer.elapsed();
}

// delete file
// if only one file remains in group, move that file to unique
void FilesModel::delete_file(QModelIndex const& index) {
    if (!index.isValid()) { return; }

    auto ptr = static_cast<Model*>(index.internalPointer());
    auto parent_ptr = ptr->root;
    auto parent_index = index.parent();
    if (!ptr->isFile) { return; }

    // delete file
    beginRemoveRows(parent_index, index.row(), index.row());
    QFile::remove(ptr->name);
    parent_ptr->lists.erase(parent_ptr->lists.begin() + index.row());
    delete ptr;
    endRemoveRows();

    if (parent_ptr == unique_group) { return; }
    if (parent_ptr->lists.size() > 1) { return; }

    // delete other child
    beginRemoveRows(parent_index, 0, 0);
    auto tmp = parent_ptr->lists.back();
    parent_ptr->lists.clear();
    endRemoveRows();

    // delete parent
    beginRemoveRows(QModelIndex(), parent_index.row(), parent_index.row());
    grouped_files.erase(grouped_files.begin() + parent_index.row());
    delete parent_ptr;
    endRemoveRows();

    // fix indexes
    auto iter = hash_to_index.find(parent_ptr->hash);
    hash_to_index.erase(iter);
    for (auto& x : hash_to_index) {
        if (x > parent_index.row()) {
            x--;
        }
    }

    // move other child to unique
    beginInsertRows(this->index(rowCount() - 1, 0, QModelIndex()), unique_group->lists.size(), unique_group->lists.size());
    tmp->root = unique_group;
    unique_group->lists.push_back(tmp);
    endInsertRows();
}

// deletes all files that have same hash as ours except our
// and moves it to unique files
void FilesModel::delete_same(QModelIndex const& index) {
    if (!index.isValid()) { return; }

    auto ptr = static_cast<Model*>(index.internalPointer());
    auto parent_ptr = ptr->root;
    if (!ptr->isFile) { return; }
    if (parent_ptr == unique_group) { return; }

    auto parent_index = index.parent();

    // delete other files
    beginRemoveRows(parent_index, 0, parent_ptr->lists.size());
    while (!parent_ptr->lists.empty()) {
        auto tmp = parent_ptr->lists.back();
        parent_ptr->lists.pop_back();
        if (tmp != ptr) {
            QFile::remove(tmp->name);
            delete tmp;
        }
    }
    endRemoveRows();

    // delete parent
    beginRemoveRows(QModelIndex(), parent_index.row(), parent_index.row());
    grouped_files.erase(grouped_files.begin() + parent_index.row());
    delete parent_ptr;
    endRemoveRows();

    // actual position changed, recalculate them
    auto iter = hash_to_index.find(ptr->hash);
    hash_to_index.erase(iter);
    for (auto& x : hash_to_index) {
        if (x > parent_index.row()) {
            x--;
        }
    }

    // move file to unique
    beginInsertRows(this->index(rowCount() - 1, 0, QModelIndex()), unique_group->lists.size(), unique_group->lists.size());
    ptr->root = unique_group;
    unique_group->lists.push_back(ptr);
    endInsertRows();

}
