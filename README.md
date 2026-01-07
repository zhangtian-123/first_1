# VisualWorkflowHost (Qt6 Widgets + C++17)

桌面上位机（Windows 优先，尽量跨平台），用于导入 `.xlsx` 生成工作队列（Segment 列表），通过串口向下位机下发配置/测试/逐段执行指令，并接收 `SETPRUN` 进度回报。

## 依赖

- Qt 6.5+（Modules: `Widgets`, `SerialPort`, `TextToSpeech`, `Xml`）
- CMake 3.19+
- 编译器：MSVC 或 MinGW
- Excel 解析：本仓库已包含 `QXlsx/` 源码（无需额外安装）

## 构建

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

运行后，配置会写入可执行文件同目录的 `config.ini`，日志写入 `logs/` 目录。

## Excel 模板与规则（必须严格遵守）

- 仅支持 `.xlsx`
- 只读取第 1 个 sheet
- 表头必须在第 1 行
- 数据行必须连续，遇到空行即停止读取（空行以下数据将被忽略）
- 不允许合并单元格

### 表头（Header）规则

必须包含：

- `工作模式`
- `LED1 ... LEDn`（n 由 Excel 表头实际出现决定，且 `n <= 10`，必须从 `LED1` 连续到 `LEDn`）

可选列：

- `BEEP`
- `VOICE`
- `DELAY`
- `风格`（1 或 2，用于选择 VOICESET1/VOICESET2）

状态页表格列按 Excel 表头从左到右生成（也等价于动作顺序按表头从左到右决定）。

### 每行（Segment）取值规则

- `工作模式`：`ALL | SEQ | RAND`
- `LEDk`：颜色编号（从 1 开始）；填 `0` 或空表示随机生成颜色编号
- `DELAY`：毫秒整数（例如 `1000`）
- `VOICE`：文本会转为 GB2312 字节并以 16 进制（空格分隔）发送
- `风格`：`1` 或 `2`，表示该段语音使用 `VOICESET1` 或 `VOICESET2`
- `BEEP`（兼容两种）：
  - 方案 A：填毫秒整数 `durationMs`（`0/空` 表示本段无 BEEP）
  - 方案 B：填 `1/0` 表示是否 BEEP（`1` 表示本段发送 `BEEP`，持续时长使用设备属性下发的配置）

## 串口协议（概览）

所有指令以 `\r\n` 结束：

- 配置下发：
  - `LEDSET:<ledCount>,<onMs>,<intervalMs>,<brightness>,<colorCount>,<hex1>,...`
  - `VOICESET1:<announcer>,<style>,<speed>,<pitch>,<volume>`
  - `VOICESET2:<announcer>,<style>,<speed>,<pitch>,<volume>`
  - `BEEPSET;<durationMs>,<freqHz>`
- 测试：
  - `LEDTEST;<colorIndex>`（`0` 表示全灭）
  - `VOICETEST:<gb2312_hex_bytes_with_spaces>,<style>`
  - `BEEP`（触发一次蜂鸣，具体时长/频率由 `BEEPSET` 决定）
- 执行：
  - `WORK:<action1>;<action2>;...;`
  - `LED` 动作：`LED,<order1..N>,<color1..N>`（`RAND` 时 order 为 1..N 的随机排列）
  - `DELAY` 动作：`DELAY,<ms>`
  - `VOICE` 动作：`VOICE,<gb2312_hex_bytes_with_spaces>,<style>`
  - `BEEP` 动作：`BEEP`
- 下位机回报：
  - `SETPRUN:<currentStep>,<startTimeMs>`

## 日志

每次点击 `Start` 视为一次 Run，会创建一个日志文件（`logs/`）。格式：

`[device_ms] [direction TX/RX] [type CONFIG/WORK/TEST/ERROR] [segmentIndex] [rawLine]`

`device_ms` 在收到首个 `SETPRUN` 前为 `-1`；收到后会用下位机时间戳与本机计时做映射生成时间轴。
