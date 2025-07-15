#include "opengl3dplot.h"
#include <QtMath>
#ifdef _WIN32
#include <windows.h>
#endif
#include <QOpenGLShader>
#include <QDebug>

OpenGL3DPlot::OpenGL3DPlot(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setMinimumSize(200, 200);
    for (auto& mod : m_modules)
        mod.vbo.create();
}

OpenGL3DPlot::~OpenGL3DPlot()
{
    makeCurrent();
    for (auto& mod : m_modules) {
        mod.vbo.destroy();
        mod.vao.destroy();
    }
    m_shader.release();
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
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // --- Projection / View ---
    m_shader.bind();
    m_shader.setUniformValue("u_proj", m_projMatrix);

    QMatrix4x4 view;
    view.lookAt(QVector3D(50, 50, 20),  // Camera position
                QVector3D(50, 50, -5),  // Target
                QVector3D(0, 1, 0));    // Up
    m_shader.setUniformValue("u_view", view);

    for (int i = 0; i < 3; ++i) {
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
    for (auto& mod : m_modules) {
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

void OpenGL3DPlot::setMaxPoints(const int maxPoints)
{
    m_maxPointsPerModule = maxPoints;
}
