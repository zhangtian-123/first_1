#pragma once
/**
 * @file excelimporter.h
 * @brief Excel (.xlsx) importer built on QXlsx. Parses the first sheet into ActionItem rows.
 *
 * Excel rules (summarised):
 * - Only .xlsx is accepted; only the first sheet is read.
 * - A header row can appear multiple times. Any row containing header tokens
 *   (LED工作模式/BEEP/VOICE/风格/DELAY/LEDn) is treated as a header row.
 * - Data rows belong to the most recent header row; parsing stops at the first empty row.
 * - Header order defines the WORK action order for that block.
 * - LED block header is: LED工作模式 | LED1..LEDn (n is dynamic per block).
 * - BEEP is 1 column; VOICE is 2 columns (VOICE + 风格); DELAY is 1 column.
 * - LED cells allow 0/empty to mean "random"; work mode accepts ALL/SEQ/RAND (Chinese aliases allowed).
 */

#include <QObject>
#include <QString>
#include <QVector>

#include "models.h"

struct ExcelTableRow
{
    bool isHeader = false;
    int excelRow = 0;                 ///< 1-based Excel row index
    QString flowName;                 ///< for data rows
    QVector<QString> cells;           ///< trimmed cell text per column
    QVector<int> ledColumns;          ///< 0-based indices into cells for LED1..LEDn
    QVector<int> timeColumns;         ///< 0-based indices into cells for step times
};

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
     * @brief Parsed rows for table display (including header rows).
     */
    const QVector<ExcelTableRow>& tableRows() const { return m_tableRows; }

    /**
     * @brief Excel column start (1-based) for table display.
     */
    int tableColumnStart() const { return m_tableColumnStart; }

    /**
     * @brief Excel column count for table display.
     */
    int tableColumnCount() const { return m_tableColumnCount; }

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
     * @brief Max LED columns detected across all header blocks.
     */
    int ledCount() const { return m_ledColumnCount; }

private:
    QString m_sourcePath;
    QVector<ActionItem> m_actions;
    int m_ledColumnCount = 0;
    int m_tableColumnStart = 1;
    int m_tableColumnCount = 0;
    QVector<ExcelTableRow> m_tableRows;
};
