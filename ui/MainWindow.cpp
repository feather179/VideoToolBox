#include "MainWindow.h"

#include "ui/VideoPlayerWidget.h"
#include "ui/VideoRecorderWidget.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    ui.setupUi(this);

    // this->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowMinMaxButtonsHint |
    // Qt::WindowCloseButtonHint);

    this->setWindowTitle("Video Tool Box");

    ui.tabWidget->addTab(new VideoPlayerWidget(this), "Player");
    ui.tabWidget->addTab(new VideoRecorderWidget(this), "Recorder");
}

MainWindow::~MainWindow() {}
