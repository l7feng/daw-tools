# DAW Tools

Windows 原生 DAW 开发版 0.1.6。唯一计划与状态：`D:/1-个人知识库_Vault/5-领域-A/06-软件制作/79-DAW Tools/Documentation.md`。

不参考旧软件轨道模板；支持保存用户自行建立的无媒体模板，保留用户轨道、路由、内置处理器和VST3状态。启动为空白工程。

## 当前后端与验证入口

本批BE-09/10/11实现冻结失效/取消恢复、不同与动态延迟PDC、隔离扫描及插件链/编辑器/缺失修复。继续回收循环MIDI控制器追踪、保存故障、共享非线性分轨和导出可靠性缺口；实际状态以Vault项目79为准，完整DAW尚未完成。

```powershell
.\scripts\build-backend.cmd
D:\Ai-Files\Tools\BuildEnv\py312\python.exe -X utf8 scripts/run-backend.py
.\scripts\build-verified.cmd
D:\Ai-Files\Tools\BuildEnv\py312\python.exe -X utf8 scripts/run-workbench.py --editor --verified
D:\Ai-Files\Tools\BuildEnv\py312\python.exe -X utf8 scripts/run-workbench.py --editor --verified --plugin-ui-test
D:\Ai-Files\Tools\BuildEnv\py312\python.exe -X utf8 scripts/open-gui.py --verified
```

Verified入口复用同一源码和构建对象，单独链接`DAW Tools Verified.exe`，保护可能被占用的默认EXE。界面和Windows文件属性均为0.1.2，不热更新旧窗口。主工具栏“插件链”支持轨道/Master、明确选定VST3扫描、插入/旁通/移除/重排、编辑器与同身份缺失修复。只加载记录的二进制路径/身份，拒绝按名称静默替换。修复前保留恢复点。

原生窗口/按钮回调自动验证与Windows真实输入分别记录；文件选择器文本输入的Computer Use验证未完成，不把回调测试冒充完整鼠标键盘验收。测试使用离线或Hosted软件设备，未重启真实设备长测。

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

当前仍是开发增量，S2未完成：已有插件链/编辑器、PDC、冻结及恢复底座；录音、完整MIDI产品功能、自动化、视频与AAF等仍有缺口。这里的编辑回归不开物理设备；Hosted软件时钟结果、原生窗口回调、实际GUI输入和真实业务验收分别记录。

## 隔离VST3扫描开发入口

`python scripts/scan-plugins.py <明确指定的插件.vst3> [其他候选] --timeout 20`

每个候选使用独立子进程；不搜索系统插件目录，不创建音频Session或设备管理器。重复路径去重；异常退出、超时、坏报告分别留证，每项完成后保存report.json，后续候选继续。结果在build/native-validation/scan-*。监督程序的故障注入测试用`python tests/test_scan_supervisor.py`运行，它验证进程控制，不代表真实插件通过。正常VST3、原生异常退出/挂起、后续候选继续扫描和插件链入口已有专项验证；产品任务目录保存scan-status.json及原始描述，具体覆盖见项目状态。第三方插件自身的外部副作用并不由进程隔离自动限制。

## 确定性VST3样本

构建入口同时生成StudioTestGain_VST3，路径为`build/editor/StudioTestGain_artefacts/Release/VST3/Studio Test Gain.vst3`；不自动复制到系统插件目录。测试处理器由本项目编写，实现0..1增益与版本化状态，并增加默认关闭的tanh饱和用于非线性共享处理对照；框架许可仍沿用锁定JUCE，不表示已批准对外分发。

```powershell
python scripts/scan-plugins.py "build/editor/StudioTestGain_artefacts/Release/VST3/Studio Test Gain.vst3" --verify-studio-fixture --timeout 30
```

此开关只用于本项目的Studio Test Gain样本，验证默认0.5增益、0.25参数改变及新实例状态恢复，48kHz、128/256采样块、误差阈值1e-7，不创建音频设备。此命令验证JUCE VST3实例通路；Tracktion插件链/PDC/原生编辑器另有专项证据，不从该命令推定。当前扫描描述的通道数为0，实际双声道处理通过；宿主路由必须后续核实实际bus配置，不能仅依赖该字段。

## 0.1.1 GUI与桌面入口

GUI调整为顶部两行操作、主时间线、右侧带可见标签的片段/混音检查器；时间标尺保持约100像素以上主刻度间距。最小窗口1240×850，默认1360×900。桌面`DAW Tools`快捷方式调用scripts/open-gui.py，每次打开当前最新构建及新的独立空白工作目录，已有工程通过“打开”恢复。更新后重新打开窗口查看版本号；不会替换正在运行的旧窗口，也不提供热重载。音频设备保持关闭。

## S1独立设备探针与ASIO

`scripts/restore-asio.py`按dependencies.lock.json恢复并逐文件核验官方ASIO SDK 2.3.4，首次下载须联网；不安装驱动。仅独立BackendProbe启用JUCE_ASIO=1，GUI不因此获得实时功能。SDK本地验证选择其GPLv3许可分支，保留原始许可；没有发行授权变更。

运行`scripts/build-backend.cmd`后，可用`python scripts/run-device.py --capture --driver asio --block 256 --seconds 15`测试已安装Yamaha Steinberg USB ASIO。shared/lowlatency用于UR22C明确Input 1/2和DAW端点。输出数字静音，实际输入会录入独立运行目录；不打开控制面板、不切换默认设备。

报告必须区分请求/实际缓冲、启动/测量窗口、FIFO丢块和驱动过载计数；计数不可用时stable为null。2026-11-01 ASIO请求128/256实际均512，不能称128/256通过。exit0仅说明本探针已执行断言通过，不能替代真实声音、实时引擎、长时稳定性或业务验收。独立异常退出测试入口为scripts/test-capture-recovery.py，只终止它自己创建的采集子进程并保留原始WAV。

## 后端冻结与工程资产

`Session::setTrackFrozen(index, true/false)`接引擎原生单轨冻结；目前要求48kHz引擎时钟已配置，测试使用Hosted软件时钟，不打开声卡。暂拒绝直接输出到其他轨道的源轨及带Aux Send轨，尚未接GUI操作。

保存冻结工程会生成同目录的`<工程文件名>.assets/freeze/`，包含按SHA256命名的冻结音频。打开前验证资产散列并恢复引擎缓存；移动工程时须一起保留这个.assets目录。恢复点也有自己的对应.assets目录；解冻保留已保存资产，未提供自动清理。用户模板清除原工程ID、冻结标记和缓存引用。

独立运行目录中的新Engine恢复已验证；跨机器、异常掉电、缺失资产修复和所有路由/插件冻结尚未验收。默认GUI程序仍可能是旧构建，后端回归使用独立Verified程序，不推定正在运行的GUI热更新。

## 分轨的共享发送语义

后端选源轨导出保留该轨的干声和它对发送总线的贡献；选标记为studioBus的Return总线导出只保留该返回链，抑制其他源轨直出干声。总线可继续经过下游总线；处理在独立Edit快照内完成。源轨贡献与Return是重叠交付，不能把两类全部相加作为总混。线性前/后发送以及自建非线性共享处理、静音/独奏已有对照。非线性源贡献的和通常不等于总混，manifest明确记录语义；侧链与所有商业插件组合仍待验证。发送返回总线默认独奏安全，保留被独奏源的湿声。

尾音效果器及确定性MIDI乐器的冻结、保存重开、解冻与再次保存已经离线验证。冻结带发送的源轨仍拒绝；本增量不扩展GUI或恢复真实设备长测。

## 后台批量分轨

`StemJob::start(session, targetDirectory, tracks, startSample, endSample, includeMaster)`创建工程状态快照并启动独立worker；目前输出48kHz双声道Float32，轨道/总线编号使用现有后端索引，尚未接GUI。通过poll收取完成结果、cancel取消；任务目录manifest.json记录每条分轨的状态、文件名及SHA256。

渲染worker将结果复制到随机同级.partial目录并逐文件核对SHA256，父进程复核后提交整个目标目录，目标必须不存在。取消、子进程失败和目标冲突不提交目录；任务目录与可能的.partial暂存保留诊断。交付目录内manifest.json的complete表示批次已交付；任务目录也保存结果。渲染子进程仅扫描工程明确引用的VST3。快照固定工程状态，媒体仍引用原文件；引用输入使用散列及Windows持续目录事件监测；不是全输入不可变副本，包外采样库/IR未自动枚举。

默认保护原GUI窗口，当前应用worker分支通过独立Verified程序验证；默认GUI EXE仍可能为旧构建。长时间声卡压力测试继续暂缓。

## 后台渲染的输入与超时保护

单文件ExportJob和批量StemJob共用工程快照、显式VST3登记、冻结资产恢复。新任务内部使用p.tracktionedit以缩短Windows路径；旧任务不自动迁移。两种start末尾可传timeoutMs，默认600000（10分钟），范围1—86400000；父轮询使用单调时钟，worker有覆盖加载/渲染/清理阶段的独立看门狗，超时标记timed_out并拒绝提交。

任务开始记录工程快照、引用音频、VST3包文件和当前冻结资产的SHA256。worker加载/完成、批量逐项及父提交前复核，变化记录input_changed，不发布结果；job-status.json含具体原因。Windows在父/子任务中持续读取相关文件及祖先目录的变化事件，通知丢失/溢出拒绝提交；修改后恢复字节与修改时间的样本已拦截。仍不持续锁定所有输入，不保证覆盖监测建立前的竞态、未声明包外依赖或其他平台。输入捕获/Hash和父进程最终复核、rename仍在调用线程，尚未端到端非阻塞。音频最终复制和散列核对已移入worker，渲染/复制/清理均受worker看门狗限制；原生CopyFileEx回调内超时退出已有实测。

单文件外部插件与冻结快照、媒体/冻结/插件资源变化、无父轮询worker超时及父进程终止均有隔离测试。旁置.assets目录仍须与工程一起保留；新程序的worker验证使用Verified构建，原GUI不会热更新。

## 冻结与恢复的当前边界

冻结完成时记录原生缓存SHA256，以及规范化轨道/插件/速度/音高状态、来源媒体、上游输入轨和VST3包文件身份；保存、打开和渲染前复核。源/参数/缓存过期时拒绝使用旧结果，先解冻再编辑/重冻。取消在原生渲染任务内生效，失败恢复片段与插件参数；失败暂存不作为当前结果。冻结和缺失修复目前通过重建Edit恢复/替换状态，内存撤销历史未保留；修复前创建可重开的恢复点。

仍拒绝带发送、输出到其他轨道的源轨及输入总线/Aux Return冻结；未枚举插件包外IR/采样库。真实断电/满盘、网络盘及跨机可靠性尚未验收。200轨离线工程保存/恢复不等于设备长测或S3压力验收。S2.3/S2.4产品功能与8/32轨长测仍暂缓。
