#pragma once
/**
 * @file excelimporter.h
 * @brief Excel (.xlsx) importer built on QXlsx. Parses the first sheet into ActionItem rows.
 *
 * Excel rules (summarised):
 * - Only .xlsx is accepted; only the first sheet is read.
 * - Header must contain "工作模式" and LED columns named LED1..LEDn (n <= 10).
 * - Optional columns: BEEP, VOICE, DELAY, 风格 (1/2). Actions are emitted following the header order.
 * - Data rows are parsed as a contiguous block starting from row 2; stop at the first empty row.
 * - Each data row becomes a Segment: one LED action plus optional Beep/Voice/Delay actions sharing the same flowName.
 * - LED cells allow 0/empty to mean "random"; work mode accepts ALL/SEQ/RAND (Chinese aliases allowed).
 */

enum class HeaderField
{
    Mode,
    LED,
    Beep,
    Voice,
    Delay,
    VoiceSet
};

struct HeaderCol
{
    HeaderField field;
    int ledIndex = 0; // for LED columns (1-based); 0 otherwise
};

#include <QObject>
#include <QString>
#include <QVector>

#include "models.h"

class ExcelImporter : public QObject
{
    Q_OBJECT
public:
    explicit ExcelImporter(QObject* parent = nullptr);

    /**
     * @brief Read .xlsx and parse into an action list.
     * @param path   .xlsx file path
     * @param errMsg failure reason (for UI)
     * @return true on success
     */
    bool loadXlsx(const QString& path, QString& errMsg);

    /**
     * @brief Clear imported data.
     */
    void clear();

    /**
     * @brief Parsed actions (order follows header order per row).
     */
    const QVector<ActionItem>& actions() const { return m_actions; }

    /**
     * @brief Parsed header columns in the original order (excluding unknown columns).
     */
    const QVector<HeaderCol>& headerColumns() const { return m_headerCols; }

    /**
     * @brief Whether the imported sheet contains a given action type.
     */
    bool hasActionType(ActionType t) const;

    /**
     * @brief Whether any LED action contains 0 (random).
     */
    bool hasRandomColorZero() const;

    /**
     * @brief Imported Excel path (for logging).
     */
    QString sourcePath() const { return m_sourcePath; }

    /**
     * @brief Number of LED columns detected from the sheet header.
     */
    int ledCount() const { return m_ledColumnCount; }

private:
    QString m_sourcePath;
    QVector<ActionItem> m_actions;
    int m_ledColumnCount = 0;
    QVector<HeaderCol> m_headerCols;
};
