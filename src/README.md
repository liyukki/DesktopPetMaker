# 源码导航

所有正式 C++ 源码都位于 `src/`，并按职责分区。新贡献者通常从以下入口开始：

- 应用如何启动：`app/main.cpp`
- 桌宠如何运行：`runtime/runtimepetwindow.cpp`
- 项目如何读写：`project/petproject.cpp`
- AI 请求如何发送：`ai/core/aiprovider.cpp`
- 编辑器如何组织：`editor/editorwindow.cpp`
- 管理中心如何组织：`app/petcontrolcenterwindow.cpp`

## 目录职责

| 目录 | 职责 | 依赖方向 |
| --- | --- | --- |
| `app/` | 程序入口、管理中心、系统托盘 | 负责组装其他模块 |
| `project/` | `pet.json`、`.petpack`、项目注册表与归档操作 | 基础数据层 |
| `runtime/` | 桌宠窗口、行为状态机、物理、渲染和实例生命周期 | 依赖 project、AI、journal |
| `ai/core/` | Provider 协议、凭据、请求协调和共享对话数据 | AI 基础设施 |
| `ai/chat/` | 多角色房间、持久化与轮次协调 | 依赖 AI core |
| `ai/ui/` | 单宠聊天、多 AI 控制台和旧设置窗口 | 依赖 AI core/chat |
| `editor/` | 桌宠项目编辑、动作素材、预览和生成辅助 | 依赖 project/import |
| `import/` | Sprite Sheet 与 Shimeji 导入 | 独立导入工具 |
| `journal/` | 心情日记数据和窗口 | 独立用户功能 |
| `integrations/` | 外部工具启动与集成边界 | 平台适配 |
| `ui/theme/` | 主题常量、全局样式和图标提供器 | 通用 UI 基础 |

完整的逐文件说明见 [源码文件索引](../docs/SOURCE_FILE_INDEX.md)。

## 约定

1. 新代码放入最接近其职责的目录，不再把 `.cpp` 或 `.h` 放到仓库根目录。
2. 跨模块依赖应从应用层指向基础层，避免 `project/` 反向依赖窗口。
3. 可复用的网络与数据逻辑放在 `ai/core/`，不要散落到对话窗口。
4. 导入解析和编辑器交互分开：解析放 `import/`，界面编排放 `editor/`。
5. 新增源文件后同步更新 `CMakeLists.txt` 和 `docs/SOURCE_FILE_INDEX.md`。
