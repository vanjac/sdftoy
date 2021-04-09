#pragma once

#include <QOpenGLWidget>
#include <QOpenGLExtraFunctions>
#include <QOpenGLDebugLogger>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QTimer>
#include <glm/glm.hpp>

class SdfGLWidget : public QOpenGLWidget, protected QOpenGLExtraFunctions
{
    Q_OBJECT
public:
    SdfGLWidget(QWidget *parent = nullptr);
    ~SdfGLWidget();

protected:
    // QOpenGLWidget events
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    // general widget events
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

signals:
    void frameTimeUpdated(int microseconds);

private slots:
    void openShader(const QString &filename);
    void refreshShaderFile();
    void handleLoggedMessage(const QOpenGLDebugMessage &message);

private:
    // OpenGL helper functions
    void reloadProgram();
    bool compileShaderCheck(GLuint shader, QString name);
    bool linkProgramCheck(GLuint program, QString name);
    // Qt IO helper function
    QByteArray loadFileString(QString filename);

    GLuint frameVAO;
    GLuint framePosBuffer, frameUVBuffer;
    GLuint vertShader, fragBaseShader;
    GLuint program = 0;
    GLuint timerQuery;
    // shader uniform locations
    GLint camPosLoc, camDirLoc, camULoc, camVLoc, timeLoc;

    QElapsedTimer timer;
    qint64 lastTime = 0;
    qint64 lastQueryTime = 0;

    bool trackMouse = false;
    QPoint prevMousePos;
    float camYaw = 0, camPitch = 0;
    glm::vec4 camPos{0, 0, 0, 1};
    glm::vec3 camVelocity{0, 0, 0};
    float flySpeed = 2;

    QFileInfo shaderFile;
    QDateTime shaderModified;

    // children
    QFileDialog *openDialog;
    QTimer *refreshTimer;
    QOpenGLDebugLogger *logger;
};

