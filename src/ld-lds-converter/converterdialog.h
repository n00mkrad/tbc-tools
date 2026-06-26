/************************************************************************

    converterdialog.h

    ld-lds-converter - 10-bit .lds to FLAC/s16 converter for ld-decode
    Copyright (C) 2026 Simon Inns

    This file is part of ld-decode-tools.

    ld-lds-converter is free software: you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

************************************************************************/

#ifndef CONVERTERDIALOG_H
#define CONVERTERDIALOG_H

#include <QDialog>

#include "dataconverter.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class ConverterDialog;
}
QT_END_NAMESPACE

class QDragEnterEvent;
class QDropEvent;

class ConverterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConverterDialog(QWidget *parent = nullptr);
    ~ConverterDialog();

    void setDefaultInput(const QString &inputFilename);
    void setDefaultOutput(const QString &outputFilename);
    void setDefaultFormat(DataConverter::OutputFormat outputFormat);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void on_inputBrowseButton_clicked();
    void on_outputBrowseButton_clicked();
    void on_convertButton_clicked();
    void on_outputFormatComboBox_currentIndexChanged(int index);

private:
    void updateOutputPathFromInput(bool forceUpdate);
    bool isLikelyLdsFile(const QString &filePath) const;
    DataConverter::OutputFormat selectedOutputFormat() const;
    void setConversionControlsEnabled(bool enabled);
    void resetProgressDisplay();
    void updateProgressDisplay(qint64 processedBytes, qint64 totalBytes);

    Ui::ConverterDialog *ui;
    QString sourceDirectory;
    bool userEditedOutput;
};

#endif // CONVERTERDIALOG_H
