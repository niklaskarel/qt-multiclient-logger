#include "opengl3dplot.h"
#include <QtMath>
#ifdef _WIN32
#include <windows.h>
#endif
#include <QOpenGLShader>
#include <QDebug>
#include <QMouseEvent>
#include <QWheelEvent>

OpenGL3DPlot::OpenGL3DPlot(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setMinimumSize(200, 200);
    setMouseTracking(true);
    for (auto& mod : m_modules)
        mod.vbo.create();
}

OpenGL3DPlot::~OpenGL3DPlot()
{
    makeCurrent();
    for (auto& mod : m_modules)
    {
        mod.vbo.destroy();
        mod.vao.destroy();
    }

    if (m_shader.isLinked())
    {
        m_shader.release();
        m_shader.removeAllShaders();  // optional cleanup
    }
    doneCurrent();
}

void OpenGL3DPlot::initializeGL()
{
    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);
    setupShaders();
    setupBuffers();
}

void OpenGL3DPlot::resizeGL(int w, int h)
{
    m_projMatrix.setToIdentity();
    m_projMatrix.perspective(45.0f, float(w) / float(h), 0.1f, 1000.0f);
}

void OpenGL3DPlot::paintGL()
{
    if (!isValid())
        return;

    if (!context() || !context()->isValid())
    {
        qDebug() << "OpenGL context invalid!";
        return;
    }

    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // --- Projection / View ---
    m_shader.bind();
    m_shader.setUniformValue("u_proj", m_projMatrix);

    QMatrix4x4 view;
    view.translate(0, 0, -150.0f * m_zoom); // Zoom control
    view.rotate(m_cameraAngleX, 1, 0, 0);    // Rotate X
    view.rotate(m_cameraAngleY, 0, 1, 0);    // Rotate Y
    view.translate(-50, -50, 0);            // Centering scene
    m_shader.setUniformValue("u_view", view);
    drawAxes();
    for (int i = 0; i < 3; ++i)
    {
        auto& mod = m_modules[i];
        if (mod.cpuBuffer.empty()) continue;

        const int bytes = mod.activePointCount * sizeof(QVector3D);

        mod.vao.bind();
        mod.vbo.bind();
        mod.vbo.write(0, mod.cpuBuffer.data(), bytes);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(QVector3D), nullptr);

        m_shader.setUniformValue("u_color", m_colors[i]);
        glDrawArrays(GL_LINE_STRIP, 0, mod.activePointCount);

        mod.vbo.release();
        mod.vao.release();
    }

    m_shader.release();
}

void OpenGL3DPlot::setPoints(int moduleId, const std::vector<QVector3D>& points)
{
    if (!isValid())
        return;

    if (!context() || !context()->isValid())
    {
        qDebug() << "OpenGL context invalid!";
        return;
    }

    if (moduleId < 0 || moduleId >= 3)
        return;

    auto& mod = m_modules[moduleId];
    int count = std::min(int(points.size()), m_maxPointsPerModule);

    mod.cpuBuffer.assign(points.end() - count, points.end());
    mod.activePointCount = count;

    update(); // triggers paintGL
}

void OpenGL3DPlot::clear()
{
    for (auto& mod : m_modules) {
        mod.cpuBuffer.clear();
        mod.activePointCount = 0;
    }
    update();
}

void OpenGL3DPlot::setupShaders()
{
    if (!m_shader.addShaderFromSourceCode(QOpenGLShader::Vertex,
                                          R"(
        #version 330 core
        layout(location = 0) in vec3 a_position;
        uniform mat4 u_proj;
        uniform mat4 u_view;
        void main() {
            gl_Position = u_proj * u_view * vec4(a_position, 1.0);
            gl_PointSize = 6.0;
        }
        )")) {
        qFatal("Vertex shader error: %s", m_shader.log().toUtf8().constData());
    }

    if (!m_shader.addShaderFromSourceCode(QOpenGLShader::Fragment,
                                          R"(
        #version 330 core
        uniform vec3 u_color;
        out vec4 FragColor;
        void main() {
            FragColor = vec4(u_color, 1.0);
        }
        )")) {
        qFatal("Fragment shader error: %s", m_shader.log().toUtf8().constData());
    }

    if (!m_shader.link()) {
        qFatal("Shader program link failed: %s", m_shader.log().toUtf8().constData());
    }
}

void OpenGL3DPlot::setupBuffers()
{
    for (auto& mod : m_modules)
    {
        mod.vao.create();
        mod.vao.bind();

        mod.vbo.create();
        mod.vbo.bind();
        mod.vbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);
        mod.vbo.allocate(m_maxPointsPerModule * sizeof(QVector3D));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(QVector3D), nullptr);

        mod.vbo.release();
        mod.vao.release();
    }
}

void OpenGL3DPlot::drawAxes()
{
    QVector3D origin(0, 0, 0);
    QVector3D xAxis(100, 0, 0);
    QVector3D yAxis(0, 100, 0);
    QVector3D zAxis(0, 0, -10);

    QVector<QVector3D> axisVertices = { origin, xAxis, origin, yAxis, origin, zAxis };
    QVector<QVector3D> axisColors   = {
        QVector3D(1,0,0), QVector3D(1,0,0),
        QVector3D(0,1,0), QVector3D(0,1,0),
        QVector3D(0,0,1), QVector3D(0,0,1),
    };

    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, axisVertices.size() * sizeof(QVector3D), axisVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(QVector3D), nullptr);
    glEnableVertexAttribArray(0);

    for (int i = 0; i < 3; ++i) {
        m_shader.setUniformValue("u_color", axisColors[i * 2]);
        glDrawArrays(GL_LINES, i * 2, 2);
    }

    glBindVertexArray(0);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
}

void OpenGL3DPlot::setMaxPoints(const int maxPoints)
{
    m_maxPointsPerModule = maxPoints;
}

void OpenGL3DPlot::mousePressEvent(QMouseEvent* e)
{
    m_lastMousePos = e->position();
}

void OpenGL3DPlot::mouseMoveEvent(QMouseEvent* e)
{
    const float dx = e->position().x() - m_lastMousePos.x();
    const float dy = e->position().y() - m_lastMousePos.y();

    if (e->buttons() & Qt::LeftButton) {
        m_cameraAngleX += dy * 0.5f;
        m_cameraAngleY += dx * 0.5f;
        update();
    }

    m_lastMousePos = e->position();
}

void OpenGL3DPlot::wheelEvent(QWheelEvent* e)
{
    if (e->angleDelta().y() > 0)
        m_zoom *= 0.9f;
    else
        m_zoom *= 1.1f;
    update();
}
