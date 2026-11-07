# DAW Tools 图标资产

本目录保存 `DAW Tools`（项目79 · 通用DAW工作站）的程序图标源文件与可复现生成脚本。图标于 2026-11-04 新增；**归属由人类当次明确裁决为 DAW Tools**，不是 `Frame Audio Host`（项目76）的资产。

## 归属与叫法

| 软件 | 固定叫法 | 本项目图标关系 |
| --- | --- | --- |
| 项目79 · 通用DAW工作站 | `DAW Tools` | 本目录，图标归属方 |
| 项目76 · 影视音频宿主 | `Frame Audio Host` | 其仓库 `assets/` 内同图案副本仅作原型侧留档，不嵌入其 EXE |
| 项目54 · 音效管理软件 | `Sound Workbench` | 无关软件，另有自己的 `src-tauri/icons/icon.ico` |

`DAW Tools` 与 `Sound Workbench` 名称相近但不是同一程序；文档与对话中一律带项目号或全名，不用「那个软件／新版／Workbench」单独指代。

## 文件清单

| 文件 | 用途 |
| --- | --- |
| `daw-tools.svg` | 手工矢量源，便于改色与二次编辑 |
| `daw-tools-512.png` | 512×512 预览图 |
| `daw-tools-sizes.png` | 16 至 256 各尺寸等比例放大对照图，用于检查小尺寸可读性 |
| `daw-tools.ico` | Windows 多尺寸图标，含 16/24/32/48/64/128/256 共 7 个条目 |
| `generate_icon.py` | 纯标准库生成脚本，不依赖 Pillow、Qt、JUCE 或系统组件 |

## 设计含义

深蓝灰圆角底代表宿主工作台；四角画幅标记对应视频监看与 Frame 概念；上下两条细线是时间轴；中央对称波形是音轨与混音，琥珀色中心条同时表示播放头与当前帧。不放文字，保证小尺寸可辨识。

## 重新生成

在仓库根执行：

```powershell
python assets/icon/generate_icon.py
```

脚本按同一套归一化参数逐尺寸重绘，不靠缩放，16 至 32 尺寸会自动加粗画幅角标并减少波形条数。16/24/32/48 条目使用传统 32 位 DIB，64/128/256 使用 PNG 压缩条目；生成后会自检 ICO 头、条目偏移与数据格式。重跑会重写同目录下的图标文件。

## 接入状态：已接入构建（2026-11-05）

图标已通过 JUCE 官方支持的 `ICON_BIG` / `ICON_SMALL` 参数接入，**未手工改写 `.rc` 文件**：

```cmake
juce_add_gui_app(DawTools PRODUCT_NAME "DAW Tools" VERSION 0.1.6
  ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon/daw-tools-512.png"
  ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon/daw-tools-512.png")
```

链路（已在固定版 JUCE 8.0.6 源码中逐处实查，非凭记忆）：

1. `juce_add_gui_app` 把两个路径做绝对化校验（`JUCEUtils.cmake:2065-2071`，要求文件存在）。
2. 构建期调用 `juceaide winicon`，把 PNG 按多档尺寸重采样后写入生成的 `icon.ico`（`juce_Icons.cpp` 的 `writeWinIcon`）。
3. `juceaide rcfile` 再把 `IDI_ICON1 ICON DISCARDABLE "<path>"` 写进 `DawTools_resources.rc`，随 EXE 链接。

两个参数都指向 512 PNG 是有意为之：JUCE 会自行按档重采样，无需另备小尺寸源图；`daw-tools.ico` 保留作外部分发与资源管理器替换用，不参与编译。

窗口运行期图标（标题栏／任务栏）在 Windows 由可执行文件内嵌的图标资源提供，`src/workbench.cpp` 无需额外调用；因此本文件此前记的「窗口图标另需核对」一项已闭合。

## 边界与状态

- 本轮改动：`CMakeLists.txt` 加 `ICON_BIG`/`ICON_SMALL` 两行；本目录 6 个文件由 `studio-workbench.*` 改名为 `daw-tools.*` 并同步脚本与文档内引用。
- 验证：ICO 7 个条目结构与数据格式自检通过；512 预览图与尺寸对照图已目视核对，小尺寸仍可辨认画幅角标和波形。真实资源管理器／任务栏渲染待重建后核验（第④批）。
- 已知代价：接入图标必然改变 EXE 字节与 SHA256，项目79 `Documentation.md` 里 0.1.6 的两个哈希已标为历史版本，新哈希待重建后登记。
- 后续方向「继续优化完 Pro Tools 式界面」已按人类指令登记在 `5-领域-A/06-软件制作/domain.md`，**不并入项目79 或项目76 的阶段计划**。
- 项目79 当前状态唯一入口：`D:/1-个人知识库_Vault/5-领域-A/06-软件制作/79-DAW Tools/Documentation.md`。
