#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QPushButton>
#include <QFileInfo>
#include <QProcess>
#include <QMessageBox>
#include <QFileDialog>
#include <QSettings>
#include <QDir>
#include <algorithm>

#ifdef _WIN32
#include "win/FuseFileSystemImpl_Win.h"
#elif __APPLE__
#include "macos/FuseFileSystemImpl_MacOS.h"
#else
#include "linux/FuseFileSystemImpl_Linux.h"
#endif

namespace {
    constexpr auto PACKAGE_NAME = "com.motioncam";
    constexpr auto APP_NAME = "MotionCam FS";

    motioncam::FileRenderOptions getRenderOptions(Ui::MainWindow& ui) {
        motioncam::FileRenderOptions options = motioncam::RENDER_OPT_NONE;

        if(ui.draftModeCheckBox->checkState() == Qt::CheckState::Checked)
            options |= motioncam::RENDER_OPT_DRAFT;

        if(ui.vignetteCorrectionCheckBox->checkState() == Qt::CheckState::Checked)
            options |= motioncam::RENDER_OPT_APPLY_VIGNETTE_CORRECTION;

        if(ui.scaleRawCheckBox->checkState() == Qt::CheckState::Checked)
            options |= motioncam::RENDER_OPT_NORMALIZE_SHADING_MAP;

        return options;
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , mDraftQuality(1)
{
    ui->setupUi(this);

#ifdef _WIN32
    mFuseFilesystem = std::make_unique<motioncam::FuseFileSystemImpl_Win>();
#elif __APPLE__
    mFuseFilesystem = std::make_unique<motioncam::FuseFileSystemImpl_MacOs>();
#else
    mFuseFilesystem = std::make_unique<motioncam::FuseFileSystemImpl_Linux>();
#endif

    ui->dragAndDropScrollArea->setAcceptDrops(true);
    ui->dragAndDropScrollArea->installEventFilter(this);

    restoreSettings();

    connect(ui->draftModeCheckBox, &QCheckBox::checkStateChanged, this, &MainWindow::onRenderSettingsChanged);
    connect(ui->vignetteCorrectionCheckBox, &QCheckBox::checkStateChanged, this, &MainWindow::onRenderSettingsChanged);
    connect(ui->scaleRawCheckBox, &QCheckBox::checkStateChanged, this, &MainWindow::onRenderSettingsChanged);
    connect(ui->draftQuality, &QComboBox::currentIndexChanged, this, &MainWindow::onDraftModeQualityChanged);
    connect(ui->changeCacheBtn, &QPushButton::clicked, this, &MainWindow::onSetCacheFolder);
}

MainWindow::~MainWindow() {
    saveSettings();
    delete ui;
}

void MainWindow::saveSettings() {
    QSettings settings(PACKAGE_NAME, APP_NAME);

    settings.setValue("draftMode", ui->draftModeCheckBox->checkState() == Qt::CheckState::Checked);
    settings.setValue("applyVignetteCorrection", ui->vignetteCorrectionCheckBox->checkState() == Qt::CheckState::Checked);
    settings.setValue("scaleRaw", ui->scaleRawCheckBox->checkState() == Qt::CheckState::Checked);
    settings.setValue("cachePath", mCacheRootFolder);
    settings.setValue("draftQuality", mDraftQuality);

    settings.beginWriteArray("mountedFiles");
    for (int i = 0; i < mMountedFiles.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("srcFile", mMountedFiles[i].srcFile);
    }
    settings.endArray();
}

void MainWindow::restoreSettings() {
    QSettings settings(PACKAGE_NAME, APP_NAME);

    ui->draftModeCheckBox->setCheckState(
        settings.value("draftMode").toBool() ? Qt::CheckState::Checked : Qt::CheckState::Unchecked);
    ui->vignetteCorrectionCheckBox->setCheckState(
        settings.value("applyVignetteCorrection").toBool() ? Qt::CheckState::Checked : Qt::CheckState::Unchecked);
    ui->scaleRawCheckBox->setCheckState(
        settings.value("scaleRaw").toBool() ? Qt::CheckState::Checked : Qt::CheckState::Unchecked);

    mCacheRootFolder = settings.value("cachePath").toString();
    mDraftQuality = std::max(1, settings.value("draftQuality").toInt());

    if(mDraftQuality == 2)
        ui->draftQuality->setCurrentIndex(0);
    else if(mDraftQuality == 4)
        ui->draftQuality->setCurrentIndex(1);
    else if(mDraftQuality == 8)
        ui->draftQuality->setCurrentIndex(2);

    auto size = settings.beginReadArray("mountedFiles");
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        auto srcFile = settings.value("srcFile").toString();
        if(QFile::exists(srcFile))
            mountFile(srcFile);
    }
    settings.endArray();

    updateUi();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (watched == ui->dragAndDropScrollArea) {
        if (event->type() == QEvent::DragEnter) {
            auto* dragEvent = static_cast<QDragEnterEvent*>(event);
            if (dragEvent->mimeData()->hasUrls()) {
                const auto urls = dragEvent->mimeData()->urls();
                for (const auto& url : urls) {
                    if (url.toLocalFile().endsWith(".mcraw", Qt::CaseInsensitive)) {
                        dragEvent->acceptProposedAction();
                        return true;
                    }
                }
            }
            return true;
        } else if (event->type() == QEvent::Drop) {
            auto* dropEvent = static_cast<QDropEvent*>(event);
            if (dropEvent->mimeData()->hasUrls()) {
                const auto urls = dropEvent->mimeData()->urls();
                for (const auto& url : urls) {
                    if (url.toLocalFile().endsWith(".mcraw", Qt::CaseInsensitive)) {
                        mountFile(url.toLocalFile());
                    }
                }
                dropEvent->acceptProposedAction();
            }
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::mountFile(const QString& filePath) {
    QFileInfo fileInfo(filePath);
    auto fileName = fileInfo.fileName();
    auto dstPath = (mCacheRootFolder.isEmpty() ? fileInfo.path() : mCacheRootFolder) + "/" + fileInfo.baseName();

    // create the mount point if it doesn't exist
    QDir().mkpath(dstPath);

    motioncam::MountId mountId;

    try {
        mountId = mFuseFilesystem->mount(
            getRenderOptions(*ui), mDraftQuality, filePath.toStdString(), dstPath.toStdString());
    } catch (std::runtime_error& e) {
        QMessageBox::critical(this, "Error", QString("There was an error mounting the file: %1").arg(e.what()));
        return;
    }

    auto* scrollContent = ui->dragAndDropScrollArea->widget();
    auto* scrollLayout = qobject_cast<QVBoxLayout*>(scrollContent->layout());

    auto* fileWidget = new QWidget(scrollContent);
    fileWidget->setFixedHeight(50);
    fileWidget->setProperty("filePath", filePath);
    fileWidget->setProperty("mountId", mountId);

    auto* fileLayout = new QHBoxLayout(fileWidget);
    fileLayout->setContentsMargins(5,5,5,5);

    auto* fileLabel = new QLabel(fileName, fileWidget);
    fileLabel->setToolTip(filePath);
    fileLayout->addWidget(fileLabel);
    fileLayout->addStretch();

    auto* playButton = new QPushButton("Play", fileWidget);
    playButton->setMaximumWidth(100);
    playButton->setMaximumHeight(30);
    playButton->setIcon(QIcon(":/assets/play_btn.png"));
    fileLayout->addWidget(playButton);

    auto* removeButton = new QPushButton("Remove", fileWidget);
    removeButton->setMaximumWidth(100);
    removeButton->setMaximumHeight(30);
    removeButton->setIcon(QIcon(":/assets/remove_btn.png"));
    fileLayout->addWidget(removeButton);

    scrollLayout->insertWidget(0, fileWidget);
    ui->dragAndDropLabel->hide();

    connect(playButton, &QPushButton::clicked, this, [this, filePath] {
        playFile(filePath);
    });
    connect(removeButton, &QPushButton::clicked, this, [this, fileWidget] {
        removeFile(fileWidget);
    });

    mMountedFiles.append(motioncam::MountedFile(mountId, filePath));
}

void MainWindow::playFile(const QString& path) {
    QStringList arguments;
    arguments << path;

    bool success = false;

#ifdef _WIN32
    success = QProcess::startDetached("MotionCam_Player.exe", arguments);
#elif __APPLE__
    success = QProcess::startDetached("/usr/bin/open", arguments);
#else
    success = QProcess::startDetached("./MCRAW_Player", arguments);
#endif

    if (!success)
        QMessageBox::warning(this, "Error", QString("Failed to launch player with file: %1").arg(path));
}

void MainWindow::removeFile(QWidget* fileWidget) {
    if (!fileWidget)
        return;

    auto* scrollContent = ui->dragAndDropScrollArea->widget();
    if (!scrollContent)
        return;

    auto* scrollLayout = qobject_cast<QVBoxLayout*>(scrollContent->layout());
    if (!scrollLayout)
        return;

    scrollLayout->removeWidget(fileWidget);
    fileWidget->deleteLater();

    if (scrollLayout->count() == 0)
        ui->dragAndDropLabel->show();

    bool ok = false;
    auto mountId = fileWidget->property("mountId").toInt(&ok);
    if (ok && mFuseFilesystem) {
        try {
            mFuseFilesystem->unmount(mountId);
        } catch (const std::exception& ex) {
            QMessageBox::warning(this, "Unmount error", QString("Failed to unmount: %1").arg(ex.what()));
        }

        auto it = std::find_if(mMountedFiles.begin(), mMountedFiles.end(),
                               [mountId](const motioncam::MountedFile& f) { return f.mountId == mountId; });
        if (it != mMountedFiles.end())
            mMountedFiles.erase(it);
    }
}

void MainWindow::updateUi() {
    ui->draftQuality->setEnabled(ui->draftModeCheckBox->checkState() == Qt::CheckState::Checked);
    ui->scaleRawCheckBox->setEnabled(ui->vignetteCorrectionCheckBox->checkState() == Qt::CheckState::Checked);
    ui->cacheFolderLabel->setText(mCacheRootFolder);
}

void MainWindow::onRenderSettingsChanged(const Qt::CheckState&) {
    auto renderOptions = getRenderOptions(*ui);
    updateUi();

    for (const auto& mountedFile : mMountedFiles)
        mFuseFilesystem->updateOptions(mountedFile.mountId, renderOptions, mDraftQuality);
}

void MainWindow::onDraftModeQualityChanged(int index) {
    if(index == 0)
        mDraftQuality = 2;
    else if(index == 1)
        mDraftQuality = 4;
    else if(index == 2)
        mDraftQuality = 8;

    onRenderSettingsChanged(Qt::CheckState::Checked);
}

void MainWindow::onSetCacheFolder(bool checked) {
    Q_UNUSED(checked);
    auto folderPath = QFileDialog::getExistingDirectory(
        this,
        tr("Select Cache Root Folder"),
        QString(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    mCacheRootFolder = folderPath;
    ui->cacheFolderLabel->setText(mCacheRootFolder);
}
