#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , glWidget(new SdfGLWidget)  // setCentralWidget takes ownership
{
    resize(640, 480);
    setCentralWidget(glWidget);
    connect(glWidget, &SdfGLWidget::frameTimeUpdated,
            this, &MainWindow::showFrameTime,
            Qt::QueuedConnection);
}

void MainWindow::showFrameTime(int microseconds)
{
    setWindowTitle(QString("sdftoy %1 us").arg(microseconds));
}
