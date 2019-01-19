#ifndef HASHWORKER_H
#define HASHWORKER_H

#include <QObject>
#include <QCryptographicHash>
#include <QVector>

struct Model {
    QVector<Model*> lists;
    QString name;
    QByteArray hash;
    qint64 size;
    Model* root;
    bool isFile;
    bool hashed;

    Model() {}
    Model(QString const& name, qint64 const& size) : lists(), name(name),
        hash(QString::number(size).toUtf8()), size(size), root(nullptr), isFile(true), hashed(false) {}
    ~Model() {
        for (auto ptr: lists) {
            delete ptr;
        }
    }
};

class HashWorker : public QObject {
    Q_OBJECT
public:
    explicit HashWorker(QObject *parent = nullptr);
    ~HashWorker();

    void stop();

public slots:
    void process(QString const& directory);
    void get_hash(Model* file);

signals:
    void file_add(Model* file);
    void calc_hash(Model* file);
    void end_scan();

private:
    QAtomicInt stop_flag;
    QCryptographicHash hash;
    int bad_files;
};

#endif // HASHWORKER_H
