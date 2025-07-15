#ifndef OPENGL3DPLOT_H
#define OPENGL3DPLOT_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QMatrix4x4>
#include <QVector3D>
#include <array>
#include <vector>


class OpenGL3DPlot :  public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT
public:
    explicit OpenGL3DPlot(QWidget* parent = nullptr);
    ~OpenGL3DPlot();

    void clear();
    void setPoints(int moduleId, const std::vector<QVector3D>& points);

    void setMaxPoints(const int maxPoints);
protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    void setupShaders();
    void setupBuffers();

    QOpenGLShaderProgram m_shader;
    QMatrix4x4 m_projMatrix;

    struct ModuleGLData {
        QOpenGLVertexArrayObject vao;
        QOpenGLBuffer vbo{ QOpenGLBuffer::VertexBuffer };
        std::vector<QVector3D> cpuBuffer;
        int activePointCount = 0;
    };

    std::array<ModuleGLData, 3> m_modules;
    std::array<QVector3D, 3> m_colors {
        QVector3D(1.0f, 0.0f, 0.0f), // red
        QVector3D(0.0f, 1.0f, 0.0f), // green
        QVector3D(0.0f, 0.0f, 1.0f)  // blue
    };

    int m_maxPointsPerModule {10}; // 10 is just a random initial value
};

#endif // OPENGL3DPLOT_H
