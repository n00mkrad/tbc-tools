/******************************************************************************
 * yuvrangedialog.h
 * ld-analyse - TBC output analysis GUI
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of ld-decode-tools.
 ******************************************************************************/

#ifndef YUVRANGEDIALOG_H
#define YUVRANGEDIALOG_H

#include <QDialog>
#include <QImage>
#include <QSize>

class QLabel;
class QResizeEvent;

class YuvRangeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit YuvRangeDialog(QWidget *parent = nullptr);
    ~YuvRangeDialog() override = default;

    void showScopeImage(const QImage &scopeImage);
    QSize scopeRenderTargetSize() const;

signals:
    void renderTargetSizeChanged(const QSize &targetSize);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateScopeLabelPixmap();

    QLabel *scopeLabel_ = nullptr;
    QImage cachedScopeImage_;
};

#endif // YUVRANGEDIALOG_H
