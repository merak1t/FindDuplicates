#include "filesmodel.h"


#include <QFile>
#include <QDebug>

FilesModel::FilesModel() :
    QAbstractItemModel(nullptr),
    worker_thread(),
    unique_group(new Model("Unique files", -1)),
    total_files(0),
    rehashing_files(0),
    end_flag(false),
    timer()
{
    unique_group->isFile = false;
    worker = new HashWorker();
    worker->moveToThread(&worker_thread);
    connect(this, &FilesModel::scan_directory, worker, &HashWorker::process);
    connect(this, &FilesModel::calc_hash, worker, &HashWorker::get_hash);
    connect(worker, &HashWorker::file_add, this, &FilesModel::add_file);
    connect(worker, &HashWorker::end_scan, this, &FilesModel::no_more_files);
    worker_thread.start();
}

FilesModel::~FilesModel() {
    delete unique_group;
    for (auto ptr : groups) {
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
        if (row == groups.size()) {
            return createIndex(row, column, unique_group);
        } else {
            return createIndex(row, column, groups[row]);
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
        return createIndex(groups.size(), 0, unique_group);
    } else {
        int parents_row = hash_to_index[parent_ptr->hash];
        return createIndex(parents_row, 0, groups[parents_row]);
    }
}

int FilesModel::rowCount(const QModelIndex &parent) const {
    if (!parent.isValid()) {
        return groups.size() + 1;
    }
    return static_cast<Model*>(parent.internalPointer())->lists.size();
}

int FilesModel::columnCount(const QModelIndex &parent) const {
    return 1;
}

Model* FilesModel::change_group(const QMap<QByteArray, int>::iterator &it) {
    // move file from unique to group
    int old_pos = it.value();
    if (old_pos >= unique_group->lists.size()) {
        old_pos = unique_group->lists.size() - 1;
    }
    while (unique_group->lists[old_pos]->hash != it.key()) {
        old_pos--;
    }

    Model* old_file = unique_group->lists[old_pos];
    beginRemoveRows(index(groups.size(), 0, QModelIndex()), old_pos, old_pos);
    unique_group->lists.erase(unique_group->lists.begin() + old_pos);
    unique_id.erase(it);
    endRemoveRows();

    return old_file;
}

void FilesModel::add_to_group(Model* file, Model* group, int parent_pos) {
    file->root = group;
    auto parent_index = index(parent_pos, 0, QModelIndex());
    beginInsertRows(parent_index, group->lists.size(), group->lists.size());
    group->lists.push_back(file);
    endInsertRows();

    emit dataChanged(parent_index, parent_index); // update group title
}

void FilesModel::add_file(Model* file) {
    if (file->hashed) {
        rehashing_files--;
        auto pos = hash_to_index.find(file->hash);
        int root_pos;
        Model* group;

        if (pos == hash_to_index.end()) { // unique
            auto unique_pos = unique_id.find(file->hash);
            if (unique_pos == unique_id.end()) {
                group = unique_group;
                root_pos = groups.size();

                unique_id[file->hash] = unique_group->lists.size();
            } else { // make new group
                group = new Model(QString(), file->size);
                group->isFile = false;
                group->hash = file->hash;
                group->hashed = true;

                root_pos = groups.size();
                hash_to_index[file->hash] = root_pos;

                beginInsertRows(QModelIndex(), root_pos, root_pos);
                groups.push_back(group);
                endInsertRows();

                auto old_file = change_group(unique_pos);
                add_to_group(old_file, group, root_pos);
            }
        } else { // add in already exsisting group
            root_pos = pos.value();
            group = groups[root_pos];
        }

        add_to_group(file, group, root_pos);

        total_files++;
        // set progress update
        emit progress_update(total_files);

        if (end_flag && rehashing_files == 0) {
            emit end_scan(total_files);
        }
    } else {
        auto size_it = size_to_model.find(file->size);
        if (size_it == size_to_model.end()) {
            size_to_model[file->size] = file;

            auto group = unique_group;
            auto root_pos = groups.size();

            unique_id[file->hash] = unique_group->lists.size();
            add_to_group(file, group, root_pos);

            total_files++;
            emit progress_update(total_files);
        } else {
            emit calc_hash(file);
            rehashing_files++;

            auto ptr = size_it.value();
            if (ptr != nullptr) {
                auto unique_pos = unique_id.find(ptr->hash);
                change_group(unique_pos);

                emit calc_hash(ptr);
                total_files--;
                rehashing_files++;

                size_to_model[ptr->size] = nullptr;
            }
        }
    }

}

void FilesModel::no_more_files() {
    if (rehashing_files == 0) {
        emit end_scan(total_files);
    } else {
        end_flag = true;
    }
}

void FilesModel::start_scan(QString const& directory) {
    beginResetModel();
    for (auto ptr: unique_group->lists) {
        delete ptr;
    }
    unique_group->lists.clear();
    unique_id.clear();

    for (auto ptr: groups) {
        delete ptr;
    }
    groups.clear();
    hash_to_index.clear();
    size_to_model.clear();

    total_files = 0;
    rehashing_files = 0;
    end_flag = false;
    endResetModel();

    timer.restart();
    emit scan_directory(directory);
}

void FilesModel::stop_scan() {
    worker->stop();
}

// if only one file remains -- file to unique
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

    // delete root
    beginRemoveRows(QModelIndex(), parent_index.row(), parent_index.row());
    groups.erase(groups.begin() + parent_index.row());
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

// deletes files with same hash except our and moves it to unique files
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

    // delete root
    beginRemoveRows(QModelIndex(), parent_index.row(), parent_index.row());
    groups.erase(groups.begin() + parent_index.row());
    delete parent_ptr;
    endRemoveRows();

    // fix indexes
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
