# 素材来源与授权声明 · PROVENANCE

本文件记录 RandomPicker 项目中**非原创素材**的来源、作者与授权，
以便开源分发时满足原作者的署名要求。

---

## 一、界面插画（核心素材）

| 项目 | 内容 |
| --- | --- |
| 本仓库文件 | `assets/whale.png` |
| 原始文件名 | `DSniang02.png` |
| 原始尺寸 | 1026 × 1026 px，RGBA（带透明通道） |
| 内容 | 蓝发女仆装角色 + 思考气泡（气泡内为空白，供叠加文字） |
| 作者 | **MeteorNOX** |
| 来源项目 | `dsh-whale-widget`（DSH 余额小挂件） |
| 仓库地址 | https://github.com/MeteorNOX/DeepSeek-Balance-Whale-Widget |
| 原始路径 | `assets/DSniang02.png` |
| 授权协议 | **MIT License**（Copyright (c) 2026 MeteorNOX） |
| 取得方式 | 直接复制自本地已安装的 npm 包 `dsh-whale-widget@0.3.17` |
| 是否修改 | **否**。文件字节完全一致，未裁剪、未调色、未重绘 |

### 使用方式

本项目将这张插画作为**展开卡片的整窗背景**，在气泡区域叠加绘制：

- 姓名文字（大号、深灰）
- 蓝色「点名」按钮

气泡的坐标由实测该图得到，写成源码顶部的 `BUBBLE_NX0/NX1/NY0/NY1` 四个归一化常量，
不依赖任何运行时图像分析。

### 署名要求

MIT 协议要求「在软件的所有副本或实质性部分中保留上述版权声明和许可声明」。
因此：

- 本仓库保留 [LICENSE](LICENSE) 文件；
- README「素材来源」一节公开署名原作者与仓库地址；
- 若你二次分发本项目，**请一并保留以上署名信息**。

---

## 二、本项目原创部分

以下内容由本项目作者 **6evan775（名名）** 原创，采用 MIT 协议：

- 全部 C++ / Win32 源码（`RandomPicker.cpp`）
- 待机椭圆的渐变与人物简笔画（GDI+ 程序化绘制，非位图）
- 托盘图标（运行时软件光栅生成，非位图）
- 全部音效（`waveOut` 实时合成，不使用任何音频文件）
- 气泡内 UI 的排版与配色
- 文档（README / PROVENANCE / LICENSE）

---

## 三、第三方库

**无。** 本项目只链接 Windows 系统自带库：

```
kernel32  user32  gdi32  gdiplus  advapi32  shell32  winmm
```

不包含任何第三方源码、不依赖任何运行时包管理器、不需要安装运行库。

---

## 四、授权边界说明

本项目的 MIT 协议**仅覆盖本项目作者原创的代码与文档**。

`assets/whale.png` 的版权归 **MeteorNOX** 所有，其授权条款以原仓库为准：

- 允许：使用、修改、再分发、商用、闭源分发
- 要求：保留版权声明与许可声明
- 不提供：任何担保

如需将该插画用于本项目以外的用途，建议直接查阅原仓库的 LICENSE：

https://github.com/MeteorNOX/DeepSeek-Balance-Whale-Widget/blob/main/LICENSE

---

## 五、致谢

感谢 **MeteorNOX** 创作并开源了这只小鲸鱼，让这个点名工具从「一个白色方块」
变成了「一只会在气泡里叫你名字的小家伙」。

原项目 dsh-whale-widget 是一个功能完整的 DSH 界面挂件（余额显示、气泡点击、
音效、角色自定义等），本项目只借用了它的美术素材，代码完全独立实现。

---

*本文件最后更新：2026-10-25（v1.0.0）*
