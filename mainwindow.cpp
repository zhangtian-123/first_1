#include "mainwindow.h"

#include <QTabWidget>
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>
#include <QHeaderView>
#include <QLabel>
#include <QGroupBox>
#include <QComboBox>
#include <QSpinBox>
#include <QMessageBox>
#include <QFileDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QColorDialog>
#include <QColor>
#include <QFileInfo>
#include <QDebug>
#include <QKeySequenceEdit>
#include <QShortcut>
#include <QEvent>
#include <QHash>
#include <QItemSelectionModel>
#include <QStatusBar>
#include <QMouseEvent>
#include <QApplication>
#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QTabBar>
#include <QScrollBar>
#include <QSlider>
#include <QSignalBlocker>
#include <QSet>

#include "src/config/appsettings.h"
#include "src/services/serialservice.h"
#include "src/core/excelimporter.h"
#include "src/core/randomcolorresolver.h"
#include "src/core/workflowengine.h"
#include "src/core/models.h"
#include "src/core/protocol.h"
#include "src/ui/queuetablemodel.h"
#include "src/ui/colortablemodel.h"
#include "src/ui/conflicttablemodel.h"
#include "src/ui/conflictcolordelegate.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_serial   = new SerialService(this);
    m_engine   = new WorkflowEngine(this);
    m_importer = new ExcelImporter(this);
    m_engine->setSerialService(m_serial);

    loadSettings();
    buildUi();
    wireSignals();
    qApp->installEventFilter(this);

    // Models / delegates
    m_queueModel = new QueueTableModel(this);
    m_tblQueue->setModel(m_queueModel);

    m_colorModel = new ColorTableModel(this);
    m_tblColors->setModel(m_colorModel);

    m_conflictModel = new ConflictTableModel(this);
    m_tblConflicts->setModel(m_conflictModel);
    m_conflictDelegate = new ConflictColorDelegate(m_colorModel, this);
    for (int c = 0; c < 3; ++c)
        m_tblConflicts->setItemDelegateForColumn(c, m_conflictDelegate);

    if (m_settings)
    {
        m_excelPath = m_settings->lastExcelPath;
        m_editExcelPath->setText(m_excelPath);
        // serial
        m_cmbBaud->setCurrentText(QString::number(m_settings->serial.baud));
        m_cmbDataBits->setCurrentText(QString::number(m_settings->serial.dataBits));
        m_cmbParity->setCurrentText(m_settings->serial.parity);
        m_cmbStopBits->setCurrentText(QString::number(m_settings->serial.stopBits));
        // device
        m_spOnMs->setValue(m_settings->device.onMs);
        m_spGapMs->setValue(m_settings->device.gapMs);
        m_spLedCount->setValue(m_settings->device.ledCount);
        m_spBrightness->setValue(m_settings->device.brightness);
        m_spBuzzerFreq->setValue(m_settings->device.buzzerFreq);
        m_spBuzzerDur->setValue(m_settings->device.buzzerDurMs);
        // voice1
        const int v1Idx = m_cmbVoice1Announcer->findData(m_settings->voice1.announcer);
        if (v1Idx >= 0) m_cmbVoice1Announcer->setCurrentIndex(v1Idx);
        m_spVoice1Style->setValue(m_settings->voice1.voiceStyle);
        m_spVoice1Speed->setValue(m_settings->voice1.voiceSpeed);
        m_spVoice1Pitch->setValue(m_settings->voice1.voicePitch);
        m_spVoice1Volume->setValue(m_settings->voice1.voiceVolume);
        // voice2
        const int v2Idx = m_cmbVoice2Announcer->findData(m_settings->voice2.announcer);
        if (v2Idx >= 0) m_cmbVoice2Announcer->setCurrentIndex(v2Idx);
        m_spVoice2Style->setValue(m_settings->voice2.voiceStyle);
        m_spVoice2Speed->setValue(m_settings->voice2.voiceSpeed);
        m_spVoice2Pitch->setValue(m_settings->voice2.voicePitch);
        m_spVoice2Volume->setValue(m_settings->voice2.voiceVolume);
        // hotkeys
        m_keyNext->setKeySequence(m_settings->hotkeys.keyNext);
        m_keyRerun->setKeySequence(m_settings->hotkeys.keyRerun);
        for (int i = 0; i < m_keyQuickColor.size() && i < m_settings->hotkeys.keyQuickColor.size(); ++i)
            m_keyQuickColor[i]->setKeySequence(m_settings->hotkeys.keyQuickColor[i]);
        m_keyAllOff->setKeySequence(m_settings->hotkeys.keyAllOff);
        rebuildShortcuts();
        updateHotkeyDuplicateHints();

        m_colorModel->setColors(m_settings->colors);
        m_conflictModel->setMaxColorIndex(m_colorModel->rowCount());
        m_conflictModel->setTriples(m_settings->conflicts);
    }

    m_serial->refreshPorts();
    m_uiState = UiRunState::NoConfig;
    applyUiState();
    m_hotkeyAutoSaveEnabled = true;
}

MainWindow::~MainWindow()
{
    qApp->removeEventFilter(this);
    saveSettings();
    delete m_settings;
}

void MainWindow::buildUi()
{
    m_tabs = new QTabWidget(this);
    setCentralWidget(m_tabs);

    m_tabs->addTab(buildStatusPage(), tr("Status"));
    m_tabs->addTab(buildSettingsPage(), tr("Settings"));

    setWindowTitle(tr("VisualWorkflowHost"));
    resize(1180, 760);
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress)
    {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton)
        {
            QWidget* hit = QApplication::widgetAt(mouseEvent->globalPos());
            if (!hit)
                return QMainWindow::eventFilter(obj, event);

            if (hit->window() != this)
                return QMainWindow::eventFilter(obj, event);

            auto isInteractive = [](QWidget* widget) {
                QWidget* cur = widget;
                while (cur)
                {
                    if (qobject_cast<QLineEdit*>(cur)
                        || qobject_cast<QComboBox*>(cur)
                        || qobject_cast<QKeySequenceEdit*>(cur)
                        || qobject_cast<QAbstractButton*>(cur)
                        || qobject_cast<QAbstractItemView*>(cur)
                        || qobject_cast<QAbstractSpinBox*>(cur)
                        || qobject_cast<QTabBar*>(cur)
                        || qobject_cast<QHeaderView*>(cur)
                        || qobject_cast<QScrollBar*>(cur)
                        || qobject_cast<QSlider*>(cur))
                    {
                        return true;
                    }
                    cur = cur->parentWidget();
                }
                return false;
            };

            if (!isInteractive(hit))
            {
                if (m_tblQueue && m_tblQueue->selectionModel())
                    m_tblQueue->clearSelection();
                if (m_tblColors && m_tblColors->selectionModel())
                    m_tblColors->clearSelection();
                if (m_tblConflicts && m_tblConflicts->selectionModel())
                    m_tblConflicts->clearSelection();

                QWidget* focusWidget = QApplication::focusWidget();
                if (focusWidget && focusWidget->window() == this)
                    focusWidget->clearFocus();
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

QWidget* MainWindow::buildStatusPage()
{
    QWidget* page = new QWidget(this);
    auto* root = new QVBoxLayout(page);

    auto* top = new QHBoxLayout();
    m_editExcelPath = new QLineEdit(page);
    m_editExcelPath->setReadOnly(true);
    m_btnPickExcel = new QPushButton(tr("选择文件"), page);
    m_btnApplyConfig = new QPushButton(tr("应用配置"), page);
    top->addWidget(new QLabel(tr("配置文件"), page));
    top->addWidget(m_editExcelPath, 1);
    top->addWidget(m_btnPickExcel);
    top->addWidget(m_btnApplyConfig);

    auto* mid = new QHBoxLayout();
    auto* left = new QVBoxLayout();
    m_btnStart = new QPushButton(tr("开始"), page);
    m_btnNext  = new QPushButton(tr("下一步"), page);
    m_btnMarkRerun = new QPushButton(tr("标记需重做"), page);
    m_btnReset = new QPushButton(tr("重置"), page);
    left->addWidget(m_btnStart);
    left->addWidget(m_btnNext);
    left->addWidget(m_btnMarkRerun);
    left->addWidget(m_btnReset);
    left->addStretch(1);

    m_tblQueue = new QTableView(page);
    m_tblQueue->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tblQueue->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tblQueue->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tblQueue->horizontalHeader()->setStretchLastSection(false);
    m_tblQueue->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_tblQueue->horizontalHeader()->setSectionsMovable(false);
    m_tblQueue->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    m_tblQueue->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

    mid->addLayout(left, 0);
    mid->addWidget(m_tblQueue, 1);

    auto* bottom = new QHBoxLayout();
    m_lblRunState = new QLabel(tr("状态：未应用配置"), page);
    m_lblHint = new QLabel(QString(), page);
    bottom->addWidget(m_lblRunState);
    bottom->addStretch(1);
    bottom->addWidget(m_lblHint);

    root->addLayout(top);
    root->addLayout(mid, 1);
    root->addLayout(bottom);
    return page;
}

QWidget* MainWindow::buildSettingsPage()
{
    QWidget* page = new QWidget(this);
    auto* root = new QHBoxLayout(page);

    auto* left = new QVBoxLayout();   // 串口 + 设备
    auto* middle = new QVBoxLayout(); // 快捷键 + 测试
    auto* right = new QVBoxLayout();  // 颜色表 + 冲突表
    // Serial box
    auto* gbSerial = new QGroupBox(tr("串口设置"), page);
    auto* gSerial = new QGridLayout(gbSerial);
    m_cmbPort = new QComboBox(gbSerial);
    m_cmbBaud = new QComboBox(gbSerial);
    m_cmbBaud->addItems({"9600","19200","38400","57600","115200","230400","460800","921600"});
    m_cmbBaud->setCurrentText("115200");
    m_cmbDataBits = new QComboBox(gbSerial);
    m_cmbDataBits->addItems({"8","7"});
    m_cmbDataBits->setCurrentText("8");
    m_cmbParity = new QComboBox(gbSerial);
    m_cmbParity->addItems({"None","Even","Odd"});
    m_cmbStopBits = new QComboBox(gbSerial);
    m_cmbStopBits->addItems({"1","2"});
    m_btnRefreshPorts = new QPushButton(tr("刷新串口"), gbSerial);
    m_btnOpenClose = new QPushButton(tr("打开串口"), gbSerial);

    int r=0;
    gSerial->addWidget(new QLabel(tr("端口"), gbSerial), r,0); gSerial->addWidget(m_cmbPort,r,1); gSerial->addWidget(m_btnRefreshPorts,r,2); r++;
    gSerial->addWidget(new QLabel(tr("波特率"), gbSerial), r,0); gSerial->addWidget(m_cmbBaud,r,1); gSerial->addWidget(m_btnOpenClose,r,2); r++;
    gSerial->addWidget(new QLabel(tr("数据位"), gbSerial), r,0); gSerial->addWidget(m_cmbDataBits,r,1); r++;
    gSerial->addWidget(new QLabel(tr("校验"), gbSerial), r,0); gSerial->addWidget(m_cmbParity,r,1); r++;
    gSerial->addWidget(new QLabel(tr("停止位"), gbSerial), r,0); gSerial->addWidget(m_cmbStopBits,r,1); r++;

    // Device box (11 fields)
    auto* gbDev = new QGroupBox(tr("设备属性"), page);
    auto* gDev = new QGridLayout(gbDev);
    m_spOnMs = new QSpinBox(gbDev);      m_spOnMs->setRange(0, 600000); m_spOnMs->setSuffix(" ms");
    m_spGapMs = new QSpinBox(gbDev);     m_spGapMs->setRange(0, 600000); m_spGapMs->setSuffix(" ms");
    m_spLedCount = new QSpinBox(gbDev);  m_spLedCount->setRange(0, 20);
    m_spBrightness = new QSpinBox(gbDev);m_spBrightness->setRange(0, 255);
    m_spBuzzerFreq = new QSpinBox(gbDev);m_spBuzzerFreq->setRange(1000, 4000);
    m_spBuzzerDur = new QSpinBox(gbDev); m_spBuzzerDur->setRange(0, 600000);

    int rd=0;
    gDev->addWidget(new QLabel(tr("点亮时长"), gbDev), rd,0); gDev->addWidget(m_spOnMs, rd,1); rd++;
    gDev->addWidget(new QLabel(tr("点亮间隔"), gbDev), rd,0); gDev->addWidget(m_spGapMs, rd,1); rd++;
    gDev->addWidget(new QLabel(tr("LED个数"), gbDev), rd,0); gDev->addWidget(m_spLedCount, rd,1); rd++;
    gDev->addWidget(new QLabel(tr("亮度"), gbDev), rd,0); gDev->addWidget(m_spBrightness, rd,1); rd++;
    gDev->addWidget(new QLabel(tr("蜂鸣器频率"), gbDev), rd,0); gDev->addWidget(m_spBuzzerFreq, rd,1); rd++;
    gDev->addWidget(new QLabel(tr("蜂鸣器时长"), gbDev), rd,0); gDev->addWidget(m_spBuzzerDur, rd,1); rd++;

    // Voice box (two sets)
    auto* gbVoice = new QGroupBox(tr("语音设置"), page);
    auto* vVoice = new QVBoxLayout(gbVoice);
    auto* hVoice = new QHBoxLayout();

    auto* gbVoice1 = new QGroupBox(tr("语音1"), gbVoice);
    auto* gV1 = new QGridLayout(gbVoice1);
    m_cmbVoice1Announcer = new QComboBox(gbVoice1);
    m_cmbVoice1Announcer->addItem(tr("艾佳（女声）"), 3);
    m_cmbVoice1Announcer->addItem(tr("艾诚（男声）"), 51);
    m_cmbVoice1Announcer->addItem(tr("艾达（男声）"), 52);
    m_cmbVoice1Announcer->addItem(tr("艾琪（女声）"), 53);
    m_cmbVoice1Announcer->addItem(tr("唐老鸭（效果器）"), 54);
    m_cmbVoice1Announcer->addItem(tr("艾彤（女童声）"), 55);
    m_spVoice1Style = new QSpinBox(gbVoice1); m_spVoice1Style->setRange(0, 2);
    m_spVoice1Speed = new QSpinBox(gbVoice1); m_spVoice1Speed->setRange(0, 10);
    m_spVoice1Pitch = new QSpinBox(gbVoice1); m_spVoice1Pitch->setRange(0, 10);
    m_spVoice1Volume = new QSpinBox(gbVoice1); m_spVoice1Volume->setRange(0, 10);
    int v1r = 0;
    gV1->addWidget(new QLabel(tr("播音员"), gbVoice1), v1r,0); gV1->addWidget(m_cmbVoice1Announcer, v1r,1); v1r++;
    gV1->addWidget(new QLabel(tr("发音风格"), gbVoice1), v1r,0); gV1->addWidget(m_spVoice1Style, v1r,1); v1r++;
    gV1->addWidget(new QLabel(tr("语速"), gbVoice1), v1r,0); gV1->addWidget(m_spVoice1Speed, v1r,1); v1r++;
    gV1->addWidget(new QLabel(tr("语调"), gbVoice1), v1r,0); gV1->addWidget(m_spVoice1Pitch, v1r,1); v1r++;
    gV1->addWidget(new QLabel(tr("响度"), gbVoice1), v1r,0); gV1->addWidget(m_spVoice1Volume, v1r,1); v1r++;

    auto* gbVoice2 = new QGroupBox(tr("语音2"), gbVoice);
    auto* gV2 = new QGridLayout(gbVoice2);
    m_cmbVoice2Announcer = new QComboBox(gbVoice2);
    m_cmbVoice2Announcer->addItem(tr("艾佳（女声）"), 3);
    m_cmbVoice2Announcer->addItem(tr("艾诚（男声）"), 51);
    m_cmbVoice2Announcer->addItem(tr("艾达（男声）"), 52);
    m_cmbVoice2Announcer->addItem(tr("艾琪（女声）"), 53);
    m_cmbVoice2Announcer->addItem(tr("唐老鸭（效果器）"), 54);
    m_cmbVoice2Announcer->addItem(tr("艾彤（女童声）"), 55);
    m_spVoice2Style = new QSpinBox(gbVoice2); m_spVoice2Style->setRange(0, 2);
    m_spVoice2Speed = new QSpinBox(gbVoice2); m_spVoice2Speed->setRange(0, 10);
    m_spVoice2Pitch = new QSpinBox(gbVoice2); m_spVoice2Pitch->setRange(0, 10);
    m_spVoice2Volume = new QSpinBox(gbVoice2); m_spVoice2Volume->setRange(0, 10);
    int v2r = 0;
    gV2->addWidget(new QLabel(tr("播音员"), gbVoice2), v2r,0); gV2->addWidget(m_cmbVoice2Announcer, v2r,1); v2r++;
    gV2->addWidget(new QLabel(tr("发音风格"), gbVoice2), v2r,0); gV2->addWidget(m_spVoice2Style, v2r,1); v2r++;
    gV2->addWidget(new QLabel(tr("语速"), gbVoice2), v2r,0); gV2->addWidget(m_spVoice2Speed, v2r,1); v2r++;
    gV2->addWidget(new QLabel(tr("语调"), gbVoice2), v2r,0); gV2->addWidget(m_spVoice2Pitch, v2r,1); v2r++;
    gV2->addWidget(new QLabel(tr("响度"), gbVoice2), v2r,0); gV2->addWidget(m_spVoice2Volume, v2r,1); v2r++;

    hVoice->addWidget(gbVoice1);
    hVoice->addWidget(gbVoice2);
    vVoice->addLayout(hVoice);


    // Colors
    auto* gbColors = new QGroupBox(tr("颜色表"), page);
    gbColors->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* vColors = new QVBoxLayout(gbColors);
    m_tblColors = new QTableView(gbColors);
    m_tblColors->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tblColors->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tblColors->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tblColors->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_tblColors->horizontalHeader()->setStretchLastSection(true);
    m_btnAddColor = new QPushButton(tr("新增"), gbColors);
    m_btnDeleteColor = new QPushButton(tr("删除所选"), gbColors);
    m_btnClearColors = new QPushButton(tr("清空"), gbColors);
    auto* colorBtns = new QHBoxLayout();
    colorBtns->addWidget(m_btnAddColor);
    colorBtns->addWidget(m_btnDeleteColor);
    colorBtns->addWidget(m_btnClearColors);
    colorBtns->addStretch(1);
    vColors->addLayout(colorBtns);
    vColors->addWidget(m_tblColors, 1);

    // Conflicts
    auto* gbConf = new QGroupBox(tr("冲突表(三元组)"), page);
    gbConf->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* vConf = new QVBoxLayout(gbConf);
    m_tblConflicts = new QTableView(gbConf);
    m_tblConflicts->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tblConflicts->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tblConflicts->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_btnAddConflict = new QPushButton(tr("新增行"), gbConf);
    m_btnClearConflicts = new QPushButton(tr("清空"), gbConf);
    auto* confBtns = new QHBoxLayout();
    confBtns->addWidget(m_btnAddConflict);
    confBtns->addWidget(m_btnClearConflicts);
    confBtns->addStretch(1);
    vConf->addLayout(confBtns);
    vConf->addWidget(m_tblConflicts, 1);
    m_btnApplySettings = new QPushButton(tr("应用"), gbConf);
    auto* applyRow = new QHBoxLayout();
    applyRow->addStretch(1);
    applyRow->addWidget(m_btnApplySettings);
    vConf->addLayout(applyRow);

    // Hotkeys
    auto* gbHot = new QGroupBox(tr("快捷键"), page);
    gbHot->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    gbHot->setMaximumHeight(400);
    auto* gHot = new QGridLayout(gbHot);
    int rh = 0;
    m_keyNext = new QKeySequenceEdit(gbHot);
    m_keyRerun = new QKeySequenceEdit(gbHot);
    gHot->addWidget(new QLabel(tr("顺序执行 (Next)"), gbHot), rh, 0);
    gHot->addWidget(m_keyNext, rh, 1); rh++;
    gHot->addWidget(new QLabel(tr("标记需重做"), gbHot), rh, 0);
    gHot->addWidget(m_keyRerun, rh, 1); rh++;

    m_keyQuickColor.clear();
    for (int i = 0; i < 7; ++i)
    {
        m_keyQuickColor.push_back(new QKeySequenceEdit(gbHot));
        gHot->addWidget(new QLabel(tr("测试颜色%1常亮").arg(i + 1), gbHot), rh, 0);
        gHot->addWidget(m_keyQuickColor[i], rh, 1);
        rh++;
    }
    m_keyAllOff = new QKeySequenceEdit(gbHot);
    gHot->addWidget(new QLabel(tr("测试全灭"), gbHot), rh, 0);
    gHot->addWidget(m_keyAllOff, rh, 1); rh++;
    gHot->addWidget(new QWidget(gbHot), rh, 1);

    // Tests
    auto* gbTest = new QGroupBox(tr("测试功能"), page);
    gbTest->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    gbTest->setMaximumHeight(200);
    auto* gTest = new QGridLayout(gbTest);
    int rt = 0;
    m_editLedTest = new QLineEdit(gbTest);
    m_editLedTest->setPlaceholderText(tr("颜色编号"));
    m_editLedTest->setText("1");
    m_btnTestLed = new QPushButton(tr("LED 测试"), gbTest);
    gTest->addWidget(new QLabel(tr("LED 测试"), gbTest), rt, 0);
    gTest->addWidget(m_editLedTest, rt, 1);
    gTest->addWidget(m_btnTestLed, rt, 2); rt++;

    m_btnTestBeep = new QPushButton(tr("BEEP 测试"), gbTest);
    gTest->addWidget(new QLabel(tr("BEEP 测试"), gbTest), rt, 0);
    gTest->addWidget(m_btnTestBeep, rt, 2); rt++;

    m_editVoiceTest = new QLineEdit(gbTest);
    m_editVoiceTest->setPlaceholderText(tr("语音文本"));
    m_cmbVoiceTestStyle = new QComboBox(gbTest);
    m_cmbVoiceTestStyle->addItem("0", 0);
    m_cmbVoiceTestStyle->addItem("1", 1);
    m_cmbVoiceTestStyle->addItem("2", 2);
    m_btnTestVoice = new QPushButton(tr("VOICE 测试"), gbTest);
    gTest->addWidget(new QLabel(tr("VOICE 测试"), gbTest), rt, 0);
    gTest->addWidget(m_editVoiceTest, rt, 1);
    gTest->addWidget(m_cmbVoiceTestStyle, rt, 2);
    gTest->addWidget(m_btnTestVoice, rt, 3); rt++;

    left->addWidget(gbSerial);
    left->addWidget(gbDev);
    left->addWidget(gbVoice);
    left->addStretch(1);

    middle->addWidget(gbHot);
    middle->addWidget(gbTest);
    middle->addStretch(1);

    right->addWidget(gbColors);
    right->addWidget(gbConf);
    right->setStretch(0, 1);
    right->setStretch(1, 1);

    root->addLayout(left, 0);
    root->addLayout(middle, 0);
    root->addLayout(right, 1);
    root->setStretch(0, 1);
    root->setStretch(1, 1);
    root->setStretch(2, 3);
    return page;
}

void MainWindow::wireSignals()
{
    // status
    connect(m_btnPickExcel, &QPushButton::clicked, this, &MainWindow::onPickExcel);
    connect(m_btnApplyConfig, &QPushButton::clicked, this, &MainWindow::onApplyConfig);
    connect(m_btnStart, &QPushButton::clicked, this, &MainWindow::onStart);
    connect(m_btnNext, &QPushButton::clicked, this, &MainWindow::onNext);
    connect(m_btnMarkRerun, &QPushButton::clicked, this, &MainWindow::onMarkRerun);
    connect(m_btnReset, &QPushButton::clicked, this, &MainWindow::onReset);

    // serial
    connect(m_btnRefreshPorts, &QPushButton::clicked, this, &MainWindow::onRefreshPorts);
    connect(m_btnOpenClose, &QPushButton::clicked, this, &MainWindow::onOpenCloseSerial);
    connect(m_cmbPort, &QComboBox::currentTextChanged, this, &MainWindow::onPortSelectionChanged);
    connect(m_serial, &SerialService::portsUpdated, this, &MainWindow::onPortsUpdated);
    connect(m_serial, &SerialService::opened, this, &MainWindow::onSerialOpened);
    connect(m_serial, &SerialService::closed, this, &MainWindow::onSerialClosed);
    connect(m_serial, &SerialService::error, this, &MainWindow::onSerialError);

    // device
    // removed per-Block save buttons; use Apply

    // colors
    connect(m_btnAddColor, &QPushButton::clicked, this, &MainWindow::onAddColor);
    connect(m_btnDeleteColor, &QPushButton::clicked, this, &MainWindow::onDeleteColor);
    connect(m_btnClearColors, &QPushButton::clicked, this, &MainWindow::onClearColors);

    // conflicts
    connect(m_btnAddConflict, &QPushButton::clicked, this, &MainWindow::onAddConflict);
    connect(m_btnClearConflicts, &QPushButton::clicked, this, &MainWindow::onClearConflicts);

    // hotkeys
    connect(m_btnApplySettings, &QPushButton::clicked, this, &MainWindow::onApplySettings);
    auto bindHotkeyEdit = [&](QKeySequenceEdit* edit){
        if (!edit) return;
        connect(edit, &QKeySequenceEdit::keySequenceChanged, this, &MainWindow::updateHotkeyDuplicateHints);
    };
    bindHotkeyEdit(m_keyNext);
    bindHotkeyEdit(m_keyRerun);
    for (auto* edit : m_keyQuickColor)
        bindHotkeyEdit(edit);
    bindHotkeyEdit(m_keyAllOff);

    // tests
    connect(m_btnTestLed, &QPushButton::clicked, this, &MainWindow::onTestLed);
    connect(m_btnTestBeep, &QPushButton::clicked, this, &MainWindow::onTestBeep);
    connect(m_btnTestVoice, &QPushButton::clicked, this, &MainWindow::onTestVoice);

    // engine
    connect(m_engine, &WorkflowEngine::idle, this, &MainWindow::onEngineIdle);
    connect(m_engine, &WorkflowEngine::segmentStarted, this, &MainWindow::onEngineSegmentStarted);
    connect(m_engine, &WorkflowEngine::actionStarted, this, [](int, const QString&, const QString&) {});
    connect(m_engine, &WorkflowEngine::progressUpdated, this, &MainWindow::onEngineProgressUpdated);
    connect(m_engine, &WorkflowEngine::rerunMarked, this, &MainWindow::onEngineRerunMarked);
    connect(m_engine, &WorkflowEngine::logLine, this, &MainWindow::onEngineLogLine);
    connect(m_serial, &SerialService::rxRaw, m_engine, &WorkflowEngine::onSerialFrame);
}

void MainWindow::applyUiState()
{
    const bool hasConfig = m_configApplied;
    const bool started = (m_uiState == UiRunState::Started || m_uiState == UiRunState::Running);

    m_btnApplyConfig->setEnabled(true);
    m_btnStart->setEnabled(hasConfig);
    m_btnNext->setEnabled(started);
    m_btnMarkRerun->setEnabled(hasConfig);
    m_btnReset->setEnabled(hasConfig);
    enableTestHotkeys(!started);

    switch (m_uiState)
    {
    case UiRunState::NoConfig: m_lblRunState->setText(tr("状态：未应用配置")); break;
    case UiRunState::Ready:    m_lblRunState->setText(tr("状态：已应用配置，等待开始")); break;
    case UiRunState::Started:  m_lblRunState->setText(tr("状态：Ready，等待下一段")); break;
    case UiRunState::Running:  m_lblRunState->setText(tr("状态：执行中")); break;
    }
}

void MainWindow::applyQueueColumnLayout()
{
    if (!m_tblQueue || !m_queueModel)
        return;

    QHeaderView* header = m_tblQueue->horizontalHeader();
    header->setStretchLastSection(false);
    header->setSectionResizeMode(QHeaderView::Fixed);
    header->setSectionsMovable(false);
    header->setDefaultAlignment(Qt::AlignCenter);

    QFontMetrics fm(m_tblQueue->font());
    int width = fm.horizontalAdvance(QStringLiteral("工作模式")) + 24;
    if (width < 40)
        width = 40;

    const int cols = m_queueModel->columnCount();
    for (int c = 0; c < cols; ++c)
        m_tblQueue->setColumnWidth(c, width);
}

bool MainWindow::loadSettings()
{
    delete m_settings;
    m_settings = new SettingsData(AppSettings::load());
    return true;
}

void MainWindow::saveSettings()
{
    if (!m_settings) return;
    AppSettings::save(*m_settings);
}

// ================= Status actions =================
void MainWindow::onPickExcel()
{
    const QString initDir = m_excelPath.isEmpty() ? QString() : QFileInfo(m_excelPath).absolutePath();
    const QString path = QFileDialog::getOpenFileName(this, tr("选择 .xlsx 配置文件"), initDir, tr("Excel Files (*.xlsx)"));
    if (path.isEmpty()) return;
    m_excelPath = path;
    m_editExcelPath->setText(path);
    if (m_settings) AppSettings::saveLastExcelPath(path);
}

bool MainWindow::importExcel(const QString &path, QString &errMsg)
{
    if (!m_importer->loadXlsx(path, errMsg))
        return false;
    return true;
}

void MainWindow::onApplyConfig()
{
    if (m_excelPath.isEmpty())
    {
        QMessageBox::warning(this, tr("提示"), tr("请先选择 .xlsx 配置文件"));
        return;
    }
    QString err;
    if (!importExcel(m_excelPath, err))
    {
        QMessageBox::warning(this, tr("导入失败"), err);
        return;
    }
    m_currentFlowName.clear();
    QHash<int, QColor> colorMap;
    if (m_settings)
    {
        for (const auto& c : m_settings->colors)
        {
            if (c.index > 0 && c.rgb.isValid())
                colorMap.insert(c.index, c.rgb);
        }
        if (!colorMap.isEmpty())
        {
            for (const auto& a : m_importer->actions())
            {
                if (a.type != ActionType::L)
                    continue;
                for (int v : a.ledColors)
                {
                    if (v > 0 && !colorMap.contains(v))
                    {
                        QMessageBox::warning(this, tr("导入失败"), tr("颜色编号 %1 不在颜色表").arg(v));
                        return;
                    }
                }
            }
        }
        else
        {
            for (const auto& a : m_importer->actions())
            {
                if (a.type != ActionType::L)
                    continue;
                for (int v : a.ledColors)
                {
                    if (v > 0)
                    {
                        QMessageBox::warning(this, tr("导入失败"), tr("颜色表为空，无法使用颜色编号 %1").arg(v));
                        return;
                    }
                }
            }
        }
    }

    // LED count from Excel overrides setting (pad to at least 5)
    if (m_settings)
    {
        int ledCountFromExcel = m_importer->ledCount();
        if (ledCountFromExcel > 0 && ledCountFromExcel < 5)
            ledCountFromExcel = 5;
        if (ledCountFromExcel > 0)
        {
            m_settings->device.ledCount = ledCountFromExcel;
            m_spLedCount->setValue(ledCountFromExcel);
        }
    }
    m_queueModel->setTableRows(m_importer->tableRows(),
                               m_importer->tableColumnStart(),
                               m_importer->tableColumnCount());
    m_queueModel->setActions(m_importer->actions());
    m_queueModel->setLedColorMap(colorMap);
    m_queueModel->clearFlowStates();
    m_queueModel->clearStepTimes();
    applyQueueColumnLayout();

    m_configApplied = true;
    m_uiState = UiRunState::Ready;
    applyUiState();
}

bool MainWindow::precheckBeforeStart(QString &errMsg)
{
    if (!m_settings) { errMsg = tr("无配置"); return false; }
    if (!m_importer || m_importer->actions().isEmpty()) { errMsg = tr("未导入Excel"); return false; }
    if (!RandomColorResolver::precheckSolvable(m_importer->actions(),
                                               m_settings->colors,
                                               m_settings->conflicts,
                                               m_settings->device.ledCount,
                                               errMsg))
        return false;
    return true;
}

void MainWindow::onStart()
{
    if (!m_configApplied)
        return;

    QString err;
    if (!precheckBeforeStart(err))
    {
        QMessageBox::warning(this, tr("开始失败"), err);
        return;
    }

    QVector<ActionItem> resolved;
    if (!RandomColorResolver::resolveAll(
            m_importer->actions(),
            m_settings->colors,
            m_settings->conflicts,
            m_settings->device.ledCount,
            resolved,
            err))
    {
        QMessageBox::warning(this, tr("开始失败"), err);
        return;
    }

    m_engine->setDeviceProps(m_settings->device);
    m_engine->setColors(m_settings->colors);
    m_engine->setVoiceSets(m_settings->voice1, m_settings->voice2);
    m_engine->beginRun();
    m_engine->sendConfigs();
    m_engine->loadPlan(resolved);
    m_queueModel->clearFlowStates();
    m_queueModel->clearStepTimes();

    m_uiState = UiRunState::Started;
    applyUiState();
}

void MainWindow::onNext()
{
    if (m_uiState != UiRunState::Started)
        return;
    m_uiState = UiRunState::Running;
    applyUiState();
    m_engine->runNextSegment();
}

void MainWindow::onMarkRerun()
{
    if (!m_configApplied) return;
    m_engine->markCurrentOrPreviousSegmentForRerun();
    m_lblHint->setText(tr("已标记需重做段，下一次执行优先重做。"));
}

void MainWindow::onReset()
{
    m_engine->resetRun();
    m_uiState = m_configApplied ? UiRunState::Ready : UiRunState::NoConfig;
    m_lblHint->clear();
    m_currentFlowName.clear();
    if (m_importer)
    {
        m_queueModel->setTableRows(m_importer->tableRows(),
                                   m_importer->tableColumnStart(),
                                   m_importer->tableColumnCount());
        m_queueModel->setActions(m_importer->actions());
    }
    m_queueModel->clearFlowStates();
    m_queueModel->clearStepTimes();
    applyUiState();
}

// ================= Serial =================
void MainWindow::onRefreshPorts()
{
    m_serial->refreshPorts();
}

void MainWindow::onPortsUpdated(const QStringList &ports)
{
    const QSignalBlocker blocker(m_cmbPort);
    m_cmbPort->clear();
    m_cmbPort->addItems(ports);
    if (m_settings && !m_settings->serial.portName.isEmpty())
    {
        const int idx = ports.indexOf(m_settings->serial.portName);
        if (idx >= 0)
            m_cmbPort->setCurrentIndex(idx);
    }
    if (m_cmbPort->currentIndex() < 0 && m_cmbPort->count() > 0)
        m_cmbPort->setCurrentIndex(0);
}

void MainWindow::onPortSelectionChanged(const QString& port)
{
    Q_UNUSED(port);
    if (m_serial && m_serial->isOpen())
        m_serial->closePort();
}

void MainWindow::onOpenCloseSerial()
{
    if (m_serial->isOpen())
    {
        m_serial->closePort();
        return;
    }
    const QString port = m_cmbPort->currentText();
    if (port.isEmpty())
    {
        QMessageBox::warning(this, tr("串口"), tr("未检测到串口，请刷新后选择端口"));
        return;
    }
    const int baud = m_cmbBaud->currentText().toInt();
    const int dataBits = m_cmbDataBits->currentText().toInt();
    const QString parity = m_cmbParity->currentText();
    const int stopBits = m_cmbStopBits->currentText().toInt();

    m_serial->openPort(port, baud, dataBits, parity, stopBits);

    if (m_settings)
    {
        m_settings->serial.portName = port;
        m_settings->serial.baud = baud;
        m_settings->serial.dataBits = dataBits;
        m_settings->serial.parity = parity;
        m_settings->serial.stopBits = stopBits;
        AppSettings::saveSerial(m_settings->serial);
    }
}

void MainWindow::onSerialOpened(bool ok, const QString &err)
{
    if (ok)
    {
        m_btnOpenClose->setText(tr("关闭串口"));
    }
    else
    {
        m_btnOpenClose->setText(tr("打开串口"));
        QMessageBox::warning(this, tr("串口错误"), err);
    }
}

void MainWindow::onSerialClosed()
{
    m_btnOpenClose->setText(tr("打开串口"));
}

void MainWindow::onSerialError(const QString &err)
{
    QMessageBox::warning(this, tr("串口错误"), err);
}

// ================= Device =================
void MainWindow::onSaveDevice()
{
    if (!m_settings) return;
    m_settings->device.onMs = m_spOnMs->value();
    m_settings->device.gapMs = m_spGapMs->value();
    m_settings->device.ledCount = m_spLedCount->value();
    m_settings->device.brightness = m_spBrightness->value();
    m_settings->device.buzzerFreq = m_spBuzzerFreq->value();
    m_settings->device.buzzerDurMs = m_spBuzzerDur->value();

    AppSettings::saveDevice(m_settings->device);
    QMessageBox::information(this, tr("提示"), tr("设备属性已保存"));

    // Spec: after saving device properties, push configs to device.
    if (m_serial && m_serial->isOpen() && m_engine)
    {
        m_engine->setDeviceProps(m_settings->device);
        m_engine->setColors(m_settings->colors);
        m_engine->setVoiceSets(m_settings->voice1, m_settings->voice2);
        m_engine->sendConfigs();
    }
}

void MainWindow::onSaveVoice()
{
    if (!m_settings) return;
    m_settings->voice1.announcer = m_cmbVoice1Announcer->currentData().toInt();
    m_settings->voice1.voiceStyle = m_spVoice1Style->value();
    m_settings->voice1.voiceSpeed = m_spVoice1Speed->value();
    m_settings->voice1.voicePitch = m_spVoice1Pitch->value();
    m_settings->voice1.voiceVolume = m_spVoice1Volume->value();

    m_settings->voice2.announcer = m_cmbVoice2Announcer->currentData().toInt();
    m_settings->voice2.voiceStyle = m_spVoice2Style->value();
    m_settings->voice2.voiceSpeed = m_spVoice2Speed->value();
    m_settings->voice2.voicePitch = m_spVoice2Pitch->value();
    m_settings->voice2.voiceVolume = m_spVoice2Volume->value();

    AppSettings::saveVoiceSets(m_settings->voice1, m_settings->voice2);
    QMessageBox::information(this, tr("提示"), tr("语音设置已保存"));

    if (m_serial && m_serial->isOpen() && m_engine)
    {
        m_engine->setVoiceSets(m_settings->voice1, m_settings->voice2);
        m_engine->sendConfigs();
    }
}

void MainWindow::onApplySettings()
{
    if (!m_settings) return;

    // serial
    m_settings->serial.portName = m_cmbPort->currentText();
    m_settings->serial.baud = m_cmbBaud->currentText().toInt();
    m_settings->serial.dataBits = m_cmbDataBits->currentText().toInt();
    m_settings->serial.parity = m_cmbParity->currentText();
    m_settings->serial.stopBits = m_cmbStopBits->currentText().toInt();

    updateDeviceVoiceColorsFromUi();

    // hotkeys
    HotkeyConfig hk = readHotkeysFromUi();
    m_settings->hotkeys = hk;
    m_keyNext->setKeySequence(hk.keyNext);
    m_keyRerun->setKeySequence(hk.keyRerun);
    for (int i = 0; i < m_keyQuickColor.size() && i < hk.keyQuickColor.size(); ++i)
        m_keyQuickColor[i]->setKeySequence(hk.keyQuickColor[i]);
    m_keyAllOff->setKeySequence(hk.keyAllOff);
    updateHotkeyDuplicateHints();
    rebuildShortcuts();

    // colors / conflicts
    m_settings->conflicts = m_conflictModel->triples();

    AppSettings::saveHotkeys(m_settings->hotkeys);
    AppSettings::save(*m_settings);

    sendConfigsToDevice(false);
}

// ================= Hotkeys & Tests =================
void MainWindow::onSaveHotkeys()
{
    if (!m_settings) return;
    HotkeyConfig hk;
    if (!collectHotkeys(hk, true))
        return;

    m_settings->hotkeys = hk;
    AppSettings::saveHotkeys(hk);
    // 显示上也同步裁剪后的单键
    m_keyNext->setKeySequence(hk.keyNext);
    m_keyRerun->setKeySequence(hk.keyRerun);
    for (int i = 0; i < m_keyQuickColor.size() && i < hk.keyQuickColor.size(); ++i)
        m_keyQuickColor[i]->setKeySequence(hk.keyQuickColor[i]);
    m_keyAllOff->setKeySequence(hk.keyAllOff);
    updateHotkeyDuplicateHints();
    rebuildShortcuts();
    QMessageBox::information(this, tr("提示"), tr("快捷键已保存"));
}

HotkeyConfig MainWindow::readHotkeysFromUi() const
{
    HotkeyConfig hk;
    hk.keyNext = readKeySequenceFromEdit(m_keyNext);
    hk.keyRerun = readKeySequenceFromEdit(m_keyRerun);
    hk.keyAllOff = readKeySequenceFromEdit(m_keyAllOff);
    hk.keyQuickColor.clear();
    for (int i = 0; i < m_keyQuickColor.size(); ++i)
        hk.keyQuickColor.push_back(readKeySequenceFromEdit(m_keyQuickColor[i]));
    return hk;
}

QKeySequence MainWindow::readKeySequenceFromEdit(const QKeySequenceEdit* edit) const
{
    if (!edit)
        return QKeySequence();

    QKeySequence seq = edit->keySequence();
    QLineEdit* line = edit->findChild<QLineEdit*>();
    if (line)
    {
        const QString text = line->text().trimmed();
        if (!text.isEmpty())
        {
            QKeySequence parsed = QKeySequence::fromString(text, QKeySequence::NativeText);
            if (parsed.isEmpty())
                parsed = QKeySequence::fromString(text, QKeySequence::PortableText);
            if (!parsed.isEmpty())
            {
                const QString seqText = seq.toString(QKeySequence::NativeText);
                if (seq.isEmpty() || seqText != text)
                    seq = parsed;
            }
        }
    }

    if (seq.count() > 1)
        seq = QKeySequence(seq[0]);
    return seq;
}

bool MainWindow::collectHotkeys(HotkeyConfig& hk, bool showWarning)
{
    auto seqKey = [](const QKeySequence& seq)->QString {
        return seq.toString(QKeySequence::PortableText);
    };

    hk = readHotkeysFromUi();

    QHash<QString, QString> used;
    auto addCheck = [&](const QString& name, const QKeySequence& seq)->bool {
        if (seq.isEmpty())
            return true;
        const QString key = seqKey(seq);
        if (used.contains(key))
        {
            if (showWarning)
            {
                QMessageBox::warning(this, tr("快捷键冲突"),
                                     tr("快捷键重复：%1 与 %2").arg(used.value(key), name));
            }
            return false;
        }
        used.insert(key, name);
        return true;
    };
    if (!addCheck(tr("顺序执行"), hk.keyNext)) return false;
    if (!addCheck(tr("标记需重做"), hk.keyRerun)) return false;
    for (int i = 0; i < hk.keyQuickColor.size(); ++i)
    {
        if (!addCheck(tr("测试颜色%1常亮").arg(i + 1), hk.keyQuickColor[i])) return false;
    }
    if (!addCheck(tr("测试全灭"), hk.keyAllOff)) return false;
    return true;
}

void MainWindow::updateHotkeyDuplicateHints()
{
    if (m_hotkeyUpdateGuard)
        return;
    struct Item
    {
        QKeySequenceEdit* edit;
        QString name;
    };
    QVector<Item> items;
    items.push_back({m_keyNext, tr("顺序执行")});
    items.push_back({m_keyRerun, tr("标记需重做")});
    for (int i = 0; i < m_keyQuickColor.size(); ++i)
        items.push_back({m_keyQuickColor[i], tr("测试颜色%1常亮").arg(i + 1)});
    items.push_back({m_keyAllOff, tr("测试全灭")});

    auto seqKey = [](const QKeySequence& seq)->QString {
        return seq.toString(QKeySequence::PortableText);
    };

    QHash<QString, QVector<int>> groups;
    for (int i = 0; i < items.size(); ++i)
    {
        const auto* edit = items[i].edit;
        if (!edit) continue;
        const QKeySequence seq = readKeySequenceFromEdit(edit);
        if (seq.isEmpty()) continue;
        const QString key = seqKey(seq);
        groups[key].push_back(i);
    }

    QSet<int> dupIdx;
    for (auto it = groups.begin(); it != groups.end(); ++it)
    {
        if (it.value().size() > 1)
        {
            for (int idx : it.value())
                dupIdx.insert(idx);
        }
    }

    QStringList conflicts;
    for (int i = 0; i < items.size(); ++i)
    {
        auto* edit = items[i].edit;
        if (!edit) continue;

        if (dupIdx.contains(i))
        {
            edit->setStyleSheet(QStringLiteral("border: 1px solid #cc0000; background: #ffecec;"));
        }
        else
        {
            edit->setStyleSheet(QString());
            edit->setToolTip(QString());
        }
    }

    for (auto it = groups.begin(); it != groups.end(); ++it)
    {
        if (it.value().size() <= 1) continue;
        QStringList names;
        for (int idx : it.value())
            names << items[idx].name;
        const QString msg = names.join(QStringLiteral(" / "));
        conflicts << msg;

        for (int idx : it.value())
        {
            if (items[idx].edit)
                items[idx].edit->setToolTip(tr("快捷键重复：%1").arg(msg));
        }
    }

    if (!conflicts.isEmpty())
        statusBar()->showMessage(tr("快捷键重复：%1").arg(conflicts.join(QStringLiteral(" | "))), 5000);
    else
        statusBar()->clearMessage();

    if (conflicts.isEmpty() && m_settings && m_hotkeyAutoSaveEnabled)
    {
        HotkeyConfig hk;
        if (collectHotkeys(hk, false))
        {
            m_hotkeyUpdateGuard = true;
            m_settings->hotkeys = hk;
            AppSettings::saveHotkeys(hk);
            m_keyNext->setKeySequence(hk.keyNext);
            m_keyRerun->setKeySequence(hk.keyRerun);
            for (int i = 0; i < m_keyQuickColor.size() && i < hk.keyQuickColor.size(); ++i)
                m_keyQuickColor[i]->setKeySequence(hk.keyQuickColor[i]);
            m_keyAllOff->setKeySequence(hk.keyAllOff);
            rebuildShortcuts();
            m_hotkeyUpdateGuard = false;
        }
    }
}

void MainWindow::updateDeviceVoiceColorsFromUi()
{
    if (!m_settings) return;

    m_settings->device.onMs = m_spOnMs->value();
    m_settings->device.gapMs = m_spGapMs->value();
    m_settings->device.ledCount = m_spLedCount->value();
    m_settings->device.brightness = m_spBrightness->value();
    m_settings->device.buzzerFreq = m_spBuzzerFreq->value();
    m_settings->device.buzzerDurMs = m_spBuzzerDur->value();

    m_settings->voice1.announcer = m_cmbVoice1Announcer->currentData().toInt();
    m_settings->voice1.voiceStyle = m_spVoice1Style->value();
    m_settings->voice1.voiceSpeed = m_spVoice1Speed->value();
    m_settings->voice1.voicePitch = m_spVoice1Pitch->value();
    m_settings->voice1.voiceVolume = m_spVoice1Volume->value();

    m_settings->voice2.announcer = m_cmbVoice2Announcer->currentData().toInt();
    m_settings->voice2.voiceStyle = m_spVoice2Style->value();
    m_settings->voice2.voiceSpeed = m_spVoice2Speed->value();
    m_settings->voice2.voicePitch = m_spVoice2Pitch->value();
    m_settings->voice2.voiceVolume = m_spVoice2Volume->value();

    m_settings->colors = m_colorModel->colors();
}

void MainWindow::sendConfigsToDevice(bool warnIfSerialClosed)
{
    if (!m_serial || !m_serial->isOpen() || !m_engine)
    {
        if (warnIfSerialClosed)
            QMessageBox::warning(this, tr("提示"), tr("串口未打开，已保存设置但未下发到设备"));
        return;
    }
    m_engine->setDeviceProps(m_settings->device);
    m_engine->setColors(m_settings->colors);
    m_engine->setVoiceSets(m_settings->voice1, m_settings->voice2);
    m_engine->sendConfigs();
}

void MainWindow::rebuildShortcuts()
{
    for (auto* sc : m_shortcuts)
        delete sc;
    m_shortcuts.clear();
    m_testShortcuts.clear();

    if (!m_settings)
        return;

    QSet<QString> used;
    auto addShortcut = [&](const QKeySequence& seq, const char* slot, bool isTestShortcut = false){
        if (seq.isEmpty())
            return;
        const QString key = seq.toString(QKeySequence::PortableText);
        if (used.contains(key))
            return;
        used.insert(key);
        auto* sc = new QShortcut(seq, this);
        sc->setContext(Qt::WindowShortcut);
        connect(sc, SIGNAL(activated()), this, slot);
        m_shortcuts.push_back(sc);
        if (isTestShortcut)
            m_testShortcuts.push_back(sc);
    };

    addShortcut(m_settings->hotkeys.keyNext, SLOT(onHotkeyNext()));
    addShortcut(m_settings->hotkeys.keyRerun, SLOT(onHotkeyRerun()));

    QVector<QKeySequence> quick = m_settings->hotkeys.keyQuickColor;
    while (quick.size() < 7) quick.push_back(QKeySequence());
    const char* quickSlots[] = {
        SLOT(onHotkeyQuickColor1()),
        SLOT(onHotkeyQuickColor2()),
        SLOT(onHotkeyQuickColor3()),
        SLOT(onHotkeyQuickColor4()),
        SLOT(onHotkeyQuickColor5()),
        SLOT(onHotkeyQuickColor6()),
        SLOT(onHotkeyQuickColor7())
    };
    for (int i = 0; i < 7; ++i)
        addShortcut(quick[i], quickSlots[i], true);

    addShortcut(m_settings->hotkeys.keyAllOff, SLOT(onHotkeyAllOff()), true);

    const bool started = (m_uiState == UiRunState::Started || m_uiState == UiRunState::Running);
    enableTestHotkeys(!started);
}

void MainWindow::enableTestHotkeys(bool enable)
{
    for (auto* sc : m_testShortcuts)
    {
        if (sc) sc->setEnabled(enable);
    }
}

void MainWindow::onHotkeyNext()
{
    onNext();
}

void MainWindow::onHotkeyRerun()
{
    onMarkRerun();
}

void MainWindow::sendTestSolidColor(int idx)
{
    if (!m_serial || !m_serial->isOpen())
        return;
    const QString frame = Protocol::packTestSolid(idx);
    if (m_engine) m_engine->logTestTx(frame);
    m_serial->sendFrame(frame);
}

void MainWindow::sendTestAllOff()
{
    if (!m_serial || !m_serial->isOpen())
        return;
    const QString frame = Protocol::packTestAllOff();
    if (m_engine) m_engine->logTestTx(frame);
    m_serial->sendFrame(frame);
}

void MainWindow::onHotkeyQuickColor1() { sendTestSolidColor(1); }
void MainWindow::onHotkeyQuickColor2() { sendTestSolidColor(2); }
void MainWindow::onHotkeyQuickColor3() { sendTestSolidColor(3); }
void MainWindow::onHotkeyQuickColor4() { sendTestSolidColor(4); }
void MainWindow::onHotkeyQuickColor5() { sendTestSolidColor(5); }
void MainWindow::onHotkeyQuickColor6() { sendTestSolidColor(6); }
void MainWindow::onHotkeyQuickColor7() { sendTestSolidColor(7); }

void MainWindow::onHotkeyAllOff()
{
    sendTestAllOff();
}

void MainWindow::onTestLed()
{
    if (!m_serial || !m_serial->isOpen())
    {
        QMessageBox::warning(this, tr("提示"), tr("请先打开串口"));
        return;
    }
    updateDeviceVoiceColorsFromUi();
    sendConfigsToDevice(false);
    bool ok = false;
    const int idx = m_editLedTest->text().trimmed().toInt(&ok);
    if (!ok)
    {
        QMessageBox::warning(this, tr("提示"), tr("请输入颜色编号"));
        return;
    }
    sendTestSolidColor(idx);
}

void MainWindow::onTestBeep()
{
    if (!m_serial || !m_serial->isOpen())
    {
        QMessageBox::warning(this, tr("提示"), tr("请先打开串口"));
        return;
    }
    updateDeviceVoiceColorsFromUi();
    sendConfigsToDevice(false);
    const DeviceProps dev = m_settings ? m_settings->device : DeviceProps();
    const QString frame = Protocol::packBeepTest(dev);
    if (m_engine) m_engine->logTestTx(frame);
    m_serial->sendFrame(frame);
}

void MainWindow::onTestVoice()
{
    if (!m_serial || !m_serial->isOpen())
    {
        QMessageBox::warning(this, tr("提示"), tr("请先打开串口"));
        return;
    }
    updateDeviceVoiceColorsFromUi();
    sendConfigsToDevice(false);
    const QString text = m_editVoiceTest->text();
    const int style = m_cmbVoiceTestStyle ? m_cmbVoiceTestStyle->currentData().toInt() : 0;
    const QString frame = Protocol::packVoiceTest(text, style);
    if (m_engine) m_engine->logTestTx(frame);
    m_serial->sendFrame(frame);
}


// ================= Colors =================
void MainWindow::onAddColor()
{
    if (!m_settings) return;
    QColor chosen = QColorDialog::getColor(Qt::white, this, tr("选择颜色"));
    if (!chosen.isValid()) return;
    QString err;
    if (!m_colorModel->addColor(chosen, err))
    {
        if (!err.isEmpty())
            QMessageBox::warning(this, tr("提示"), err);
        return;
    }
    m_settings->colors = m_colorModel->colors();
    m_conflictModel->setMaxColorIndex(m_colorModel->rowCount());
}

void MainWindow::onDeleteColor()
{
    if (!m_settings) return;
    const auto sel = m_tblColors->selectionModel() ? m_tblColors->selectionModel()->selectedRows() : QModelIndexList();
    if (sel.isEmpty())
        return;
    const int row = sel.first().row();
    const int removedIdx = row + 1; // 1-based

    if (!m_colorModel->removeRowAt(row))
        return;
    m_settings->colors = m_colorModel->colors();

    // Remap conflicts: deleted index -> 0; above -> -1
    QVector<ConflictTriple> triples = m_conflictModel->triples();
    for (auto& t : triples)
    {
        auto remap = [&](int& v){
            if (v == removedIdx) v = 0;
            else if (v > removedIdx) v -= 1;
        };
        remap(t.c1); remap(t.c2); remap(t.c3);
    }
    m_conflictModel->setMaxColorIndex(m_colorModel->rowCount());
    m_conflictModel->setTriples(triples);
    m_settings->conflicts = m_conflictModel->triples();
}

void MainWindow::onSaveColors()
{
    if (!m_settings) return;
    m_settings->colors = m_colorModel->colors();
    AppSettings::saveColors(m_settings->colors);
}

void MainWindow::onClearColors()
{
    if (!m_settings) return;
    m_colorModel->clearAll();
    m_settings->colors = m_colorModel->colors();

    m_conflictModel->setMaxColorIndex(0);
    m_conflictModel->clearAll();
    m_settings->conflicts.clear();
}

// ================= Conflicts =================
void MainWindow::onAddConflict()
{
    if (!m_settings) return;
    m_conflictModel->insertRow(m_conflictModel->rowCount());
    m_settings->conflicts = m_conflictModel->triples();
}

void MainWindow::onSaveConflicts()
{
    if (!m_settings) return;
    m_settings->conflicts = m_conflictModel->triples();
    AppSettings::saveConflicts(m_settings->conflicts);
}

void MainWindow::onClearConflicts()
{
    if (!m_settings) return;
    m_conflictModel->clearAll();
    m_settings->conflicts.clear();
}

// ================= Engine callbacks =================
void MainWindow::onEngineIdle()
{
    m_uiState = UiRunState::Started;
    applyUiState();
}

void MainWindow::onEngineSegmentStarted(const QString &name, int startRow, int endRow)
{
    Q_UNUSED(endRow);
    m_lblRunState->setText(tr("执行段：%1").arg(name));
    if (m_engine && startRow >= 0 && startRow < m_engine->plan().size())
    {
        const QString flow = m_engine->plan()[startRow].flowName;
        m_currentFlowName = flow;
        m_queueModel->setFlowRunning(flow);
        m_queueModel->setStepRunning(flow, 1);
    }
}

void MainWindow::onEngineProgressUpdated(int currentStep, qint64 deviceMs)
{
    m_lblHint->setText(tr("下位机进度：Step=%1  DeviceMs=%2").arg(currentStep).arg(deviceMs));
    if (!m_currentFlowName.isEmpty())
    {
        m_queueModel->setStepTime(m_currentFlowName, currentStep, deviceMs);
        const int stepCount = m_queueModel->stepCountForFlow(m_currentFlowName);
        if (stepCount > 0 && currentStep >= stepCount)
        {
            m_queueModel->setFlowDone(m_currentFlowName);
            m_currentFlowName.clear();
        }
        else
        {
            m_queueModel->setStepRunning(m_currentFlowName, currentStep + 1);
        }
    }
}

void MainWindow::onEngineRerunMarked(const QString& flowName)
{
    if (!flowName.isEmpty() && m_queueModel)
        m_queueModel->setFlowRerunMarked(flowName);
}

void MainWindow::onEngineLogLine(const QString &line)
{
    qDebug() << line;
}
