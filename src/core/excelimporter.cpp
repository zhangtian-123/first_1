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
    m_headerCols.clear();
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
static bool isRowEmpty(Document& doc, int row, const QVector<int>& columns)
{
    for (int c : columns)
    {
        const QVariant v = doc.read(row, c);
        if (v.isValid() && !v.toString().trimmed().isEmpty())
            return false;
    }
    return true;
}

static QString joinIntList(const QVector<int>& values)
{
    QStringList tmp;
    tmp.reserve(values.size());
    for (int v : values)
        tmp << QString::number(v);
    return tmp.join(",");
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

    const int firstRow = range.firstRow();   // typically 1
    const int lastRow  = range.lastRow();
    const int firstCol = range.firstColumn();
    const int lastCol  = range.lastColumn();

    // Spec: header must be in row 1.
    if (firstRow != 1)
    {
        errMsg = QStringLiteral("Header must be at row 1 (found first row=%1).").arg(firstRow);
        return false;
    }

    if (lastRow <= firstRow)
    {
        errMsg = QStringLiteral("Excel has no data rows under the header.");
        return false;
    }

    // ----------------------------
    // Read header row (row 1)
    // ----------------------------
    QVector<QString> headers;
    headers.reserve(lastCol - firstCol + 1);
    for (int c = firstCol; c <= lastCol; ++c)
        headers.push_back(doc.read(firstRow, c).toString().trimmed());

    int workModeCol = -1;
    int beepCol = -1;
    int voiceCol = -1;
    int delayCol = -1;
    int voiceSetCol = -1;
    struct LedColInfo { int col; int idx; };
    QVector<LedColInfo> ledCols;
    m_headerCols.clear();

    QRegularExpression ledRegex(QStringLiteral("^led(\\d+)$"), QRegularExpression::CaseInsensitiveOption);

    auto headerKey = [](const QString& h)->QString {
        QString k = h.trimmed().toLower();
        k.replace(" ", "");
        return k;
    };

    for (int idx = 0; idx < headers.size(); ++idx)
    {
        const QString raw = headers[idx];
        const QString key = headerKey(raw);
        const int col = firstCol + idx;

        if (key == QStringLiteral("工作模式") || key == QStringLiteral("mode") || key == QStringLiteral("workmode"))
        {
            workModeCol = col;
            m_headerCols.push_back({HeaderField::Mode, 0});
            continue;
        }

        const auto m = ledRegex.match(raw);
        if (m.hasMatch())
        {
            const int ledIdx = m.captured(1).toInt();
            ledCols.push_back({col, ledIdx});
            m_headerCols.push_back({HeaderField::LED, ledIdx});
            continue;
        }

        if (key == QStringLiteral("beep"))
        {
            beepCol = col;
            m_headerCols.push_back({HeaderField::Beep, 0});
            continue;
        }
        if (key == QStringLiteral("voice"))
        {
            voiceCol = col;
            m_headerCols.push_back({HeaderField::Voice, 0});
            continue;
        }
        if (key == QStringLiteral("delay"))
        {
            delayCol = col;
            m_headerCols.push_back({HeaderField::Delay, 0});
            continue;
        }
        if (key == QStringLiteral("风格")
            || key == QStringLiteral("语音风格")
            || key == QStringLiteral("voiceset")
            || key == QStringLiteral("voiceset1")
            || key == QStringLiteral("voiceset2")
            || key == QStringLiteral("voicestyle")
            || key == QStringLiteral("voice_style")
            || key == QStringLiteral("style"))
        {
            voiceSetCol = col;
            m_headerCols.push_back({HeaderField::VoiceSet, 0});
            continue;
        }
    }

    std::sort(ledCols.begin(), ledCols.end(), [](const LedColInfo& a, const LedColInfo& b){
        return a.idx < b.idx;
    });
    ledCols.erase(std::unique(ledCols.begin(), ledCols.end(), [](const LedColInfo& a, const LedColInfo& b){
        return a.idx == b.idx;
    }), ledCols.end());

    if (workModeCol < 0)
    {
        errMsg = QStringLiteral("Header must contain column \"工作模式\".");
        return false;
    }
    if (ledCols.isEmpty())
    {
        errMsg = QStringLiteral("Header must contain at least one LED column (LED1..LEDn).");
        return false;
    }
    if (ledCols.size() > 10)
    {
        errMsg = QStringLiteral("LED column count exceeds the limit (10).");
        return false;
    }

    // ensure LED indices are 1..n sequential
    for (int i = 0; i < ledCols.size(); ++i)
    {
        if (ledCols[i].idx != i + 1)
        {
            errMsg = QStringLiteral("LED columns must be sequential from LED1..LED%1.").arg(ledCols.size());
            return false;
        }
    }

    m_ledColumnCount = ledCols.size();
    // Columns to check for "empty row" detection
    QVector<int> checkCols;
    for (const auto& lc : ledCols) checkCols << lc.col;
    checkCols << workModeCol;
    if (beepCol >= 0) checkCols << beepCol;
    if (voiceCol >= 0) checkCols << voiceCol;
    if (delayCol >= 0) checkCols << delayCol;
    if (voiceSetCol >= 0) checkCols << voiceSetCol;

    auto readString = [&](int row, int col)->QString {
        const QVariant v = doc.read(row, col);
        if (!v.isValid())
            return QString();
        return v.toString().trimmed();
    };

    // ----------------------------
    // Parse each data row (contiguous block starting at row 2)
    // ----------------------------
    int dataRowIndex = 0;
    for (int r = firstRow + 1; r <= lastRow; ++r)
    {
        if (isRowEmpty(doc, r, checkCols))
        {
            // Stop at the first empty row; ignore anything below.
            break;
        }

        const QString modeCell = readString(r, workModeCol);
        if (modeCell.isEmpty())
        {
            errMsg = QStringLiteral("Row %1: 工作模式 is empty.").arg(r);
            return false;
        }

        QString mode = normalizeLedMode(modeCell).toUpper();
        if (mode != QStringLiteral("ALL") && mode != QStringLiteral("SEQ") && mode != QStringLiteral("RAND"))
        {
            errMsg = QStringLiteral("Row %1: 工作模式 \"%2\" is invalid (must be ALL/SEQ/RAND).")
                         .arg(r)
                         .arg(modeCell);
            return false;
        }

        QVector<int> ledColors;
        ledColors.reserve(ledCols.size());
        for (const auto& lc : ledCols)
        {
            const QString text = readString(r, lc.col);
            if (text.isEmpty())
            {
                ledColors.push_back(0);
                continue;
            }
            bool ok=false;
            const int v = text.toInt(&ok);
            if (!ok || v < 0)
            {
                errMsg = QStringLiteral("Row %1: LED column contains invalid value \"%2\" (must be >=0).")
                             .arg(r)
                             .arg(text);
                return false;
            }
                ledColors.push_back(v);
        }

        const QString flowName = QStringLiteral("Row%1").arg(++dataRowIndex);

        int voiceSet = 1;
        if (voiceSetCol >= 0)
        {
            const QString vsText = readString(r, voiceSetCol);
            if (!vsText.isEmpty())
            {
                bool ok = false;
                const int v = vsText.toInt(&ok);
                if (!ok || (v != 1 && v != 2))
                {
                    errMsg = QStringLiteral("Row %1: 风格 value \"%2\" is invalid (must be 1 or 2).")
                                 .arg(r)
                                 .arg(vsText);
                    return false;
                }
                voiceSet = v;
            }
        }

        ActionItem ledAction;
        ledAction.flowName = flowName;
        ledAction.type = ActionType::L;
        ledAction.ledMode = mode;
        ledAction.ledColors = ledColors;
        ledAction.rawParamText = QStringLiteral("mode=%1 colors=%2").arg(mode, joinIntList(ledColors));
        ledAction.voiceSet = voiceSet;
        // optional actions
        bool hasBeepAction = false;
        ActionItem beepAction;
        bool hasVoiceAction = false;
        ActionItem voiceAction;
        bool hasDelayAction = false;
        ActionItem delayAction;

        if (beepCol >= 0)
        {
            const QString beepText = readString(r, beepCol);
            if (!beepText.isEmpty())
            {
                bool ok=false;
                const int beepVal = beepText.toInt(&ok);
                if (!ok || beepVal < 0)
                {
                    errMsg = QStringLiteral("Row %1: BEEP value \"%2\" is invalid (must be >=0).")
                                 .arg(r)
                                 .arg(beepText);
                    return false;
                }
                if (beepVal > 0)
                {
                    hasBeepAction = true;
                    beepAction.flowName = flowName;
                    beepAction.type = ActionType::B;
                    beepAction.beepFreqHz = 0;   // Device property driven; importer only keeps duration flag/value.
                    // 1 表示采用设备默认时长；其他>1 表示具体毫秒
                    beepAction.beepDurMs  = (beepVal == 1 ? 0 : beepVal);
                    beepAction.rawParamText = beepText;
                    beepAction.voiceSet = voiceSet;
                }
            }
        }

        if (voiceCol >= 0)
        {
            const QString voiceText = readString(r, voiceCol);
            if (!voiceText.isEmpty())
            {
                hasVoiceAction = true;
                voiceAction.flowName = flowName;
                voiceAction.type = ActionType::V;
                voiceAction.voiceText = voiceText;
                voiceAction.voiceMs = 0;
                voiceAction.rawParamText = voiceText;
                voiceAction.voiceSet = voiceSet;
            }
        }

        if (delayCol >= 0)
        {
            const QString delayText = readString(r, delayCol);
            if (!delayText.isEmpty())
            {
                bool ok=false;
                const int delay = delayText.toInt(&ok);
                if (!ok || delay < 0)
                {
                    errMsg = QStringLiteral("Row %1: DELAY value \"%2\" is invalid (must be >=0).")
                                 .arg(r)
                                 .arg(delayText);
                    return false;
                }

                hasDelayAction = true;
                delayAction.flowName = flowName;
                delayAction.type = ActionType::D;
                delayAction.delayMs = delay;
                delayAction.rawParamText = delayText;
                delayAction.voiceSet = voiceSet;
            }
        }

        // emit in header order
        bool ledPushed = false;
        for (const auto& hc : m_headerCols)
        {
            switch (hc.field)
            {
            case HeaderField::Mode:
                // 工作模式仅用于 LED 动作，实际动作在 LED 行里体现
                break;
            case HeaderField::LED:
                if (!ledPushed)
                {
                    m_actions.push_back(ledAction);
                    ledPushed = true;
                }
                break;
            case HeaderField::Beep:
                if (hasBeepAction)
                    m_actions.push_back(beepAction);
                break;
            case HeaderField::Voice:
                if (hasVoiceAction)
                    m_actions.push_back(voiceAction);
                break;
            case HeaderField::Delay:
                if (hasDelayAction)
                    m_actions.push_back(delayAction);
                break;
            case HeaderField::VoiceSet:
                // style column is a row attribute, not an action
                break;
            }
        }
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
