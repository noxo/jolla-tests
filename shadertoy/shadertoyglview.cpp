#include "shadertoyglview.h"
#include "sys/time.h"

ShaderToyGLView::ShaderToyGLView(QQuickWindow *window)
    : QObject()
    , window(window)
    , program(0)
    , time()
    , running(false)
{
    connect(window, SIGNAL(afterRendering()),
            this, SLOT(renderGL()),
            Qt::DirectConnection);
    connect(window, SIGNAL(sceneGraphInitialized()),
                    this, SLOT(cleanup()), Qt::DirectConnection);
    mutex = new QMutex;
    time.start();
}

void
ShaderToyGLView::start(QString fragmentShaderFilename, QString vertexShaderFilename, QString textureFilename)
{
    qDebug() << "start, fragshader=" + fragmentShaderFilename;

    this->fragmentShaderFilename = fragmentShaderFilename;
    this->vertexShaderFilename = vertexShaderFilename;
    this->textureFilename = textureFilename;

    float fps = 60.f;
    timerId = startTimer(1000.f / fps);

    running = true;
}

void
ShaderToyGLView::stop()
{

    QMutexLocker locker(mutex);
    qDebug() << "stopping";

    running = false;
    killTimer(timerId);

    glDeleteProgram(_program);
    glDeleteBuffers(1,&_vbo_quad);

    program = NULL;
    texture = NULL;

    glUseProgram(0);

    window->resetOpenGLState();

    qDebug() << "stopped";

}

void
ShaderToyGLView::cleanup()
{
    qDebug() << "cleanup";
}

QString
ShaderToyGLView::loadShaderSourceFile(QString filename)
{
    QFile file(filename);

    if(!file.open(QFile::ReadOnly | QFile::Text)){
        qDebug() << "could not open file for read";
        return NULL;
    }

    QTextStream in(&file);
    QString data = in.readAll();

    file.close();
    return data;
}

void
ShaderToyGLView::timerEvent(QTimerEvent *event)
{
    // Ideally you should stop the timer whenever the window is
    // minimized / hidden or the screen is turned off.
    window->update();
}

float
ShaderToyGLView::getDeltaTimeS()
{
    timeval currentTime;
    gettimeofday(&currentTime, NULL);

    float deltaTime = (currentTime.tv_sec - _startTime.tv_sec);
    deltaTime += (currentTime.tv_usec - _startTime.tv_usec) / 1000000.0; // us to s
    return deltaTime;
}

void
ShaderToyGLView::renderGL()
{
    QMutexLocker locker(mutex);

    if (!running)
    {
        return;
    }

    if (!program) {

        GLfloat triangle_vertices[] = {
            -1.0, -1.0,
            1.0, -1.0,
            -1.0,  1.0,
            1.0, -1.0,
            1.0,  1.0,
            -1.0,  1.0
        };

        glGenBuffers(1, &_vbo_quad);
        glBindBuffer(GL_ARRAY_BUFFER, _vbo_quad);
        glBufferData(GL_ARRAY_BUFFER, sizeof(triangle_vertices), triangle_vertices, GL_STATIC_DRAW);

        program = new QOpenGLShaderProgram();

        if (vertexShaderFilename == NULL || vertexShaderFilename.isEmpty())
        {
            program->addShaderFromSourceCode(QOpenGLShader::Vertex,
                                             "precision highp float;\n"
                                             "attribute vec2 coord2d;\n"

                                             "void main() {\n"
                                             "  gl_Position = vec4(coord2d, 0.0, 1.0);\n"
                                             "}\n"
                                             );
        }
        else
        {
            program->addShaderFromSourceCode(QOpenGLShader::Vertex, loadShaderSourceFile(vertexShaderFilename));
        }

        program->addShaderFromSourceCode(QOpenGLShader::Fragment, loadShaderSourceFile(fragmentShaderFilename));

        program->link();
        qDebug() << "Program link result:" << program->log();

        _program = program->programId();
        _attribute_coord2d = glGetAttribLocation(_program, "coord2d");

        // Start timer
        gettimeofday(&_startTime, NULL);

        if (textureFilename != NULL && !textureFilename.isEmpty())
        {
            texture = new QOpenGLTexture(QImage(textureFilename).mirrored());
            texture->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
            texture->setMagnificationFilter(QOpenGLTexture::Linear);
        }
    }

    if (program->bind()) {

        float r=(float)rand()/(float)RAND_MAX;
        float g=(float)rand()/(float)RAND_MAX;
        float b=(float)rand()/(float)RAND_MAX;

        // set the clear colour
        glClearColor(r,g,b,1);

        // clear screen
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        GLint unif_resolution, unif_time, unif_tex0;

        unif_time = glGetUniformLocation(_program, "time");
        float deltaTimeS = getDeltaTimeS();
        glUniform1f(unif_time, deltaTimeS);

        unif_resolution = glGetUniformLocation(_program, "resolution");

        glUniform2f(unif_resolution, window->width(),window->height());

        unif_tex0 = glGetUniformLocation(_program, "tex0");

        if (unif_tex0 != -1)
        {
            if (texture != NULL)
            {
                glUniform1i(unif_tex0, 0);
                glActiveTexture(GL_TEXTURE0);
                texture->bind();
            }
        }

        /* Describe our vertices array to OpenGL */
        glBindBuffer(GL_ARRAY_BUFFER, _vbo_quad);
        glVertexAttribPointer(
                    _attribute_coord2d, // attribute
                    2,                 // number of elements per vertex, here (x,y)
                    GL_FLOAT,          // the type of each element
                    GL_FALSE,          // take our values as-is
                    0,                 // no extra data between each position
                    0                  // offset of first element
                    );
        glEnableVertexAttribArray(_attribute_coord2d);

        /* Push each element in buffer_vertices to the vertex shader */
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glDisableVertexAttribArray(_attribute_coord2d);

        program->release();
    }

}
