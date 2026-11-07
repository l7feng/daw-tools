#pragma once
// 界面语言层：中文为源语言，英文表按中文原文作键。
// 现存调用处一律写 zh("中文")，切换语言只改语言表，不动布局与业务代码。
namespace i18n {

enum class Lang { zh, en };

inline Lang current = Lang::zh;

// 中文原文 -> 英文译文。未收录的条目回退为中文原文，不会出现空字符。
inline const std::map<juce::String, juce::String>& english()
{
    static const std::map<juce::String, juce::String> table {
        { juce::String::fromUTF8("文件"), "File" },
        { juce::String::fromUTF8("编辑"), "Edit" },
        { juce::String::fromUTF8("视图"), "View" },
        { juce::String::fromUTF8("轨道"), "Track" },
        { juce::String::fromUTF8("片段"), "Clip" },
        { juce::String::fromUTF8("选项"), "Options" },
        { juce::String::fromUTF8("窗口"), "Window" },
        { juce::String::fromUTF8("帮助"), "Help" },
        { juce::String::fromUTF8("主输出"), "Master" },
        { juce::String::fromUTF8("主输出名称"), "Master name" },
        { juce::String::fromUTF8("插入 A–E"), "Inserts A-E" },
        { juce::String::fromUTF8("输入/输出"), "I/O" },
        { juce::String::fromUTF8("发送"), "Sends" },
        { juce::String::fromUTF8("编组"), "Groups" },
        { juce::String::fromUTF8("无编组"), "No groups" },
        { juce::String::fromUTF8("波形"), "Waveform" },
        { juce::String::fromUTF8("选择"), "Select" },
        { juce::String::fromUTF8("裁剪"), "Trim" },
        { juce::String::fromUTF8("抓取"), "Grabber" },
        { juce::String::fromUTF8("滑移"), "Shuffle" },
        { juce::String::fromUTF8("替换"), "Spot" },
        { juce::String::fromUTF8("自由"), "Slip" },
        { juce::String::fromUTF8("网格"), "Grid" },
        { juce::String::fromUTF8("定位到工程开头"), "Go to session start" },
        { juce::String::fromUTF8("录音工作流暂缓"), "Recording is deferred" },
        { juce::String::fromUTF8("音量 · dB；双击归零"), "Volume (dB), double-click to reset" },
        { juce::String::fromUTF8("声像；双击居中"), "Pan, double-click to centre" },
        { juce::String::fromUTF8("插入插件"), "Insert plug-in" },
        { juce::String::fromUTF8("时间线水平缩放"), "Timeline zoom" },
        { juce::String::fromUTF8("片段属性"), "Clip" },
        { juce::String::fromUTF8("路由"), "Routing" },
        { juce::String::fromUTF8("任务"), "Tasks" },
        { juce::String::fromUTF8("就绪"), "Ready" },
        { juce::String::fromUTF8("采样率"), "Sample rate" },
        { juce::String::fromUTF8("离线"), "Offline" },
        { juce::String::fromUTF8("添加轨道"), "Add track" },
        { juce::String::fromUTF8("导入音频"), "Import audio" },
        { juce::String::fromUTF8("保存"), "Save" },
        { juce::String::fromUTF8("打开"), "Open" },
        { juce::String::fromUTF8("另存为"), "Save as" },
        { juce::String::fromUTF8("撤销"), "Undo" },
        { juce::String::fromUTF8("重做"), "Redo" },
        { juce::String::fromUTF8("导出 WAV"), "Export WAV" },
        { juce::String::fromUTF8("保存恢复点"), "Save recovery point" },
        { juce::String::fromUTF8("插件链"), "Plug-in chain" },
        { juce::String::fromUTF8("编辑工作区"), "Edit workspace" },
    };
    return table;
}

inline juce::String tr(const char* zhText)
{
    if (current == Lang::zh)
        return juce::String::fromUTF8(zhText);
    const auto key = juce::String::fromUTF8(zhText);
    const auto& table = english();
    const auto it = table.find(key);
    return it != table.end() ? it->second : key;
}

inline void setLang(Lang lang) { current = lang; }

} // namespace i18n
