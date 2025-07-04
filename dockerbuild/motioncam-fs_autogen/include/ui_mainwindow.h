/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.2.4
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QVBoxLayout *verticalLayout;
    QFrame *dropArea;
    QVBoxLayout *verticalLayout_2;
    QScrollArea *dragAndDropScrollArea;
    QWidget *scrollAreaWidgetContents;
    QVBoxLayout *verticalLayout_3;
    QSpacerItem *verticalSpacer;
    QLabel *dragAndDropLabel;
    QFrame *globalSettings;
    QGridLayout *gridLayout;
    QCheckBox *scaleRawCheckBox;
    QHBoxLayout *cacheLayout;
    QLabel *descCacheFolder;
    QLabel *cacheFolderLabel;
    QPushButton *changeCacheBtn;
    QCheckBox *vignetteCorrectionCheckBox;
    QHBoxLayout *draftLayout;
    QCheckBox *draftModeCheckBox;
    QComboBox *draftQuality;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName(QString::fromUtf8("MainWindow"));
        MainWindow->resize(800, 600);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName(QString::fromUtf8("centralwidget"));
        verticalLayout = new QVBoxLayout(centralwidget);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        dropArea = new QFrame(centralwidget);
        dropArea->setObjectName(QString::fromUtf8("dropArea"));
        dropArea->setFrameShape(QFrame::Shape::StyledPanel);
        dropArea->setFrameShadow(QFrame::Shadow::Raised);
        verticalLayout_2 = new QVBoxLayout(dropArea);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        dragAndDropScrollArea = new QScrollArea(dropArea);
        dragAndDropScrollArea->setObjectName(QString::fromUtf8("dragAndDropScrollArea"));
        dragAndDropScrollArea->setWidgetResizable(true);
        dragAndDropScrollArea->setAlignment(Qt::AlignmentFlag::AlignLeading|Qt::AlignmentFlag::AlignLeft|Qt::AlignmentFlag::AlignTop);
        scrollAreaWidgetContents = new QWidget();
        scrollAreaWidgetContents->setObjectName(QString::fromUtf8("scrollAreaWidgetContents"));
        scrollAreaWidgetContents->setGeometry(QRect(0, 0, 772, 470));
        verticalLayout_3 = new QVBoxLayout(scrollAreaWidgetContents);
        verticalLayout_3->setSpacing(5);
        verticalLayout_3->setObjectName(QString::fromUtf8("verticalLayout_3"));
        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Expanding, QSizePolicy::Minimum);

        verticalLayout_3->addItem(verticalSpacer);

        dragAndDropScrollArea->setWidget(scrollAreaWidgetContents);

        verticalLayout_2->addWidget(dragAndDropScrollArea);

        dragAndDropLabel = new QLabel(dropArea);
        dragAndDropLabel->setObjectName(QString::fromUtf8("dragAndDropLabel"));

        verticalLayout_2->addWidget(dragAndDropLabel);


        verticalLayout->addWidget(dropArea);

        globalSettings = new QFrame(centralwidget);
        globalSettings->setObjectName(QString::fromUtf8("globalSettings"));
        globalSettings->setFrameShape(QFrame::Shape::StyledPanel);
        globalSettings->setFrameShadow(QFrame::Shadow::Raised);
        gridLayout = new QGridLayout(globalSettings);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        scaleRawCheckBox = new QCheckBox(globalSettings);
        scaleRawCheckBox->setObjectName(QString::fromUtf8("scaleRawCheckBox"));
        scaleRawCheckBox->setEnabled(true);

        gridLayout->addWidget(scaleRawCheckBox, 1, 1, 1, 1);

        cacheLayout = new QHBoxLayout();
        cacheLayout->setObjectName(QString::fromUtf8("cacheLayout"));
        descCacheFolder = new QLabel(globalSettings);
        descCacheFolder->setObjectName(QString::fromUtf8("descCacheFolder"));
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(descCacheFolder->sizePolicy().hasHeightForWidth());
        descCacheFolder->setSizePolicy(sizePolicy);

        cacheLayout->addWidget(descCacheFolder);

        cacheFolderLabel = new QLabel(globalSettings);
        cacheFolderLabel->setObjectName(QString::fromUtf8("cacheFolderLabel"));
        QSizePolicy sizePolicy1(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy1.setHorizontalStretch(1);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(cacheFolderLabel->sizePolicy().hasHeightForWidth());
        cacheFolderLabel->setSizePolicy(sizePolicy1);

        cacheLayout->addWidget(cacheFolderLabel);

        changeCacheBtn = new QPushButton(globalSettings);
        changeCacheBtn->setObjectName(QString::fromUtf8("changeCacheBtn"));
        changeCacheBtn->setMinimumSize(QSize(100, 30));

        cacheLayout->addWidget(changeCacheBtn);


        gridLayout->addLayout(cacheLayout, 1, 0, 1, 1);

        vignetteCorrectionCheckBox = new QCheckBox(globalSettings);
        vignetteCorrectionCheckBox->setObjectName(QString::fromUtf8("vignetteCorrectionCheckBox"));
        vignetteCorrectionCheckBox->setEnabled(true);

        gridLayout->addWidget(vignetteCorrectionCheckBox, 0, 1, 1, 1);

        draftLayout = new QHBoxLayout();
        draftLayout->setObjectName(QString::fromUtf8("draftLayout"));
        draftModeCheckBox = new QCheckBox(globalSettings);
        draftModeCheckBox->setObjectName(QString::fromUtf8("draftModeCheckBox"));
        QSizePolicy sizePolicy2(QSizePolicy::Minimum, QSizePolicy::Fixed);
        sizePolicy2.setHorizontalStretch(1);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(draftModeCheckBox->sizePolicy().hasHeightForWidth());
        draftModeCheckBox->setSizePolicy(sizePolicy2);

        draftLayout->addWidget(draftModeCheckBox);

        draftQuality = new QComboBox(globalSettings);
        draftQuality->addItem(QString());
        draftQuality->addItem(QString());
        draftQuality->addItem(QString());
        draftQuality->setObjectName(QString::fromUtf8("draftQuality"));
        draftQuality->setMinimumSize(QSize(100, 30));

        draftLayout->addWidget(draftQuality);


        gridLayout->addLayout(draftLayout, 0, 0, 1, 1);


        verticalLayout->addWidget(globalSettings);

        MainWindow->setCentralWidget(centralwidget);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "MotionCam FS", nullptr));
        dragAndDropLabel->setText(QCoreApplication::translate("MainWindow", "Drag and drop files to start", nullptr));
        scaleRawCheckBox->setText(QCoreApplication::translate("MainWindow", "Scale RAW data", nullptr));
        descCacheFolder->setText(QCoreApplication::translate("MainWindow", "Cache Folder:", nullptr));
        cacheFolderLabel->setText(QCoreApplication::translate("MainWindow", "-", nullptr));
        changeCacheBtn->setText(QCoreApplication::translate("MainWindow", "Change", nullptr));
        vignetteCorrectionCheckBox->setText(QCoreApplication::translate("MainWindow", "Apply vignette correction", nullptr));
        draftModeCheckBox->setText(QCoreApplication::translate("MainWindow", "Draft Mode", nullptr));
        draftQuality->setItemText(0, QCoreApplication::translate("MainWindow", "2x", nullptr));
        draftQuality->setItemText(1, QCoreApplication::translate("MainWindow", "4x", nullptr));
        draftQuality->setItemText(2, QCoreApplication::translate("MainWindow", "8x", nullptr));

    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
