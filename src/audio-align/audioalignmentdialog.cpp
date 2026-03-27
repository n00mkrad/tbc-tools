/******************************************************************************
 * audioalignmentdialog.cpp
 * tbc-audio-align - Audio alignment GUI for ld-decode
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 *
 * This file is part of ld-decode-tools.
 ******************************************************************************/

#include "audioalignmentdialog.h"
#include "ui_audioalignmentdialog.h"

#include "audioalignmentutil.h"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMessageBox>
#include <QSaveFile>
#include <QSignalBlocker>

AudioAlignmentDialog::AudioAlignmentDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AudioAlignmentDialog)
{
    ui->setupUi(this);

    const QString rfTimebaseWarning = tr("RF Video Sample Rate must match the decoder timebase used to generate metadata JSON: use 20000000 for 20 Msps no-resampling captures, 16000000 for 16 Msps captures, or 40000000 when the decoder used internal resampling to 40 Msps.");
    if (ui->rfVideoRatePresetComboBox) {
        ui->rfVideoRatePresetComboBox->setCurrentIndex(0);
        ui->rfVideoRatePresetComboBox->setToolTip(rfTimebaseWarning);
        connect(ui->rfVideoRatePresetComboBox,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this,
                [this](int presetIndex) {
            const bool customSelected = (presetIndex == 3);
            if (ui->rfVideoSampleRateCustomSpinBox) {
                ui->rfVideoSampleRateCustomSpinBox->setEnabled(customSelected);
                if (!customSelected) {
                    if (presetIndex == 1) {
                        ui->rfVideoSampleRateCustomSpinBox->setValue(20000000);
                    } else if (presetIndex == 2) {
                        ui->rfVideoSampleRateCustomSpinBox->setValue(16000000);
                    } else {
                        ui->rfVideoSampleRateCustomSpinBox->setValue(40000000);
                    }
                }
            }
        });
    }
    if (ui->rfVideoSampleRateCustomSpinBox) {
        ui->rfVideoSampleRateCustomSpinBox->setValue(40000000);
        ui->rfVideoSampleRateCustomSpinBox->setEnabled(false);
        ui->rfVideoSampleRateCustomSpinBox->setToolTip(rfTimebaseWarning);
    }
    if (ui->rfVideoRatePresetLabel) {
        ui->rfVideoRatePresetLabel->setToolTip(rfTimebaseWarning);
    }
    if (ui->rfRateWarningLabel) {
        ui->rfRateWarningLabel->setToolTip(rfTimebaseWarning);
    }

    if (ui->overwriteCheckBox) {
        ui->overwriteCheckBox->setChecked(true);
    }
    const QString loadTracksForExportTooltip = tr("When launched from ld-analyse, load aligned track files and names into Export audio tracks.");
    if (ui->loadTracksForExportCheckBox) {
        ui->loadTracksForExportCheckBox->setToolTip(loadTracksForExportTooltip);
    }
    if (ui->loadTracksForExportLabel) {
        ui->loadTracksForExportLabel->setToolTip(loadTracksForExportTooltip);
    }

    if (ui->statusLabel) {
        ui->statusLabel->setText(tr("Using internal AAA workflow (ffprobe + ffmpeg + VhsDecodeAutoAudioAlign)."));
    }

    if (ui->linearInputLineEdit) {
        connect(ui->linearInputLineEdit, &QLineEdit::textChanged, this, [this]() {
            updateLinearOutputFromInput(false);
            if (ui->statusLabel) {
                ui->statusLabel->clear();
            }
        });
    }
    if (ui->hifiInputLineEdit) {
        connect(ui->hifiInputLineEdit, &QLineEdit::textChanged, this, [this]() {
            updateHifiOutputFromInput(false);
            if (ui->statusLabel) {
                ui->statusLabel->clear();
            }
        });
    }
    if (ui->linearOutputLineEdit) {
        connect(ui->linearOutputLineEdit, &QLineEdit::textEdited, this, [this]() {
            userEditedLinearOutput = true;
            if (ui->statusLabel) {
                ui->statusLabel->clear();
            }
        });
    }
    if (ui->hifiOutputLineEdit) {
        connect(ui->hifiOutputLineEdit, &QLineEdit::textEdited, this, [this]() {
            userEditedHifiOutput = true;
            if (ui->statusLabel) {
                ui->statusLabel->clear();
            }
        });
    }
    if (ui->jsonLineEdit) {
        connect(ui->jsonLineEdit, &QLineEdit::textChanged, this, [this]() {
            tryAutoDetectInputsFromJson(false);
            if (ui->statusLabel) {
                ui->statusLabel->clear();
            }
        });
    }
}

AudioAlignmentDialog::~AudioAlignmentDialog()
{
    delete ui;
}

void AudioAlignmentDialog::setSourceDirectory(const QString &directory)
{
    if (!directory.isEmpty()) {
        sourceDirectory = directory;
    }
}

void AudioAlignmentDialog::setDefaultJson(const QString &jsonFilename)
{
    const QString normalizedJson = AudioAlignmentUtil::normalizePathForCurrentPlatform(jsonFilename);
    if (normalizedJson.isEmpty()) {
        return;
    }

    ui->jsonLineEdit->setText(normalizedJson);
    const QFileInfo jsonInfo(normalizedJson);
    if (jsonInfo.exists()) {
        sourceDirectory = jsonInfo.absolutePath();
    }
    tryAutoDetectInputsFromJson(false);
}

void AudioAlignmentDialog::setDefaultInputFile(const QString &inputFilename)
{
    const QString normalizedInput = AudioAlignmentUtil::normalizePathForCurrentPlatform(inputFilename);
    if (normalizedInput.isEmpty()) {
        return;
    }

    ui->linearInputLineEdit->setText(normalizedInput);
    updateLinearOutputFromInput(true);
    const QFileInfo inputInfo(normalizedInput);
    if (inputInfo.exists()) {
        sourceDirectory = inputInfo.absolutePath();
    }
}

void AudioAlignmentDialog::setDefaultOutputFile(const QString &outputFilename)
{
    const QString normalizedOutput = AudioAlignmentUtil::normalizePathForCurrentPlatform(outputFilename);
    if (normalizedOutput.isEmpty()) {
        return;
    }

    userEditedLinearOutput = true;
    ui->linearOutputLineEdit->setText(normalizedOutput);
    const QFileInfo outputInfo(normalizedOutput);
    if (!outputInfo.absolutePath().isEmpty()) {
        sourceDirectory = outputInfo.absolutePath();
    }
}

void AudioAlignmentDialog::setDefaultRfVideoSampleRate(quint32 sampleRateHz)
{
    if (!ui->rfVideoRatePresetComboBox || !ui->rfVideoSampleRateCustomSpinBox) {
        return;
    }

    if (sampleRateHz == 40000000) {
        ui->rfVideoRatePresetComboBox->setCurrentIndex(0);
    } else if (sampleRateHz == 20000000) {
        ui->rfVideoRatePresetComboBox->setCurrentIndex(1);
    } else if (sampleRateHz == 16000000) {
        ui->rfVideoRatePresetComboBox->setCurrentIndex(2);
    } else {
        ui->rfVideoRatePresetComboBox->setCurrentIndex(3);
        ui->rfVideoSampleRateCustomSpinBox->setValue(static_cast<int>(sampleRateHz));
    }
}

void AudioAlignmentDialog::setExportTrackOutputFile(const QString &outputFilename)
{
    exportTrackOutputFile = AudioAlignmentUtil::normalizePathForCurrentPlatform(outputFilename);
}

void AudioAlignmentDialog::on_jsonBrowseButton_clicked()
{
    const QString filter = tr("TBC metadata JSON (*.json);;All Files (*)");
    const QString jsonFileName = QFileDialog::getOpenFileName(this,
                                                              tr("Select metadata JSON"),
                                                              sourceDirectory,
                                                              filter);
    if (jsonFileName.isEmpty()) {
        return;
    }

    const QString normalizedJson = AudioAlignmentUtil::normalizePathForCurrentPlatform(jsonFileName);
    ui->jsonLineEdit->setText(normalizedJson);
    tryAutoDetectInputsFromJson(true);
    if (ui->statusLabel) {
        ui->statusLabel->clear();
    }

    const QFileInfo selectedInfo(normalizedJson);
    if (selectedInfo.exists()) {
        sourceDirectory = selectedInfo.absolutePath();
    }
}

void AudioAlignmentDialog::on_linearInputBrowseButton_clicked()
{
    const QString filter = tr("Audio input (*.wav *.flac);;All Files (*)");
    const QString inputFileName = QFileDialog::getOpenFileName(this,
                                                               tr("Select linear/baseband input audio"),
                                                               sourceDirectory,
                                                               filter);
    if (inputFileName.isEmpty()) {
        return;
    }

    const QString normalizedInput = AudioAlignmentUtil::normalizePathForCurrentPlatform(inputFileName);
    ui->linearInputLineEdit->setText(normalizedInput);
    updateLinearOutputFromInput(true);
    if (ui->statusLabel) {
        ui->statusLabel->clear();
    }

    const QFileInfo selectedInfo(normalizedInput);
    if (selectedInfo.exists()) {
        sourceDirectory = selectedInfo.absolutePath();
    }
}

void AudioAlignmentDialog::on_linearOutputBrowseButton_clicked()
{
    QString suggestedOutput = ui->linearOutputLineEdit->text().trimmed();
    if (suggestedOutput.isEmpty()) {
        suggestedOutput = AudioAlignmentUtil::defaultAlignedOutputPath(ui->linearInputLineEdit->text());
    }

    const QString filter = tr("Aligned audio (*.flac);;All Files (*)");
    const QString outputFileName = QFileDialog::getSaveFileName(this,
                                                                tr("Select aligned linear/baseband output"),
                                                                suggestedOutput,
                                                                filter);
    if (outputFileName.isEmpty()) {
        return;
    }

    userEditedLinearOutput = true;
    ui->linearOutputLineEdit->setText(AudioAlignmentUtil::normalizePathForCurrentPlatform(outputFileName));
    if (ui->statusLabel) {
        ui->statusLabel->clear();
    }

    const QFileInfo selectedInfo(ui->linearOutputLineEdit->text());
    if (!selectedInfo.absolutePath().isEmpty()) {
        sourceDirectory = selectedInfo.absolutePath();
    }
}

void AudioAlignmentDialog::on_hifiInputBrowseButton_clicked()
{
    const QString filter = tr("Audio input (*.wav *.flac);;All Files (*)");
    const QString inputFileName = QFileDialog::getOpenFileName(this,
                                                               tr("Select hifi input audio"),
                                                               sourceDirectory,
                                                               filter);
    if (inputFileName.isEmpty()) {
        return;
    }

    const QString normalizedInput = AudioAlignmentUtil::normalizePathForCurrentPlatform(inputFileName);
    ui->hifiInputLineEdit->setText(normalizedInput);
    updateHifiOutputFromInput(true);
    if (ui->statusLabel) {
        ui->statusLabel->clear();
    }

    const QFileInfo selectedInfo(normalizedInput);
    if (selectedInfo.exists()) {
        sourceDirectory = selectedInfo.absolutePath();
    }
}

void AudioAlignmentDialog::on_hifiOutputBrowseButton_clicked()
{
    QString suggestedOutput = ui->hifiOutputLineEdit->text().trimmed();
    if (suggestedOutput.isEmpty()) {
        suggestedOutput = AudioAlignmentUtil::defaultAlignedOutputPath(ui->hifiInputLineEdit->text());
    }

    const QString filter = tr("Aligned audio (*.flac);;All Files (*)");
    const QString outputFileName = QFileDialog::getSaveFileName(this,
                                                                tr("Select aligned hifi output"),
                                                                suggestedOutput,
                                                                filter);
    if (outputFileName.isEmpty()) {
        return;
    }

    userEditedHifiOutput = true;
    ui->hifiOutputLineEdit->setText(AudioAlignmentUtil::normalizePathForCurrentPlatform(outputFileName));
    if (ui->statusLabel) {
        ui->statusLabel->clear();
    }

    const QFileInfo selectedInfo(ui->hifiOutputLineEdit->text());
    if (!selectedInfo.absolutePath().isEmpty()) {
        sourceDirectory = selectedInfo.absolutePath();
    }
}

void AudioAlignmentDialog::on_alignButton_clicked()
{
    const QString jsonFileName = AudioAlignmentUtil::normalizePathForCurrentPlatform(ui->jsonLineEdit->text());
    QString linearInputFileName = AudioAlignmentUtil::normalizePathForCurrentPlatform(ui->linearInputLineEdit->text());
    QString linearOutputFileName = AudioAlignmentUtil::normalizePathForCurrentPlatform(ui->linearOutputLineEdit->text());
    QString hifiInputFileName = AudioAlignmentUtil::normalizePathForCurrentPlatform(ui->hifiInputLineEdit->text());
    QString hifiOutputFileName = AudioAlignmentUtil::normalizePathForCurrentPlatform(ui->hifiOutputLineEdit->text());

    if (linearOutputFileName.isEmpty() && !linearInputFileName.isEmpty()) {
        linearOutputFileName = AudioAlignmentUtil::defaultAlignedOutputPath(linearInputFileName);
    }
    if (hifiOutputFileName.isEmpty() && !hifiInputFileName.isEmpty()) {
        hifiOutputFileName = AudioAlignmentUtil::defaultAlignedOutputPath(hifiInputFileName);
    }

    if (jsonFileName.isEmpty()) {
        ui->statusLabel->setText(tr("Please choose metadata JSON."));
        return;
    }
    if (!QFileInfo::exists(jsonFileName)) {
        ui->statusLabel->setText(tr("Metadata JSON does not exist."));
        return;
    }

    const bool hasLinearTrack = !linearInputFileName.isEmpty();
    const bool hasHifiTrack = !hifiInputFileName.isEmpty();
    if (!hasLinearTrack && !hasHifiTrack) {
        ui->statusLabel->setText(tr("Please choose at least one input audio track (Linear/Baseband or HiFi)."));
        return;
    }
    if (hasLinearTrack && !QFileInfo::exists(linearInputFileName)) {
        ui->statusLabel->setText(tr("Linear/Baseband input audio file does not exist."));
        return;
    }
    if (hasHifiTrack && !QFileInfo::exists(hifiInputFileName)) {
        ui->statusLabel->setText(tr("HiFi input audio file does not exist."));
        return;
    }

    auto comparablePathForMatch = [](const QString &path) -> QString {
        const QFileInfo info(path);
        const QString canonicalPath = info.canonicalFilePath();
        if (!canonicalPath.isEmpty()) {
            return canonicalPath;
        }
        return info.absoluteFilePath();
    };

    if (hasLinearTrack && hasHifiTrack
        && comparablePathForMatch(linearOutputFileName) == comparablePathForMatch(hifiOutputFileName)) {
        ui->statusLabel->setText(tr("Linear/Baseband and HiFi outputs must be different files."));
        return;
    }

    if (hasLinearTrack && QFileInfo::exists(linearOutputFileName) && !ui->overwriteCheckBox->isChecked()) {
        ui->statusLabel->setText(tr("Linear/Baseband output exists. Enable overwrite or choose a new output path."));
        return;
    }
    if (hasHifiTrack && QFileInfo::exists(hifiOutputFileName) && !ui->overwriteCheckBox->isChecked()) {
        ui->statusLabel->setText(tr("HiFi output exists. Enable overwrite or choose a new output path."));
        return;
    }

    {
        const QSignalBlocker jsonBlocker(ui->jsonLineEdit);
        ui->jsonLineEdit->setText(jsonFileName);
    }
    {
        const QSignalBlocker linearInputBlocker(ui->linearInputLineEdit);
        ui->linearInputLineEdit->setText(linearInputFileName);
    }
    {
        const QSignalBlocker linearOutputBlocker(ui->linearOutputLineEdit);
        ui->linearOutputLineEdit->setText(linearOutputFileName);
    }
    {
        const QSignalBlocker hifiInputBlocker(ui->hifiInputLineEdit);
        ui->hifiInputLineEdit->setText(hifiInputFileName);
    }
    {
        const QSignalBlocker hifiOutputBlocker(ui->hifiOutputLineEdit);
        ui->hifiOutputLineEdit->setText(hifiOutputFileName);
    }

    ui->alignButton->setEnabled(false);
    QApplication::setOverrideCursor(Qt::WaitCursor);

    const int totalTracks = (hasLinearTrack ? 1 : 0) + (hasHifiTrack ? 1 : 0);
    int completedTracks = 0;
    auto updateProgressStatus = [&](const QString &trackLabel, int trackPercent, const QString &stageMessage) {
        const int boundedTrackPercent = qBound(0, trackPercent, 100);
        const double completedFraction = static_cast<double>(completedTracks)
                                         + (static_cast<double>(boundedTrackPercent) / 100.0);
        const int overallPercent = qBound(0,
                                          static_cast<int>(((completedFraction / qMax(1, totalTracks)) * 100.0) + 0.5),
                                          100);
        QString progressText = stageMessage.trimmed();
        if (progressText.isEmpty()) {
            progressText = tr("Running alignment for %1...").arg(trackLabel);
        } else {
            progressText = tr("%1: %2").arg(trackLabel, progressText);
        }
        ui->statusLabel->setText(tr("%1 (%2%)").arg(progressText).arg(overallPercent));
        QApplication::processEvents();
    };

    auto runTrack = [&](const QString &trackLabel, const QString &inputFileName, const QString &outputFileName) {
        updateProgressStatus(trackLabel, 0, QString());

        QString errorMessage;
        const bool success = AudioAlignmentUtil::runStreamAlign(
            jsonFileName,
            inputFileName,
            outputFileName,
            currentRfVideoSampleRateHz(),
            ui->overwriteCheckBox->isChecked(),
            [&](int stagePercent, const QString &stageMessage) {
                updateProgressStatus(trackLabel, stagePercent, stageMessage);
            },
            &errorMessage);
        if (!success) {
            ui->statusLabel->setText(tr("%1 alignment failed.").arg(trackLabel));
            QMessageBox::warning(this, tr("Error"),
                                 errorMessage.isEmpty()
                                     ? tr("Auto audio alignment failed for %1.").arg(trackLabel)
                                     : errorMessage);
            return false;
        }
        completedTracks++;
        updateProgressStatus(trackLabel, 100, tr("Alignment complete."));
        return true;
    };

    bool success = true;
    if (hasLinearTrack) {
        success = runTrack(tr("Baseband"), linearInputFileName, linearOutputFileName);
    }
    if (success && hasHifiTrack) {
        success = runTrack(tr("HiFi-Decode"), hifiInputFileName, hifiOutputFileName);
    }

    QApplication::restoreOverrideCursor();
    ui->alignButton->setEnabled(true);

    if (!success) {
        return;
    }

    if (hasLinearTrack && hasHifiTrack) {
        ui->statusLabel->setText(tr("Alignment complete for Baseband and HiFi-Decode tracks."));
    } else if (hasLinearTrack) {
        ui->statusLabel->setText(tr("Alignment complete for Baseband track."));
    } else {
        ui->statusLabel->setText(tr("Alignment complete for HiFi-Decode track."));
    }
    const bool shouldPrepareExportTracks = ui->loadTracksForExportCheckBox
                                           && ui->loadTracksForExportCheckBox->isChecked()
                                           && !exportTrackOutputFile.trimmed().isEmpty();
    if (shouldPrepareExportTracks) {
        QString payloadError;
        if (!writeExportTrackPayload(linearOutputFileName,
                                     hifiOutputFileName,
                                     hasLinearTrack,
                                     hasHifiTrack,
                                     &payloadError)) {
            QMessageBox::warning(this, tr("Export track auto-load"),
                                 payloadError.isEmpty()
                                     ? tr("Alignment completed, but export track auto-load data could not be written.")
                                     : payloadError);
        } else if (ui->statusLabel) {
            ui->statusLabel->setText(ui->statusLabel->text() + tr(" Export tracks prepared."));
        }
    }

    const QFileInfo outputInfo(hasHifiTrack ? hifiOutputFileName : linearOutputFileName);
    if (!outputInfo.absolutePath().isEmpty()) {
        sourceDirectory = outputInfo.absolutePath();
    }
}

void AudioAlignmentDialog::updateLinearOutputFromInput(bool forceOutputUpdate)
{
    const QString normalizedInput = AudioAlignmentUtil::normalizePathForCurrentPlatform(ui->linearInputLineEdit->text());
    if (!normalizedInput.isEmpty() && normalizedInput != ui->linearInputLineEdit->text()) {
        const QSignalBlocker blocker(ui->linearInputLineEdit);
        ui->linearInputLineEdit->setText(normalizedInput);
    }

    if (normalizedInput.isEmpty()) {
        if (forceOutputUpdate || !userEditedLinearOutput) {
            const QSignalBlocker blocker(ui->linearOutputLineEdit);
            ui->linearOutputLineEdit->clear();
            userEditedLinearOutput = false;
        }
        return;
    }

    const QString suggestedOutput = AudioAlignmentUtil::defaultAlignedOutputPath(normalizedInput);
    if (suggestedOutput.isEmpty()) {
        return;
    }

    const bool outputEmpty = ui->linearOutputLineEdit->text().trimmed().isEmpty();
    if (forceOutputUpdate || outputEmpty || !userEditedLinearOutput) {
        const QSignalBlocker blocker(ui->linearOutputLineEdit);
        ui->linearOutputLineEdit->setText(suggestedOutput);
        userEditedLinearOutput = false;
    }
}

void AudioAlignmentDialog::updateHifiOutputFromInput(bool forceOutputUpdate)
{
    const QString normalizedInput = AudioAlignmentUtil::normalizePathForCurrentPlatform(ui->hifiInputLineEdit->text());
    if (!normalizedInput.isEmpty() && normalizedInput != ui->hifiInputLineEdit->text()) {
        const QSignalBlocker blocker(ui->hifiInputLineEdit);
        ui->hifiInputLineEdit->setText(normalizedInput);
    }

    if (normalizedInput.isEmpty()) {
        if (forceOutputUpdate || !userEditedHifiOutput) {
            const QSignalBlocker blocker(ui->hifiOutputLineEdit);
            ui->hifiOutputLineEdit->clear();
            userEditedHifiOutput = false;
        }
        return;
    }

    const QString suggestedOutput = AudioAlignmentUtil::defaultAlignedOutputPath(normalizedInput);
    if (suggestedOutput.isEmpty()) {
        return;
    }

    const bool outputEmpty = ui->hifiOutputLineEdit->text().trimmed().isEmpty();
    if (forceOutputUpdate || outputEmpty || !userEditedHifiOutput) {
        const QSignalBlocker blocker(ui->hifiOutputLineEdit);
        ui->hifiOutputLineEdit->setText(suggestedOutput);
        userEditedHifiOutput = false;
    }
}

void AudioAlignmentDialog::tryAutoDetectInputsFromJson(bool forceReplaceInputs)
{
    const bool replaceLinearInput = forceReplaceInputs || ui->linearInputLineEdit->text().trimmed().isEmpty();
    const bool replaceHifiInput = forceReplaceInputs || ui->hifiInputLineEdit->text().trimmed().isEmpty();
    if (!replaceLinearInput && !replaceHifiInput) {
        return;
    }

    QString detectedLinearInput;
    QString detectedHifiInput;

    if (replaceLinearInput) {
        const QString excludePath = replaceHifiInput ? ui->hifiInputLineEdit->text() : QString();
        detectedLinearInput = AudioAlignmentUtil::autoDetectLinearInputAudioFile(ui->jsonLineEdit->text(), excludePath);
        if (detectedLinearInput.isEmpty()) {
            detectedLinearInput = AudioAlignmentUtil::autoDetectInputAudioFile(ui->jsonLineEdit->text(), excludePath);
        }
    }
    if (replaceHifiInput) {
        const QString excludePath = !detectedLinearInput.isEmpty() ? detectedLinearInput : ui->linearInputLineEdit->text();
        detectedHifiInput = AudioAlignmentUtil::autoDetectHifiInputAudioFile(ui->jsonLineEdit->text(), excludePath);
        if (detectedHifiInput.isEmpty()) {
            detectedHifiInput = AudioAlignmentUtil::autoDetectInputAudioFile(ui->jsonLineEdit->text(), excludePath);
        }
    }

    if (replaceLinearInput) {
        const QSignalBlocker inputBlocker(ui->linearInputLineEdit);
        ui->linearInputLineEdit->setText(detectedLinearInput);
    }
    if (replaceHifiInput) {
        const QSignalBlocker inputBlocker(ui->hifiInputLineEdit);
        ui->hifiInputLineEdit->setText(detectedHifiInput);
    }

    if (replaceLinearInput) {
        updateLinearOutputFromInput(true);
    }
    if (replaceHifiInput) {
        updateHifiOutputFromInput(true);
    }

    if (!detectedHifiInput.isEmpty()) {
        const QFileInfo detectedInfo(detectedHifiInput);
        if (detectedInfo.exists()) {
            sourceDirectory = detectedInfo.absolutePath();
        }
    } else if (!detectedLinearInput.isEmpty()) {
        const QFileInfo detectedInfo(detectedLinearInput);
        if (detectedInfo.exists()) {
            sourceDirectory = detectedInfo.absolutePath();
        }
    }
}

bool AudioAlignmentDialog::writeExportTrackPayload(const QString &linearOutputFile,
                                                   const QString &hifiOutputFile,
                                                   bool includeLinearTrack,
                                                   bool includeHifiTrack,
                                                   QString *errorMessage) const
{
    if (errorMessage) {
        errorMessage->clear();
    }

    const QString payloadPath = AudioAlignmentUtil::normalizePathForCurrentPlatform(exportTrackOutputFile);
    if (payloadPath.trimmed().isEmpty()) {
        return true;
    }

    const QFileInfo payloadInfo(payloadPath);
    const QString payloadDirectory = payloadInfo.absolutePath();
    if (!payloadDirectory.isEmpty() && !QDir().mkpath(payloadDirectory)) {
        if (errorMessage) {
            *errorMessage = tr("Unable to create export track payload directory:\n%1").arg(payloadDirectory);
        }
        return false;
    }

    QJsonArray tracks;
    auto appendTrack = [&tracks](bool includeTrack,
                                 const QString &trackFilePath,
                                 const QString &trackRole,
                                 const QString &trackName) {
        if (!includeTrack) {
            return;
        }
        const QString normalizedTrackPath = AudioAlignmentUtil::normalizePathForCurrentPlatform(trackFilePath);
        if (normalizedTrackPath.isEmpty()) {
            return;
        }
        const QFileInfo trackInfo(normalizedTrackPath);
        const QString absoluteTrackPath = trackInfo.isAbsolute()
            ? trackInfo.absoluteFilePath()
            : QDir::current().absoluteFilePath(normalizedTrackPath);
        if (absoluteTrackPath.trimmed().isEmpty()) {
            return;
        }
        QJsonObject trackObject;
        trackObject.insert(QStringLiteral("role"), trackRole);
        trackObject.insert(QStringLiteral("name"), trackName);
        trackObject.insert(QStringLiteral("file"), QDir::toNativeSeparators(absoluteTrackPath));
        tracks.append(trackObject);
    };

    appendTrack(includeLinearTrack,
                linearOutputFile,
                QStringLiteral("linear"),
                tr("Baseband"));
    appendTrack(includeHifiTrack,
                hifiOutputFile,
                QStringLiteral("hifi"),
                tr("HiFi-Decode"));

    if (tracks.isEmpty()) {
        QFile::remove(payloadPath);
        return true;
    }

    QJsonObject rootObject;
    rootObject.insert(QStringLiteral("format"), QStringLiteral("ld-analyse-export-audio-tracks"));
    rootObject.insert(QStringLiteral("version"), 1);
    rootObject.insert(QStringLiteral("tracks"), tracks);

    QSaveFile payloadFile(payloadPath);
    if (!payloadFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = tr("Unable to open export track payload file for writing:\n%1").arg(payloadPath);
        }
        return false;
    }
    const QByteArray payloadData = QJsonDocument(rootObject).toJson(QJsonDocument::Compact);
    if (payloadFile.write(payloadData) != payloadData.size()) {
        if (errorMessage) {
            *errorMessage = tr("Unable to write export track payload file:\n%1").arg(payloadPath);
        }
        payloadFile.cancelWriting();
        return false;
    }
    if (!payloadFile.commit()) {
        if (errorMessage) {
            *errorMessage = tr("Unable to finalize export track payload file:\n%1").arg(payloadPath);
        }
        return false;
    }

    return true;
}

quint32 AudioAlignmentDialog::currentRfVideoSampleRateHz() const
{
    if (!ui->rfVideoRatePresetComboBox || !ui->rfVideoSampleRateCustomSpinBox) {
        return 40000000;
    }

    switch (ui->rfVideoRatePresetComboBox->currentIndex()) {
    case 1:
        return 20000000;
    case 2:
        return 16000000;
    case 3:
        return static_cast<quint32>(ui->rfVideoSampleRateCustomSpinBox->value());
    case 0:
    default:
        return 40000000;
    }
}
