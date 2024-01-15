#include "VideoGLWidget.h"

#include "foundation/Log.h"

extern "C" {
#include "libavutil/frame.h"
}

VideoGLWidget::VideoGLWidget(QWidget *parent) : QOpenGLWidget(parent) {
    mpVShader = nullptr;
    mpFShader = nullptr;
    mpShaderProgram = nullptr;

    mTextureUniformY = 0;
    mTextureUniformU = 0;
    mTextureUniformV = 0;

    mTextureIdY = 0;
    mTextureIdU = 0;
    mTextureIdV = 0;

    mpTextureY = nullptr;
    mpTextureU = nullptr;
    mpTextureV = nullptr;

    mVideoFrameWidth = 0;
    mVideoFrameHeight = 0;
}

VideoGLWidget::~VideoGLWidget() {
    if (mpTextureY) {
        mpTextureY->destroy();
        delete mpTextureY;
        mpTextureY = nullptr;
    }
    if (mpTextureU) {
        mpTextureU->destroy();
        delete mpTextureU;
        mpTextureU = nullptr;
    }
    if (mpTextureV) {
        mpTextureV->destroy();
        delete mpTextureV;
        mpTextureV = nullptr;
    }

    // if (mVideoFrameBuffer)
    //     mVideoFrameBuffer = nullptr;
}

void VideoGLWidget::render(std::shared_ptr<AVFrameBuffer> pFrame) {
    // std::unique_lock<std::mutex> lock(mRenderMutex);

    int lumaSize = (*pFrame)->linesize[0] * (*pFrame)->height;
    int chromaSize = (*pFrame)->linesize[1] * (*pFrame)->height / 2;
    if (mVideoFrameWidth != (*pFrame)->width || mVideoFrameHeight != (*pFrame)->height) {
        mVideoFrameWidth = (*pFrame)->width;
        mVideoFrameHeight = (*pFrame)->height;

        // this->setFixedSize(mVideoFrameWidth, mVideoFrameHeight);

        LOGD("VideoGLWidget render width:%d height:%d\n", mVideoFrameWidth, mVideoFrameHeight);

        mFrameBufferY = std::make_shared<PacketBuffer>(lumaSize);
        mFrameBufferU = std::make_shared<PacketBuffer>(chromaSize);
        mFrameBufferV = std::make_shared<PacketBuffer>(chromaSize);
    }

    memcpy(mFrameBufferY->data(), (*pFrame)->data[0], lumaSize);
    memcpy(mFrameBufferU->data(), (*pFrame)->data[1], chromaSize);
    memcpy(mFrameBufferV->data(), (*pFrame)->data[2], chromaSize);

    // trigger paintGL
    update();
}

void VideoGLWidget::initializeGL() {
    initializeOpenGLFunctions();

    mpVShader = new QOpenGLShader(QOpenGLShader::Vertex, this);
    const char *vsrc = "attribute vec4 vertexIn;\n"
                       "attribute vec2 textureIn;\n"
                       "varying vec2 textureOut;\n"
                       "void main(void)\n"
                       "{\n"
                       "   gl_Position = vertexIn;\n"
                       "   textureOut = textureIn;\n"
                       "}\n";
    mpVShader->compileSourceCode(vsrc);

    mpFShader = new QOpenGLShader(QOpenGLShader::Fragment, this);
    const char *fsrc = "varying vec2 textureOut;\n"
                       "uniform sampler2D tex_y;\n"
                       "uniform sampler2D tex_u;\n"
                       "uniform sampler2D tex_v;\n"
                       "void main(void)\n"
                       "{\n"
                       "   vec3 yuv;\n"
                       "   vec3 rgb;\n"
                       "   yuv.x = texture2D(tex_y, textureOut).r;\n"
                       "   yuv.y = texture2D(tex_u, textureOut).r - 0.5;\n"
                       "   yuv.z = texture2D(tex_v, textureOut).r - 0.5;\n"
                       "   rgb = mat3( 1,       1,         1,\n"
                       "               0,       -0.39465,  2.03211,\n"
                       "               1.13983, -0.58060,  0) * yuv;\n"
                       "   gl_FragColor = vec4(rgb, 1);\n"
                       "}\n";
    mpFShader->compileSourceCode(fsrc);

    mpShaderProgram = new QOpenGLShaderProgram(this);
    mpShaderProgram->addShader(mpVShader);
    mpShaderProgram->addShader(mpFShader);
    mpShaderProgram->bindAttributeLocation("vertexIn", ATTRIB_VERTEX);
    mpShaderProgram->bindAttributeLocation("textureIn", ATTRIB_TEXTURE);
    mpShaderProgram->link();
    mpShaderProgram->bind();

    mTextureUniformY = mpShaderProgram->uniformLocation("tex_y");
    mTextureUniformU = mpShaderProgram->uniformLocation("tex_u");
    mTextureUniformV = mpShaderProgram->uniformLocation("tex_v");

    // �������
    static const GLfloat vertexVertices[] = {
        -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f,
    };

    // ��������
    static const GLfloat textureVertices[] = {
        0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
    };

    glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0, vertexVertices);
    glEnableVertexAttribArray(ATTRIB_VERTEX);
    glVertexAttribPointer(ATTRIB_TEXTURE, 2, GL_FLOAT, 0, 0, textureVertices);
    glEnableVertexAttribArray(ATTRIB_TEXTURE);

    mpTextureY = new QOpenGLTexture(QOpenGLTexture::Target2D);
    mpTextureU = new QOpenGLTexture(QOpenGLTexture::Target2D);
    mpTextureV = new QOpenGLTexture(QOpenGLTexture::Target2D);
    mpTextureY->create();
    mpTextureU->create();
    mpTextureV->create();
    mTextureIdY = mpTextureY->textureId();
    mTextureIdU = mpTextureU->textureId();
    mTextureIdV = mpTextureV->textureId();

    glBindTexture(GL_TEXTURE_2D, mTextureIdY); // ������
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, mTextureIdU);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, mTextureIdV);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glClearColor(0.0, 0.0, 0.0, 0.0);
}

void VideoGLWidget::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
}

void VideoGLWidget::paintGL() {
    // std::unique_lock<std::mutex> lock(mRenderMutex);

    if (mVideoFrameWidth > 0 && mVideoFrameHeight > 0 && mFrameBufferY && mFrameBufferU &&
        mFrameBufferV) {
        // this->setFixedSize(mVideoFrameWidth, mVideoFrameHeight);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mTextureIdY);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, mVideoFrameWidth, mVideoFrameHeight, 0,
                     GL_LUMINANCE, GL_UNSIGNED_BYTE, mFrameBufferY->data());
        glUniform1i(mTextureUniformY, 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, mTextureIdU);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, mVideoFrameWidth / 2, mVideoFrameHeight / 2, 0,
                     GL_LUMINANCE, GL_UNSIGNED_BYTE, mFrameBufferU->data());
        glUniform1i(mTextureUniformU, 1);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, mTextureIdV);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, mVideoFrameWidth / 2, mVideoFrameHeight / 2, 0,
                     GL_LUMINANCE, GL_UNSIGNED_BYTE, mFrameBufferV->data());
        glUniform1i(mTextureUniformV, 2);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
}