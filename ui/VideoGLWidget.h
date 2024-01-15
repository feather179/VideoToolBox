#ifndef VIDEO_GL_WIDGET_H
#define VIDEO_GL_WIDGET_H

#include <QtGui/QOpenGLFunctions>
#include <QtOpenGL/QOpenGLShader>
#include <QtOpenGL/QOpenGLShaderProgram>
#include <QtOpenGL/QOpenGLTexture>
#include <QtOpenGLWidgets/QOpenGLWidget>

#include <mutex>

#include "foundation/PacketBuffer.h"
#include "foundation/FFBuffer.h"

#define ATTRIB_VERTEX 3
#define ATTRIB_TEXTURE 4

class VideoGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    VideoGLWidget(QWidget *parent = nullptr);
    ~VideoGLWidget();

    void render(std::shared_ptr<AVFrameBuffer> pFrame);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    QOpenGLShader *mpVShader;
    QOpenGLShader *mpFShader;
    QOpenGLShaderProgram *mpShaderProgram;

    GLuint mTextureUniformY;
    GLuint mTextureUniformU;
    GLuint mTextureUniformV;
    QOpenGLTexture *mpTextureY;
    QOpenGLTexture *mpTextureU;
    QOpenGLTexture *mpTextureV;
    GLuint mTextureIdY;
    GLuint mTextureIdU;
    GLuint mTextureIdV;

    int mVideoFrameWidth;
    int mVideoFrameHeight;
    std::shared_ptr<PacketBuffer> mFrameBufferY;
    std::shared_ptr<PacketBuffer> mFrameBufferU;
    std::shared_ptr<PacketBuffer> mFrameBufferV;
};

#endif
