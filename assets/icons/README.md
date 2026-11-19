# DAW Tools 图标资产

本目录保存 `DAW Tools`（项目79 · 通用DAW工作站）的程序图标源文件与可复现生成脚本。图标于 2026-11-04 新增；**归属由人类当次明确裁决为 DAW Tools**，不是 `Frame Audio Host`（项目76）的资产。

## 归属与叫法

| 软件 | 固定叫法 | 本项目图标关系 |
| --- | --- | --- |
| 项目79 · 通用DAW工作站 | `DAW Tools` | 本目录，图标归属方 |
| 项目76 · 影视音频宿主 | `Frame Audio Host` | 其仓库 `assets/` 内同图案副本仅作原型侧留档，不嵌入其 EXE |
| 项目54 · 音效管理软件 | `SFX Tools`（旧称 Sound Workbench） | 无关软件，图标在 `sfx-tools` 仓 `assets/icons/app.ico` |

`DAW Tools` 与 `Sound Workbench` 名称相近但不是同一程序；文档与对话中一律带项目号或全名，不用「那个软件／新版／Workbench」单独指代。

## 文件清单

| 文件 | 用途 |
| --- | --- |
| `app.svg` | 2026-11-05 波形版矢量源；未随 2026-11-11 推子版改版，仅作历史留档 |
| `app.png` | 1024×1024 母版，与作品集 `icons\daw.png` 同源 |
| `app-sizes.png` | 16 至 256 各尺寸对照图（小尺寸放大），用于检查可读性 |
| `app.ico` | Windows 多尺寸图标，含 16/24/32/48/64/128/256 共 7 个条目 |
| `generate_icon.py` | 由母版重采样生成 `app.ico` 与尺寸对照图（依赖 Pillow） |

## 设计含义

深蓝灰圆角底代表宿主工作台；画面中央是三根混音推子（左白、中蓝、右白），各自停在不同刻度，蓝色为当前聚焦通道；每根推子的刻度点线表示电平区间与时间轴。不放文字，保证小尺寸可辨识。

## 重新生成

在仓库根执行：

```powershell
python assets/icons/generate_icon.py
```

脚本从母版 `app.png` 按 16/24/32/48/64/128/256 逐档重采样生成 `app.ico`，并重画 `app-sizes.png` 尺寸对照图，不再逐像素重绘图形。换图时替换 `app.png` 后重跑脚本即可；桌面快捷方式直接引用这里的 `app.ico`，不再另存一份副本。脚本依赖 Pillow。

## 接入状态：已接入构建（2026-11-05）

图标已通过 JUCE 官方支持的 `ICON_BIG` / `ICON_SMALL` 参数接入，**未手工改写 `.rc` 文件**：

```cmake
juce_add_gui_app(DawTools PRODUCT_NAME "DAW Tools" VERSION 0.1.6
  ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/app.png"
  ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/app.png")
```

链路（已在固定版 JUCE 8.0.6 源码中逐处实查，非凭记忆）：

1. `juce_add_gui_app` 把两个路径做绝对化校验（`JUCEUtils.cmake:2065-2071`，要求文件存在）。
2. 构建期调用 `juceaide winicon`，把 PNG 按多档尺寸重采样后写入生成的 `icon.ico`（`juce_Icons.cpp` 的 `writeWinIcon`）。
3. `juceaide rcfile` 再把 `IDI_ICON1 ICON DISCARDABLE "<path>"` 写进 `DawTools_resources.rc`，随 EXE 链接。

两个参数都指向 1024 母版 PNG 是有意为之：JUCE 会自行按档重采样，无需另备小尺寸源图；`app.ico` 保留作外部分发与资源管理器替换用，不参与编译。改图后必须重建，否则 EXE 内嵌图标不会更新。

窗口运行期图标（标题栏／任务栏）在 Windows 由可执行文件内嵌的图标资源提供，`src/workbench.cpp` 无需额外调用；因此本文件此前记的「窗口图标另需核对」一项已闭合。

## 边界与状态

- 本轮改动：`CMakeLists.txt` 加 `ICON_BIG`/`ICON_SMALL` 两行；本目录 6 个文件由 `studio-workbench.*` 改名为 `daw-tools.*` 并同步脚本与文档内引用。
- 2026-11-13 图形回正：此前桌面 ICO 与仓内图标被换成波形版，经人类指认作品集 `icons\daw.png`（2026-11-11 推子图）才是正确图形。已换回推子版，生成脚本改为从母版重采样，并在 `CMakeLists.txt` 补上 `.rc` 资源对 `app.png` 的依赖——此前改图后重建，EXE 内嵌图标仍是旧图。
- 验证：ICO 7 个条目结构与数据格式自检通过；推子版母版与尺寸对照图已目视核对。真实资源管理器／任务栏渲染仍未在另一台机器观察。
- 已知代价：接入图标必然改变 EXE 字节与 SHA256，项目79 `Documentation.md` 里 0.1.6 的两个哈希已标为历史版本，新哈希待重建后登记。
- 后续方向「继续优化完 Pro Tools 式界面」已按人类指令登记在 `5-领域-A/06-软件制作/domain.md`，**不并入项目79 或项目76 的阶段计划**。
- 项目79 当前状态唯一入口：`D:/1-个人知识库_Vault/5-领域-A/06-软件制作/79-DAW Tools/Documentation.md`。
