# 股票监控系统 (C# WinForms)

基于C# WinForms开发的股票实时监控应用程序，支持股票搜索、实时数据更新和交互式管理。

## 🎯 功能特性

### 核心功能
- ✅ **智能股票搜索**: 输入股票代码或名称进行实时搜索
- ✅ **下拉搜索结果**: 显示匹配的股票列表，每行带"+"按钮
- ✅ **实时数据更新**: 1Hz频率自动刷新股票数据
- ✅ **交互式表格**: 支持右键删除和拖拽排序
- ✅ **颜色编码**: 红涨绿跌的直观显示

### 界面特性
- 🔍 **搜索框**: 支持模糊搜索，自动显示/隐藏结果面板
- 📊 **数据表格**: 显示代码、名称、现价、涨跌、涨跌幅等信息
- 📈 **状态栏**: 显示股票统计和最后更新时间
- 🎨 **现代UI**: 简洁美观的界面设计

## 🏗️ 技术架构

### 项目结构
```
StockMonitor/
├── Models/
│   └── StockInfo.cs           # 股票数据模型
├── Services/
│   └── StockApiService.cs     # API服务层
├── Controls/
│   ├── StockSearchControl.cs  # 搜索控件
│   └── StockDataGridView.cs   # 数据表格控件
├── MainForm.cs                # 主窗体
├── Program.cs                 # 程序入口
└── StockMonitor.csproj        # 项目文件
```

### 技术栈
- **框架**: .NET Framework 4.8
- **UI**: Windows Forms
- **HTTP客户端**: System.Net.Http
- **JSON解析**: Newtonsoft.Json
- **数据源**: 新浪财经API + 腾讯财经API

## 🚀 快速开始

### 环境要求
- Windows 7 或更高版本
- .NET Framework 4.8 或更高版本
- Visual Studio 2019 或更高版本

### 构建和运行
```bash
# 克隆项目
git clone <repository-url>
cd StockMonitor

# 构建项目
dotnet build

# 运行应用程序
dotnet run
```

### 使用方法
1. **搜索股票**: 在搜索框中输入股票代码或名称
2. **添加股票**: 点击搜索结果右侧的"+"按钮
3. **查看数据**: 在表格中查看实时股票数据
4. **管理股票**: 右键删除或拖拽排序

## 📡 API集成

### 数据源
- **搜索API**: 新浪财经 (`suggest3.sinajs.cn`)
- **行情API**: 新浪财经 (`hq.sinajs.cn`)

### API示例
```csharp
// 搜索股票
var results = await StockApiService.Instance.SearchStocksAsync("平安银行");

// 获取股票数据
var stockInfo = await StockApiService.Instance.GetStockInfoAsync("SZ000001");

// 批量获取数据
var stockInfos = await StockApiService.Instance.GetStockInfosAsync(codes);
```

## 🎨 界面预览

### 主界面功能
- **搜索区域**: 顶部搜索框，支持实时搜索
- **数据表格**: 中央数据显示区域
- **状态栏**: 底部状态信息显示

### 交互功能
- **搜索**: 输入2个字符开始搜索
- **添加**: 点击"+"按钮添加股票
- **删除**: 右键菜单删除股票
- **排序**: 拖拽行进行重新排序
- **更新**: 每秒自动刷新数据

## 🔧 自定义配置

### 修改刷新频率
```csharp
// 在MainForm.cs中修改
_refreshTimer.Interval = 2000; // 改为2秒刷新
```

### 添加新的数据列
```csharp
// 在StockDataGridView.cs的AddColumns方法中添加
this.Columns.Add(new DataGridViewTextBoxColumn
{
    Name = "Volume",
    HeaderText = "成交量",
    Width = 100,
    ReadOnly = true
});
```

## 🐛 故障排除

### 常见问题
1. **网络连接失败**: 检查网络连接和防火墙设置
2. **数据获取失败**: API可能暂时不可用，稍后重试
3. **搜索无结果**: 检查输入的股票代码或名称是否正确

### 调试信息
应用程序会在控制台输出详细的错误信息，便于调试。

## 📈 扩展功能

### 计划中的功能
- [ ] K线图表显示
- [ ] 价格预警功能
- [ ] 数据导出功能
- [ ] 自选股分组
- [ ] 历史数据查看
- [ ] 配置文件保存

### 贡献指南
欢迎提交Issue和Pull Request来改进这个项目。

## 📄 许可证

本项目采用MIT许可证，详见LICENSE文件。

---

**注意**: 本应用程序仅供学习和研究使用，不构成投资建议。股市有风险，投资需谨慎。
