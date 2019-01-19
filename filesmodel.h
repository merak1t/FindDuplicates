#ifndef FILESMODEL_H
#define FILESMODEL_H

#include "hashworker.h"

#include <QAbstractItemModel>
#include <QByteArray>
#include <QVector>
#include <QMap>
#include <QThread>
#include <QElapsedTimer>

class FilesModel :public QAbstractItemModel
{
    Q_OBJECT
public:
    FilesModel();
    ~FilesModel() override;

    QVariant data(const QModelIndex &index, int role) const override;

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;

    QModelIndex parent(const QModelIndex &index) const override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

public slots:
    void add_file(Model* file);
    void no_more_files();
    void start_scan(QString const& directory);
    void stop_scan();
    void delete_file(QModelIndex const& index);
    void delete_same(QModelIndex const& index);

signals:
    void scan_directory(QString const& directory);
    void end_scan(int files_scanned);
    void progress_update(int files_scanned);
    void calc_hash(Model* file);

private:
    QMap<QByteArray, int> hash_to_index;
    QMap<qint64, Model*> size_to_model;
    QVector<Model*> groups;
    QThread thread;
    HashWorker* worker;

    Model* unique_group;
    QMap<QByteArray, int> unique_id;

    int total_files;
    int rehashing_files;
    bool end_flag;

    Model* change_group(QMap<QByteArray, int>::iterator const&);
    void add_to_group(Model* file, Model* group, int parent_pos);

    QElapsedTimer timer;
};
#endif // FILESMODEL_H
