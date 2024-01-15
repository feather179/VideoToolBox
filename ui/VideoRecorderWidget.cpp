#include "VideoRecorderWidget.h"

#include "foundation/Log.h"

#include <QtGui/QResizeEvent>

enum CAPTURE_INDEX {
    CAPTURE_INDEX_SCREEN_DXGI = 0,
    CAPTURE_INDEX_SCREEN_WGC,
    CAPTURE_INDEX_SCREEN_GDI,
    CAPTURE_INDEX_CAMERA,
};

enum ENCODER_INDEX {
    ENCODER_INDEX_X264 = 0,
    ENCODER_INDEX_X265,
    ENCODER_INDEX_QSV_H264,
    ENCODER_INDEX_QSV_HEVC,
    ENCODER_INDEX_NVENC_H264,
    ENCODER_INDEX_NVENC_HEVC,
};

struct ComboItem {
    int index;
    QString text;
};

static ComboItem captureItems[] = {
    {CAPTURE_INDEX_SCREEN_DXGI, "Screen-DXGI"},
    {CAPTURE_INDEX_SCREEN_WGC, "Screen-WGC"},
    {CAPTURE_INDEX_SCREEN_GDI, "Screen-GDI"},
    {CAPTURE_INDEX_CAMERA, "Camera"},
};

static ComboItem encoderItems[] = {
    {ENCODER_INDEX_X264, "x264"},
    {ENCODER_INDEX_X265, "x265"},
    {ENCODER_INDEX_QSV_H264, "QSV H.264"},
    {ENCODER_INDEX_QSV_HEVC, "QSV HEVC"},
    {ENCODER_INDEX_NVENC_H264, "NVENC H.264"},
    {ENCODER_INDEX_NVENC_HEVC, "NVENC HEVC"},
};

VideoRecorderWidget::VideoRecorderWidget(QWidget *parent) : QWidget(parent) {
    ui.setupUi(this);

    ui.splitter->setStretchFactor(0, 1);
    ui.splitter->setStretchFactor(1, 0);

    // ui.videoContainerWidget->installEventFilter(this);

    for (auto &item : captureItems) {
        ui.captureComboBox->insertItem(item.index, item.text);
    }
    ui.captureComboBox->setCurrentIndex(-1);
    connect(ui.captureComboBox, &QComboBox::currentIndexChanged, this,
            &VideoRecorderWidget::slotCaptureDeviceChanged);

    for (auto &item : encoderItems) {
        ui.encoderComboBox->insertItem(item.index, item.text);
    }
    ui.encoderComboBox->setCurrentIndex(-1);
}

VideoRecorderWidget::~VideoRecorderWidget() {}

bool VideoRecorderWidget::eventFilter(QObject *object, QEvent *event) {

    // if (object == ui.videoContainerWidget) {
    //	if (event->type() == QEvent::Resize) {
    //		QResizeEvent *e = (QResizeEvent *)event;
    //		//LOGD("%s wxh=%dx%d\n", __PRETTY_FUNCTION__, e->size().width(),
    // e->size().height()); 		ui.videoD3D11Widget->updateSize(e->size().width(),
    // e->size().height()); 		return true;
    //	}
    // }

    return QWidget::eventFilter(object, event);
}

void VideoRecorderWidget::slotCaptureDeviceChanged(int index) {
    static int lastIndex = -1;
    if (index < 0 || index == lastIndex) return;

    lastIndex = index;

    switch (index) {
        // case CAPTURE_INDEX_SCREEN_DXGI:
        //     break;
        case CAPTURE_INDEX_SCREEN_WGC:
            mRecorder = std::make_unique<WGCRecorder>();
            break;
        // case CAPTURE_INDEX_SCREEN_GDI:
        //     break;
        // case CAPTURE_INDEX_CAMERA:
        //     break;
        default:
            mRecorder = nullptr;
    }

    if (mRecorder) {
        mRecorder->setFrameArrivedCallback(
            std::bind(&VideoRecorderWidget::onFrameArrived, this, std::placeholders::_1));
        mRecorder->init();
        mRecorder->start((HWND)this->winId());
    }
}

void VideoRecorderWidget::onFrameArrived(void *buffer) {
    ui.renderWidget->render(buffer);
}