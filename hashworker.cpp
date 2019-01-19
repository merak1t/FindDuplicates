#include "hashworker.h"

#include <QDirIterator>

HashWorker::HashWorker(QObject *parent) : interrupt_flag(0), hash(QCryptographicHash::Algorithm::Sha3_512), bad_files(-1) {}

HashWorker::~HashWorker() {}

void HashWorker::process(QString const& directory) {
    interrupt_flag = 0;
    bad_files = -1;

    QDirIterator it(directory, QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        auto name = it.next();
        QFile file(name);

        Model* file_node = new Model(name, file.size());

        if (!(file.permissions() & QFileDevice::Permission::ReadUser)) {
            file_node->hash = QString::number(bad_files).toUtf8();
            file_node->hashed = true;
            bad_files--;
        }

        if (interrupt_flag == 0) {
            emit file_processed(file_node);
        } else {
            delete file_node;
            return;
        }
    }

    emit end_scan();
}

void HashWorker::get_hash(Model* file_node) {
    if (interrupt_flag == 1) {
        delete file_node;
        return;
    }

    QFile file(file_node->name);
    file.open(QIODevice::ReadOnly);

    hash.reset();
    hash.addData(&file);
    auto file_hash = hash.result().toHex();

    file_node->hash = file_hash;
    file_node->hashed = true;
    if (interrupt_flag == 0) {
        emit file_processed(file_node);
    } else {
        delete file_node;
        return;
    }
}

void HashWorker::stop() {
    interrupt_flag = 1;
}
