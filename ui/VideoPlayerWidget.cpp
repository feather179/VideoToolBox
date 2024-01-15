#include "VideoPlayerWidget.h"

#include <QtWidgets/QFileDialog>
#include <QtWidgets/QInputDialog>

#include "foundation/FFBuffer.h"
#include "foundation/Log.h"
#include "vp/Player.h"

#include "libyuv.h"

VideoPlayerWidget::VideoPlayerWidget(QWidget *parent) : QWidget(parent) {
    ui.setupUi(this);

    mpContextMenu = new QMenu(this);

    QMenu *pMenuOpen = new QMenu(this);
    pMenuOpen->setTitle(tr("Open"));
    // mpActionOpenFile = pMenuOpen->addAction(tr("File"));
    // mpActionOpenRTSP = pMenuOpen->addAction(tr("RTSP"));

    mpActionOpenFile = mpContextMenu->addAction(tr("Open File"));
    mpActionOpenRTSP = mpContextMenu->addAction(tr("Open RTSP"));
    // mpContextMenu->addMenu(pMenuOpen);
    mpContextMenu->addSeparator();
    mpContextMenu->addAction(tr("123as"));

    connect(mpContextMenu, &QMenu::triggered, this, &VideoPlayerWidget::slotContextMenuClicked);
}

VideoPlayerWidget::~VideoPlayerWidget() {}

void VideoPlayerWidget::contextMenuEvent(QContextMenuEvent *event) {
    mpContextMenu->exec(QCursor::pos());
}

void VideoPlayerWidget::slotContextMenuClicked(QAction *action) {
    if (action == mpActionOpenFile) {
        QString fileName = QFileDialog::getOpenFileName(this, tr("Select a video file."), "D:/",
                                                        tr("video files(*.mp4);;All files(*.*)"));
        mFileUrl = fileName.toStdString();

    } else if (action == mpActionOpenRTSP) {
        QString text = QInputDialog::getText(this, tr("Input RTSP link"), tr("RTSP link"),
                                             QLineEdit::Normal, tr("rtsp://"));
        mFileUrl = text.toStdString();
    }

    LOGD("mFileUrl:%s\n", mFileUrl.c_str());
    // this->setWindowTitle(QString(mFileUrl.c_str()));

    if (!mFileUrl.empty()) {
        mPlayer = std::make_unique<Player>(mFileUrl);
        mPlayer->setRenderVideoBufferCallback(
            std::bind(&VideoPlayerWidget::onRenderVideoBuffer, this, std::placeholders::_1));
        mPlayer->start();
    }
}

void VideoPlayerWidget::onRenderVideoBuffer(std::shared_ptr<AVFrameBuffer> pFrame) {
    static std::shared_ptr<AVFrameBuffer> pRGBAFrame;

    if (!pRGBAFrame || pRGBAFrame->get()->width != pFrame->get()->width ||
        pRGBAFrame->get()->height != pFrame->get()->height) {

        pRGBAFrame = std::make_shared<AVFrameBuffer>();

        pRGBAFrame->get()->width = pFrame->get()->width;
        pRGBAFrame->get()->height = pFrame->get()->height;
        pRGBAFrame->get()->format = AV_PIX_FMT_RGBA;
        av_frame_get_buffer(pRGBAFrame->get(), 1);
    }

    // YUV420P to RGBA
    libyuv::I420ToABGR(pFrame->get()->data[0], pFrame->get()->linesize[0], pFrame->get()->data[1],
                       pFrame->get()->linesize[1], pFrame->get()->data[2],
                       pFrame->get()->linesize[2], pRGBAFrame->get()->data[0],
                       pRGBAFrame->get()->linesize[0], pFrame->get()->width, pFrame->get()->height);

    ui.renderWidget->render(pRGBAFrame);
}