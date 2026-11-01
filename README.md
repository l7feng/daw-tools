# Studio Workbench

Windows 原生 DAW 开发版 0.1.0。唯一计划与状态：`D:/1-个人知识库_Vault/4-项目-P/79-通用DAW工作站/Documentation.md`。

不参考旧软件轨道模板；支持保存用户自行建立的无媒体模板，保留轨道、路由与内置混音处理器。启动为空白工程。

## 构建

在 VS 2022 x64 Native Tools 环境使用 CMake：

```powershell
cmake -S . -B build/platform -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/platform
.\build\platform\PlatformProbe.exe
```

PlatformProbe 只枚举 Windows 音频/MIDI 设备，不打开输入输出，不证明 Tracktion、ASIO、实时播放或录音通过。依赖固定见 dependencies.lock.json。依赖与产物位于忽略的 third_party/build，不发布第三方插件或真实媒体。

## 固定依赖

Tracktion v3.2.0 与该提交指定的 JUCE 8.0.6 子模块分别放在 `third_party/tracktion_engine-3.2.0`、`third_party/JUCE-19edd538429c93d277bf95b55aaa7e3eb545f951`。官方归档与散列见锁文件，需核对散列再解压；不得用其他 JUCE 版本顶替。

本次设备的构建入口为 `scripts/build-platform.cmd` 和 `scripts/build-engine.cmd`，使用现有 D 盘 VS 2022 工具链。后者编译官方 TestRunner；不自动运行其测试（上游运行器含默认缓存清理，执行前须完成测试目录隔离）。两者构建失败均应查看退出码。

上游依赖保留原许可；Tracktion为GPL3或商业，所锁JUCE为AGPLv3或商业。本仓暂无公开发行许可决定，未设置远端。

## 编辑与混音开发版

```powershell
.\scripts\build-editor.cmd
D:\Ai-Files\Tools\BuildEnv\py312\python.exe -X utf8 scripts/run-workbench.py --editor
# 手动查看开发界面（不打开音频设备）：
D:\Ai-Files\Tools\BuildEnv\py312\python.exe -X utf8 scripts/run-workbench.py --editor --gui
```

自动测试涵盖整数采样编辑、撤销重做、保存重开、交叉淡化、素材身份重定位、用户模板、轨道混音、直接总线、前后推子发送、反馈环拒绝，以及独立进程快照导出/取消/文件冲突。报告在每次新建的 `build/native-validation/offline-*`，同时登记实际 EXE 与应用源代码散列。构建入口使用 UTF-8 控制台，并匹配本机中文 MSVC 的包含文件前缀，避免 Ninja 漏跟踪头文件。

界面数字位置均为 48kHz 采样数。选择片段后可移动、向内裁剪、切分、复制、增益/淡化；交叉淡化作用于当前片段与同轨的下一个部分重叠片段。导出使用“起点—终点”字段，选择 PCM24 或 Float32；任务开始时固定工程快照，后续编辑不影响本次结果。目标存在时拒绝覆盖，取消和失败保留任务目录中的证据，不生成成功目标。

选择轨道头后修改推子、声像、静音/独奏。输出列表可选 Master 或其他轨道；“添加发送”要求选择用户创建的总线，支持推子前/后。当前使用 Tracktion 线性声像：中心 L/R 各 0dB，极左/右该侧约 +6.02dB，另一侧静音；不是恒功率声像。导出包括 Master 处理。恢复点每60秒在修改后写入一次，也可手动保存；通过“打开”选择 recovery 目录的工程恢复。

保存先写同目录临时文件并验证 XML，再替换现有工程；另存与导出拒绝既有目标。自动恢复点不替代正式保存，存储介质掉电/满盘/进程崩溃场景尚未完整验证。

当前仍是开发增量，S2 未完成：VST3扫描/编辑器/PDC/冻结、实际录音/MIDI、自动化、视频与AAF尚待实现及验证；波形界面和真实操作链尚未验收。全部测试禁用音频设备，不播放、不录音。仅有单机离线结果，不能视为实时、GUI、业务或发行通过。

## 隔离VST3扫描开发入口

`python scripts/scan-plugins.py <明确指定的插件.vst3> [其他候选] --timeout 20`

每个候选使用独立子进程；不搜索系统插件目录，不创建音频Session或设备管理器。重复路径去重；异常退出、超时、坏报告分别留证，每项完成后保存report.json，后续候选继续。结果在build/native-validation/scan-*。监督程序的故障注入测试用`python tests/test_scan_supervisor.py`运行，它验证进程控制，不代表真实插件通过。当前仅验证原生扫描器拒绝无效/缺失文件；正常VST3样本、GUI插件列表和实际插件链尚待接入。第三方插件自身的外部副作用并不由进程隔离自动限制。

## 确定性VST3样本

构建入口同时生成StudioTestGain_VST3，路径为`build/editor/StudioTestGain_artefacts/Release/VST3/Studio Test Gain.vst3`；不自动复制到系统插件目录。测试处理器由本项目编写，只实现0..1线性增益与版本化状态；框架许可仍沿用锁定JUCE，不表示已批准对外分发。

```powershell
python scripts/scan-plugins.py "build/editor/StudioTestGain_artefacts/Release/VST3/Studio Test Gain.vst3" --verify-studio-fixture --timeout 30
```

此开关只用于本项目的Studio Test Gain样本，验证默认0.5增益、0.25参数改变及新实例状态恢复，48kHz、128/256采样块、误差阈值1e-7，不创建音频设备。它验证JUCE VST3实例通路，不代表Tracktion工程插件链/PDC/GUI编辑器已经通过。当前扫描描述的通道数为0，实际双声道处理通过；宿主路由必须后续核实实际bus配置，不能仅依赖该字段。

## 0.1.1 GUI与桌面入口

GUI调整为顶部两行操作、主时间线、右侧带可见标签的片段/混音检查器；时间标尺保持约100像素以上主刻度间距。最小窗口1240×850，默认1360×900。桌面`Studio Workbench GUI`快捷方式调用scripts/open-gui.py，每次打开当前最新构建及新的独立空白工作目录，已有工程通过“打开”恢复。更新后重新打开窗口查看版本号；不会替换正在运行的旧窗口，也不提供热重载。音频设备保持关闭。
