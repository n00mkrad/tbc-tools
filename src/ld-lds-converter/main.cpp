/************************************************************************

    main.cpp

    ld-lds-converter - 10-bit .lds to FLAC/s16 converter for ld-decode
    Copyright (C) 2019-2026 Simon Inns

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

#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QtGlobal>
#include <cstdio>
#ifdef Q_OS_WIN
#include <io.h>
#else
#include <unistd.h>
#endif

#include "tbc/logging.h"
#include "converterdialog.h"
#include "dataconverter.h"

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

DataConverter::OutputFormat parseOutputFormat(const QString &formatString, bool *ok = nullptr)
{
    const QString normalizedFormat = formatString.trimmed().toLower();
    if (normalizedFormat == QStringLiteral("flac")) {
        if (ok) *ok = true;
        return DataConverter::OutputFormat::Flac;
    }
    if (normalizedFormat == QStringLiteral("s16") ||
        normalizedFormat == QStringLiteral("raw") ||
        normalizedFormat == QStringLiteral("raw16")) {
        if (ok) *ok = true;
        return DataConverter::OutputFormat::S16Raw;
    }
    if (normalizedFormat == QStringLiteral("riff") ||
        normalizedFormat == QStringLiteral("wav")) {
        if (ok) *ok = true;
        return DataConverter::OutputFormat::RiffWave;
    }

    if (ok) *ok = false;
    return DataConverter::OutputFormat::Flac;
}

bool wantsGui(int argc, char *argv[])
{
    const auto stdinIsInteractive = []() -> bool {
#ifdef Q_OS_WIN
        return _isatty(_fileno(stdin)) != 0;
#else
        return isatty(fileno(stdin)) != 0;
#endif
    };
    if (argc <= 1) {
        // Keep legacy CLI piping behavior when stdin is redirected.
        return stdinIsInteractive();
    }

    bool hasExplicitCliOption = false;
    int positionalArgumentCount = 0;

    for (int argumentIndex = 1; argumentIndex < argc; argumentIndex++) {
        const QString argument = QString::fromLocal8Bit(argv[argumentIndex]);
        if (argument == QLatin1String("--gui") || argument == QLatin1String("-g")) {
            return true;
        }

        if (argument == QLatin1String("--help") || argument == QLatin1String("-h") ||
            argument == QLatin1String("--version") || argument == QLatin1String("-v")) {
            return false;
        }

        if (argument == QLatin1String("--pack") || argument == QLatin1String("-p") ||
            argument == QLatin1String("--unpack") || argument == QLatin1String("-u") ||
            argument == QLatin1String("--riff") || argument == QLatin1String("-r") ||
            argument == QLatin1String("--input") || argument == QLatin1String("-i") ||
            argument == QLatin1String("--output") || argument == QLatin1String("-o") ||
            argument == QLatin1String("--format") ||
            argument == QLatin1String("--s16") ||
            argument == QLatin1String("--flac") ||
            argument == QLatin1String("--sample-rate") ||
            argument == QLatin1String("--compression-level")) {
            hasExplicitCliOption = true;
        }

        if (!argument.startsWith(QLatin1Char('-'))) {
            positionalArgumentCount++;
        }
    }

    // If only one positional argument is passed (common for desktop drag/drop),
    // treat this as a GUI launch and pre-fill that file.
    if (!hasExplicitCliOption && positionalArgumentCount == 1) {
        return true;
    }

    return false;
}
} // namespace

int main(int argc, char *argv[])
{
    // Set 'binary mode' for stdin and stdout on Windows
    setBinaryMode();

    // Install the local debug message handler
    setDebug(true);
    qInstallMessageHandler(debugOutputHandler);

    if (wantsGui(argc, argv)) {
        QApplication a(argc, argv);

        QCoreApplication::setApplicationName("ld-lds-converter");
        QCoreApplication::setApplicationVersion(QString("ld-decode-tools - Branch: %1 / Commit: %2").arg(APP_BRANCH, APP_COMMIT));
        QCoreApplication::setOrganizationDomain("domesday86.com");

        QCommandLineParser parser;
        parser.setApplicationDescription(
            "ld-lds-converter - Convert 10-bit .lds files to FLAC (default) or s16 raw output\n"
            "\n"
            "(c)2018-2026 Simon Inns\n"
            "GPLv3 Open-Source - github: https://github.com/happycube/ld-decode");
        parser.addHelpOption();
        parser.addVersionOption();

        addStandardDebugOptions(parser);

        QCommandLineOption guiOption(QStringList() << "g" << "gui",
                                     QCoreApplication::translate("main", "Launch the GUI (default when no explicit CLI mode is provided)."));
        parser.addOption(guiOption);

        QCommandLineOption sourceVideoFileOption(QStringList() << "i" << "input",
                    QCoreApplication::translate("main", "Specify input laserdisc sample file"), QCoreApplication::translate("main", "file"));
        parser.addOption(sourceVideoFileOption);

        QCommandLineOption targetVideoFileOption(QStringList() << "o" << "output",
                    QCoreApplication::translate("main", "Specify output file"), QCoreApplication::translate("main", "file"));
        parser.addOption(targetVideoFileOption);

        QCommandLineOption outputFormatOption(QStringList() << "format",
                    QCoreApplication::translate("main", "Output format for unpack mode: flac (default), s16 or riff"),
                    QCoreApplication::translate("main", "format"), "flac");
        parser.addOption(outputFormatOption);

        parser.process(a);
        processStandardDebugOptions(parser);

        bool outputFormatIsValid = false;
        const DataConverter::OutputFormat outputFormat = parseOutputFormat(parser.value(outputFormatOption), &outputFormatIsValid);
        if (!outputFormatIsValid) {
            qCritical("Invalid --format value. Use one of: flac, s16, riff");
            return -1;
        }

        QString inputFileName = normalizePathForCurrentPlatform(parser.value(sourceVideoFileOption));
        if (inputFileName.isEmpty()) {
            const QStringList positionalArguments = parser.positionalArguments();
            if (!positionalArguments.isEmpty()) {
                inputFileName = normalizePathForCurrentPlatform(positionalArguments.first());
            }
        }

        const QString outputFileName = normalizePathForCurrentPlatform(parser.value(targetVideoFileOption));

        ConverterDialog dialog;
        dialog.setDefaultFormat(outputFormat);
        if (!inputFileName.isEmpty()) {
            dialog.setDefaultInput(inputFileName);
        }
        if (!outputFileName.isEmpty()) {
            dialog.setDefaultOutput(outputFileName);
        }
        dialog.exec();
        return 0;
    }

    QCoreApplication a(argc, argv);

    // Set application name and version
    QCoreApplication::setApplicationName("ld-lds-converter");
    QCoreApplication::setApplicationVersion(QString("ld-decode-tools - Branch: %1 / Commit: %2").arg(APP_BRANCH, APP_COMMIT));
    QCoreApplication::setOrganizationDomain("domesday86.com");

    // Set up the command line parser
    QCommandLineParser parser;
    parser.setApplicationDescription(
                "ld-lds-converter - Convert 10-bit .lds files to FLAC (default) or s16 raw output\n"
                "\n"
                "(c)2018-2026 Simon Inns\n"
                "GPLv3 Open-Source - github: https://github.com/happycube/ld-decode");
    parser.addHelpOption();
    parser.addVersionOption();

    // Add the standard debug options --debug and --quiet
    addStandardDebugOptions(parser);

    // Option to launch the GUI
    QCommandLineOption guiOption(QStringList() << "g" << "gui",
                                 QCoreApplication::translate("main", "Launch the GUI."));
    parser.addOption(guiOption);

    // Option to specify input video file (-i)
    QCommandLineOption sourceVideoFileOption(QStringList() << "i" << "input",
                QCoreApplication::translate("main", "Specify input laserdisc sample file (default is stdin)"),
                QCoreApplication::translate("main", "file"));
    parser.addOption(sourceVideoFileOption);

    // Option to specify output video file (-o)
    QCommandLineOption targetVideoFileOption(QStringList() << "o" << "output",
                QCoreApplication::translate("main", "Specify output file (default is inferred from input, or stdout for stdin input)"),
                QCoreApplication::translate("main", "file"));
    parser.addOption(targetVideoFileOption);

    // Option to unpack 10-bit data (-u)
    QCommandLineOption showUnpackOption(QStringList() << "u" << "unpack",
                                       QCoreApplication::translate("main", "Unpack 10-bit data into selected output format"));
    parser.addOption(showUnpackOption);

    // Option to pack 16-bit data (-p)
    QCommandLineOption showPackOption(QStringList() << "p" << "pack",
                                       QCoreApplication::translate("main", "Pack 16-bit data into 10-bit"));
    parser.addOption(showPackOption);

    // Option to unpack 10-bit data with RIFF WAV headers (-r)
    QCommandLineOption showRIFFOption(QStringList() << "r" << "riff",
                                        QCoreApplication::translate("main", "Unpack 10-bit data into 16-bit with RIFF WAV headers (legacy mode)"));
    parser.addOption(showRIFFOption);

    // Option to choose output format
    QCommandLineOption outputFormatOption(QStringList() << "format",
                QCoreApplication::translate("main", "Output format for unpack mode: flac (default), s16 or riff"),
                QCoreApplication::translate("main", "format"), "flac");
    parser.addOption(outputFormatOption);

    // Convenience options for unpack output formats
    QCommandLineOption showS16Option(QStringList() << "s16",
                                     QCoreApplication::translate("main", "Shortcut for --format s16"));
    parser.addOption(showS16Option);

    QCommandLineOption showFlacOption(QStringList() << "flac",
                                      QCoreApplication::translate("main", "Shortcut for --format flac"));
    parser.addOption(showFlacOption);

    QCommandLineOption flacSampleRateOption(QStringList() << "sample-rate",
                                            QCoreApplication::translate("main", "FLAC sample rate in Hz (default: 40000)"),
                                            QCoreApplication::translate("main", "hz"), "40000");
    parser.addOption(flacSampleRateOption);

    QCommandLineOption flacCompressionLevelOption(QStringList() << "compression-level",
                                                  QCoreApplication::translate("main", "FLAC compression level 0-8 (default: 5)"),
                                                  QCoreApplication::translate("main", "level"), "5");
    parser.addOption(flacCompressionLevelOption);

    // Process the command line arguments given by the user
    parser.process(a);

    // Standard logging options
    processStandardDebugOptions(parser);

    // Get the configured settings from the parser
    const bool isUnpacking = parser.isSet(showUnpackOption);
    const bool isPacking = parser.isSet(showPackOption);
    const bool isRIFF = parser.isSet(showRIFFOption);
    const bool isS16 = parser.isSet(showS16Option);
    const bool isFlac = parser.isSet(showFlacOption);

    // Check that both pack and unpack are not set
    if (isUnpacking && isPacking) {
        qCritical("Specify only --unpack (-u) or --pack (-p) - not both!");
        return -1;
    }

    if (isPacking && (isRIFF || isS16 || isFlac || parser.isSet(outputFormatOption))) {
        qCritical("Output-format options (--riff, --s16, --flac, --format) are only valid with unpack mode.");
        return -1;
    }

    if (isRIFF && (isS16 || isFlac || parser.isSet(outputFormatOption))) {
        qCritical("Specify only one unpack output format option: --riff OR --s16/--flac/--format.");
        return -1;
    }

    if (isS16 && isFlac) {
        qCritical("Specify only one of --s16 or --flac.");
        return -1;
    }

    bool modePacking = false;
    if (isPacking) {
        modePacking = true;
    }

    DataConverter::OutputFormat outputFormat = DataConverter::OutputFormat::Flac;
    if (isRIFF) {
        outputFormat = DataConverter::OutputFormat::RiffWave;
    } else if (isS16) {
        outputFormat = DataConverter::OutputFormat::S16Raw;
    } else if (isFlac) {
        outputFormat = DataConverter::OutputFormat::Flac;
    } else if (!modePacking) {
        bool outputFormatIsValid = false;
        outputFormat = parseOutputFormat(parser.value(outputFormatOption), &outputFormatIsValid);
        if (!outputFormatIsValid) {
            qCritical("Invalid --format value. Use one of: flac, s16, riff");
            return -1;
        }
    }

    bool sampleRateOk = false;
    const int flacSampleRate = parser.value(flacSampleRateOption).toInt(&sampleRateOk);
    if (!sampleRateOk || flacSampleRate <= 0) {
        qCritical("Invalid --sample-rate value. It must be a positive integer.");
        return -1;
    }

    bool compressionLevelOk = false;
    const int flacCompressionLevel = parser.value(flacCompressionLevelOption).toInt(&compressionLevelOk);
    if (!compressionLevelOk || flacCompressionLevel < 0 || flacCompressionLevel > 8) {
        qCritical("Invalid --compression-level value. It must be in the range 0-8.");
        return -1;
    }

    QString inputFileName = normalizePathForCurrentPlatform(parser.value(sourceVideoFileOption));
    QString outputFileName = normalizePathForCurrentPlatform(parser.value(targetVideoFileOption));
    if (inputFileName.isEmpty()) {
        const QStringList positionalArguments = parser.positionalArguments();
        if (!positionalArguments.isEmpty()) {
            inputFileName = normalizePathForCurrentPlatform(positionalArguments.first());
        }
    }

    if (outputFileName.isEmpty() && !inputFileName.isEmpty()) {
        outputFileName = normalizePathForCurrentPlatform(DataConverter::defaultOutputPath(inputFileName, modePacking, outputFormat));
    }

    // Initialise the data conversion object
    DataConverter dataConverter(inputFileName, outputFileName, modePacking, outputFormat, flacSampleRate, flacCompressionLevel);

    // Process the data conversion
    if (!dataConverter.process()) {
        return 1;
    }

    // Quit with success
    return 0;
}