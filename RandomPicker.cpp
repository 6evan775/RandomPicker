// ============================================================================
//  随机点名 · Windows 原生悬浮窗程序  v1.0.0
//  ----------------------------------------------------------------------------
//  作者：6evan775（名名）   https://github.com/6evan775
//  协议：MIT License
//  ----------------------------------------------------------------------------
//  · 纯 Win32 API + C++11 + GDI+，不依赖任何第三方 GUI 框架
//  · MinGW-w64 (g++) 与 MSVC (cl) 均可直接编译
//  · 待机：右边缘一枚蓝色椭圆胶囊 + 人物简笔画图标，可上下拖动
//  · 展开：整窗是一张「小鲸鱼 + 气泡」插画（assets/whale.png），
//          姓名 / 点名按钮绘制在气泡内部；点人物本体（气泡外）收起
//  · 逐像素 Alpha 分层窗口（UpdateLayeredWindow），插画边缘平滑无白边
//  · 读取同目录 names.txt，滚动约 1 秒后减速定格（带音效）
//  · 排除最近 2 次抽取结果，名单人数 <= 2 时自动解除限制
//  · 配置全部放在同目录 settings.ini（含开机自启开关）
//
//  美术素材：assets/whale.png 取自 dsh-whale-widget 插件，作者 MeteorNOX，
//            详见 PROVENANCE.md 与 README「素材来源」一节。
//
//  编译（MinGW / Dev-C++ 自带 g++）：
//    g++ RandomPicker.cpp -o RandomPicker.exe -std=c++11 -O2 -mwindows -static
//        -static-libgcc -static-libstdc++ -lgdiplus -lgdi32 -luser32
//        -lshell32 -ladvapi32 -lwinmm
//
//  编译（MSVC 开发者命令提示符）：
//    cl /nologo /utf-8 /std:c++14 /O2 /EHsc /DUNICODE /D_UNICODE RandomPicker.cpp
//       /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib advapi32.lib shell32.lib
//       winmm.lib gdiplus.lib
//
//  ----------------------------------------------------------------------------
//  AI 使用声明
//  ----------------------------------------------------------------------------
//  本项目在开发过程中使用了 AI 辅助进行代码编写。
//  所有功能逻辑均由开发者人工审核、调试与修改：
//  · 每一处功能实现均经过实际编译、运行与交互验证；
//  · 绘制、动画、音频、注册表等关键路径均由人工逐项测试确认；
//  · AI 生成内容仅作为初稿参考，最终交付代码以人工审核后的版本为准。
//  使用者应知悉：本项目按「现状」提供，不附带任何明示或暗示的担保。
// ============================================================================

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601        // Windows 7+（GetTickCount64 等 API 需要）
#endif
#ifndef WINVER
#define WINVER 0x0601
#endif

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <mmsystem.h>
#include <objidl.h>        // GDI+ 在 MinGW 下需要先声明 PROPID
#include <gdiplus.h>

#include <string>
#include <vector>
#include <random>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <algorithm>

#ifdef _MSC_VER
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "gdiplus.lib")
#endif

// ============================================================================
//  一、可自定义参数（改完重新编译即可，无需改动核心逻辑）
//      所有尺寸均以 96 DPI 为基准像素，程序会自动按系统缩放比例放大。
// ============================================================================

// ---------------------------- 待机椭圆（右边缘） ----------------------------
#define DOCK_WIDTH              46     // 椭圆外接矩形宽（≈胶囊宽度）
#define DOCK_HEIGHT             64     // 椭圆外接矩形高
#define DOCK_MARGIN_RIGHT       4      // 椭圆与屏幕右边缘的间隙
#define DOCK_ICON_RATIO         0.62   // 人物图标占椭圆宽度的比例
#define DOCK_BREATH_MS          2200   // 待机呼吸动画周期（毫秒）

// ---------------------------- 展开后的卡片（小鲸鱼插画） --------------------
// 整张卡片就是 assets/whale.png 这张插画：姓名 + 「点名」按钮画在气泡里，
// 点人物本体（气泡外的角色）即可收起。窗口是正方形，插画按窗口等比铺满。
#define CARD_WIDTH              340    // 卡片边长（96 DPI 基准；越大越占地方）
#define CARD_RIGHT_MARGIN       24     // 展开时卡片右侧与屏幕右边缘的距离

// 气泡在插画中的归一化位置（实测自 DSniang02.png，原图 1026x1026）
// 气泡外框 x 94..813、y 32..471，中心 (453.5, 251.5)
#define BUBBLE_NX0              0.0916 // 气泡左边缘 / 图宽
#define BUBBLE_NX1              0.7924 // 气泡右边缘 / 图宽
#define BUBBLE_NY0              0.0312 // 气泡上边缘 / 图高
#define BUBBLE_NY1              0.4591 // 气泡下边缘 / 图高

// 气泡内部的排版（以气泡半轴为单位）
// 气泡里只放「姓名」和「点名」按钮两样东西，上下留白由椭圆边界决定。
#define BUBBLE_NAME_DY0         (-0.60) // 姓名区上边
#define BUBBLE_NAME_DY1         0.02    // 姓名区下边
#define BUBBLE_BUTTON_DY0       0.16    // 按钮上边
#define BUBBLE_BUTTON_DY1       0.58    // 按钮下边
#define BUBBLE_BUTTON_WFACTOR   1.16    // 按钮宽度 = 半轴 × 该系数
#define BUBBLE_TEXT_WFACTOR     1.48    // 姓名可用宽度 = 半轴 × 该系数
#define BUBBLE_FILL_INSET       0.965   // 白色内填的椭圆缩放（留出描边）

// 字号按气泡半轴等比缩放，这样改卡片尺寸/DPI 时排版不会错位
#define NAME_FONT_RATIO         0.62    // 姓名字号 = 气泡半轴 × 该系数
#define BUTTON_FONT_RATIO       0.27    // 按钮字号 = 气泡半轴 × 该系数
#define HINT_FONT_RATIO         0.26    // 提示文字字号 = 气泡半轴 × 该系数

#define WHALE_ASSET_DIR         L"assets"
#define WHALE_ASSET_NAME        L"whale.png"

// ---------------------------- 字体（像素） ----------------------------------
#define FONT_FACE               L"Microsoft YaHei UI"   // 缺失时系统自动替换
#define FONT_SIZE_NAME_MIN      14     // 姓名自动缩小的下限（96 DPI 基准）

// ---------------------------- 配色（取自插画） -------------------------------
#define COLOR_BLUE              RGB(0x1A, 0x73, 0xE8)   // 主蓝
#define COLOR_BLUE_HOVER        RGB(0x17, 0x66, 0xD0)   // 悬停蓝
#define COLOR_BLUE_DARK         RGB(0x11, 0x53, 0xAE)   // 按下蓝
#define COLOR_BLUE_LIGHT        RGB(0x4A, 0x9B, 0xF5)   // 高光蓝
#define COLOR_BLUE_SOFT         RGB(0xE8, 0xF1, 0xFD)   // 浅蓝底
#define COLOR_BLUE_DISABLED     RGB(0x9E, 0xC3, 0xF2)   // 滚动中按钮
#define COLOR_CARD_BG           RGB(0xFF, 0xFF, 0xFF)   // 气泡内白色
#define COLOR_INK               RGB(0x27, 0x2C, 0x5B)   // 插画描边墨蓝 (39,44,91)
#define COLOR_INK_SOFT          RGB(0x41, 0x49, 0x5F)   // 气泡描边 (65,73,95)
#define COLOR_TEXT_MAIN         RGB(0x22, 0x26, 0x2E)   // 姓名正文
#define COLOR_TEXT_SUB          RGB(0x8A, 0x92, 0x9E)   // 次要文字
#define COLOR_DIVIDER           RGB(0xE3, 0xE8, 0xF0)   // 分隔线
#define COLOR_BUTTON_TEXT       RGB(0xFF, 0xFF, 0xFF)   // 按钮文字

// ---------------------------- 点名行为 -------------------------------------
#define NAMES_FILE_NAME         L"names.txt"   // 同目录名单文件名
#define SETTINGS_FILE_NAME      L"settings.ini"
#define HISTORY_EXCLUDE_COUNT   2              // 排除最近 N 次结果（0 = 不排除）
#define ROLL_DURATION_MS        1000           // 滚动总时长（毫秒）
#define ROLL_INTERVAL_START_MS  30             // 滚动起始间隔（越小越快）
#define ROLL_INTERVAL_END_MS    190            // 滚动结束间隔（越大越慢）
#define RESULT_POP_MS           300            // 定格后名字弹跳动画时长

// ---------------------------- 动画 -----------------------------------------
#define ANIM_EXPAND_MS          260    // 弹出动画默认时长（settings.ini 可改）
#define ANIM_COLLAPSE_MS        200    // 收起动画时长
#define ANIM_ENTER_MS           360    // 启动入场动画时长
#define ANIM_FRAME_MS           15     // 动画帧间隔（≈66fps）

// ---------------------------- 系统集成 -------------------------------------
#define AUTOSTART_VALUE_NAME    L"Win32RandomNamePicker"
#define AUTO_CREATE_NAMES_TEMPLATE 1   // 1 = 缺少 names.txt 时自动生成示例
#define SINGLE_INSTANCE         1      // 1 = 只允许运行一个实例

#define RUN_KEY_PATH L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define WINDOW_CLASS_NAME L"RandomPickerFloatingWindow"
#define WINDOW_TITLE L"随机点名"

#define TIMER_ANIM  1
#define TIMER_IDLE  2
#define TIMER_ROLL  3
#define TIMER_REAP  4

#define WM_PICKER_SHOW_CARD (WM_APP + 1)   // 第二个实例请求：展开卡片
#define WM_TRAYICON         (WM_APP + 3)

#define IDM_TOGGLE      2001
#define IDM_RELOAD      2002
#define IDM_EDIT        2003
#define IDM_SOUND       2004
#define IDM_AUTOSTART   2005
#define IDM_OPENSETTINGS 2006
#define IDM_ABOUT       2007
#define IDM_EXIT        2008

// ============================================================================
//  二、数据结构与全局状态
// ============================================================================

struct Settings
{
    int  sound;       // 1 = 开音效
    int  volume;      // 0 ~ 100
    int  autostart;   // 1 = 开机自启
    int  dockY;       // 待机条 Y 坐标，-1 = 自动居中
    int  animMs;      // 弹出动画时长
};

struct CardLayout
{
    RECT name;
    RECT button;
    RECT bubble;      // 气泡外接矩形
    int  bubbleRx;    // 气泡水平半轴
    int  bubbleRy;    // 气泡垂直半轴
};

enum AnimKind
{
    ANIM_NONE = 0,
    ANIM_EXPAND,
    ANIM_COLLAPSE,
    ANIM_ENTER,
    ANIM_POP
};

struct AnimState
{
    AnimKind  kind;
    ULONGLONG start;
    int       duration;
    RECT      from;
    RECT      to;
    int       radiusFrom;
    int       radiusTo;
    int       alphaFrom;
    int       alphaTo;
};

static HINSTANCE g_hInst = NULL;
static HWND      g_hWnd  = NULL;
static int       g_dpi   = 96;

// 字体缓存（用于文字宽高测量）
static HFONT g_fontHint     = NULL;

// 光标
static HCURSOR g_curHand    = NULL;
static HCURSOR g_curSizeAll = NULL;
static HCURSOR g_curSizeNS  = NULL;

// 名单
static std::vector<std::wstring> g_names;
static std::vector<int>          g_history;
static int                       g_displayIndex = -1;
static std::wstring              g_exeDir;
static std::wstring              g_namesPath;
static std::wstring              g_settingsPath;

// 配置
static Settings g_cfg = { 1, 70, 1, -1, ANIM_EXPAND_MS };

// 窗口状态
static bool g_expanded     = false;
static bool g_rolling      = false;
static bool g_dockHover    = false;
static bool g_dockPressed  = false;
static bool g_buttonHover  = false;
static bool g_buttonDown   = false;
static bool g_bodyHover    = false;   // 悬停在气泡外的人物本体上
static bool g_cardDragging = false;
static bool g_cardMoved    = false;
static bool g_dockDragging = false;
static bool g_dockMoved    = false;
static bool g_trackingMouse= false;

// 动画
static AnimState g_anim = { ANIM_NONE, 0, 0, {0,0,0,0}, {0,0,0,0}, 0, 0, 0, 0 };
static double    g_popT = 1.0;         // 定格弹跳进度 0~1
static bool      g_popActive = false;
static double    g_hoverT = 0.0;       // 悬停过渡 0~1
static double    g_breath = 0.0;       // 呼吸动画相位 0~1

// 拖动
static POINT g_dragCursorStart = {0, 0};
static RECT  g_dragWindowStart = {0, 0, 0, 0};
static int   g_dockDragStartY  = 0;

// 滚动
static ULONGLONG g_rollStart     = 0;
static int       g_rollFinal     = -1;
static int       g_rollLastShown = -1;
static ULONGLONG g_lastTickSound = 0;

// 托盘
static NOTIFYICONDATAW g_nid;
static HICON           g_trayIcon = NULL;
static bool            g_trayAdded = false;
static UINT            g_taskbarCreatedMsg = 0;

// 随机数
static std::mt19937 g_rng;

// GDI+ 与插画
static ULONG_PTR              g_gdiplusToken = 0;
static Gdiplus::Bitmap*       g_whaleBitmap  = NULL;   // 原始插画
static Gdiplus::Bitmap*       g_whaleScaled  = NULL;   // 按当前窗口缩放后的版本
static int                    g_whaleScaledW = 0;
static int                    g_whaleScaledH = 0;
static std::wstring           g_whalePath;
static bool                   g_whaleMissing = false;

// 逐像素 Alpha 分层窗口的离屏缓冲
static HDC     g_memDC     = NULL;
static HBITMAP g_memBmp    = NULL;
static HGDIOBJ g_memOldBmp = NULL;
static int     g_memW      = 0;
static int     g_memH      = 0;


// ============================================================================
//  三、通用工具
// ============================================================================

static int S(int value) { return MulDiv(value, g_dpi, 96); }

static int ClampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

static double Clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

static double LerpD(double a, double b, double t) { return a + (b - a) * t; }

static int LerpI(int a, int b, double t) { return (int)(a + (b - a) * t + 0.5); }

static RECT LerpRect(const RECT& a, const RECT& b, double t)
{
    RECT r;
    r.left   = LerpI(a.left,   b.left,   t);
    r.top    = LerpI(a.top,    b.top,    t);
    r.right  = LerpI(a.right,  b.right,  t);
    r.bottom = LerpI(a.bottom, b.bottom, t);
    return r;
}

static double EaseOutCubic(double t) { double u = 1.0 - t; return 1.0 - u * u * u; }
static double EaseInCubic(double t)  { return t * t * t; }

static double EaseOutBack(double t)
{
    const double s = 1.05;
    double u = t - 1.0;
    return u * u * ((s + 1.0) * u + s) + 1.0;
}

static RECT ClientRectOf(HWND hwnd)
{
    RECT rc = {0, 0, 0, 0};
    GetClientRect(hwnd, &rc);
    return rc;
}

static RECT WorkAreaRect()
{
    MONITORINFO mi;
    ZeroMemory(&mi, sizeof(mi));
    mi.cbSize = sizeof(mi);
    HMONITOR mon = MonitorFromWindow(g_hWnd, MONITOR_DEFAULTTONEAREST);
    if (mon != NULL && GetMonitorInfoW(mon, &mi))
        return mi.rcWork;

    RECT rc = {0, 0, 0, 0};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &rc, 0);
    return rc;
}

static HFONT MakeFont(int sizePx, int weight, const wchar_t* face)
{
    if (sizePx < 1)
        sizePx = 1;

    LOGFONTW lf;
    ZeroMemory(&lf, sizeof(lf));
    lf.lfHeight         = -sizePx;
    lf.lfWeight         = weight;
    lf.lfCharSet        = DEFAULT_CHARSET;
    lf.lfQuality        = CLEARTYPE_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    lstrcpynW(lf.lfFaceName, face, LF_FACESIZE);
    return CreateFontIndirectW(&lf);
}

static int MeasureTextWidth(HDC dc, HFONT font, const std::wstring& text)
{
    HGDIOBJ oldFont = SelectObject(dc, font);
    SIZE sz;
    sz.cx = 0;
    sz.cy = 0;
    GetTextExtentPoint32W(dc, text.c_str(), (int)text.size(), &sz);
    SelectObject(dc, oldFont);
    return sz.cx;
}

// 计算「能塞进 maxWidth 的最大字号」
static int FitFontPx(HDC dc, const std::wstring& text, int maxWidth, int startPx, int minPx)
{
    int px = startPx;
    while (px > minPx)
    {
        HFONT font = MakeFont(px, FW_BOLD, FONT_FACE);
        int width  = MeasureTextWidth(dc, font, text);
        DeleteObject(font);
        if (width <= maxWidth)
            return px;
        px -= (px > 30) ? 4 : 2;
    }
    return minPx;
}

// ============================================================================
//  四、布局
// ============================================================================

// 气泡在给定窗口尺寸下的外接矩形与半轴
static void BubbleGeometry(int w, int h, RECT& out, int& rx, int& ry)
{
    out.left   = (LONG)(w * BUBBLE_NX0);
    out.right  = (LONG)(w * BUBBLE_NX1);
    out.top    = (LONG)(h * BUBBLE_NY0);
    out.bottom = (LONG)(h * BUBBLE_NY1);

    rx = (out.right - out.left) / 2;
    ry = (out.bottom - out.top) / 2;
    if (rx < 8) rx = 8;
    if (ry < 8) ry = 8;
}

// 所有可交互元素都排在气泡内部，气泡中心为原点、以水平半轴 rx 为单位
static CardLayout LayoutFor(int w, int h)
{
    CardLayout L;
    BubbleGeometry(w, h, L.bubble, L.bubbleRx, L.bubbleRy);

    int cx = (L.bubble.left + L.bubble.right) / 2;
    int cy = (L.bubble.top + L.bubble.bottom) / 2;
    int rx = L.bubbleRx;
    int ry = L.bubbleRy;

    // 姓名区（气泡里只放姓名和按钮，标题已去掉）
    L.name.left   = cx - (int)(rx * BUBBLE_TEXT_WFACTOR * 0.5);
    L.name.right  = cx + (int)(rx * BUBBLE_TEXT_WFACTOR * 0.5);
    L.name.top    = cy + (int)(ry * BUBBLE_NAME_DY0);
    L.name.bottom = cy + (int)(ry * BUBBLE_NAME_DY1);

    // 按钮
    int bw = (int)(rx * BUBBLE_BUTTON_WFACTOR);
    L.button.left   = cx - bw / 2;
    L.button.right  = cx + bw / 2;
    L.button.top    = cy + (int)(ry * BUBBLE_BUTTON_DY0);
    L.button.bottom = cy + (int)(ry * BUBBLE_BUTTON_DY1);

    return L;
}

// 前向声明：插画缺失提示（实现见「十五、程序入口」）
static void WarnIfWhaleMissing();

// 判断某点是否落在气泡椭圆内；气泡外即「人物本体」，点它收起卡片
static bool PointInBubble(int x, int y, const CardLayout& L)
{
    double dx = (double)(x - (L.bubble.left + L.bubble.right) / 2) / (double)L.bubbleRx;
    double dy = (double)(y - (L.bubble.top + L.bubble.bottom) / 2) / (double)L.bubbleRy;
    return (dx * dx + dy * dy) <= 1.0;
}

// ============================================================================
//  五、绘制（GDI+ 逐像素 Alpha）
//      GDI 的 Ellipse / FillRect 不写 Alpha 通道，做不出真正的透明边缘，
//      所以整窗内容统一用 GDI+ 画到 32bpp 预乘 Alpha 位图上。
// ============================================================================

// COLORREF -> GDI+ Color
static Gdiplus::Color GP(COLORREF c, BYTE a = 255)
{
    return Gdiplus::Color(a, GetRValue(c), GetGValue(c), GetBValue(c));
}

static Gdiplus::Color GP(BYTE r, BYTE g, BYTE b, BYTE a = 255)
{
    return Gdiplus::Color(a, r, g, b);
}

// 圆角矩形路径
static void AddRoundRect(Gdiplus::GraphicsPath& path, const RECT& rc, int radius)
{
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0)
        return;

    if (radius < 0) radius = 0;
    int maxR = (w < h ? w : h) / 2;
    if (radius > maxR) radius = maxR;

    if (radius == 0)
    {
        path.AddRectangle(Gdiplus::Rect(rc.left, rc.top, w, h));
        return;
    }

    int d = radius * 2;
    path.AddArc(rc.left, rc.top, d, d, 180.0f, 90.0f);
    path.AddArc(rc.right - d, rc.top, d, d, 270.0f, 90.0f);
    path.AddArc(rc.right - d, rc.bottom - d, d, d, 0.0f, 90.0f);
    path.AddArc(rc.left, rc.bottom - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();
}

static void FillRoundRectG(Gdiplus::Graphics& g, const RECT& rc, int radius, Gdiplus::Color color)
{
    Gdiplus::GraphicsPath path;
    AddRoundRect(path, rc, radius);
    Gdiplus::SolidBrush brush(color);
    g.FillPath(&brush, &path);
}

// 用 GDI+ 画一段居中单行文字
static void DrawTextG(Gdiplus::Graphics& g, const RECT& rc, const std::wstring& text,
                      Gdiplus::Color color, const wchar_t* face, int px,
                      int weight, Gdiplus::StringAlignment align, bool ellipsis)
{
    if (px < 4) px = 4;

    Gdiplus::Font font(face, (Gdiplus::REAL)px, Gdiplus::FontStyleRegular,
                       Gdiplus::UnitPixel);
    Gdiplus::FontFamily family(face);
    if (!font.IsAvailable())
    {
        Gdiplus::Font fallback(L"SimHei", (Gdiplus::REAL)px, Gdiplus::FontStyleRegular,
                               Gdiplus::UnitPixel);
        Gdiplus::SolidBrush brush(color);
        Gdiplus::StringFormat fmt;
        fmt.SetAlignment(align);
        fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        fmt.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
        if (ellipsis)
            fmt.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        Gdiplus::RectF box((Gdiplus::REAL)rc.left, (Gdiplus::REAL)rc.top,
                           (Gdiplus::REAL)(rc.right - rc.left),
                           (Gdiplus::REAL)(rc.bottom - rc.top));
        g.DrawString(text.c_str(), (INT)text.size(), &fallback, box, &fmt, &brush);
        return;
    }

    // 需要粗体时用模拟加粗
    if (weight >= FW_SEMIBOLD)
    {
        Gdiplus::Font bold(face, (Gdiplus::REAL)px, Gdiplus::FontStyleBold,
                           Gdiplus::UnitPixel);
        Gdiplus::SolidBrush brush(color);
        Gdiplus::StringFormat fmt;
        fmt.SetAlignment(align);
        fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        fmt.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
        if (ellipsis)
            fmt.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        Gdiplus::RectF box((Gdiplus::REAL)rc.left, (Gdiplus::REAL)rc.top,
                           (Gdiplus::REAL)(rc.right - rc.left),
                           (Gdiplus::REAL)(rc.bottom - rc.top));
        g.DrawString(text.c_str(), (INT)text.size(), &bold, box, &fmt, &brush);
        return;
    }

    Gdiplus::SolidBrush brush(color);
    Gdiplus::StringFormat fmt;
    fmt.SetAlignment(align);
    fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    fmt.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
    if (ellipsis)
        fmt.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
    Gdiplus::RectF box((Gdiplus::REAL)rc.left, (Gdiplus::REAL)rc.top,
                       (Gdiplus::REAL)(rc.right - rc.left),
                       (Gdiplus::REAL)(rc.bottom - rc.top));
    g.DrawString(text.c_str(), (INT)text.size(), &font, box, &fmt, &brush);
}

// ---------------------------------------------------------------------------
//  待机椭圆（GDI+ 绘制，逐像素 Alpha）
// ---------------------------------------------------------------------------
static void PaintDock(Gdiplus::Graphics& g, int w, int h)
{
    using namespace Gdiplus;

    if (w <= 0 || h <= 0)
        return;

    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);

    // 主色：悬停 / 按下
    COLORREF base = COLOR_BLUE;
    if (g_dockPressed)
        base = COLOR_BLUE_DARK;
    else if (g_dockHover || g_hoverT > 0.01)
        base = COLOR_BLUE_HOVER;

    // 竖向渐变（上亮下深）
    RectF box(0.0f, 0.0f, (REAL)w, (REAL)h);
    LinearGradientBrush grad(
        box,
        GP(COLOR_BLUE_LIGHT),
        GP(GetRValue(base) * 86 / 100, GetGValue(base) * 86 / 100, GetBValue(base) * 86 / 100),
        LinearGradientModeVertical);

    g.FillEllipse(&grad, box);

    // 外描边
    {
        Pen pen(GP(COLOR_BLUE_DARK), 1.0f);
        g.DrawEllipse(&pen, RectF(0.5f, 0.5f, (REAL)w - 1.0f, (REAL)h - 1.0f));
    }

    // 呼吸光圈
    {
        double phase = g_breath;
        double rr = LerpD(0.30, 0.50, phase);
        REAL penW = (REAL)(S(2) * (1.0 - phase * 0.7)) + 1.0f;

        BYTE a = (BYTE)(120 * (1.0 - phase));
        if (a > 0)
        {
            Pen pen(Color(a, 255, 255, 255), penW);
            REAL rx = (REAL)(w * rr);
            REAL ry = (REAL)(h * rr);
            g.DrawEllipse(&pen, RectF((REAL)w / 2 - rx, (REAL)h / 2 - ry, rx * 2, ry * 2));
        }
    }

    // 人物简笔画：头 + 肩，纯白
    {
        double scale = 1.0 + 0.06 * g_hoverT + 0.03 * (g_breath * 2.0 - 1.0);
        REAL side = (REAL)(w * DOCK_ICON_RATIO * scale);
        if (side < 6.0f) side = 6.0f;

        REAL cx = (REAL)w / 2.0f;
        REAL cy = (REAL)h / 2.0f;
        REAL unit = side;

        SolidBrush white(Color(255, 255, 255, 255));

        REAL headR  = unit * 0.150f;
        REAL headCy = cy - unit * 0.150f;
        g.FillEllipse(&white, cx - headR, headCy - headR, headR * 2, headR * 2);

        REAL bodyRx = unit * 0.290f;
        REAL bodyRy = unit * 0.255f;
        REAL bodyCy = cy + unit * 0.300f;
        g.FillEllipse(&white, cx - bodyRx, bodyCy - bodyRy, bodyRx * 2, bodyRy * 2);
    }
}

// ---------------------------------------------------------------------------
//  卡片：小鲸鱼插画 + 气泡内 UI
// ---------------------------------------------------------------------------

// 把插画按窗口尺寸等比缩放并缓存（窗口尺寸变化时才重建）
static void EnsureWhaleScaled(int w, int h)
{
    if (g_whaleBitmap == NULL || w <= 0 || h <= 0)
        return;
    if (g_whaleScaled != NULL && g_whaleScaledW == w && g_whaleScaledH == h)
        return;

    if (g_whaleScaled != NULL)
    {
        delete g_whaleScaled;
        g_whaleScaled = NULL;
    }

    g_whaleScaled = new Gdiplus::Bitmap(w, h, PixelFormat32bppARGB);
    if (g_whaleScaled == NULL)
        return;

    Gdiplus::Graphics g(g_whaleScaled);
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    g.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
    g.DrawImage(g_whaleBitmap, 0, 0, w, h);

    g_whaleScaledW = w;
    g_whaleScaledH = h;
}

static void PaintCard(Gdiplus::Graphics& g, int w, int h)
{
    using namespace Gdiplus;

    if (w <= 0 || h <= 0)
        return;

    CardLayout L = LayoutFor(w, h);

    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);

    // 字号统一按气泡垂直半轴换算，改卡片尺寸/DPI 时排版自动跟着变
    int ry = L.bubbleRy;

    // ---------- 1. 背景：整张插画 ----------
    EnsureWhaleScaled(w, h);
    if (g_whaleScaled != NULL)
    {
        g.SetCompositingMode(CompositingModeSourceOver);
        g.DrawImage(g_whaleScaled, 0, 0, w, h);
    }

    // ---------- 2. 气泡内白色底 ----------
    {
        REAL cx = (REAL)((L.bubble.left + L.bubble.right) / 2);
        REAL cy = (REAL)((L.bubble.top + L.bubble.bottom) / 2);
        REAL rx = (REAL)L.bubbleRx * (REAL)BUBBLE_FILL_INSET;
        REAL ry = (REAL)L.bubbleRy * (REAL)BUBBLE_FILL_INSET;

        SolidBrush white(GP(COLOR_CARD_BG));
        g.FillEllipse(&white, cx - rx, cy - ry, rx * 2, ry * 2);
    }

    // ---------- 3. 姓名 ----------
    {
        std::wstring text;
        Gdiplus::Color textColor = GP(COLOR_TEXT_MAIN);
        int basePx = (int)(ry * NAME_FONT_RATIO);
        bool isName = false;

        if (g_names.empty())
        {
            text      = L"未找到 names.txt";
            textColor = GP(COLOR_TEXT_SUB);
            basePx    = (int)(ry * HINT_FONT_RATIO);
        }
        else if (g_displayIndex >= 0 && g_displayIndex < (int)g_names.size())
        {
            text   = g_names[g_displayIndex];
            isName = true;
        }
        else
        {
            text      = L"点击下方按钮";
            textColor = GP(COLOR_TEXT_SUB);
            basePx    = (int)(ry * HINT_FONT_RATIO);
        }

        if (isName)
        {
            HDC screen = GetDC(NULL);
            if (screen != NULL)
            {
                int available = (L.name.right - L.name.left) - S(8);
                basePx = FitFontPx(screen, text, available,
                                   basePx, S(FONT_SIZE_NAME_MIN));
                ReleaseDC(NULL, screen);
            }
        }

        // 定格弹跳：字号瞬间放大再回落
        if (g_popActive && isName)
            basePx = (int)(basePx * (1.0 + 0.22 * (1.0 - EaseOutCubic(Clamp01(g_popT)))));

        DrawTextG(g, L.name, text, textColor, FONT_FACE, basePx, FW_BOLD,
                  StringAlignmentCenter, true);
    }

    // ---------- 4. 蓝色「点名」按钮 ----------
    {
        COLORREF buttonColor = COLOR_BLUE;
        if (g_rolling)
            buttonColor = COLOR_BLUE_DISABLED;
        else if (g_buttonDown)
            buttonColor = COLOR_BLUE_DARK;
        else if (g_buttonHover)
            buttonColor = COLOR_BLUE_HOVER;

        int buttonRadius = (L.button.bottom - L.button.top) / 2;
        FillRoundRectG(g, L.button, buttonRadius, GP(buttonColor));

        const wchar_t* buttonText = g_rolling ? L"抽取中…" : L"点 名";
        DrawTextG(g, L.button, buttonText, GP(COLOR_BUTTON_TEXT), FONT_FACE,
                  (int)(ry * BUTTON_FONT_RATIO), FW_BOLD,
                  StringAlignmentCenter, false);
    }
}

// 动画期间：把整张卡片渲染到全尺寸位图，再等比缩放居中到当前窗口
static void PaintCardScaled(Gdiplus::Graphics& g, int curW, int curH)
{
    int fullW = S(CARD_WIDTH);
    int fullH = fullW;
    if (fullW < 8) fullW = 8;
    if (fullH < 8) fullH = 8;

    double sx = (double)curW / (double)fullW;
    double sy = (double)curH / (double)fullH;
    double s  = (sx < sy) ? sx : sy;

    int drawW = (int)(fullW * s + 0.5);
    int drawH = (int)(fullH * s + 0.5);
    if (drawW < 1) drawW = 1;
    if (drawH < 1) drawH = 1;

    int dx = (curW - drawW) / 2;
    int dy = (curH - drawH) / 2;

    Gdiplus::Bitmap buffer(fullW, fullH, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics bg(&buffer);
        bg.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
        bg.Clear(Gdiplus::Color(0, 0, 0, 0));
        bg.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
        PaintCard(bg, fullW, fullH);
    }

    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    g.DrawImage(&buffer, dx, dy, drawW, drawH);
}

// ---------------------------------------------------------------------------
//  逐像素 Alpha 渲染：画到 32 位 DIB，再用 UpdateLayeredWindow 提交
// ---------------------------------------------------------------------------
static void ReleaseMemSurface()
{
    if (g_memDC != NULL)
    {
        if (g_memOldBmp != NULL)
            SelectObject(g_memDC, g_memOldBmp);
        DeleteDC(g_memDC);
        g_memDC = NULL;
    }
    if (g_memBmp != NULL)
    {
        DeleteObject(g_memBmp);
        g_memBmp = NULL;
    }
    g_memOldBmp = NULL;
    g_memW = 0;
    g_memH = 0;
}

// 确保离屏 DIB 尺寸与窗口一致（32bpp 预乘 Alpha）
static bool EnsureMemSurface(HDC refDC, int w, int h)
{
    if (w <= 0 || h <= 0)
        return false;

    if (g_memDC != NULL && g_memW == w && g_memH == h)
        return true;

    ReleaseMemSurface();

    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = w;
    bmi.bmiHeader.biHeight      = -h;          // 自上而下
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = NULL;
    g_memBmp = CreateDIBSection(refDC, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (g_memBmp == NULL || bits == NULL)
    {
        if (g_memBmp != NULL)
        {
            DeleteObject(g_memBmp);
            g_memBmp = NULL;
        }
        return false;
    }

    g_memDC = CreateCompatibleDC(refDC);
    if (g_memDC == NULL)
    {
        DeleteObject(g_memBmp);
        g_memBmp = NULL;
        return false;
    }

    g_memOldBmp = SelectObject(g_memDC, g_memBmp);
    g_memW = w;
    g_memH = h;
    return true;
}

// 把整窗内容画进离屏缓冲并提交给分层窗口
static void RenderFrame(int x, int y, int w, int h, int alpha)
{
    if (w <= 0 || h <= 0)
        return;

    HDC screen = GetDC(NULL);
    if (screen == NULL)
        return;

    if (EnsureMemSurface(screen, w, h))
    {
        {
            // GDI+ 直接在 DIB 上作画，Alpha 通道由 GDI+ 维护
            Gdiplus::Graphics g(g_memDC);
            g.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
            g.Clear(Gdiplus::Color(0, 0, 0, 0));
            g.SetCompositingMode(Gdiplus::CompositingModeSourceOver);

            if (g_anim.kind == ANIM_EXPAND || g_anim.kind == ANIM_COLLAPSE)
                PaintCardScaled(g, w, h);
            else if (g_expanded)
                PaintCard(g, w, h);
            else
                PaintDock(g, w, h);
        }

        GdiFlush();

        POINT dst;
        dst.x = x;
        dst.y = y;
        SIZE size;
        size.cx = w;
        size.cy = h;
        POINT src;
        src.x = 0;
        src.y = 0;

        BLENDFUNCTION bf;
        bf.BlendOp             = AC_SRC_OVER;
        bf.BlendFlags          = 0;
        bf.SourceConstantAlpha = (BYTE)ClampInt(alpha, 0, 255);
        bf.AlphaFormat         = AC_SRC_ALPHA;

        UpdateLayeredWindow(g_hWnd, screen, &dst, &size,
                            g_memDC, &src, 0, &bf, ULW_ALPHA);
    }

    ReleaseDC(NULL, screen);
}

static void OnPaint(HWND hwnd)
{
    // 分层窗口由 UpdateLayeredWindow 提交内容，WM_PAINT 只做确认
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    (void)dc;
    EndPaint(hwnd, &ps);
}

// ============================================================================
//  六、窗口形态与动画
// ============================================================================

static RECT ComputeDockRect()
{
    RECT work = WorkAreaRect();
    int w = S(DOCK_WIDTH);
    int h = S(DOCK_HEIGHT);

    int x = work.right - w - S(DOCK_MARGIN_RIGHT);
    int y;

    if (g_cfg.dockY < 0)
        y = work.top + ((work.bottom - work.top) - h) / 2;
    else
        y = g_cfg.dockY;

    y = ClampInt(y, work.top, work.bottom - h);
    if (x < work.left) x = work.left;

    RECT r;
    r.left   = x;
    r.top    = y;
    r.right  = x + w;
    r.bottom = y + h;
    return r;
}

// 卡片边长：设计尺寸，但在小屏 / 高缩放下自动收缩，保证整窗不出屏
static int CardSizePx()
{
    RECT work = WorkAreaRect();
    int availW = work.right - work.left - S(CARD_RIGHT_MARGIN) * 2;
    int availH = work.bottom - work.top - S(16);

    int size = S(CARD_WIDTH);
    if (size > availW) size = availW;
    if (size > availH) size = availH;

    int minSize = S(260);
    if (size < minSize) size = minSize;
    return size;
}

static RECT ComputeCardRect()
{
    RECT work = WorkAreaRect();
    RECT dock = ComputeDockRect();

    int size = CardSizePx();
    int w = size;
    int h = size;

    int x = work.right - w - S(CARD_RIGHT_MARGIN);
    int dockCy = (dock.top + dock.bottom) / 2;
    int y = dockCy - h / 2;

    x = ClampInt(x, work.left, work.right - w);
    y = ClampInt(y, work.top, work.bottom - h);

    RECT r;
    r.left   = x;
    r.top    = y;
    r.right  = x + w;
    r.bottom = y + h;
    return r;
}

// 提交一帧：定位窗口 + 重绘整窗内容（逐像素 Alpha，无需窗口区域）
static void ApplyWindowRect(const RECT& rc, int radius, int alpha)
{
    (void)radius;      // 逐像素 Alpha 下不需要圆角区域，形变靠绘制内容本身

    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w < 1) w = 1;
    if (h < 1) h = 1;

    SetWindowPos(g_hWnd, HWND_TOPMOST, rc.left, rc.top, w, h,
                 SWP_NOACTIVATE | SWP_NOOWNERZORDER);

    RenderFrame(rc.left, rc.top, w, h, alpha);
}

static void SetDockRegion()
{
    // 待机椭圆靠逐像素 Alpha 出形，无需 SetWindowRgn
    RECT rc = ComputeDockRect();
    RenderFrame(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, 255);
}

static void SetCardRegion()
{
    RECT rc = ComputeCardRect();
    RenderFrame(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, 255);
}

// 分层窗口的内容由 UpdateLayeredWindow 提交，InvalidateRect 不会触发重绘，
// 因此所有「刷新界面」的地方统一调用这里：取当前窗口矩形重新渲染一帧。
static void RepaintNow()
{
    if (g_hWnd == NULL)
        return;

    RECT rc;
    GetWindowRect(g_hWnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0)
        return;

    RenderFrame(rc.left, rc.top, w, h, 255);
}

static void StartAnim(AnimKind kind, const RECT& from, const RECT& to,
                      int radiusFrom, int radiusTo, int duration,
                      int alphaFrom, int alphaTo)
{
    g_anim.kind       = kind;
    g_anim.start      = GetTickCount64();
    g_anim.duration   = (duration < 40) ? 40 : duration;
    g_anim.from       = from;
    g_anim.to         = to;
    g_anim.radiusFrom = radiusFrom;
    g_anim.radiusTo   = radiusTo;
    g_anim.alphaFrom  = alphaFrom;
    g_anim.alphaTo    = alphaTo;

    SetTimer(g_hWnd, TIMER_ANIM, ANIM_FRAME_MS, NULL);
}

static void StopAnimTimer()
{
    KillTimer(g_hWnd, TIMER_ANIM);
    g_anim.kind = ANIM_NONE;
}

static void StartIdleTimer()
{
    SetTimer(g_hWnd, TIMER_IDLE, 40, NULL);
}

static void StopIdleTimer()
{
    KillTimer(g_hWnd, TIMER_IDLE);
}

static void BeginExpandAnimation()
{
    if (g_expanded || g_anim.kind == ANIM_EXPAND)
        return;

    StopIdleTimer();
    g_dockHover = false;
    g_hoverT    = 0.0;

    RECT from = ComputeDockRect();
    RECT to   = ComputeCardRect();

    StartAnim(ANIM_EXPAND, from, to,
              S(DOCK_HEIGHT) / 2, S(DOCK_HEIGHT) / 2,
              g_cfg.animMs, 90, 255);

    SetForegroundWindow(g_hWnd);
    SetFocus(g_hWnd);
}

static void BeginCollapseAnimation()
{
    if (!g_expanded || g_anim.kind == ANIM_COLLAPSE)
        return;

    RECT from;
    GetWindowRect(g_hWnd, &from);
    RECT to = ComputeDockRect();

    StartAnim(ANIM_COLLAPSE, from, to,
              S(DOCK_HEIGHT) / 2, S(DOCK_HEIGHT) / 2,
              ANIM_COLLAPSE_MS, 255, 40);
}

static void FinishExpand()
{
    g_expanded      = true;
    g_buttonHover   = false;

    RECT rc = ComputeCardRect();
    SetWindowPos(g_hWnd, HWND_TOPMOST, rc.left, rc.top,
                 rc.right - rc.left, rc.bottom - rc.top,
                 SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    SetCardRegion();
    StopAnimTimer();
    RepaintNow();
}

static void FinishCollapse()
{
    g_expanded = false;
    g_buttonHover   = false;
    g_buttonDown    = false;

    RECT rc = ComputeDockRect();
    SetWindowPos(g_hWnd, HWND_TOPMOST, rc.left, rc.top,
                 rc.right - rc.left, rc.bottom - rc.top,
                 SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    SetDockRegion();
    StopAnimTimer();
    StartIdleTimer();
    RepaintNow();
}

static void AnimTick()
{
    if (g_anim.kind == ANIM_NONE)
    {
        StopAnimTimer();
        return;
    }

    ULONGLONG now = GetTickCount64();
    double t = (double)(now - g_anim.start) / (double)g_anim.duration;
    if (t > 1.0) t = 1.0;

    if (g_anim.kind == ANIM_POP)
    {
        g_popT = t;
        if (t >= 1.0)
        {
            g_popActive = false;
            g_popT = 1.0;
            StopAnimTimer();
        }
        RepaintNow();
        return;
    }

    double e;
    if (g_anim.kind == ANIM_EXPAND)
        e = EaseOutBack(t);
    else if (g_anim.kind == ANIM_COLLAPSE)
        e = EaseInCubic(t);
    else
        e = EaseOutCubic(t);

    RECT rc = LerpRect(g_anim.from, g_anim.to, e);

    // 防止缓动过冲把窗口推到屏幕外
    RECT work = WorkAreaRect();
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (rc.left < work.left)                 rc.left = work.left;
    if (rc.top < work.top)                   rc.top = work.top;
    if (rc.left + w > work.right)            rc.left = work.right - w;
    if (rc.top + h > work.bottom)            rc.top = work.bottom - h;
    rc.right  = rc.left + w;
    rc.bottom = rc.top + h;

    int radius = LerpI(g_anim.radiusFrom, g_anim.radiusTo, Clamp01(e));
    int alpha  = LerpI(g_anim.alphaFrom, g_anim.alphaTo, Clamp01(t));

    ApplyWindowRect(rc, radius, alpha);
    RepaintNow();

    if (t >= 1.0)
    {
        if (g_anim.kind == ANIM_EXPAND)
            FinishExpand();
        else if (g_anim.kind == ANIM_COLLAPSE)
            FinishCollapse();
        else
        {
            StopAnimTimer();
            RepaintNow();
        }
    }
}

static void IdleTick()
{
    if (g_expanded)
    {
        StopIdleTimer();
        return;
    }

    // 呼吸相位
    ULONGLONG now = GetTickCount64();
    g_breath = (double)(now % DOCK_BREATH_MS) / (double)DOCK_BREATH_MS;

    // 悬停过渡
    double target = g_dockHover ? 1.0 : 0.0;
    if (fabs(target - g_hoverT) > 0.001)
    {
        double step = 0.18;
        if (g_hoverT < target) g_hoverT = (g_hoverT + step > 1.0) ? 1.0 : g_hoverT + step;
        else                   g_hoverT = (g_hoverT - step < 0.0) ? 0.0 : g_hoverT - step;
    }

    RepaintNow();
}

// ============================================================================
//  七、音效（waveOut 实时合成，不依赖任何音频文件）
// ============================================================================

#define VOICE_COUNT 16
#define AUDIO_RATE  44100

struct Voice
{
    WAVEHDR       hdr;
    short*        pcm;
    volatile LONG state;   // 0 = 空闲, 1 = 播放中, 2 = 已播完
};

static HWAVEOUT g_wave       = NULL;
static bool     g_audioReady = false;
static Voice    g_voices[VOICE_COUNT];

static void CALLBACK WaveOutProc(HWAVEOUT hwo, UINT msg, DWORD_PTR inst,
                                 DWORD_PTR p1, DWORD_PTR p2)
{
    (void)hwo; (void)inst; (void)p2;
    if (msg != WOM_DONE)
        return;

    WAVEHDR* hdr = (WAVEHDR*)p1;
    for (int i = 0; i < VOICE_COUNT; ++i)
    {
        if (&g_voices[i].hdr == hdr)
        {
            InterlockedExchange(&g_voices[i].state, 2);
            break;
        }
    }
}

static void InitAudio()
{
    ZeroMemory(g_voices, sizeof(g_voices));

    WAVEFORMATEX wf;
    ZeroMemory(&wf, sizeof(wf));
    wf.wFormatTag      = WAVE_FORMAT_PCM;
    wf.nChannels       = 1;
    wf.nSamplesPerSec  = AUDIO_RATE;
    wf.wBitsPerSample  = 16;
    wf.nBlockAlign     = (WORD)(wf.nChannels * wf.wBitsPerSample / 8);
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;

    MMRESULT r = waveOutOpen(&g_wave, WAVE_MAPPER, &wf,
                             (DWORD_PTR)WaveOutProc, 0, CALLBACK_FUNCTION);
    if (r != MMSYSERR_NOERROR)
    {
        g_wave       = NULL;
        g_audioReady = false;   // 无声卡时静默降级
        return;
    }

    g_audioReady = true;
    SetTimer(g_hWnd, TIMER_REAP, 300, NULL);
}

static void ShutdownAudio()
{
    if (!g_audioReady)
        return;

    if (g_hWnd != NULL)
        KillTimer(g_hWnd, TIMER_REAP);

    waveOutReset(g_wave);

    for (int i = 0; i < VOICE_COUNT; ++i)
    {
        Voice& v = g_voices[i];
        if (v.pcm != NULL)
        {
            waveOutUnprepareHeader(g_wave, &v.hdr, sizeof(WAVEHDR));
            free(v.pcm);
            v.pcm = NULL;
        }
        v.state = 0;
    }

    waveOutClose(g_wave);
    g_wave       = NULL;
    g_audioReady = false;
}

static void ReapVoices()
{
    if (!g_audioReady)
        return;

    for (int i = 0; i < VOICE_COUNT; ++i)
    {
        Voice& v = g_voices[i];
        if (v.pcm != NULL && v.state == 2)
        {
            waveOutUnprepareHeader(g_wave, &v.hdr, sizeof(WAVEHDR));
            free(v.pcm);
            v.pcm   = NULL;
            v.state = 0;
        }
    }
}

// 把一段正弦 / 噪声写进缓冲区
static void RenderTone(short* buf, int totalSamples, int startSample,
                       int durSamples, double f0, double f1,
                       double amp, int wave, double attackMs)
{
    if (startSample < 0) startSample = 0;
    if (startSample >= totalSamples) return;
    if (startSample + durSamples > totalSamples)
        durSamples = totalSamples - startSample;
    if (durSamples <= 0) return;

    double phase    = 0.0;
    int    attack   = (int)(attackMs * AUDIO_RATE / 1000.0);
    if (attack < 1) attack = 1;
    if (attack > durSamples) attack = durSamples;

    unsigned int seed = 0x12345u;

    for (int i = 0; i < durSamples; ++i)
    {
        double p = (double)i / (double)durSamples;
        double f = f0 + (f1 - f0) * p;
        phase += 2.0 * 3.14159265358979323846 * f / AUDIO_RATE;
        if (phase > 6.283185307179586) phase -= 6.283185307179586;

        double s;
        if (wave == 1)                     // 噪声（用于「哒」的点击感）
        {
            seed = seed * 1664525u + 1013904223u;
            s = ((double)((seed >> 16) & 0x7FFF) / 16383.5) - 1.0;
        }
        else if (wave == 2)                // 三角波（更柔和的提示音）
        {
            double x = phase / 6.283185307179586;
            s = 4.0 * fabs(x - floor(x + 0.5)) - 1.0;
        }
        else                               // 正弦
        {
            s = sin(phase);
        }

        double env;
        if (i < attack)
            env = (double)i / (double)attack;
        else
        {
            double q = (double)(i - attack) / (double)(durSamples - attack);
            env = exp(-4.5 * q);
        }

        int idx = startSample + i;
        double mix = buf[idx] / 32767.0 + s * env * amp;
        if (mix > 1.0)  mix = 1.0;
        if (mix < -1.0) mix = -1.0;
        buf[idx] = (short)(mix * 32767.0);
    }
}

static void PlayBuffer(short* pcm, int samples)
{
    if (!g_audioReady || pcm == NULL)
    {
        if (pcm != NULL) free(pcm);
        return;
    }

    for (int i = 0; i < VOICE_COUNT; ++i)
    {
        Voice& v = g_voices[i];
        if (InterlockedCompareExchange(&v.state, 1, 0) != 0)
            continue;

        v.pcm = pcm;
        ZeroMemory(&v.hdr, sizeof(WAVEHDR));
        v.hdr.lpData         = (LPSTR)pcm;
        v.hdr.dwBufferLength = (DWORD)(samples * sizeof(short));

        if (waveOutPrepareHeader(g_wave, &v.hdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
        {
            free(pcm);
            v.pcm   = NULL;
            v.state = 0;
            return;
        }

        if (waveOutWrite(g_wave, &v.hdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
        {
            waveOutUnprepareHeader(g_wave, &v.hdr, sizeof(WAVEHDR));
            free(pcm);
            v.pcm   = NULL;
            v.state = 0;
            return;
        }
        return;
    }

    free(pcm);   // 没有空闲声道，直接丢弃
}

static short* AllocBuffer(int samples)
{
    short* buf = (short*)calloc((size_t)samples, sizeof(short));
    return buf;
}

// ---- 具体音效 ----

static void SoundPop()          // 展开
{
    if (!g_cfg.sound) return;
    double vol = g_cfg.volume / 100.0 * 0.55;
    int n = AUDIO_RATE * 110 / 1000;
    short* b = AllocBuffer(n);
    if (!b) return;
    RenderTone(b, n, 0, n, 380.0, 820.0, vol, 0, 2.0);
    PlayBuffer(b, n);
}

static void SoundCollapse()     // 收起
{
    if (!g_cfg.sound) return;
    double vol = g_cfg.volume / 100.0 * 0.45;
    int n = AUDIO_RATE * 120 / 1000;
    short* b = AllocBuffer(n);
    if (!b) return;
    RenderTone(b, n, 0, n, 700.0, 320.0, vol, 0, 2.0);
    PlayBuffer(b, n);
}

static void SoundClick()        // 按钮按下
{
    if (!g_cfg.sound) return;
    double vol = g_cfg.volume / 100.0 * 0.40;
    int n = AUDIO_RATE * 40 / 1000;
    short* b = AllocBuffer(n);
    if (!b) return;
    RenderTone(b, n, 0, n, 1200.0, 900.0, vol, 1, 1.0);
    PlayBuffer(b, n);
}

static void SoundTick()         // 滚动中的「哒」
{
    if (!g_cfg.sound) return;

    ULONGLONG now = GetTickCount64();
    if (now - g_lastTickSound < 42)
        return;
    g_lastTickSound = now;

    double vol = g_cfg.volume / 100.0 * 0.22;
    int n = AUDIO_RATE * 18 / 1000;
    short* b = AllocBuffer(n);
    if (!b) return;
    RenderTone(b, n, 0, n, 1600.0, 1400.0, vol, 1, 0.5);
    PlayBuffer(b, n);
}

static void SoundResult()       // 定格提示音（两音上行）
{
    if (!g_cfg.sound) return;
    double vol = g_cfg.volume / 100.0 * 0.55;
    int n = AUDIO_RATE * 420 / 1000;
    short* b = AllocBuffer(n);
    if (!b) return;

    int seg1 = AUDIO_RATE * 130 / 1000;
    int seg2 = AUDIO_RATE * 260 / 1000;
    RenderTone(b, n, 0,      seg1, 660.0,  660.0, vol, 2, 4.0);
    RenderTone(b, n, seg1,   seg2, 990.0,  990.0, vol, 2, 4.0);
    RenderTone(b, n, seg1,   seg2, 1320.0, 1320.0, vol * 0.5, 0, 4.0);
    PlayBuffer(b, n);
}

// ============================================================================
//  八、托盘
// ============================================================================

// 用软件光栅生成带 Alpha 的托盘图标：蓝色圆 + 白色人形
static HICON CreateTrayIcon(int size)
{
    if (size < 8) size = 16;

    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = size;
    bmi.bmiHeader.biHeight      = -size;      // 自上而下
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = NULL;
    HDC screen = GetDC(NULL);
    HBITMAP color = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    ReleaseDC(NULL, screen);
    if (color == NULL || bits == NULL)
    {
        if (color != NULL) DeleteObject(color);
        return NULL;
    }

    const int SS = 4;
    const double inv = 1.0 / (SS * SS);
    BYTE* px = (BYTE*)bits;

    const double br = GetRValue(COLOR_BLUE);
    const double bg = GetGValue(COLOR_BLUE);
    const double bb = GetBValue(COLOR_BLUE);

    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            double accR = 0, accG = 0, accB = 0, cov = 0;

            for (int sy = 0; sy < SS; ++sy)
            {
                for (int sx = 0; sx < SS; ++sx)
                {
                    double u = (x + (sx + 0.5) / SS) / (double)size;
                    double v = (y + (sy + 0.5) / SS) / (double)size;

                    double dx = u - 0.5, dy = v - 0.5;
                    if (dx * dx + dy * dy > 0.25)
                        continue;

                    cov += 1.0;

                    bool person = false;
                    double hx = u - 0.5, hy = v - 0.345;
                    if (hx * hx + hy * hy <= 0.155 * 0.155)
                        person = true;

                    double ex = (u - 0.5) / 0.290;
                    double ey = (v - 0.880) / 0.300;
                    if (ex * ex + ey * ey <= 1.0)
                        person = true;

                    if (person) { accR += 255; accG += 255; accB += 255; }
                    else        { accR += br;  accG += bg;  accB += bb;  }
                }
            }

            if (cov <= 0.0)
            {
                px[0] = 0; px[1] = 0; px[2] = 0; px[3] = 0;
            }
            else
            {
                px[0] = (BYTE)(accB / cov);
                px[1] = (BYTE)(accG / cov);
                px[2] = (BYTE)(accR / cov);
                px[3] = (BYTE)(cov * inv * 255.0 + 0.5);
            }
            px += 4;
        }
    }

    // 掩码全 0（不透明），实际透明度交给 32 位 Alpha 通道
    std::vector<BYTE> maskBits(((size + 15) / 16) * 2 * size, 0);
    HBITMAP mask = CreateBitmap(size, size, 1, 1, &maskBits[0]);

    ICONINFO ii;
    ZeroMemory(&ii, sizeof(ii));
    ii.fIcon    = TRUE;
    ii.hbmMask  = mask;
    ii.hbmColor = color;

    HICON icon = CreateIconIndirect(&ii);

    DeleteObject(color);
    if (mask != NULL) DeleteObject(mask);
    return icon;
}

static void AddTrayIcon()
{
    if (g_trayIcon == NULL)
        g_trayIcon = CreateTrayIcon(32);
    if (g_trayIcon == NULL)
        return;

    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize           = sizeof(g_nid);
    g_nid.hWnd             = g_hWnd;
    g_nid.uID              = 1;
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon            = g_trayIcon;
    lstrcpynW(g_nid.szTip, L"随机点名 · 左键展开，右键菜单", 128);

    Shell_NotifyIconW(NIM_ADD, &g_nid);
    g_trayAdded = true;
}

static void UpdateTrayTip(const wchar_t* tip)
{
    if (!g_trayAdded) return;
    lstrcpynW(g_nid.szTip, tip, 128);
    g_nid.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

static void RemoveTrayIcon()
{
    if (!g_trayAdded) return;
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
    g_trayAdded = false;
}

// ============================================================================
//  九、配置读写（settings.ini）
// ============================================================================

// 前向声明：配置文件读取复用第十节的通用文件读取（实现见下）
static bool ReadAllBytes(const std::wstring& path, std::string& out);

static std::string TrimAscii(const std::string& s)
{
    size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n'))
        ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' || s[e - 1] == '\n'))
        --e;
    return s.substr(b, e - b);
}

static std::string LowerAscii(std::string s)
{
    for (size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] >= 'A' && s[i] <= 'Z')
            s[i] = (char)(s[i] - 'A' + 'a');
    }
    return s;
}

static std::string BuildSettingsText()
{
    char buf[256];
    std::string out;

    out += "; ============================================================\r\n";
    out += ";  随机点名 · 配置文件\r\n";
    out += ";  修改后保存，重新启动程序生效\r\n";
    out += "; ============================================================\r\n";
    out += "\r\n[general]\r\n";
    out += "; 音效开关：1 = 开，0 = 关\r\n";
    wsprintfA(buf, "sound=%d\r\n", g_cfg.sound ? 1 : 0);      out += buf;
    out += "; 音量：0 ~ 100\r\n";
    wsprintfA(buf, "volume=%d\r\n", g_cfg.volume);            out += buf;
    out += "; 开机自启：1 = 开，0 = 关\r\n";
    out += "; 本文件就是开关，程序启动时按这里的值同步注册表\r\n";
    wsprintfA(buf, "autostart=%d\r\n", g_cfg.autostart ? 1 : 0); out += buf;
    out += "\r\n[ui]\r\n";
    out += "; 待机椭圆条的垂直位置：-1 = 自动居中，其它值为屏幕 Y 坐标（拖动后自动写入）\r\n";
    wsprintfA(buf, "dock_y=%d\r\n", g_cfg.dockY);             out += buf;
    out += "; 弹出动画时长（毫秒），范围 80 ~ 1500\r\n";
    wsprintfA(buf, "anim_ms=%d\r\n", g_cfg.animMs);           out += buf;

    return out;
}

static bool SaveSettings()
{
    std::string text = BuildSettingsText();

    HANDLE hFile = CreateFileW(g_settingsPath.c_str(), GENERIC_WRITE, 0, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return false;

    DWORD written = 0;
    BOOL ok = WriteFile(hFile, text.c_str(), (DWORD)text.size(), &written, NULL);
    CloseHandle(hFile);
    return ok == TRUE;
}

static void LoadSettings()
{
    g_cfg.sound     = 1;
    g_cfg.volume    = 70;
    g_cfg.autostart = 1;
    g_cfg.dockY     = -1;
    g_cfg.animMs    = ANIM_EXPAND_MS;

    std::string raw;
    if (!ReadAllBytes(g_settingsPath, raw) || raw.empty())
    {
        SaveSettings();          // 首次运行：生成默认配置文件
        return;
    }

    // 去 BOM
    if (raw.size() >= 3 && (unsigned char)raw[0] == 0xEF &&
        (unsigned char)raw[1] == 0xBB && (unsigned char)raw[2] == 0xBF)
        raw.erase(0, 3);

    size_t pos = 0;
    while (pos < raw.size())
    {
        size_t eol = raw.find('\n', pos);
        std::string line = (eol == std::string::npos) ? raw.substr(pos)
                                                      : raw.substr(pos, eol - pos);
        pos = (eol == std::string::npos) ? raw.size() : eol + 1;

        line = TrimAscii(line);
        if (line.empty() || line[0] == ';' || line[0] == '#' || line[0] == '[')
            continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;

        std::string key = LowerAscii(TrimAscii(line.substr(0, eq)));
        std::string val = TrimAscii(line.substr(eq + 1));

        if (key == "sound")          g_cfg.sound     = (atoi(val.c_str()) != 0) ? 1 : 0;
        else if (key == "volume")    g_cfg.volume    = ClampInt(atoi(val.c_str()), 0, 100);
        else if (key == "autostart") g_cfg.autostart = (atoi(val.c_str()) != 0) ? 1 : 0;
        else if (key == "dock_y")    g_cfg.dockY     = atoi(val.c_str());
        else if (key == "anim_ms")   g_cfg.animMs    = ClampInt(atoi(val.c_str()), 80, 1500);
    }
}

// ============================================================================
//  十、名单读取（UTF-8 / ANSI 自动识别）
// ============================================================================

static std::wstring TrimW(const std::wstring& s)
{
    size_t begin = 0;
    size_t end   = s.size();
    while (begin < end)
    {
        wchar_t c = s[begin];
        if (c == L' ' || c == L'\t' || c == L'\r' || c == L'\n' ||
            c == (wchar_t)0x3000 || c == (wchar_t)0x00A0)
            ++begin;
        else
            break;
    }
    while (end > begin)
    {
        wchar_t c = s[end - 1];
        if (c == L' ' || c == L'\t' || c == L'\r' || c == L'\n' ||
            c == (wchar_t)0x3000 || c == (wchar_t)0x00A0)
            --end;
        else
            break;
    }
    return s.substr(begin, end - begin);
}

static bool ReadAllBytes(const std::wstring& path, std::string& out)
{
    out.clear();
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return false;

    LARGE_INTEGER size;
    size.QuadPart = 0;
    if (!GetFileSizeEx(hFile, &size) || size.QuadPart <= 0 || size.QuadPart > (8 * 1024 * 1024))
    {
        CloseHandle(hFile);
        return false;
    }

    out.resize((size_t)size.QuadPart);
    DWORD total = 0;
    while (total < (DWORD)out.size())
    {
        DWORD got = 0;
        if (!ReadFile(hFile, &out[total], (DWORD)(out.size() - total), &got, NULL) || got == 0)
            break;
        total += got;
    }
    out.resize(total);
    CloseHandle(hFile);
    return total > 0;
}

static bool DecodeUtf8(const std::string& bytes, std::wstring& out)
{
    out.clear();
    if (bytes.empty())
        return true;

    int need = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                   bytes.c_str(), (int)bytes.size(), NULL, 0);
    if (need <= 0)
        return false;

    out.resize(need);
    int got = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                  bytes.c_str(), (int)bytes.size(), &out[0], need);
    if (got <= 0)
    {
        out.clear();
        return false;
    }
    out.resize(got);
    return true;
}

static std::wstring DecodeAnsi(const std::string& bytes)
{
    if (bytes.empty())
        return std::wstring();

    int need = MultiByteToWideChar(CP_ACP, 0, bytes.c_str(), (int)bytes.size(), NULL, 0);
    if (need <= 0)
        return std::wstring();

    std::wstring out(need, L'\0');
    int got = MultiByteToWideChar(CP_ACP, 0, bytes.c_str(), (int)bytes.size(), &out[0], need);
    out.resize(got > 0 ? got : 0);
    return out;
}

static bool WriteTemplateNamesFile()
{
    static const char kTemplate[] =
        "\xEF\xBB\xBF"
        "张三\r\n"
        "李四\r\n"
        "王五\r\n"
        "赵六\r\n"
        "钱七\r\n"
        "孙八\r\n"
        "周九\r\n"
        "吴十\r\n";

    HANDLE hFile = CreateFileW(g_namesPath.c_str(), GENERIC_WRITE, 0, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return false;

    DWORD written = 0;
    BOOL  ok = WriteFile(hFile, kTemplate, (DWORD)strlen(kTemplate), &written, NULL);
    CloseHandle(hFile);
    return ok && written == (DWORD)strlen(kTemplate);
}

static void ShowMissingNamesHint()
{
    std::wstring msg =
        L"当前没有可用的点名名单。\n\n"
        L"请在程序所在目录放置 names.txt（每行一个姓名），例如：\n"
        L"    张三\n    李四\n    王五\n\n"
        L"保存后，右键托盘图标选择「重新加载名单」即可，无需重启程序。\n\n"
        L"名单路径：\n" + g_namesPath;
    MessageBoxW(NULL, msg.c_str(), L"随机点名 · 提示", MB_OK | MB_ICONINFORMATION);
}

static bool LoadNames(bool showErrors)
{
    g_names.clear();
    g_history.clear();
    g_displayIndex = -1;

    std::string raw;
    bool ok = ReadAllBytes(g_namesPath, raw);
    bool justCreated = false;

    if (!ok)
    {
#if AUTO_CREATE_NAMES_TEMPLATE
        justCreated = WriteTemplateNamesFile();
        if (justCreated)
            ok = ReadAllBytes(g_namesPath, raw);
#endif
    }

    if (!ok)
    {
        if (showErrors)
        {
            std::wstring msg =
                L"未能读取名单文件：\n" + g_namesPath +
                L"\n\n请确认程序所在目录下存在 names.txt，"
                L"每行一个姓名（UTF-8 或 ANSI 编码均可）。";
            MessageBoxW(NULL, msg.c_str(), L"随机点名 · 提示", MB_OK | MB_ICONWARNING);
        }
        return false;
    }

    std::wstring text;
    if (!DecodeUtf8(raw, text))
        text = DecodeAnsi(raw);

    if (!text.empty() && text[0] == (wchar_t)0xFEFF)
        text.erase(0, 1);

    size_t pos = 0;
    while (pos <= text.size())
    {
        size_t eol = text.find_first_of(L"\r\n", pos);
        std::wstring line =
            (eol == std::wstring::npos) ? text.substr(pos) : text.substr(pos, eol - pos);
        line = TrimW(line);

        if (!line.empty() && line[0] != L'#')
            g_names.push_back(line);

        if (eol == std::wstring::npos)
            break;

        pos = eol + 1;
        if (pos < text.size() && (text[pos] == L'\n' || text[pos] == L'\r'))
            ++pos;
    }

    if (g_names.empty())
    {
        if (showErrors)
        {
            std::wstring msg =
                L"名单文件为空：\n" + g_namesPath +
                L"\n\n请至少写入一行姓名（每行一个）。";
            MessageBoxW(NULL, msg.c_str(), L"随机点名 · 提示", MB_OK | MB_ICONWARNING);
        }
        return false;
    }

    if (justCreated && showErrors)
    {
        std::wstring msg =
            L"未找到 names.txt，已在程序目录自动生成示例名单：\n" + g_namesPath +
            L"\n\n请用记事本编辑成你自己的名单，"
            L"然后在托盘图标上点右键选择「重新加载名单」。";
        MessageBoxW(NULL, msg.c_str(), L"随机点名 · 提示", MB_OK | MB_ICONINFORMATION);
    }

    return true;
}

// ============================================================================
//  十一、随机抽取与滚动动画
// ============================================================================

static int RandInt(int count)
{
    if (count <= 1)
        return 0;
    std::uniform_int_distribution<int> dist(0, count - 1);
    return dist(g_rng);
}

static bool InHistory(int index)
{
    return std::find(g_history.begin(), g_history.end(), index) != g_history.end();
}

static int PickFinalIndex()
{
    int count = (int)g_names.size();
    if (count <= 0)
        return -1;

    bool exclude = (HISTORY_EXCLUDE_COUNT > 0) && (count > HISTORY_EXCLUDE_COUNT);

    std::vector<int> pool;
    for (int i = 0; i < count; ++i)
    {
        if (exclude && InHistory(i))
            continue;
        pool.push_back(i);
    }
    if (pool.empty())
    {
        for (int i = 0; i < count; ++i)
            pool.push_back(i);
    }

    return pool[RandInt((int)pool.size())];
}

static int PickRollingIndex()
{
    int count = (int)g_names.size();
    if (count <= 0)
        return -1;
    if (count == 1)
        return 0;

    for (int attempt = 0; attempt < 64; ++attempt)
    {
        int index = RandInt(count);
        if (index != g_rollLastShown && index != g_rollFinal)
            return index;
    }
    for (int i = 0; i < count; ++i)
    {
        if (i != g_rollLastShown && i != g_rollFinal)
            return i;
    }
    return RandInt(count);
}

static void PushHistory(int index)
{
    if (HISTORY_EXCLUDE_COUNT <= 0 || index < 0)
        return;

    g_history.push_back(index);
    while ((int)g_history.size() > HISTORY_EXCLUDE_COUNT)
        g_history.erase(g_history.begin());
}

static void FinishRoll()
{
    KillTimer(g_hWnd, TIMER_ROLL);
    g_rolling      = false;
    g_displayIndex = g_rollFinal;
    PushHistory(g_rollFinal);
    g_rollFinal = -1;

    // 定格弹跳 + 提示音
    g_popActive = true;
    g_popT      = 0.0;
    StartAnim(ANIM_POP, ClientRectOf(g_hWnd), ClientRectOf(g_hWnd), 0, 0, RESULT_POP_MS, 0, 0);
    SoundResult();

    RepaintNow();
}

static void RollTick()
{
    ULONGLONG now     = GetTickCount64();
    ULONGLONG elapsed = now - g_rollStart;

    if (elapsed >= (ULONGLONG)ROLL_DURATION_MS)
    {
        FinishRoll();
        return;
    }

    int shown = PickRollingIndex();
    if (shown >= 0)
    {
        g_displayIndex  = shown;
        g_rollLastShown = shown;
        SoundTick();
    }

    double progress = (double)elapsed / (double)ROLL_DURATION_MS;
    int interval = ROLL_INTERVAL_START_MS +
                   (int)((ROLL_INTERVAL_END_MS - ROLL_INTERVAL_START_MS) *
                         progress * progress);
    if (interval < 10)
        interval = 10;

    SetTimer(g_hWnd, TIMER_ROLL, (UINT)interval, NULL);
    RepaintNow();
}

static void StartRoll()
{
    if (g_rolling)
        return;
    if (g_names.empty())
    {
        ShowMissingNamesHint();
        return;
    }

    int finalIndex = PickFinalIndex();
    if (finalIndex < 0)
        return;

    g_rollFinal     = finalIndex;
    g_rollLastShown = -1;
    g_rolling       = true;
    g_popActive     = false;
    g_rollStart     = GetTickCount64();

    SoundClick();
    RollTick();
}

// ============================================================================
//  十二、开机自启动（HKCU，无需管理员权限）
// ============================================================================

static std::wstring GetExePath()
{
    std::vector<wchar_t> buffer(MAX_PATH);
    for (;;)
    {
        DWORD len = GetModuleFileNameW(NULL, &buffer[0], (DWORD)buffer.size());
        if (len == 0)
            return std::wstring();
        if (len < buffer.size())
            return std::wstring(&buffer[0], len);
        buffer.resize(buffer.size() * 2);
    }
}

static void EnsureAutoStart()
{
    std::wstring exePath = GetExePath();
    if (exePath.empty())
        return;

    std::wstring command = L"\"" + exePath + L"\"";

    HKEY hKey = NULL;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, RUN_KEY_PATH, 0, NULL, 0,
                        KEY_READ | KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS)
        return;

    bool already = false;
    wchar_t existing[1024];
    DWORD   type = 0;
    DWORD   cb   = sizeof(existing);
    if (RegQueryValueExW(hKey, AUTOSTART_VALUE_NAME, NULL, &type,
                         (LPBYTE)existing, &cb) == ERROR_SUCCESS &&
        type == REG_SZ)
    {
        already = (lstrcmpiW(existing, command.c_str()) == 0);
    }

    if (!already)
    {
        RegSetValueExW(hKey, AUTOSTART_VALUE_NAME, 0, REG_SZ,
                       (const BYTE*)command.c_str(),
                       (DWORD)((command.size() + 1) * sizeof(wchar_t)));
    }

    RegCloseKey(hKey);
}

static void RemoveAutoStart()
{
    HKEY hKey = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY_PATH, 0, KEY_SET_VALUE, &hKey)
        == ERROR_SUCCESS)
    {
        RegDeleteValueW(hKey, AUTOSTART_VALUE_NAME);
        RegCloseKey(hKey);
    }
}

static void ApplyAutostartSetting()
{
    if (g_cfg.autostart)
        EnsureAutoStart();
    else
        RemoveAutoStart();
}

// ============================================================================
//  十三、交互
// ============================================================================

static void TrackMouseLeave(HWND hwnd)
{
    if (g_trackingMouse)
        return;

    TRACKMOUSEEVENT tme;
    tme.cbSize      = sizeof(tme);
    tme.dwFlags     = TME_LEAVE;
    tme.hwndTrack   = hwnd;
    tme.dwHoverTime = 0;
    if (TrackMouseEvent(&tme))
        g_trackingMouse = true;
}

static void SaveDockPosition(int y)
{
    g_cfg.dockY = y;
    SaveSettings();
}

static void OnMouseMove(HWND hwnd, int x, int y)
{
    TrackMouseLeave(hwnd);

    // ---------- 拖动待机椭圆（仅上下） ----------
    if (g_dockDragging)
    {
        POINT cur;
        GetCursorPos(&cur);
        int dy = cur.y - g_dragCursorStart.y;
        if (!g_dockMoved && abs(dy) > S(3))
            g_dockMoved = true;

        if (g_dockMoved)
        {
            RECT work = WorkAreaRect();
            int h = S(DOCK_HEIGHT);
            int newTop = ClampInt(g_dockDragStartY + dy, work.top, work.bottom - h);
            g_cfg.dockY = newTop;
            RECT rc = ComputeDockRect();
            SetWindowPos(hwnd, HWND_TOPMOST, rc.left, rc.top, 0, 0,
                         SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
        }
        return;
    }

    // ---------- 拖动卡片 ----------
    if (g_cardDragging)
    {
        POINT cur;
        GetCursorPos(&cur);
        int dx = cur.x - g_dragCursorStart.x;
        int dy = cur.y - g_dragCursorStart.y;

        if (!g_cardMoved && (dx * dx + dy * dy) > S(4) * S(4))
            g_cardMoved = true;

        if (g_cardMoved)
        {
            RECT rc = g_dragWindowStart;
            OffsetRect(&rc, dx, dy);

            RECT work = WorkAreaRect();
            int w = rc.right - rc.left;
            int h = rc.bottom - rc.top;
            rc.left = ClampInt(rc.left, work.left, work.right - w);
            rc.top  = ClampInt(rc.top,  work.top,  work.bottom - h);
            rc.right  = rc.left + w;
            rc.bottom = rc.top + h;

            SetWindowPos(hwnd, HWND_TOPMOST, rc.left, rc.top, 0, 0,
                         SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
        }
        return;
    }

    // ---------- 待机条悬停 ----------
    if (!g_expanded)
    {
        if (!g_dockHover)
        {
            g_dockHover = true;
            RepaintNow();
        }
        return;
    }

    // ---------- 卡片悬停 ----------
    RECT client = ClientRectOf(hwnd);
    CardLayout L = LayoutFor(client.right, client.bottom);

    POINT pt;
    pt.x = x;
    pt.y = y;

    bool overButton = (PtInRect(&L.button, pt) != 0);

    if (overButton != g_buttonHover)
    {
        g_buttonHover = overButton;
        RepaintNow();
    }

    // 气泡之外是人物本体，悬停时给出「可点击收起」的手型光标
    bool overBody = !PointInBubble(x, y, L);
    if (overBody != g_bodyHover)
    {
        g_bodyHover = overBody;
        RepaintNow();
    }
}

static void OnLButtonDown(HWND hwnd, int x, int y)
{
    SetFocus(hwnd);

    POINT pt;
    pt.x = x;
    pt.y = y;

    // ---------- 待机椭圆：按下，可能是拖动或点击 ----------
    if (!g_expanded)
    {
        g_dockPressed  = true;
        g_dockDragging = true;
        g_dockMoved    = false;
        GetCursorPos(&g_dragCursorStart);
        g_dockDragStartY = ComputeDockRect().top;
        SetCapture(hwnd);
        RepaintNow();
        return;
    }

    if (g_anim.kind == ANIM_EXPAND || g_anim.kind == ANIM_COLLAPSE)
        return;   // 动画中忽略点击

    RECT client = ClientRectOf(hwnd);
    CardLayout L = LayoutFor(client.right, client.bottom);

    // ---------- 点名按钮 ----------
    if (PtInRect(&L.button, pt))
    {
        if (!g_rolling)
        {
            g_buttonDown = true;
            SetCapture(hwnd);
            RepaintNow();
        }
        return;
    }

    // ---------- 其余区域（气泡外的人物本体 / 气泡空白）：按下先当作拖动 ----------
    g_cardDragging = true;
    g_cardMoved    = false;
    GetCursorPos(&g_dragCursorStart);
    GetWindowRect(hwnd, &g_dragWindowStart);
    SetCapture(hwnd);
}

static void OnLButtonUp(HWND hwnd, int x, int y)
{
    POINT pt;
    pt.x = x;
    pt.y = y;

    bool wasDockDown    = g_dockPressed;
    bool wasDockDrag    = g_dockDragging;
    bool wasDockMoved   = g_dockMoved;
    bool wasButtonDown  = g_buttonDown;
    bool wasCardDrag    = g_cardDragging;
    bool wasCardMoved   = g_cardMoved;

    g_dockPressed  = false;
    g_dockDragging = false;
    g_dockMoved    = false;
    g_buttonDown   = false;
    g_cardDragging = false;
    g_cardMoved    = false;

    if (GetCapture() == hwnd)
        ReleaseCapture();

    // ---------- 待机椭圆 ----------
    if (wasDockDown || wasDockDrag)
    {
        RepaintNow();
        if (wasDockMoved)
        {
            // 拖动结束：记住位置
            SaveDockPosition(ComputeDockRect().top);
        }
        else if (!g_expanded)
        {
            SoundPop();
            BeginExpandAnimation();
        }
        return;
    }

    // ---------- 点名按钮 ----------
    if (wasButtonDown)
    {
        RECT client = ClientRectOf(hwnd);
        CardLayout L = LayoutFor(client.right, client.bottom);
        RepaintNow();
        if (PtInRect(&L.button, pt))
            StartRoll();
        return;
    }

    // ---------- 气泡外点一下：人物本体，收起卡片 ----------
    // 拖动过就不算点击，避免拖窗口时误收起
    if (wasCardDrag && !wasCardMoved)
    {
        RECT client = ClientRectOf(hwnd);
        CardLayout L = LayoutFor(client.right, client.bottom);
        if (!PointInBubble(x, y, L) && g_expanded)
        {
            SoundCollapse();
            BeginCollapseAnimation();
            return;
        }
    }

    if (wasCardDrag && wasCardMoved)
        RepaintNow();
}

static void ShowContextMenu(HWND hwnd)
{
    POINT pt;
    GetCursorPos(&pt);

    HMENU menu = CreatePopupMenu();
    if (menu == NULL)
        return;

    AppendMenuW(menu, MF_STRING, IDM_TOGGLE, g_expanded ? L"收起卡片" : L"展开卡片");
    AppendMenuW(menu, MF_STRING, IDM_RELOAD, L"重新加载名单");
    AppendMenuW(menu, MF_STRING, IDM_EDIT,   L"编辑 names.txt");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING | (g_cfg.sound ? MF_CHECKED : MF_UNCHECKED),
                IDM_SOUND, L"音效");
    AppendMenuW(menu, MF_STRING | (g_cfg.autostart ? MF_CHECKED : MF_UNCHECKED),
                IDM_AUTOSTART, L"开机自启");
    AppendMenuW(menu, MF_STRING, IDM_OPENSETTINGS, L"打开配置文件");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, IDM_ABOUT, L"关于");
    AppendMenuW(menu, MF_STRING, IDM_EXIT,  L"退出");

    SetForegroundWindow(hwnd);
    int cmd = (int)TrackPopupMenu(menu,
                                  TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
                                  pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(menu);

    // 让菜单正常消失
    PostMessageW(hwnd, WM_NULL, 0, 0);

    switch (cmd)
    {
    case IDM_TOGGLE:
        if (g_expanded)
        {
            SoundCollapse();
            BeginCollapseAnimation();
        }
        else
        {
            SoundPop();
            BeginExpandAnimation();
        }
        break;

    case IDM_RELOAD:
        if (LoadNames(false))
        {
            wchar_t countText[32];
            wsprintfW(countText, L"%u", (unsigned)g_names.size());
            std::wstring msg = L"名单已重新加载，共 ";
            msg += countText;
            msg += L" 人。";
            MessageBoxW(NULL, msg.c_str(), L"随机点名", MB_OK | MB_ICONINFORMATION);
        }
        else
        {
            MessageBoxW(NULL, L"重新加载失败：请检查程序目录下的 names.txt。",
                        L"随机点名", MB_OK | MB_ICONWARNING);
        }
        RepaintNow();
        break;

    case IDM_EDIT:
        if (GetFileAttributesW(g_namesPath.c_str()) == INVALID_FILE_ATTRIBUTES)
            WriteTemplateNamesFile();
        ShellExecuteW(NULL, L"open", g_namesPath.c_str(), NULL,
                      g_exeDir.c_str(), SW_SHOWNORMAL);
        break;

    case IDM_SOUND:
        g_cfg.sound = g_cfg.sound ? 0 : 1;
        SaveSettings();
        if (g_cfg.sound)
            SoundClick();
        break;

    case IDM_AUTOSTART:
        g_cfg.autostart = g_cfg.autostart ? 0 : 1;
        SaveSettings();
        ApplyAutostartSetting();
        MessageBoxW(NULL,
                    g_cfg.autostart
                        ? L"已开启开机自启（写入当前用户注册表，无需管理员权限）。"
                        : L"已关闭开机自启。",
                    L"随机点名", MB_OK | MB_ICONINFORMATION);
        break;

    case IDM_OPENSETTINGS:
        ShellExecuteW(NULL, L"open", g_settingsPath.c_str(), NULL,
                      g_exeDir.c_str(), SW_SHOWNORMAL);
        break;

    case IDM_ABOUT:
        MessageBoxW(NULL,
                    L"随机点名 v0.9.9\n\n"
                    L"纯 Win32 API 实现的桌面点名浮窗。\n"
                    L"左键点右侧椭圆展开，右键菜单可重新加载名单 / 开关音效 / 退出。\n\n"
                    L"配置文件：settings.ini\n名单文件：names.txt",
                    L"关于 · 随机点名", MB_OK | MB_ICONINFORMATION);
        break;

    case IDM_EXIT:
        DestroyWindow(hwnd);
        break;

    default:
        break;
    }
}

// ============================================================================
//  十四、窗口过程
// ============================================================================

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_PAINT:
        OnPaint(hwnd);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_TIMER:
        if (wParam == TIMER_ANIM)
            AnimTick();
        else if (wParam == TIMER_IDLE)
            IdleTick();
        else if (wParam == TIMER_ROLL)
            RollTick();
        else if (wParam == TIMER_REAP)
            ReapVoices();
        return 0;

    case WM_LBUTTONDOWN:
        OnLButtonDown(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_MOUSEMOVE:
        OnMouseMove(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_LBUTTONUP:
        OnLButtonUp(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_MOUSELEAVE:
        g_trackingMouse = false;
        g_dockHover     = false;
        g_buttonHover   = false;
            RepaintNow();
        return 0;

    case WM_CAPTURECHANGED:
        g_cardDragging = false;
        g_cardMoved    = false;
        g_dockPressed  = false;
        g_dockDragging = false;
        g_dockMoved    = false;
        g_buttonDown   = false;
        return 0;

    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT)
        {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);

            if (!g_expanded)
            {
                SetCursor(g_dockDragging ? g_curSizeNS : g_curHand);
                return TRUE;
            }

            RECT client = ClientRectOf(hwnd);
            CardLayout L = LayoutFor(client.right, client.bottom);

            if (PtInRect(&L.button, pt))
                SetCursor(g_curHand);                    // 按钮：手型
            else if (!PointInBubble(pt.x, pt.y, L))
                SetCursor(g_curHand);                    // 人物本体：点了收起
            else
                SetCursor(g_curSizeAll);                 // 气泡空白：可拖动
            return TRUE;
        }
        break;

    case WM_RBUTTONUP:
        ShowContextMenu(hwnd);
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE && g_expanded)
        {
            SoundCollapse();
            BeginCollapseAnimation();
            return 0;
        }
        break;

    case WM_SETTINGCHANGE:
    case WM_DISPLAYCHANGE:
        if (!g_expanded && g_anim.kind == ANIM_NONE)
        {
            RECT rc = ComputeDockRect();
            SetWindowPos(hwnd, HWND_TOPMOST, rc.left, rc.top, 0, 0,
                         SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
            RepaintNow();
        }
        return 0;

    case WM_TRAYICON:
    {
        UINT ev = (UINT)LOWORD(lParam);
        if (ev == WM_LBUTTONUP || ev == NIN_SELECT || ev == NIN_KEYSELECT)
        {
            if (g_expanded)
            {
                SoundCollapse();
                BeginCollapseAnimation();
            }
            else
            {
                SoundPop();
                BeginExpandAnimation();
            }
        }
        else if (ev == WM_RBUTTONUP || ev == WM_CONTEXTMENU)
        {
            ShowContextMenu(hwnd);
        }
        return 0;
    }

    case WM_PICKER_SHOW_CARD:
        if (!g_expanded && g_anim.kind == ANIM_NONE)
        {
            SoundPop();
            BeginExpandAnimation();
        }
        return 0;

    case WM_APP + 9:
        // 插画缺失提示（启动后延迟弹出，避免挡住入场动画）
        WarnIfWhaleMissing();
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, TIMER_ANIM);
        KillTimer(hwnd, TIMER_IDLE);
        KillTimer(hwnd, TIMER_ROLL);
        KillTimer(hwnd, TIMER_REAP);
        ShutdownAudio();
        RemoveTrayIcon();
        PostQuitMessage(0);
        return 0;

    default:
        if (msg == g_taskbarCreatedMsg && g_taskbarCreatedMsg != 0)
        {
            g_trayAdded = false;
            AddTrayIcon();
            return 0;
        }
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ============================================================================
//  十五、程序入口
// ============================================================================

static void InitFonts()
{
    // 字体现在由 GDI+ 按需创建，这里只保留一个 GDI 字体用于文字宽度测量
    g_fontHint = MakeFont(S(FONT_SIZE_NAME_MIN), FW_NORMAL, FONT_FACE);
}

static void DestroyFonts()
{
    if (g_fontHint != NULL)
    {
        DeleteObject(g_fontHint);
        g_fontHint = NULL;
    }
}

// ---------- GDI+ ----------
static void InitGdiplus()
{
    Gdiplus::GdiplusStartupInput input;
    if (Gdiplus::GdiplusStartup(&g_gdiplusToken, &input, NULL) != Gdiplus::Ok)
        g_gdiplusToken = 0;
}

static void ShutdownGdiplus()
{
    if (g_whaleScaled != NULL)
    {
        delete g_whaleScaled;
        g_whaleScaled = NULL;
    }
    if (g_whaleBitmap != NULL)
    {
        delete g_whaleBitmap;
        g_whaleBitmap = NULL;
    }
    ReleaseMemSurface();

    if (g_gdiplusToken != 0)
    {
        Gdiplus::GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }
}

// 从 exe 同目录 assets/whale.png 加载插画
static void LoadWhaleImage()
{
    g_whalePath = g_exeDir + L"\\" + WHALE_ASSET_DIR + L"\\" + WHALE_ASSET_NAME;

    if (GetFileAttributesW(g_whalePath.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        g_whaleMissing = true;
        return;
    }

    Gdiplus::Bitmap* bmp = new Gdiplus::Bitmap(g_whalePath.c_str(), FALSE);
    if (bmp == NULL)
    {
        g_whaleMissing = true;
        return;
    }

    if (bmp->GetLastStatus() != Gdiplus::Ok || bmp->GetWidth() < 2 || bmp->GetHeight() < 2)
    {
        delete bmp;
        g_whaleMissing = true;
        return;
    }

    g_whaleBitmap  = bmp;
    g_whaleMissing = false;
}

// 插画缺失时的兜底提示（不影响待机条与点名功能）
static void WarnIfWhaleMissing()
{
    if (!g_whaleMissing)
        return;

    std::wstring msg =
        L"未找到界面插画：\n" + g_whalePath +
        L"\n\n程序仍可正常点名，但展开后的卡片会是纯色背景。\n"
        L"请把 assets 文件夹（内含 whale.png）放到程序所在目录。\n\n"
        L"素材来源：dsh-whale-widget 插件，作者 MeteorNOX（MIT）。";
    MessageBoxW(NULL, msg.c_str(), L"随机点名 · 提示", MB_OK | MB_ICONWARNING);
}

static void EnableDpiAwareness()
{
    typedef BOOL (WINAPI *SetProcessDPIAwareFn)(void);
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32 == NULL)
        user32 = LoadLibraryW(L"user32.dll");
    if (user32 == NULL)
        return;

    SetProcessDPIAwareFn fn =
        (SetProcessDPIAwareFn)(void*)GetProcAddress(user32, "SetProcessDPIAware");
    if (fn != NULL)
        fn();
}

static void SeedRandom()
{
    std::random_device rd;
    unsigned seed = (unsigned)GetTickCount64();
    seed ^= (unsigned)(uintptr_t)&g_rng;
    seed ^= (unsigned)GetCurrentProcessId() * 2654435761u;
    seed ^= (unsigned)rd();
    g_rng.seed(seed);
}

static bool CommandLineHas(const wchar_t* switchName)
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == NULL)
        return false;

    bool found = false;
    for (int i = 1; i < argc; ++i)
    {
        if (lstrcmpiW(argv[i], switchName) == 0)
        {
            found = true;
            break;
        }
    }
    LocalFree(argv);
    return found;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    g_hInst = hInstance;

    // ---------- DPI ----------
    EnableDpiAwareness();
    {
        HDC screen = GetDC(NULL);
        if (screen != NULL)
        {
            g_dpi = GetDeviceCaps(screen, LOGPIXELSX);
            ReleaseDC(NULL, screen);
        }
        if (g_dpi <= 0)
            g_dpi = 96;
    }

    SeedRandom();
    InitFonts();
    InitGdiplus();

    g_curHand    = LoadCursor(NULL, IDC_HAND);
    g_curSizeAll = LoadCursor(NULL, IDC_SIZEALL);
    g_curSizeNS  = LoadCursor(NULL, IDC_SIZENS);

    // ---------- 路径 ----------
    std::wstring exePath = GetExePath();
    g_exeDir = exePath;
    size_t slash = g_exeDir.find_last_of(L"\\/");
    if (slash != std::wstring::npos)
        g_exeDir = g_exeDir.substr(0, slash);
    g_namesPath    = g_exeDir + L"\\" + NAMES_FILE_NAME;
    g_settingsPath = g_exeDir + L"\\" + SETTINGS_FILE_NAME;

    // ---------- 配置 ----------
    LoadSettings();

    // ---------- 命令行：取消开机自启 ----------
    if (CommandLineHas(L"--remove-autostart"))
    {
        RemoveAutoStart();
        g_cfg.autostart = 0;
        SaveSettings();
        MessageBoxW(NULL,
                    L"已取消开机自启动，并已把 settings.ini 里的 autostart 改为 0。\n\n"
                    L"（注册表项：HKEY_CURRENT_USER\\Software\\Microsoft\\"
                    L"Windows\\CurrentVersion\\Run\\Win32RandomNamePicker）",
                    L"随机点名", MB_OK | MB_ICONINFORMATION);
        DestroyFonts();
        return 0;
    }

#if SINGLE_INSTANCE
    // 已有实例：唤醒它并展开卡片，自己退出（放在读名单之前，避免重复弹框）
    HWND existing = FindWindowW(WINDOW_CLASS_NAME, NULL);
    if (existing != NULL)
    {
        PostMessageW(existing, WM_PICKER_SHOW_CARD, 0, 0);
        SetForegroundWindow(existing);
        DestroyFonts();
        return 0;
    }
#endif

    // ---------- 名单 ----------
    LoadNames(true);

    // ---------- 界面插画 ----------
    LoadWhaleImage();

    // ---------- 窗口类 ----------
    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = WINDOW_CLASS_NAME;

    if (!RegisterClassExW(&wc))
    {
        DestroyFonts();
        return 1;
    }

    // ---------- 无边框置顶分层窗口 ----------
    RECT dockRect = ComputeDockRect();

    g_hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        WINDOW_CLASS_NAME,
        WINDOW_TITLE,
        WS_POPUP,
        dockRect.left, dockRect.top,
        dockRect.right - dockRect.left, dockRect.bottom - dockRect.top,
        NULL, NULL, hInstance, NULL);

    if (g_hWnd == NULL)
    {
        DestroyFonts();
        ShutdownGdiplus();
        return 1;
    }

    // 逐像素 Alpha 分层窗口：内容由 UpdateLayeredWindow 提交
    ShowWindow(g_hWnd, SW_SHOWNOACTIVATE);

    // ---------- 托盘 / 音频 / 自启 ----------
    AddTrayIcon();
    {
        // 托盘提示里带上名单人数，鼠标悬停即可确认加载是否成功
        wchar_t tip[128];
        wsprintfW(tip, L"随机点名 · 名单 %u 人 · 左键展开", (unsigned)g_names.size());
        UpdateTrayTip(tip);
    }
    InitAudio();
    ApplyAutostartSetting();

    g_taskbarCreatedMsg = RegisterWindowMessageW(L"TaskbarCreated");

    // ---------- 入场动画：从右侧滑入 ----------
    RECT from = dockRect;
    OffsetRect(&from, S(DOCK_WIDTH) + S(12), 0);
    StartAnim(ANIM_ENTER, from, dockRect,
              S(DOCK_HEIGHT) / 2, S(DOCK_HEIGHT) / 2,
              ANIM_ENTER_MS, 0, 255);

    StartIdleTimer();

    // 插画缺失时给出一次提示（不影响点名）
    if (g_whaleMissing)
        PostMessageW(g_hWnd, WM_APP + 9, 0, 0);

    // ---------- 消息循环 ----------
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DestroyFonts();
    ShutdownGdiplus();
    if (g_trayIcon != NULL)
    {
        DestroyIcon(g_trayIcon);
        g_trayIcon = NULL;
    }
    return (int)msg.wParam;
}
