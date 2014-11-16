#ifndef SHADERTOYGL_H
#define SHADERTOYGL_H

#include <QtGui>
#include <QtQuick>
#include <sailfishapp.h>

class ShaderToyGLView : public QObject
{
    Q_OBJECT

public:
    ShaderToyGLView(QQuickWindow *window);
    void timerEvent(QTimerEvent *event);
    QString loadShaderSourceFile(QString filename);

public slots:
    void renderGL();
    void start(QString fragmentShaderFilename, QString vertexShaderFilename, QString textureFilename);
    void stop();
    void cleanup();

private:
    float getDeltaTimeS();

    int timerId;
    QMutex *mutex;

    QString fragmentShaderFilename;
    QString vertexShaderFilename;
    QString textureFilename;

    QQuickWindow *window;
    QOpenGLShaderProgram *program;
    QTime time;
    QOpenGLTexture *texture;

    timeval     _startTime;

    GLuint      _vbo_quad;
    GLuint      _program;
    GLint       _attribute_coord2d;

    bool        running;
};

#endif // SHADERTOYGL_H
