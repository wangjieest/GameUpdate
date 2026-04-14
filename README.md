# HotUpdatePlugin

Unreal Engine 5.7 热更新（OTA 补丁）插件，支持 Pak/IoStore 打包、增量/差异更新、版本对比和运行时补丁下载挂载。

## 功能特性

- **运行时热更新** — 检测版本 → 下载补丁 → 挂载 Pak，全流程自动化
- **增量更新** — 基于 diff 的差异下载，减少下载量
- **并发下载** — HTTP 多线程下载，支持暂停/恢复/重试
- **Pak 挂载** — 运行时动态挂载/卸载 Pak 文件，支持加密密钥注册
- **版本管理** — JSON 清单格式，支持链式更新（1.0.0 → 1.0.1 → 1.0.2）
- **编辑器工具** — 4 合 1 Slate 面板：基础包构建、补丁打包、版本对比、Pak 查看器
- **Blueprint 全支持** — 所有运行时 API 均可从蓝图调用

## 环境要求

- Unreal Engine 5.7
- Visual Studio 2022 / Rider
- Windows 平台（当前）

## 快速开始

### 打开项目

1. 克隆仓库
2. 右键 `GameUpdate.uproject` → Generate Visual Studio project files
3. 打开 `GameUpdate.uproject`

### 配置热更新服务器

编辑 `Config/DefaultGame.ini`，填写你的服务器地址：

```ini
[/Script/HotUpdate.HotUpdateSettings]
ManifestUrl="http://your-server.com/hotpatch/manifest.json"
ResourceBaseUrl="http://your-server.com/hotpatch/"
bAllowHttpConnection=True
```

### 打包命令

在编辑器中通过 **HotUpdateEditor** 面板操作，或使用命令行：

```bash
# 构建基础包
UnrealEditor-Cmd GameUpdate -run=HotUpdate -mode=base -version=1.0.0 -platform=Windows

# 构建增量补丁
UnrealEditor-Cmd GameUpdate -run=HotUpdate -mode=patch -version=1.0.1 -baseversion=1.0.0 -platform=Windows
```

## 项目架构

### 插件模块

| 模块 | 类型 | 说明 |
|------|------|------|
| HotUpdate | Runtime | 运行时热更新核心，随游戏发布 |
| HotUpdateEditor | Editor | 编辑器打包工具，仅开发时使用 |

### 运行时核心类

| 类 | 说明 |
|----|------|
| `UHotUpdateManager` | 核心调度器，流程：CheckForUpdate → StartDownload → ApplyUpdate |
| `UHotUpdateHttpDownloader` | HTTP 并发下载，暂停/恢复/重试 |
| `UHotUpdatePakManager` | Pak 挂载/卸载/校验 |
| `UHotUpdateManifestParser` | JSON 清单解析 |
| `UHotUpdateIncrementalCalculator` | 增量差异计算 |
| `UHotUpdateVersionStorage` | 本地版本持久化 |
| `UHotUpdateSettings` | 开发者配置（服务器地址、并发数、路径等） |

### 编辑器面板

| 面板 | 说明 |
|------|------|
| Base Version Panel | 构建完整基础包 |
| Packaging Panel | 构建补丁/差异包 |
| Version Diff Panel | 版本对比 |
| Pak Viewer Panel | 查看 Pak/IoStore 内容 |

### 数据流

```
编辑器: 资产 → Chunk 分配 → IoStore 构建 → 清单生成 → 版本注册
运行时: 检查更新(HTTP) → 解析清单 → 增量计算 → 下载(并发) → 校验哈希 → 挂载Pak → 更新版本
```

## 目录结构

```
GameUpdate/
├── Config/                          # 项目配置
├── Content/                         # UE 资产
├── Source/GameUpdate/               # 游戏模块
│   └── UI/HotUpdateWidget.h         # 运行时 UMG 热更新界面
├── Plugins/HotUpdatePlugin/
│   └── Source/
│       ├── HotUpdate/               # 运行时模块
│       │   ├── Public/              # 头文件
│       │   │   ├── Core/            # 核心类
│       │   │   ├── Download/        # 下载相关
│       │   │   ├── Manifest/        # 清单相关
│       │   │   └── Pak/             # Pak 管理
│       │   └── Private/             # 实现文件
│       └── HotUpdateEditor/         # 编辑器模块
│           ├── Public/
│           └── Private/
│               └── Widgets/         # Slate 面板
└── Build/                           # 自动化脚本
```

## License

[MIT](LICENSE)

Copyright czm. All Rights Reserved.