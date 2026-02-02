![C++](https://img.shields.io/badge/C++-17-blue.svg)
![Qt](https://img.shields.io/badge/Qt-5.15+-41CD52.svg?logo=qt)
![CMake](https://img.shields.io/badge/CMake-3.16+-064F8C.svg?logo=cmake)
![Platform](https://img.shields.io/badge/Platform-Windows-0078D7.svg?logo=windows)
![License](https://img.shields.io/badge/License-MIT-green.svg)
![Version](https://img.shields.io/badge/version-1.6.0-orange.svg)

# StockMonitor

基于 Qt5 和 C++17 开发的轻量级实时股票监控应用。

## 功能特性

- **实时股票数据**: 可配置刷新间隔的实时股价监控
- **多数据源**: 支持新浪、腾讯、东方财富 API（A股、港股、美股）
- **全局快捷键**: 使用 `Ctrl+Alt+S` 切换窗口显示（可配置）
- **搜索添加**: 通过代码或名称快速搜索添加股票
- **拖拽排序**: 支持拖拽重新排列股票列表
- **主题切换**: 支持亮色/暗色主题
- **窗口透明度**: 按住 Alt + 鼠标滚轮调整窗口透明度
- **自动隐藏**: 配置延迟后自动隐藏窗口
- **系统托盘**: 最小化到系统托盘后台监控
- **指数追踪**: 显示上证指数、深证成指、创业板指

## 截图

## 系统要求

- Windows 10/11
- MSVC 2019 或更高版本
- Qt 5.15+
- OpenSSL 3.0+

## 编译

### 使用 CMake Presets（推荐）

```bash
# 配置并编译（静态链接）
cmake --preset static-release
cmake --build --preset static-release

# 输出: Build/static-release/StockMonitor.exe
```

### 使用 build.bat

```batch
build.bat
```

输出: `build\Release\StockMonitor.exe`

### 手动编译

```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2019" -A x64
cmake --build . --config Release
```

## 配置

在程序目录下创建 `stocks.ini`:

```ini
[api]
# 数据源: sina, tencent, easymoney
source=sina

[StockList]
StockCodes=000001,600519

[UI]
Theme=light
WindowOpacity=1.0
RefreshInterval=5000
RowHeight=25
AutoHideSeconds=0

[Hotkey]
Key=Ctrl+Alt+S
```

## 使用说明

### 键盘快捷键

- `Ctrl+Alt+S` - 切换窗口显示/隐藏
- `Escape` - 隐藏窗口或清空搜索
- `↑/↓` - 导航股票列表
- `Enter` - 选择搜索结果
- `Delete` - 删除选中股票
- `Alt + 鼠标滚轮` - 调整窗口透明度

### 添加股票

1. 按 `Ctrl+Alt+S` 显示窗口
2. 在搜索框输入股票代码或名称
3. 从下拉结果中选择
4. 股票将添加到监控列表

### 管理股票

- **重新排序**: 拖拽行来重新排列
- **删除**: 右键 → 删除，或按 `Delete` 键
- **移动**: 右键 → 上移/下移/移至顶部/移至底部

## 数据源对比

| 数据源 | 说明 |
|--------|------|
| 新浪财经 | 默认，支持A股、港股、美股 |
| 腾讯财经 | 支持A股、港股、美股 |
| 东方财富 | 支持A股、港股、美股 |

## 许可证

MIT License

## 致谢

- [QHotkey](https://github.com/Skycoder42/QHotkey) - 全局快捷键支持
- [Qt](https://www.qt.io/) - 跨平台应用框架
- [UPX](https://upx.github.io/) - 可执行文件压缩工具
