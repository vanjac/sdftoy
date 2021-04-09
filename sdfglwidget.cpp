#include "sdfglwidget.h"
#include <QFile>
#include "opengllog.h"
#include "shaders.h"
#include <glm/ext/matrix_transform.hpp>

const GLsizei NUM_FRAME_VERTS = 6;
const GLuint VERT_POSITION_LOC = 0;
const GLuint VERT_UV_LOC = 1;

const float FLY_SPEED_ADJUST = 0.0015f;

// blender coordinate system
// (the only good coordinate system)
const glm::vec3 CAM_FORWARD(0, 1, 0);
const glm::vec3 CAM_RIGHT(1, 0, 0);
const glm::vec3 CAM_UP(0, 0, 1);

const float FOCAL_LENGTH = 1;

const int REFRESH_TIME = 500;
const int QUERY_TIME = 1000;

SdfGLWidget::SdfGLWidget(QWidget *parent)
    : QOpenGLWidget(parent)
    , openDialog(new QFileDialog(this, "Open shader", QDir::homePath()))
    , refreshTimer(new QTimer(this))
    , logger(new QOpenGLDebugLogger(this))
{
    // simpler behavior
    setUpdateBehavior(UpdateBehavior::PartialUpdate);
    // get mouse move events even if button isn't pressed
    //setMouseTracking(true);
    setFocusPolicy(Qt::FocusPolicy::ClickFocus);
    setTextureFormat(GL_SRGB8_ALPHA8);

    openDialog->setFileMode(QFileDialog::ExistingFile);
    connect(openDialog, &QFileDialog::fileSelected,
            this, &SdfGLWidget::openShader);

    connect(refreshTimer, &QTimer::timeout,
            this, &SdfGLWidget::refreshShaderFile);
}

SdfGLWidget::~SdfGLWidget()
{
    makeCurrent();

    logger->stopLogging();
    // etc

    doneCurrent();
}

void SdfGLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    if (!logger->initialize())
        qWarning() << "OpenGL logging not available! (requires extension KHR_debug)";
    else {
        connect(logger, &QOpenGLDebugLogger::messageLogged,
                this, &SdfGLWidget::handleLoggedMessage);
        logger->startLogging();
    }

    qDebug() << "OpenGL renderer:" << (char *)glGetString(GL_RENDERER);
    qDebug() << "OpenGL version:" << (char *)glGetString(GL_VERSION);


    // linear to sRGB conversion
    // requires QSurfaceFormat color space to be SRgb (see main.cpp)
    // and QGLWidget texture format to be SRGB (see constructor)
    // TODO doesn't work on intel driver?
    glEnable(GL_FRAMEBUFFER_SRGB);  // linear to sRGB conversion


    vertShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertShader, 1, &vertShaderSrc, nullptr);
    if (!compileShaderCheck(vertShader, "Vertex"))
        exit(EXIT_FAILURE);

    fragBaseShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragBaseShader, 1, &fragShaderSrc, nullptr);
    if (!compileShaderCheck(fragBaseShader, "FragBase"))
        exit(EXIT_FAILURE);

    openShader(":/default.frag");


    glGenVertexArrays(1, &frameVAO);
    glBindVertexArray(frameVAO);

    // just a rectangle to fill the screen
    GLfloat vertices[NUM_FRAME_VERTS][2] {
        {-1, -1}, {1, -1}, {-1, 1},
        {1, 1}, {-1, 1}, {1, -1}
    };
    glGenBuffers(1, &framePosBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, framePosBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(VERT_POSITION_LOC, 2, GL_FLOAT,
                          GL_FALSE, 0, (void *)0);
    glEnableVertexAttribArray(VERT_POSITION_LOC);

    // UV coordinates specify a portion of normalized device coordinates
    // changes with the aspect ratio in resizeGL()
    glGenBuffers(1, &frameUVBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, frameUVBuffer);
    // default aspect ratio (1, 1)
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(VERT_UV_LOC, 2, GL_FLOAT,
                          GL_FALSE, 0, (void *)0);
    glEnableVertexAttribArray(VERT_UV_LOC);

    // used to measure frame time
    glGenQueries(1, &timerQuery);

    timer.start();
    refreshTimer->start(REFRESH_TIME);
}

void SdfGLWidget::resizeGL(int w, int h)
{
    // update UV coordinates to match aspect ratio
    float aspect = (float)w / h;
    GLfloat uv[NUM_FRAME_VERTS][2] {
        {-aspect, -1}, {aspect, -1}, {-aspect, 1},
        {aspect, 1}, {-aspect, 1}, {aspect, -1}
    };
    glBindBuffer(GL_ARRAY_BUFFER, frameUVBuffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(uv), uv);
}

void SdfGLWidget::paintGL()
{
    qint64 time = timer.elapsed();
    qint64 deltaTime = time - lastTime;
    lastTime = time;

    glBindVertexArray(frameVAO);
    glUseProgram(program);

    // set uniforms...

    glm::mat4 camMatrix = glm::identity<glm::mat4>();
    camMatrix = glm::rotate(camMatrix, camYaw, CAM_UP);
    camMatrix = glm::rotate(camMatrix, camPitch, CAM_RIGHT);

    // apply velocity
    camPos += camMatrix * glm::vec4(camVelocity
        * flySpeed * (deltaTime / 1000.0f), 0);

    glm::vec4 camDir = camMatrix * glm::vec4(CAM_FORWARD, 0) * FOCAL_LENGTH;
    glm::vec4 camU = camMatrix * glm::vec4(CAM_RIGHT, 0);
    glm::vec4 camV = camMatrix * glm::vec4(CAM_UP, 0);
    glUniform3f(camPosLoc, camPos.x, camPos.y, camPos.z);
    glUniform3f(camDirLoc, camDir.x, camDir.y, camDir.z);
    glUniform3f(camULoc, camU.x, camU.y, camU.z);
    glUniform3f(camVLoc, camV.x, camV.y, camV.z);

    glUniform1f(timeLoc, time / 1000.0f);

    bool measureTime = time - lastQueryTime > QUERY_TIME;
    if (measureTime) {
        // measure render time
        if (lastQueryTime != 0) {
            GLuint nanoseconds;
            glGetQueryObjectuiv(timerQuery, GL_QUERY_RESULT, &nanoseconds);
            emit frameTimeUpdated(nanoseconds / 1000);
        }
        glBeginQuery(GL_TIME_ELAPSED, timerQuery);
        lastQueryTime = time;
    }

    glDrawArrays(GL_TRIANGLES, 0, NUM_FRAME_VERTS);

    if (measureTime)
        glEndQuery(GL_TIME_ELAPSED);

    glFlush();
    update();  // schedule an update to animate
}


void SdfGLWidget::mouseMoveEvent(QMouseEvent *event)
{
    QPoint pos = event->pos();
    QPoint delta(0, 0);
    if (trackMouse)
        delta = pos - prevMousePos;
    prevMousePos = pos;
    trackMouse = true;

    // move the camera
    camYaw -= glm::radians((float)delta.x() * 0.4);
    camPitch -= glm::radians((float)delta.y() * 0.4);
    float pitchLimit = glm::pi<float>() / 2;
    if (camPitch > pitchLimit)
        camPitch = pitchLimit;
    else if (camPitch < -pitchLimit)
        camPitch = -pitchLimit;
}

void SdfGLWidget::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    trackMouse = false;
}

void SdfGLWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat()) {
        QOpenGLWidget::keyPressEvent(event);
        return;
    }
    switch(event->key()) {
    case Qt::Key_W:
        camVelocity += CAM_FORWARD; break;
    case Qt::Key_S:
        camVelocity -= CAM_FORWARD; break;
    case Qt::Key_D:
        camVelocity += CAM_RIGHT; break;
    case Qt::Key_A:
        camVelocity -= CAM_RIGHT; break;
    case Qt::Key_E:
        camVelocity += CAM_UP; break;
    case Qt::Key_Q:
        camVelocity -= CAM_UP; break;
    case Qt::Key_C:  // recenter camera
        camPos = glm::vec4(0, 0, 0, 1);
        camYaw = 0;
        camPitch = 0;
        break;
    case Qt::Key_T:
        timer.restart();
        break;
    case Qt::Key_O:
        openDialog->open();
        break;
    default:
        QOpenGLWidget::keyPressEvent(event);
    }
}

void SdfGLWidget::keyReleaseEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat()) {
        QOpenGLWidget::keyReleaseEvent(event);
        return;
    }
    switch(event->key()) {
    case Qt::Key_W:
        camVelocity -= CAM_FORWARD; break;
    case Qt::Key_S:
        camVelocity += CAM_FORWARD; break;
    case Qt::Key_D:
        camVelocity -= CAM_RIGHT; break;
    case Qt::Key_A:
        camVelocity += CAM_RIGHT; break;
    case Qt::Key_E:
        camVelocity -= CAM_UP; break;
    case Qt::Key_Q:
        camVelocity += CAM_UP; break;
    default:
        QOpenGLWidget::keyReleaseEvent(event);
    }
}

void SdfGLWidget::wheelEvent(QWheelEvent *event)
{
    // eighths of a degree
    int delta = event->angleDelta().y();
    flySpeed *= qExp(delta * FLY_SPEED_ADJUST);
}


void SdfGLWidget::handleLoggedMessage(const QOpenGLDebugMessage &message)
{
    logGLMessage(message);
}


void SdfGLWidget::openShader(const QString &filename)
{
    shaderFile.setFile(filename);
    reloadProgram();
    shaderModified = shaderFile.lastModified();
}

void SdfGLWidget::refreshShaderFile()
{
    shaderFile.refresh();
    QDateTime modified = shaderFile.lastModified();
    if (modified > shaderModified) {
        shaderModified = modified;
        qDebug() << "Reloading shader"
            << qPrintable(modified.toString("hh:mm:ss.zzz"));
        reloadProgram();
    }
}

void SdfGLWidget::reloadProgram()
{
    GLuint fragShader = glCreateShader(GL_FRAGMENT_SHADER);
    QByteArray fragSrcArr = loadFileString(shaderFile.filePath());
    const char *fragSrc = fragSrcArr.constData();
    glShaderSource(fragShader, 1, &fragSrc, nullptr);
    if (!compileShaderCheck(fragShader, "Fragment")) {
        glDeleteShader(fragShader);
        return;
    }

    GLuint newProgram = glCreateProgram();
    glAttachShader(newProgram, vertShader);
    glAttachShader(newProgram, fragBaseShader);
    glAttachShader(newProgram, fragShader);
    if (!linkProgramCheck(newProgram, "Program")) {
        glDeleteShader(fragShader);
        glDeleteProgram(newProgram);
        return;
    }

    if (program != 0)
        glDeleteProgram(program);
    program = newProgram;

    glDeleteShader(fragShader);

    // get program uniforms
    camPosLoc = glGetUniformLocation(program, "iCamPos");
    camDirLoc = glGetUniformLocation(program, "iCamDir");
    camULoc = glGetUniformLocation(program, "iCamU");
    camVLoc = glGetUniformLocation(program, "iCamV");
    timeLoc = glGetUniformLocation(program, "iTime");
}

bool SdfGLWidget::compileShaderCheck(GLuint shader, QString name)
{
    glCompileShader(shader);
    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint logLen;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
        char *log = new char[logLen];
        glGetShaderInfoLog(shader, logLen, NULL, log);
        qCritical() << name << "shader compile error:" << log;
        delete[] log;
    }
    return compiled;
}

bool SdfGLWidget::linkProgramCheck(GLuint program, QString name)
{
    glLinkProgram(program);
    GLint linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint logLen;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);
        char *log = new char[logLen];
        glGetProgramInfoLog(program, logLen, NULL, log);
        qCritical() << name << "link error:" << log;
        delete[] log;
    }
    return linked;
}

QByteArray SdfGLWidget::loadFileString(QString filename)
{
    QFile f(filename);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical() << "Error reading file" << filename;
        return QByteArray("");
    }
    return f.readAll();
}
