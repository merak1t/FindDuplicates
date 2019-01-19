#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "filesmodel.h"

#include <QDesktopServices>
#include <QFileInfo>
#include <QUrl>
#include <QFileDialog>

const QString homePath = "/home/damm1t/";

FindWorker::FindWorker(QObject *parent) : QObject (parent) {}


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    scan(false),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    listModel = new QFileSystemModel(this);
    listModel->setFilter(QDir::QDir::AllEntries);
    listModel->setRootPath(homePath);
    ui->lvSource->setModel(listModel);
    ui->lvSource->setRootIndex(listModel->index(homePath));

    FilesModel* model = new FilesModel();
    ui->treeView->setModel(model);

    connect(model, &FilesModel::end_scan, this, &MainWindow::set_progress_complete);
    connect(model, &FilesModel::progress_update, this, &MainWindow::set_progress_update);
    connect(this, &MainWindow::scan_directory, model, &FilesModel::start_scan);
    connect(this, &MainWindow::abort_scan, model, &FilesModel::stop_scan);
    connect(this, &MainWindow::delete_file, model, &FilesModel::delete_file);
    connect(this, &MainWindow::delete_same, model, &FilesModel::delete_same);

    ui->treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->treeView, &QTreeView::customContextMenuRequested, this, &MainWindow::getContextMenu);

    ui->progressBar->reset();

    label = new QLabel(statusBar());
    statusBar()->addWidget(label);

    connect(ui->btn_start, &QPushButton::clicked, this, &MainWindow::click_start);
    connect(ui->btn_stop, &QPushButton::clicked, this, &MainWindow::click_stop);

    ui->btn_stop->setEnabled(false);

}

MainWindow::~MainWindow()
{
    delete ui;
}

void FindWorker::runFind(QString path)
{
    QDir curDir = QDir(path);

}

QByteArray FindWorker::fileChecksum(const QString &fileName, QCryptographicHash::Algorithm hashAlgorithm)
{
    QFile file(fileName);
    if (file.open(QFile::ReadOnly)) {
           QCryptographicHash hash(hashAlgorithm);
           if (hash.addData(&file)) {
               return hash.result();
           }
       }
       return QByteArray();
}


void MainWindow::on_lvSource_doubleClicked(const QModelIndex &index)
{
    QListView* listView = qobject_cast<QListView*>(sender());
    QFileInfo fileInfo = listModel->fileInfo(index);
    if (fileInfo.fileName() == ".."){
        QDir dir = fileInfo.dir();
        dir.cdUp();
        listView->setRootIndex(listModel->index(dir.absolutePath()));
    }
    else if(fileInfo.fileName() == "."){
        listView->setRootIndex(listModel->index(homePath));
    }
    else if(fileInfo.isDir()){
        listView->setRootIndex(index);
    }
}


void MainWindow::enable_buttons(bool state) {
    ui->btn_start->setEnabled(state);
    ui->lvSource->setEnabled(state);
    ui->btn_stop->setEnabled(!state);
}

void MainWindow::set_progress_complete(int count) {
    ui->progressBar->setMinimum(0);
    ui->progressBar->setMaximum(1);
    ui->progressBar->setValue(1);

    enable_buttons(true);

    label->setText("Files scanned: " + QString::number(count));

    scan = false;
}

void MainWindow::set_progress_update(int count) {
    label->setText("Files scanned: " + QString::number(count));
}

void MainWindow::click_start() {
    QString dir = listModel->filePath(ui->lvSource->rootIndex());
    ui->progressBar->setMinimum(0);
    ui->progressBar->setMaximum(0);

    label->setText("Files scanned: 0");
    enable_buttons(false);

    emit scan_directory(dir);
    scan = true;

}

void MainWindow::click_stop() {
    emit abort_scan();
    ui->progressBar->setMaximum(1);
    ui->progressBar->reset();

    enable_buttons(true);

    scan = false;
}

void MainWindow::getContextMenu(QPoint const& pos) {
    QModelIndex index = ui->treeView->indexAt(pos);
    Model* ptr = static_cast<Model*>(index.internalPointer());
    if (!ptr->isFile) { return; }

    QString filename = ptr->name;

    QMenu* menu = new QMenu(ui->treeView);
    QAction* act_open = menu->addAction("Open file");
    connect(act_open, &QAction::triggered, this, [filename](){
        QDesktopServices::openUrl(QUrl(filename));
    });

    QAction* act_folder = menu->addAction("Open folder");
    connect(act_folder, &QAction::triggered, this, [filename](){
        QFileInfo info(filename);
        QDesktopServices::openUrl(QUrl(info.absolutePath()));
    });

    QAction* act_delete = menu->addAction("Delete file");
    connect(act_delete, &QAction::triggered, this, [this, index]() {
        emit this->delete_file(index);
    });


    QAction* act_delete_same = menu->addAction("Delete same except this");
    connect(act_delete_same, &QAction::triggered, this, [this, index]() {
        emit this->delete_same(index);
    });

    if (scan) {
        act_delete->setEnabled(false);
        act_delete_same->setEnabled(false);
    }

    menu->exec(ui->treeView->mapToGlobal(pos));
}

