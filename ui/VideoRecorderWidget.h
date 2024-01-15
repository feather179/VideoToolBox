#ifndef VIDEO_RECORDER_WIDGET_H
#define VIDEO_RECORDER_WIDGET_H

#include <QtWidgets/QWidget>
#include "ui_VideoRecorderWidget.h"

#include <memory>

#include "vr/WGCRecorder.h"

class VideoRecorderWidget : public QWidget {
    Q_OBJECT

private:
    Ui::VideoRecorderWidgetClass ui;

    std::unique_ptr<Recorder> mRecorder;

private:
    void onFrameArrived(void *buffer);

private slots:
    void slotCaptureDeviceChanged(int index);

public:
    VideoRecorderWidget(QWidget *parent = nullptr);
    ~VideoRecorderWidget();
    virtual bool eventFilter(QObject *object, QEvent *event) override;
};

#endif
