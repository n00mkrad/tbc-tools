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
#include <QCoreApplication>
#include <QEventLoop>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
#include <QPalette>
#include <QSignalBlocker>
#include <QUrl>
#include <algorithm>

namespace {
QString normalizePathForCurrentPlatform(const QString &path)
{
    QString normalizedInput = path.trimmed();
    if (normalizedInput.isEmpty()) {
        return QString();
    }

    const QUrl inputUrl(normalizedInput);
    if (inputUrl.isValid() && inputUrl.scheme().compare(QStringLiteral("file"), Qt::CaseInsensitive) == 0) {
        const QString localFile = inputUrl.toLocalFile();
        if (!localFile.isEmpty()) {
            normalizedInput = localFile;
        }
    }

    const QString normalized = QDir::cleanPath(QDir::fromNativeSeparators(normalizedInput));
    return QDir::toNativeSeparators(normalized);
}

void applyHighContrastStylesheet(QWidget *widget)
{
    if (widget == nullptr) {
        return;
    }

    const bool isDarkBackground = widget->palette().color(QPalette::Window).lightnessF() < 0.5;
    if (isDarkBackground) {
        widget->setStyleSheet(QStringLiteral(
            "QDialog#ConverterDialog {"
            "  background-color: #202124;"
            "  color: #F5F7FA;"
            "}"
            "QDialog#ConverterDialog QLabel {"
            "  color: #F5F7FA;"
            "}"
            "QDialog#ConverterDialog QLineEdit,"
            "QDialog#ConverterDialog QComboBox {"
            "  background-color: #2C2F33;"
            "  color: #F5F7FA;"
            "  border: 1px solid #5F6368;"
            "  border-radius: 4px;"
            "  padding: 4px;"
            "  selection-background-color: #8AB4F8;"
            "  selection-color: #202124;"
            "}"
            "QDialog#ConverterDialog QLineEdit {"
            "  placeholder-text-color: #D0D4D9;"
            "}"
            "QDialog#ConverterDialog QComboBox QAbstractItemView {"
            "  background-color: #2C2F33;"
            "  color: #F5F7FA;"
            "  selection-background-color: #8AB4F8;"
            "  selection-color: #202124;"
            "}"
            "QDialog#ConverterDialog QPushButton {"
            "  background-color: #3C4043;"
            "  color: #F5F7FA;"
            "  border: 1px solid #5F6368;"
            "  border-radius: 4px;"
            "  padding: 4px 10px;"
            "}"
            "QDialog#ConverterDialog QPushButton:hover {"
            "  background-color: #4A4F53;"
            "}"
            "QDialog#ConverterDialog QPushButton:pressed {"
            "  background-color: #303336;"
            "}"
            "QDialog#ConverterDialog QProgressBar {"
            "  background-color: #2C2F33;"
            "  color: #F5F7FA;"
            "  border: 1px solid #5F6368;"
            "  border-radius: 4px;"
            "  text-align: center;"
            "}"
            "QDialog#ConverterDialog QProgressBar::chunk {"
            "  background-color: #8AB4F8;"
            "}"
            "QDialog#ConverterDialog QLabel#progressPercentLabel {"
            "  color: #F5F7FA;"
            "}"));
        return;
    }

    widget->setStyleSheet(QStringLiteral(
        "QDialog#ConverterDialog {"
        "  background-color: #F5F6F8;"
        "  color: #16181C;"
        "}"
        "QDialog#ConverterDialog QLabel {"
        "  color: #16181C;"
        "}"
        "QDialog#ConverterDialog QLineEdit,"
        "QDialog#ConverterDialog QComboBox {"
        "  background-color: #FFFFFF;"
        "  color: #16181C;"
        "  border: 1px solid #8E949B;"
        "  border-radius: 4px;"
        "  padding: 4px;"
        "  selection-background-color: #0B57D0;"
        "  selection-color: #FFFFFF;"
        "}"
        "QDialog#ConverterDialog QLineEdit {"
        "  placeholder-text-color: #5F6368;"
        "}"
        "QDialog#ConverterDialog QComboBox QAbstractItemView {"
        "  background-color: #FFFFFF;"
        "  color: #16181C;"
        "  selection-background-color: #0B57D0;"
        "  selection-color: #FFFFFF;"
        "}"
        "QDialog#ConverterDialog QPushButton {"
        "  background-color: #E5E8EC;"
        "  color: #16181C;"
        "  border: 1px solid #8E949B;"
        "  border-radius: 4px;"
        "  padding: 4px 10px;"
        "}"
        "QDialog#ConverterDialog QPushButton:hover {"
        "  background-color: #D8DDE3;"
        "}"
        "QDialog#ConverterDialog QPushButton:pressed {"
        "  background-color: #C9D0D8;"
        "}"
        "QDialog#ConverterDialog QProgressBar {"
        "  background-color: #FFFFFF;"
        "  color: #16181C;"
        "  border: 1px solid #8E949B;"
        "  border-radius: 4px;"
        "  text-align: center;"
        "}"
        "QDialog#ConverterDialog QProgressBar::chunk {"
        "  background-color: #0B57D0;"
        "}"
        "QDialog#ConverterDialog QLabel#progressPercentLabel {"
        "  color: #16181C;"
        "}"));
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
    applyHighContrastStylesheet(this);
    resetProgressDisplay();

    if (ui->outputFormatComboBox) {
        ui->outputFormatComboBox->clear();
        ui->outputFormatComboBox->addItem(tr("FLAC (default)"), static_cast<int>(DataConverter::OutputFormat::Flac));
        ui->outputFormatComboBox->addItem(tr("s16 uncompressed"), static_cast<int>(DataConverter::OutputFormat::S16Raw));
        ui->outputFormatComboBox->setCurrentIndex(0);
    }

    if (ui->inputLineEdit) {
        connect(ui->inputLineEdit, &QLineEdit::textChanged, this, [this]() {
            updateOutputPathFromInput(false);
            resetProgressDisplay();
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
    resetProgressDisplay();
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
    resetProgressDisplay();
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
    connect(&converter, &DataConverter::progressUpdated,
            this, &ConverterDialog::updateProgressDisplay);

    if (ui->statusLabel) {
        ui->statusLabel->setText(tr("Converting..."));
    }
    resetProgressDisplay();
    setConversionControlsEnabled(false);
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    const bool conversionOk = converter.process();

    setConversionControlsEnabled(true);
    if (!conversionOk) {
        ui->statusLabel->setText(tr("Conversion failed."));
        QMessageBox::warning(this, tr("Error"), tr("Could not convert the selected file."));
        return;
    }
    updateProgressDisplay(1, 1);

    ui->statusLabel->setText(tr("Conversion complete."));
}

void ConverterDialog::on_outputFormatComboBox_currentIndexChanged(int index)
{
    Q_UNUSED(index)
    updateOutputPathFromInput(false);
    resetProgressDisplay();
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
    return QFileInfo(normalizePathForCurrentPlatform(filePath))
               .suffix()
               .compare(QStringLiteral("lds"), Qt::CaseInsensitive) == 0;
}

DataConverter::OutputFormat ConverterDialog::selectedOutputFormat() const
{
    const QVariant formatData = ui->outputFormatComboBox->currentData();
    if (!formatData.isValid()) {
        return DataConverter::OutputFormat::Flac;
    }
    return static_cast<DataConverter::OutputFormat>(formatData.toInt());
}

void ConverterDialog::setConversionControlsEnabled(bool enabled)
{
    if (ui->convertButton) {
        ui->convertButton->setEnabled(enabled);
    }
    if (ui->inputBrowseButton) {
        ui->inputBrowseButton->setEnabled(enabled);
    }
    if (ui->outputBrowseButton) {
        ui->outputBrowseButton->setEnabled(enabled);
    }
    if (ui->inputLineEdit) {
        ui->inputLineEdit->setEnabled(enabled);
    }
    if (ui->outputLineEdit) {
        ui->outputLineEdit->setEnabled(enabled);
    }
    if (ui->outputFormatComboBox) {
        ui->outputFormatComboBox->setEnabled(enabled);
    }
}

void ConverterDialog::resetProgressDisplay()
{
    if (ui->progressBar) {
        ui->progressBar->setRange(0, 100);
        ui->progressBar->setValue(0);
    }
    if (ui->progressPercentLabel) {
        ui->progressPercentLabel->setText(tr("0%"));
    }
}

void ConverterDialog::updateProgressDisplay(qint64 processedBytes, qint64 totalBytes)
{
    if (ui->progressBar == nullptr || ui->progressPercentLabel == nullptr) {
        return;
    }

    if (totalBytes <= 0) {
        ui->progressBar->setRange(0, 0);
        ui->progressPercentLabel->setText(tr("--%"));
    } else {
        const qint64 clampedProcessed = std::clamp(processedBytes, static_cast<qint64>(0), totalBytes);
        const int percentage = static_cast<int>((clampedProcessed * 100) / totalBytes);
        ui->progressBar->setRange(0, 100);
        ui->progressBar->setValue(percentage);
        ui->progressPercentLabel->setText(tr("%1%").arg(percentage));
    }

    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}
