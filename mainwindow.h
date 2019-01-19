#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileSystemModel>
#include <QDir>
#include <QThread>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QLabel>

namespace Ui {
class MainWindow;
}

class FindWorker : public QObject{
    Q_OBJECT

public:
    explicit FindWorker(QObject *parent = nullptr);

public slots:
    void runFind(QString path);

signals:
    void findFinished();

private:
    // return hash of file
    QByteArray fileChecksum(const QString &fileName,
                            QCryptographicHash::Algorithm hashAlgorithm = QCryptographicHash::Sha256);
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void on_lvSource_doubleClicked(const QModelIndex &index);
    void set_progress_complete(int count);
    void set_progress_update(int count);
    void click_start();
    void click_stop();
    void getContextMenu(QPoint const& pos);

signals:
    void scan_directory(QString const& dir);
    void abort_scan();
    void delete_file(QModelIndex const& index);
    void delete_same(QModelIndex const& index);

private:
    bool scan;
    Ui::MainWindow *ui;
    QLabel* label;
    QFileSystemModel *listModel;
    void enable_buttons(bool state);
};

#endif // MAINWINDOW_H
