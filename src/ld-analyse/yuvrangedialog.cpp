/******************************************************************************
 * yuvrangedialog.cpp
 * ld-analyse - TBC output analysis GUI
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of ld-decode-tools.
 ******************************************************************************/

#include "yuvrangedialog.h"

#include <QLabel>
#include <QPixmap>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QVBoxLayout>

YuvRangeDialog::YuvRangeDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("YUV Range"));
    setWindowFlags(Qt::Window);
    setModal(false);
    resize(960, 540);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(0);

    scopeLabel_ = new QLabel(this);
    scopeLabel_->setAlignment(Qt::AlignCenter);
    scopeLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    scopeLabel_->setScaledContents(false);
    scopeLabel_->setText(tr("No YUV range data"));
    mainLayout->addWidget(scopeLabel_);
}

void YuvRangeDialog::showScopeImage(const QImage &scopeImage)
{
    cachedScopeImage_ = scopeImage;
    updateScopeLabelPixmap();
}

QSize YuvRangeDialog::scopeRenderTargetSize() const
{
    if (scopeLabel_) {
        return scopeLabel_->size().expandedTo(QSize(1, 1));
    }
    return size().expandedTo(QSize(1, 1));
}

void YuvRangeDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    updateScopeLabelPixmap();
    emit renderTargetSizeChanged(scopeRenderTargetSize());
}

void YuvRangeDialog::updateScopeLabelPixmap()
{
    if (!scopeLabel_) {
        return;
    }

    if (cachedScopeImage_.isNull()) {
        scopeLabel_->setPixmap(QPixmap());
        scopeLabel_->setText(tr("No YUV range data"));
        return;
    }

    const QSize targetSize = scopeLabel_->size().expandedTo(QSize(1, 1));
    const QPixmap pixmap = QPixmap::fromImage(cachedScopeImage_).scaled(
        targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    scopeLabel_->setText(QString());
    scopeLabel_->setPixmap(pixmap);
}
