#pragma once
/**
 * @brief Main window: Status (Excel-driven queue) and Settings.
 */

#include <QMainWindow>
#include <QPointer>
#include <QVector>
#include <QKeySequence>
#include <QStringList>

#include "src/config/appsettings.h"

class QTabWidget;
class QLineEdit;
class QPushButton;
class QTableView;
class QLabel;
class QComboBox;
class QSpinBox;
class QKeySequenceEdit;
class QShortcut;

class SerialService;
class WorkflowEngine;
class ExcelImporter;
class QueueTableModel;
class ColorTableModel;
class ConflictTableModel;
class ConflictColorDelegate;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    // Status page
    void onPickExcel();
    void onApplyConfig();
    void onStart();
    void onNext();
    void onMarkRerun();
    void onReset();

    // Settings: serial
    void onRefreshPorts();
    void onOpenCloseSerial();
    void onPortsUpdated(const QStringList& ports);
    void onPortSelectionChanged(const QString& port);

    // Settings: device
    void onSaveDevice();
    void onSaveVoice();
    void onApplySettings();

    // Settings: colors
    void onAddColor();
    void onDeleteColor();
    void onSaveColors();
    void onClearColors();

    // Settings: conflicts
    void onAddConflict();
    void onSaveConflicts();
    void onClearConflicts();

    // Hotkeys
    void onSaveHotkeys();
    void onHotkeyNext();
    void onHotkeyRerun();
    void onHotkeyQuickColor1();
    void onHotkeyQuickColor2();
    void onHotkeyQuickColor3();
    void onHotkeyQuickColor4();
    void onHotkeyQuickColor5();
    void onHotkeyQuickColor6();
    void onHotkeyQuickColor7();
    void onHotkeyAllOff();

    // Tests
    void onTestLed();
    void onTestBeep();
    void onTestVoice();

    // Serial callbacks
    void onSerialOpened(bool ok, const QString& err);
    void onSerialClosed();
    void onSerialError(const QString& err);

    // Engine callbacks
    void onEngineIdle();
    void onEngineSegmentStarted(const QString& name, int startRow, int endRow);
    void onEngineActionFinished(int row, bool ok, int code, const QString& msg);
    void onEngineProgressUpdated(int currentStep, qint64 deviceMs);
    void onEngineLogLine(const QString& line);

private:
    void buildUi();
    QWidget* buildStatusPage();
    QWidget* buildSettingsPage();
    void wireSignals();
    void applyUiState();

    bool loadSettings();
    void saveSettings();
    bool importExcel(const QString& path, QString& err);
    bool precheckBeforeStart(QString& err);
    void rebuildShortcuts();
    void enableTestHotkeys(bool enable);
    void updateHotkeyDuplicateHints();
    bool collectHotkeys(HotkeyConfig& hk, bool showWarning);
    void updateDeviceVoiceColorsFromUi();
    void sendConfigsToDevice(bool warnIfSerialClosed);
    bool eventFilter(QObject* obj, QEvent* event) override;
    void sendTestSolidColor(int idx);
    void sendTestAllOff();
    HotkeyConfig readHotkeysFromUi() const;
    QKeySequence readKeySequenceFromEdit(const QKeySequenceEdit* edit) const;

private:
    enum class UiRunState { NoConfig, Ready, Started, Running };
    UiRunState m_uiState = UiRunState::NoConfig;

    SettingsData* m_settings = nullptr;
    QString m_excelPath;
    bool m_configApplied = false;

    QPointer<SerialService>  m_serial;
    QPointer<WorkflowEngine> m_engine;
    QPointer<ExcelImporter>  m_importer;

    // Status page widgets
    QTabWidget* m_tabs = nullptr;
    QLineEdit*  m_editExcelPath = nullptr;
    QPushButton* m_btnPickExcel = nullptr;
    QPushButton* m_btnApplyConfig = nullptr;
    QPushButton* m_btnStart = nullptr;
    QPushButton* m_btnNext  = nullptr;
    QPushButton* m_btnMarkRerun = nullptr;
    QPushButton* m_btnReset = nullptr;
    QTableView* m_tblQueue = nullptr;
    QLabel* m_lblRunState = nullptr;
    QLabel* m_lblHint = nullptr;

    // Serial widgets
    QComboBox* m_cmbPort = nullptr;
    QComboBox* m_cmbBaud = nullptr;
    QComboBox* m_cmbDataBits = nullptr;
    QComboBox* m_cmbParity = nullptr;
    QComboBox* m_cmbStopBits = nullptr;
    QPushButton* m_btnRefreshPorts = nullptr;
    QPushButton* m_btnOpenClose = nullptr;

    // Device widgets
    QSpinBox* m_spOnMs = nullptr;
    QSpinBox* m_spGapMs = nullptr;
    QSpinBox* m_spLedCount = nullptr;
    QSpinBox* m_spBrightness = nullptr;
    QSpinBox* m_spBuzzerFreq = nullptr;
    QSpinBox* m_spBuzzerDur = nullptr;

    // Voice sets
    QComboBox* m_cmbVoice1Announcer = nullptr;
    QSpinBox* m_spVoice1Style = nullptr;
    QSpinBox* m_spVoice1Speed = nullptr;
    QSpinBox* m_spVoice1Pitch = nullptr;
    QSpinBox* m_spVoice1Volume = nullptr;
    QComboBox* m_cmbVoice2Announcer = nullptr;
    QSpinBox* m_spVoice2Style = nullptr;
    QSpinBox* m_spVoice2Speed = nullptr;
    QSpinBox* m_spVoice2Pitch = nullptr;
    QSpinBox* m_spVoice2Volume = nullptr;

    // Colors
    QTableView* m_tblColors = nullptr;
    QPushButton* m_btnAddColor = nullptr;
    QPushButton* m_btnDeleteColor = nullptr;
    QPushButton* m_btnClearColors = nullptr;

    // Conflicts
    QTableView* m_tblConflicts = nullptr;
    QPushButton* m_btnAddConflict = nullptr;
    QPushButton* m_btnClearConflicts = nullptr;

    QPushButton* m_btnApplySettings = nullptr;

    // Hotkeys
    QKeySequenceEdit* m_keyNext = nullptr;
    QKeySequenceEdit* m_keyRerun = nullptr;
    QVector<QKeySequenceEdit*> m_keyQuickColor;
    QKeySequenceEdit* m_keyAllOff = nullptr;
    QVector<QShortcut*> m_shortcuts;
    QVector<QShortcut*> m_testShortcuts;
    bool m_hotkeyUpdateGuard = false;
    bool m_hotkeyAutoSaveEnabled = false;

    // Tests
    QLineEdit* m_editLedTest = nullptr;
    QLineEdit* m_editVoiceTest = nullptr;
    QComboBox* m_cmbVoiceTestStyle = nullptr;
    QPushButton* m_btnTestLed = nullptr;
    QPushButton* m_btnTestBeep = nullptr;
    QPushButton* m_btnTestVoice = nullptr;

    // Models / delegates
    QueueTableModel* m_queueModel = nullptr;
    ColorTableModel* m_colorModel = nullptr;
    ConflictTableModel* m_conflictModel = nullptr;
    ConflictColorDelegate* m_conflictDelegate = nullptr;

protected:
};
