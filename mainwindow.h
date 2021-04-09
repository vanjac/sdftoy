#pragma once

#include <QMainWindow>
#include <sdfglwidget.h>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);

private slots:
    void showFrameTime(int microseconds);

private:
    SdfGLWidget *glWidget;
};
