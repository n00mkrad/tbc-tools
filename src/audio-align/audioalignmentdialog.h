/******************************************************************************
 * audioalignmentdialog.h
 * tbc-audio-align - Audio alignment GUI for ld-decode
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 *
 * This file is part of ld-decode-tools.
 ******************************************************************************/

#ifndef AUDIOALIGNMENTDIALOG_H
#define AUDIOALIGNMENTDIALOG_H

#include <QDialog>
#include <QtGlobal>

namespace Ui {
class AudioAlignmentDialog;
}

class AudioAlignmentDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AudioAlignmentDialog(QWidget *parent = nullptr);
    ~AudioAlignmentDialog();

    void setSourceDirectory(const QString &directory);
    void setDefaultJson(const QString &jsonFilename);
    void setDefaultInputFile(const QString &inputFilename);
    void setDefaultOutputFile(const QString &outputFilename);
    void setDefaultRfVideoSampleRate(quint32 sampleRateHz);
    void setExportTrackOutputFile(const QString &outputFilename);

private slots:
    void on_jsonBrowseButton_clicked();
    void on_linearInputBrowseButton_clicked();
    void on_linearOutputBrowseButton_clicked();
    void on_hifiInputBrowseButton_clicked();
    void on_hifiOutputBrowseButton_clicked();
    void on_alignButton_clicked();

private:
    void updateLinearOutputFromInput(bool forceOutputUpdate);
    void updateHifiOutputFromInput(bool forceOutputUpdate);
    void tryAutoDetectInputsFromJson(bool forceReplaceInputs);
    bool writeExportTrackPayload(const QString &linearOutputFile,
                                 const QString &hifiOutputFile,
                                 bool includeLinearTrack,
                                 bool includeHifiTrack,
                                 QString *errorMessage) const;
    quint32 currentRfVideoSampleRateHz() const;
    Ui::AudioAlignmentDialog *ui;
    QString sourceDirectory;
    QString exportTrackOutputFile;
    bool userEditedLinearOutput = false;
    bool userEditedHifiOutput = false;
};

#endif // AUDIOALIGNMENTDIALOG_H
