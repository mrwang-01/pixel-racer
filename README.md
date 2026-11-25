# PIXEL RACER · 像素赛车

一个复古像素风的躲避式赛车小游戏：在三条车道间变道，避开迎面来车、收集金币，并通过「擦身而过（Near Miss）」累积连击得分。

同一套玩法与参数提供了三种独立实现，全部由 `game-config.json` 统一驱动。

## 在线游玩

**网页版**：直接用浏览器打开 `index.html` 即可，无需构建、无外部依赖。

若本仓库启用了 GitHub Pages，可通过 `https://<用户名>.github.io/pixel-racer/` 在线访问。

## 操作方式

| 按键 | 功能 |
| --- | --- |
| `←` / `→` 或 `A` / `D` | 左右变道 |
| 任意键 / 「开始·重开」按钮 | 开始游戏、结束后重开 |
| `P` | 暂停 / 继续 |
| `M` | 音效开关 |

网页版另提供触屏按钮，可在移动设备上游玩。

## 三种实现

| 实现 | 入口 | 运行方式 |
| --- | --- | --- |
| HTML5 Canvas（推荐） | `index.html` | 浏览器直接打开；纯前端，零依赖 |
| C# / WinForms | `PixelRacer.sln`、`Program.cs` | Visual Studio 打开解决方案按 F5；或双击 `build.bat` 用 Windows 自带 .NET Framework 编译器生成 `PixelRacer.exe` |
| C / Win32 GDI | `main.c`、`PixelRacerC.sln` | 双击 `build-c.bat`（优先 MSVC，回退 MinGW gcc）；或在 Visual Studio 中按 F5 |

`PixelRacer.csproj` 面向 `net8.0-windows`，同时 `Program.cs` 保持 C# 5 语法以兼容 `build.bat` 的免 SDK 编译路径。

## 配置同步

所有可调参数（棋盘尺寸、颜色、难度、计分规则）集中在 `game-config.json`。修改后执行：

```bash
node sync-config.js   # 生成 Config.cs、game-config.h，并同步网页版内嵌配置
node verify.js        # 回归校验，确认三种实现参数一致
```

之后再重新构建 C# / C 版本。

## 文件说明

- `index.html` — 网页版游戏（含全部逻辑与样式）
- `Program.cs` / `Config.cs` — C# 游戏逻辑与生成的配置
- `main.c` / `game-config.h` — C / Win32 GDI 实现与生成的配置
- `game-config.json` — 唯一配置源
- `sync-config.js` / `verify.js` — 配置同步与一致性校验
- `build.bat` / `build-c.bat` — Windows 构建脚本
- `*.sln` / `*.csproj` / `*.vcxproj` — Visual Studio 工程文件

编译产物（`*.exe`、`bin/`、`obj/` 等）不纳入版本控制。
