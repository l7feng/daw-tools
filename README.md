# Studio Workbench

Windows 原生 DAW 底座工程。唯一计划与状态：`D:/1-个人知识库_Vault/4-项目-P/79-通用DAW工作站/Documentation.md`。

不参考旧软件轨道模板；用户自定义模板由后续编辑功能实现。

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
