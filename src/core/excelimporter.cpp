#include "excelimporter.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>
#include <algorithm>
#include <QVariant>

// ----------------------------------------------------------------------------
// QXlsx compatibility includes
// ----------------------------------------------------------------------------
#if __has_include(<QXlsx/Document>)
    #include <QXlsx/Document>
    #include <QXlsx/CellRange>
    #include <QXlsx/Worksheet>
    using QXlsx::Document;
    using QXlsx::CellRange;
    using QXlsx::Worksheet;
#elif __has_include("xlsxdocument.h")
    #include "xlsxdocument.h"
    #include "xlsxcellrange.h"
    #include "xlsxworksheet.h"
    using QXlsx::Document;
    using QXlsx::CellRange;
    using QXlsx::Worksheet;
#else
    #error "QXlsx headers not found. Please ensure QXlsx is added and include path is correct."
#endif

ExcelImporter::ExcelImporter(QObject *parent)
    : QObject(parent)
{
}

void ExcelImporter::clear()
{
    m_sourcePath.clear();
    m_actions.clear();
    m_ledColumnCount = 0;
    m_tableColumnStart = 1;
    m_tableColumnCount = 0;
    m_tableRows.clear();
}

bool ExcelImporter::hasActionType(ActionType t) const
{
    for (const auto& a : m_actions)
    {
        if (a.type == t)
            return true;
    }
    return false;
}

bool ExcelImporter::hasRandomColorZero() const
{
    for (const auto& a : m_actions)
    {
        if (a.type != ActionType::L)
            continue;
        for (int c : a.ledColors)
        {
            if (c == 0)
                return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------
static bool isRowEmpty(Document& doc, int row, int firstCol, int lastCol)
{
    for (int c = firstCol; c <= lastCol; ++c)
    {
        const QVariant v = doc.read(row, c);
        if (v.isValid() && !v.toString().trimmed().isEmpty())
            return false;
    }
    return true;
}

static QVector<QString> readRowCells(Document& doc, int row, int firstCol, int lastCol)
{
    QVector<QString> cells;
    cells.reserve(lastCol - firstCol + 1);
    for (int c = firstCol; c <= lastCol; ++c)
        cells.push_back(doc.read(row, c).toString().trimmed());
    return cells;
}

static QString joinIntList(const QVector<int>& values)
{
    QStringList tmp;
    tmp.reserve(values.size());
    for (int v : values)
        tmp << QString::number(v);
    return tmp.join(",");
}

static QString headerKey(const QString& text)
{
    QString k = text.trimmed().toLower();
    k.remove(' ');
    return k;
}

static bool isTokenLed(const QString& text)
{
    return text.trimmed() == QStringLiteral("LED工作模式");
}

static bool isTokenBeep(const QString& text)
{
    return headerKey(text) == QStringLiteral("beep");
}

static bool isTokenVoice(const QString& text)
{
    return headerKey(text) == QStringLiteral("voice");
}

static bool isTokenDelay(const QString& text)
{
    return headerKey(text) == QStringLiteral("delay");
}

static bool isTokenMode(const QString& text)
{
    const QString t = text.trimmed();
    return t == QStringLiteral("工作模式");
}

static bool isTokenStyle(const QString& text)
{
    const QString t = text.trimmed();
    if (t == QStringLiteral("风格") || t == QStringLiteral("语音风格"))
        return true;
    const QString k = headerKey(text);
    return (k == QStringLiteral("voiceset")
            || k == QStringLiteral("voiceset1")
            || k == QStringLiteral("voiceset2")
            || k == QStringLiteral("voicestyle")
            || k == QStringLiteral("voice_style")
            || k == QStringLiteral("style"));
}

static bool isHeaderRow(const QVector<QString>& cells)
{
    QRegularExpression ledRegex(QStringLiteral("^LED\\d+$"), QRegularExpression::CaseInsensitiveOption);
    for (const auto& cell : cells)
    {
        if (cell.trimmed().isEmpty())
            continue;
        if (isTokenLed(cell) || isTokenBeep(cell) || isTokenVoice(cell)
            || isTokenDelay(cell) || isTokenStyle(cell))
            return true;
        if (ledRegex.match(cell).hasMatch())
            return true;
    }
    return false;
}

namespace
{
enum class BlockType
{
    Led,
    Beep,
    Voice,
    Delay
};

struct BlockDef
{
    BlockType type = BlockType::Led;
    int ledMarkerCol = -1;
    int modeCol = -1;
    QVector<int> ledCols;
    int beepCol = -1;
    int voiceCol = -1;
    int voiceStyleCol = -1;
    int delayCol = -1;
};

struct HeaderDef
{
    QVector<BlockDef> blocks;
    QVector<int> ledCols; // absolute Excel columns (1-based), all LED columns across blocks
};
}

static bool parseHeaderRow(const QVector<QString>& cells,
                           int firstCol,
                           int lastCol,
                           HeaderDef& out,
                           QString& errMsg)
{
    out = HeaderDef();
    errMsg.clear();

    QRegularExpression ledRegex(QStringLiteral("^LED(\\d+)$"), QRegularExpression::CaseInsensitiveOption);

    int col = firstCol;
    while (col <= lastCol)
    {
        const QString text = cells[col - firstCol].trimmed();
        if (text.isEmpty())
        {
            ++col;
            continue;
        }

        if (isTokenLed(text))
        {
            const int modeCol = col;

            QVector<int> ledCols;
            int scan = col + 1;
            while (scan <= lastCol)
            {
                const QString h = cells[scan - firstCol].trimmed();
                if (h.isEmpty())
                    break;
                if (isTokenLed(h) || isTokenBeep(h) || isTokenVoice(h)
                    || isTokenDelay(h) || isTokenMode(h) || isTokenStyle(h))
                    break;
                const auto m = ledRegex.match(h);
                if (!m.hasMatch())
                    break;
                ledCols.push_back(scan);
                ++scan;
            }

            if (ledCols.isEmpty())
            {
                errMsg = QStringLiteral("LED工作模式 must be followed by LED1..LEDn columns.");
                return false;
            }

            for (int i = 0; i < ledCols.size(); ++i)
            {
                const QString h = cells[ledCols[i] - firstCol].trimmed();
                const auto m = ledRegex.match(h);
                const int idx = m.hasMatch() ? m.captured(1).toInt() : -1;
                if (idx != i + 1)
                {
                    errMsg = QStringLiteral("LED columns must be sequential from LED1.");
                    return false;
                }
            }

            BlockDef b;
            b.type = BlockType::Led;
            b.ledMarkerCol = col;
            b.modeCol = modeCol;
            b.ledCols = ledCols;
            out.blocks.push_back(b);
            for (int c : ledCols)
                out.ledCols.push_back(c);
            col = scan;
            continue;
        }

        if (isTokenBeep(text))
        {
            BlockDef b;
            b.type = BlockType::Beep;
            b.beepCol = col;
            out.blocks.push_back(b);
            ++col;
            continue;
        }

        if (isTokenVoice(text))
        {
            const int styleCol = col + 1;
            if (styleCol > lastCol || !isTokenStyle(cells[styleCol - firstCol]))
            {
                errMsg = QStringLiteral("VOICE block must be followed by 风格.");
                return false;
            }
            BlockDef b;
            b.type = BlockType::Voice;
            b.voiceCol = col;
            b.voiceStyleCol = styleCol;
            out.blocks.push_back(b);
            col = styleCol + 1;
            continue;
        }

        if (isTokenDelay(text))
        {
            BlockDef b;
            b.type = BlockType::Delay;
            b.delayCol = col;
            out.blocks.push_back(b);
            ++col;
            continue;
        }

        if (isTokenMode(text) || isTokenStyle(text) || ledRegex.match(text).hasMatch())
        {
            errMsg = QStringLiteral("Header row has misplaced LED/VOICE columns.");
            return false;
        }

        ++col; // ignore unknown header text
    }

    if (out.blocks.isEmpty())
    {
        errMsg = QStringLiteral("Header row has no valid blocks.");
        return false;
    }
    return true;
}

// ----------------------------------------------------------------------------
// Main entry
// ----------------------------------------------------------------------------
bool ExcelImporter::loadXlsx(const QString &path, QString &errMsg)
{
    errMsg.clear();
    clear();

    QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile())
    {
        errMsg = QStringLiteral("File not found: %1").arg(path);
        return false;
    }
    if (fi.suffix().compare(QStringLiteral("xlsx"), Qt::CaseInsensitive) != 0)
    {
        errMsg = QStringLiteral("Only .xlsx is supported: %1").arg(path);
        return false;
    }

    Document doc(path);
    if (!doc.load())
    {
        errMsg = QStringLiteral("Failed to open Excel (maybe locked or corrupted): %1").arg(path);
        return false;
    }

    // Always use the first sheet
    bool sheetOk = doc.selectSheet(0);
    if (!sheetOk)
    {
        const QStringList names = doc.sheetNames();
        if (!names.isEmpty())
            sheetOk = doc.selectSheet(names.first());
    }
    if (!sheetOk)
    {
        errMsg = QStringLiteral("Failed to select the first sheet.");
        return false;
    }

    Worksheet* ws = doc.currentWorksheet();
    if (!ws)
    {
        errMsg = QStringLiteral("No worksheet available.");
        return false;
    }

    if (!ws->mergedCells().isEmpty())
    {
        errMsg = QStringLiteral("Merged cells are not allowed in the sheet.");
        return false;
    }

    const CellRange range = doc.dimension();
    if (!range.isValid())
    {
        errMsg = QStringLiteral("Excel sheet is empty (invalid dimension).");
        return false;
    }

    const int firstRow = range.firstRow();
    const int lastRow  = range.lastRow();
    const int firstCol = range.firstColumn();
    const int lastCol  = range.lastColumn();

    m_tableColumnStart = firstCol;
    m_tableColumnCount = lastCol - firstCol + 1;

    HeaderDef currentHeader;
    bool hasHeader = false;
    int dataRowIndex = 0;
    int maxDisplayCols = 0;

    auto buildDisplayCells = [&](const QVector<QString>& rawCells,
                                 const HeaderDef& header,
                                 bool isHeaderRow,
                                 QVector<int>* outRawToDisplay,
                                 QVector<int>* outTimeCols)->QVector<QString>
    {
        QVector<int> blockEnds;
        blockEnds.reserve(header.blocks.size());
        for (const auto& block : header.blocks)
        {
            int endCol = -1;
            switch (block.type)
            {
            case BlockType::Led:
                endCol = block.ledCols.isEmpty() ? block.modeCol : block.ledCols.last();
                break;
            case BlockType::Beep:
                endCol = block.beepCol;
                break;
            case BlockType::Voice:
                endCol = block.voiceStyleCol;
                break;
            case BlockType::Delay:
                endCol = block.delayCol;
                break;
            }
            if (endCol > 0)
                blockEnds.push_back(endCol);
        }

        QVector<QString> display;
        display.reserve(rawCells.size() + blockEnds.size());
        if (outRawToDisplay)
        {
            outRawToDisplay->clear();
            outRawToDisplay->resize(rawCells.size());
        }
        if (outTimeCols)
            outTimeCols->clear();

        int blockIdx = 0;
        for (int rawIdx = 0; rawIdx < rawCells.size(); ++rawIdx)
        {
            display.push_back(rawCells[rawIdx]);
            if (outRawToDisplay)
                (*outRawToDisplay)[rawIdx] = display.size() - 1;

            const int absCol = firstCol + rawIdx;
            if (blockIdx < blockEnds.size() && absCol == blockEnds[blockIdx])
            {
                display.push_back(isHeaderRow ? QStringLiteral("时间") : QString());
                if (outTimeCols)
                    outTimeCols->push_back(display.size() - 1);
                ++blockIdx;
            }
        }
        return display;
    };

    for (int r = firstRow; r <= lastRow; ++r)
    {
        if (isRowEmpty(doc, r, firstCol, lastCol))
            break;

        const QVector<QString> rawCells = readRowCells(doc, r, firstCol, lastCol);
        if (isHeaderRow(rawCells))
        {
            HeaderDef parsed;
            QString headerErr;
            if (!parseHeaderRow(rawCells, firstCol, lastCol, parsed, headerErr))
            {
                errMsg = QStringLiteral("Row %1: %2").arg(r).arg(headerErr);
                return false;
            }
            int headerLedMax = 0;
            for (const auto& block : parsed.blocks)
            {
                if (block.type != BlockType::Led)
                    continue;
                const int count = block.ledCols.size();
                if (count > 20)
                {
                    errMsg = QStringLiteral("Row %1: LED column count exceeds the limit (20).").arg(r);
                    return false;
                }
                headerLedMax = std::max(headerLedMax, count);
            }

            ExcelTableRow row;
            row.isHeader = true;
            row.excelRow = r;
            row.cells = buildDisplayCells(rawCells, parsed, true, nullptr, &row.timeColumns);
            m_tableRows.push_back(row);

            currentHeader = parsed;
            hasHeader = true;
            m_ledColumnCount = std::max(m_ledColumnCount, headerLedMax);
            int lastNonEmpty = -1;
            for (int i = row.cells.size() - 1; i >= 0; --i)
            {
                if (!row.cells[i].trimmed().isEmpty())
                {
                    lastNonEmpty = i;
                    break;
                }
            }
            if (lastNonEmpty >= 0)
                maxDisplayCols = std::max(maxDisplayCols, lastNonEmpty + 1);
            continue;
        }

        if (!hasHeader)
        {
            errMsg = QStringLiteral("Row %1: data row appears before any header row.").arg(r);
            return false;
        }

        const QString flowName = QStringLiteral("流程%1").arg(++dataRowIndex);

        ExcelTableRow row;
        row.isHeader = false;
        row.excelRow = r;
        row.flowName = flowName;
        QVector<int> rawToDisplay;
        QVector<int> blockTimeCols;
        row.cells = buildDisplayCells(rawCells, currentHeader, false, &rawToDisplay, &blockTimeCols);
        row.ledColumns.reserve(currentHeader.ledCols.size());
        for (int col : currentHeader.ledCols)
        {
            const int rawIdx = col - firstCol;
            if (rawIdx >= 0 && rawIdx < rawToDisplay.size())
                row.ledColumns.push_back(rawToDisplay[rawIdx]);
        }

        auto cellAt = [&](int col)->QString {
            if (col < firstCol || col > lastCol)
                return QString();
            return rawCells[col - firstCol].trimmed();
        };

        for (int bi = 0; bi < currentHeader.blocks.size(); ++bi)
        {
            const auto& block = currentHeader.blocks[bi];
            switch (block.type)
            {
            case BlockType::Led:
            {
                const QString modeCell = cellAt(block.modeCol);
                if (modeCell.isEmpty())
                {
                    errMsg = QStringLiteral("Row %1: work mode is empty.").arg(r);
                    return false;
                }

                const QString mode = normalizeLedMode(modeCell).toUpper();
                if (mode != QStringLiteral("ALL") && mode != QStringLiteral("SEQ") && mode != QStringLiteral("RAND"))
                {
                    errMsg = QStringLiteral("Row %1: work mode \"%2\" is invalid (must be ALL/SEQ/RAND).").arg(r).arg(modeCell);
                    return false;
                }

                QVector<int> ledColors;
                ledColors.reserve(block.ledCols.size());
                for (int col : block.ledCols)
                {
                    const QString text = cellAt(col);
                    if (text.isEmpty())
                    {
                        ledColors.push_back(0);
                        continue;
                    }
                    bool ok=false;
                    const int v = text.toInt(&ok);
                    if (!ok || v < 0)
                    {
                        errMsg = QStringLiteral("Row %1: LED value \"%2\" is invalid (must be >=0).").arg(r).arg(text);
                        return false;
                    }
                    ledColors.push_back(v);
                }

                ActionItem ledAction;
                ledAction.flowName = flowName;
                ledAction.type = ActionType::L;
                ledAction.ledMode = mode;
                ledAction.ledColors = ledColors;
                ledAction.rawParamText = QStringLiteral("mode=%1 colors=%2").arg(mode, joinIntList(ledColors));
                m_actions.push_back(ledAction);
                row.timeColumns.push_back(blockTimeCols.value(bi, -1));
                break;
            }
            case BlockType::Beep:
            {
                ActionItem beepAction;
                beepAction.flowName = flowName;
                beepAction.type = ActionType::B;
                beepAction.rawParamText = QStringLiteral("BEEP");
                m_actions.push_back(beepAction);
                row.timeColumns.push_back(blockTimeCols.value(bi, -1));
                break;
            }
            case BlockType::Voice:
            {
                const QString voiceText = cellAt(block.voiceCol);
                if (voiceText.isEmpty())
                {
                    errMsg = QStringLiteral("Row %1: VOICE text is empty.").arg(r);
                    return false;
                }

                const QString styleText = cellAt(block.voiceStyleCol);
                if (styleText.isEmpty())
                {
                    errMsg = QStringLiteral("Row %1: style is empty.").arg(r);
                    return false;
                }
                bool ok=false;
                const int style = styleText.toInt(&ok);
                if (!ok || (style != 1 && style != 2))
                {
                    errMsg = QStringLiteral("Row %1: style value \"%2\" is invalid (must be 1 or 2).").arg(r).arg(styleText);
                    return false;
                }

                ActionItem voiceAction;
                voiceAction.flowName = flowName;
                voiceAction.type = ActionType::V;
                voiceAction.voiceText = voiceText;
                voiceAction.rawParamText = voiceText;
                voiceAction.voiceSet = style;
                m_actions.push_back(voiceAction);
                row.timeColumns.push_back(blockTimeCols.value(bi, -1));
                break;
            }
            case BlockType::Delay:
            {
                const QString delayText = cellAt(block.delayCol);
                if (delayText.isEmpty())
                    break;

                bool ok=false;
                const int delay = delayText.toInt(&ok);
                if (!ok || delay < 0)
                {
                    errMsg = QStringLiteral("Row %1: DELAY value \"%2\" is invalid (must be >=0).").arg(r).arg(delayText);
                    return false;
                }

                ActionItem delayAction;
                delayAction.flowName = flowName;
                delayAction.type = ActionType::D;
                delayAction.delayMs = delay;
                delayAction.rawParamText = delayText;
                m_actions.push_back(delayAction);
                row.timeColumns.push_back(blockTimeCols.value(bi, -1));
                break;
            }
            }
        }

        int lastNonEmpty = -1;
        for (int i = row.cells.size() - 1; i >= 0; --i)
        {
            if (!row.cells[i].trimmed().isEmpty())
            {
                lastNonEmpty = i;
                break;
            }
        }
        if (lastNonEmpty >= 0)
            maxDisplayCols = std::max(maxDisplayCols, lastNonEmpty + 1);
        m_tableRows.push_back(row);
    }

    if (maxDisplayCols < 0)
        maxDisplayCols = 0;
    m_tableColumnCount = maxDisplayCols;
    for (auto& row : m_tableRows)
    {
        if (row.cells.size() > m_tableColumnCount)
            row.cells.resize(m_tableColumnCount);
    }

    if (!hasHeader)
    {
        errMsg = QStringLiteral("Excel has no header rows.");
        return false;
    }
    if (dataRowIndex <= 0)
    {
        errMsg = QStringLiteral("Excel has no data rows under the header.");
        return false;
    }
    if (m_actions.isEmpty())
    {
        errMsg = QStringLiteral("No valid actions were parsed from the Excel sheet.");
        return false;
    }

    m_sourcePath = path;
    return true;
}
