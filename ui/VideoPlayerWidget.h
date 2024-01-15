#ifndef VIDEO_PLAYER_WIDGET_H
#define VIDEO_PLAYER_WIDGET_H

#include <QtWidgets/QWidget>
#include <QtWidgets/QMenu>
#include <QtGui/QAction>
#include "ui_VideoPlayerWidget.h"

#include <string>
#include <memory>

class Player;
class AVFrameBuffer;

class VideoPlayerWidget : public QWidget {
    Q_OBJECT

public:
    VideoPlayerWidget(QWidget *parent = nullptr);
    ~VideoPlayerWidget();

private:
    Ui::VideoPlayerWidgetClass ui;

    QMenu *mpContextMenu;
    QAction *mpActionOpenFile;
    QAction *mpActionOpenRTSP;

    std::string mFileUrl;

    std::unique_ptr<Player> mPlayer;

    void onRenderVideoBuffer(std::shared_ptr<AVFrameBuffer> pFrame);

private slots:
    void slotContextMenuClicked(QAction *);

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
};

#endif
