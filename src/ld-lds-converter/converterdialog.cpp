/************************************************************************

    converterdialog.cpp

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

#include "converterdialog.h"
#include "ui_converterdialog.h"

#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
#include <QSignalBlocker>
#include <QUrl>

namespace {
QString normalizePathForCurrentPlatform(const QString &path)
{
    const QString trimmedPath = path.trimmed();
    if (trimmedPath.isEmpty()) {
        return QString();
    }
    const QString normalized = QDir::cleanPath(QDir::fromNativeSeparators(trimmedPath));
    return QDir::toNativeSeparators(normalized);
}
} // namespace

ConverterDialog::ConverterDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::ConverterDialog),
      sourceDirectory(QDir::homePath()),
      userEditedOutput(false)
{
    ui->setupUi(this);
    setAcceptDrops(true);

    if (ui->outputFormatComboBox) {
        ui->outputFormatComboBox->clear();
        ui->outputFormatComboBox->addItem(tr("FLAC (default)"), static_cast<int>(DataConverter::OutputFormat::Flac));
        ui->outputFormatComboBox->addItem(tr("s16 uncompressed"), static_cast<int>(DataConverter::OutputFormat::S16Raw));
        ui->outputFormatComboBox->setCurrentIndex(0);
    }

    if (ui->inputLineEdit) {
        connect(ui->inputLineEdit, &QLineEdit::textChanged, this, [this]() {
            updateOutputPathFromInput(false);
            if (ui->statusLabel) {
                ui->statusLabel->clear();
            }
        });
    }
}

ConverterDialog::~ConverterDialog()
{
    delete ui;
}

void ConverterDialog::setDefaultInput(const QString &inputFilename)
{
    const QString normalizedInput = normalizePathForCurrentPlatform(inputFilename);
    if (normalizedInput.isEmpty()) {
        return;
    }

    {
        const QSignalBlocker blocker(ui->inputLineEdit);
        ui->inputLineEdit->setText(normalizedInput);
    }
    userEditedOutput = false;
    updateOutputPathFromInput(true);

    const QFileInfo inputInfo(normalizedInput);
    if (inputInfo.exists()) {
        sourceDirectory = inputInfo.absolutePath();
    }
}

void ConverterDialog::setDefaultOutput(const QString &outputFilename)
{
    const QString normalizedOutput = normalizePathForCurrentPlatform(outputFilename);
    if (normalizedOutput.isEmpty()) {
        return;
    }

    ui->outputLineEdit->setText(normalizedOutput);
    userEditedOutput = true;
}

void ConverterDialog::setDefaultFormat(DataConverter::OutputFormat outputFormat)
{
    if (!ui->outputFormatComboBox) {
        return;
    }

    const int comboIndex = ui->outputFormatComboBox->findData(static_cast<int>(outputFormat));
    if (comboIndex >= 0) {
        ui->outputFormatComboBox->setCurrentIndex(comboIndex);
    }
}

void ConverterDialog::dragEnterEvent(QDragEnterEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    if (mimeData == nullptr || !mimeData->hasUrls()) {
        event->ignore();
        return;
    }

    const QList<QUrl> urls = mimeData->urls();
    for (const QUrl &url : urls) {
        if (!url.isLocalFile()) {
            continue;
        }
        const QString localFile = normalizePathForCurrentPlatform(url.toLocalFile());
        if (!localFile.isEmpty() && isLikelyLdsFile(localFile)) {
            event->acceptProposedAction();
            return;
        }
    }

    event->ignore();
}

void ConverterDialog::dropEvent(QDropEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    if (mimeData == nullptr || !mimeData->hasUrls()) {
        event->ignore();
        return;
    }

    const QList<QUrl> urls = mimeData->urls();
    for (const QUrl &url : urls) {
        if (!url.isLocalFile()) {
            continue;
        }

        const QString localFile = normalizePathForCurrentPlatform(url.toLocalFile());
        if (!localFile.isEmpty() && isLikelyLdsFile(localFile)) {
            setDefaultInput(localFile);
            if (ui->statusLabel) {
                ui->statusLabel->setText(tr("Loaded input from drag/drop."));
            }
            event->acceptProposedAction();
            return;
        }
    }

    if (ui->statusLabel) {
        ui->statusLabel->setText(tr("Drop a .lds file to auto-fill the input path."));
    }
    event->ignore();
}

void ConverterDialog::on_inputBrowseButton_clicked()
{
    const QString filter = tr("LaserDisc samples (*.lds);;All Files (*)");
    const QString inputFileName = QFileDialog::getOpenFileName(this,
                                                               tr("Select .lds input file"),
                                                               sourceDirectory,
                                                               filter);
    if (inputFileName.isEmpty()) {
        return;
    }

    setDefaultInput(inputFileName);
    if (ui->statusLabel) {
        ui->statusLabel->clear();
    }
}

void ConverterDialog::on_outputBrowseButton_clicked()
{
    const DataConverter::OutputFormat format = selectedOutputFormat();
    const QString filter = format == DataConverter::OutputFormat::Flac
                               ? tr("FLAC output (*.flac);;All Files (*)")
                               : tr("Signed 16-bit raw (*.s16 *.raw *.pcm);;All Files (*)");

    QString suggestedOutput = normalizePathForCurrentPlatform(ui->outputLineEdit->text());
    if (suggestedOutput.isEmpty()) {
        suggestedOutput = DataConverter::defaultOutputPath(normalizePathForCurrentPlatform(ui->inputLineEdit->text()),
                                                           false,
                                                           format);
    }

    const QString outputFileName = QFileDialog::getSaveFileName(this,
                                                                tr("Select output file"),
                                                                suggestedOutput,
                                                                filter);
    if (outputFileName.isEmpty()) {
        return;
    }

    userEditedOutput = true;
    ui->outputLineEdit->setText(normalizePathForCurrentPlatform(outputFileName));
    if (ui->statusLabel) {
        ui->statusLabel->clear();
    }

    const QFileInfo selectedInfo(ui->outputLineEdit->text());
    if (!selectedInfo.absolutePath().isEmpty()) {
        sourceDirectory = selectedInfo.absolutePath();
    }
}

void ConverterDialog::on_convertButton_clicked()
{
    const QString inputFileName = normalizePathForCurrentPlatform(ui->inputLineEdit->text());
    QString outputFileName = normalizePathForCurrentPlatform(ui->outputLineEdit->text());
    const DataConverter::OutputFormat format = selectedOutputFormat();

    if (inputFileName.isEmpty()) {
        ui->statusLabel->setText(tr("Please choose an input .lds file."));
        return;
    }
    if (!QFileInfo::exists(inputFileName)) {
        ui->statusLabel->setText(tr("Input .lds file does not exist."));
        return;
    }

    if (outputFileName.isEmpty()) {
        outputFileName = DataConverter::defaultOutputPath(inputFileName, false, format);
        ui->outputLineEdit->setText(outputFileName);
    }

    if (outputFileName.isEmpty()) {
        ui->statusLabel->setText(tr("Please choose an output file."));
        return;
    }

    DataConverter converter(inputFileName, outputFileName, false, format);
    if (!converter.process()) {
        ui->statusLabel->setText(tr("Conversion failed."));
        QMessageBox::warning(this, tr("Error"), tr("Could not convert the selected file."));
        return;
    }

    ui->statusLabel->setText(tr("Conversion complete."));
}

void ConverterDialog::on_outputFormatComboBox_currentIndexChanged(int index)
{
    Q_UNUSED(index)
    updateOutputPathFromInput(false);
}

void ConverterDialog::updateOutputPathFromInput(bool forceUpdate)
{
    const QString normalizedInput = normalizePathForCurrentPlatform(ui->inputLineEdit->text());
    if (normalizedInput.isEmpty()) {
        return;
    }

    if (!forceUpdate && userEditedOutput && !ui->outputLineEdit->text().trimmed().isEmpty()) {
        return;
    }

    const QString suggestedOutput = DataConverter::defaultOutputPath(normalizedInput,
                                                                     false,
                                                                     selectedOutputFormat());
    if (suggestedOutput.isEmpty()) {
        return;
    }

    const QSignalBlocker blocker(ui->outputLineEdit);
    ui->outputLineEdit->setText(suggestedOutput);
    userEditedOutput = false;
}

bool ConverterDialog::isLikelyLdsFile(const QString &filePath) const
{
    return QFileInfo(filePath).suffix().compare(QStringLiteral("lds"), Qt::CaseInsensitive) == 0;
}

DataConverter::OutputFormat ConverterDialog::selectedOutputFormat() const
{
    const QVariant formatData = ui->outputFormatComboBox->currentData();
    if (!formatData.isValid()) {
        return DataConverter::OutputFormat::Flac;
    }
    return static_cast<DataConverter::OutputFormat>(formatData.toInt());
}
