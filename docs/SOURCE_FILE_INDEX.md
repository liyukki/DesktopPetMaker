# 源码文件用途索引

这份索引用于回答“某个文件负责什么”。头文件通常声明接口和状态，配套的 `.cpp` 实现行为。当前正式运行时仍是 `RuntimePetWindow`。

## `src/app`：应用组装

| 文件 | 用途 |
| --- | --- |
| `main.cpp` | 程序入口；解析普通模式、控制中心模式和 CLI 多桌宠参数，创建顶层服务。 |
| `petcontrolcenterwindow.h/.cpp` | 桌宠管理中心主窗口；组织项目、运行实例、AI 服务和工具页面。 |
| `systemtraycontroller.h/.cpp` | Windows 系统托盘图标与恢复、显示、退出等菜单命令。 |

## `src/project`：项目数据与持久化

| 文件 | 用途 |
| --- | --- |
| `petproject.h/.cpp` | `pet.json` 核心模型；动作、画布、锚点、运行配置、AI 角色配置和 `.petpack` 导入导出。 |
| `petprojectregistry.h/.cpp` | 扫描并维护本机已登记的桌宠项目列表。 |
| `archivecommandrunner.h/.cpp` | 归档命令抽象与 PowerShell 实现，为 `.petpack` 安全解包提供边界。 |

## `src/runtime`：正式桌宠运行时

| 文件 | 用途 |
| --- | --- |
| `runtimepetwindow.h/.cpp` | 唯一正式桌宠窗口；动画状态机、拖拽、掉落、弹跳、巡逻、跟随、睡眠和右键菜单。 |
| `runtimepetmanager.h/.cpp` | 普通模式下当前桌宠的生命周期、切换和项目选择。 |
| `petruntimeinstance.h/.cpp` | 将单个运行窗口、主动消息气泡和共享对话上下文绑定成一个实例。 |
| `renderbackend.h/.cpp` | PNG/GIF 等渲染后端接口、帧加载和安全降级。 |
| `screenplacementutil.h/.cpp` | 多显示器可用区域、Anchor 落地位置和窗口坐标修正。 |
| `petspeechbubblewindow.h/.cpp` | 不抢焦点的主动 AI 消息气泡；文本截断、计时隐藏和跟随定位。 |
| `runtimeactionresult.h` | 运行时动作投递和 AI 回复投递的结构化结果。 |
| `petbehaviorrule.h` | 可扩展桌宠行为规则的数据结构。 |

## `src/ai/core`：AI 基础设施

| 文件 | 用途 |
| --- | --- |
| `aiprovider.h/.cpp` | OpenAI-compatible HTTP 请求、Provider 特有请求体和响应解析。 |
| `aiproviderprofileregistry.h/.cpp` | AI Provider 配置档案查询和默认档案选择。 |
| `aiproviderprofileservice.h/.cpp` | Provider 档案增删改与凭据存储协调。 |
| `credentialstore.h/.cpp` | Windows Credential Manager 凭据读写；避免 API Key 进入项目文件。 |
| `petairequestcoordinator.h/.cpp` | 限制并追踪桌宠 AI 请求生命周期，处理取消和并发。 |
| `aiactiondescriptor.h` | 模型可请求的桌宠动作描述结构。 |
| `aiactionvalidator.h/.cpp` | 校验 AI 动作名称和参数，拒绝不安全或不存在的动作。 |
| `chathistoryentry.h` | 统一聊天历史条目结构。 |
| `petconversationcontext.h` | 单只桌宠的共享对话历史和请求上下文。 |

## `src/ai/chat`：多角色 AI 会话

| 文件 | 用途 |
| --- | --- |
| `aiconversationroom.h/.cpp` | 房间、参与角色、消息和角色关系的数据模型。 |
| `aiconversationroomrepository.h/.cpp` | 多 AI 房间的本地持久化。 |
| `aiconversationroommanager.h/.cpp` | 房间生命周期、轮流发言、参与者解析和动作投递协调。 |

## `src/ai/ui`：AI 界面

| 文件 | 用途 |
| --- | --- |
| `aidialogwindow.h/.cpp` | 单只桌宠的完整聊天窗口，复用该桌宠共享历史。 |
| `aiconversationconsolewindow.h/.cpp` | 多 AI 对话控制台；创建房间、管理参与者和查看消息。 |
| `aisettingsdialog.h/.cpp` | 历史 AI 设置对话框，当前保留用于兼容审查但未链接进正式目标。 |

## `src/editor`：桌宠制作器

| 文件 | 用途 |
| --- | --- |
| `editorwindow.h/.cpp` | 项目编辑器主窗口；动作、FPS、Loop、Next、Anchor 和 AI 角色字段。 |
| `previewcanvas.h/.cpp` | 编辑器动画预览画布与 Anchor 可视化。 |
| `actionmaterialwindow.h/.cpp` | 动作与素材工作台；导入、检查和动作配置入口。 |
| `alphaboundingboxutil.h/.cpp` | 根据透明像素计算素材有效边界。 |
| `assetqualityanalyzer.h/.cpp` | 检查帧尺寸、透明边界、清晰度和动作连续性等素材质量。 |
| `proceduralmotiongenerator.h/.cpp` | 由已有帧生成轻量位移、缩放和姿态变化序列。 |
| `motionpromptlibrary.h/.cpp` | AI 动作素材生成提示词模板库。 |
| `aiassetpromptwizard.h/.cpp` | 面向用户的动作素材提示词向导。 |

## `src/import`：素材导入

| 文件 | 用途 |
| --- | --- |
| `spritesheetslicer.h/.cpp` | 按行列、间距和边距切分 Sprite Sheet。 |
| `spritesheetimportdialog.h/.cpp` | Sprite Sheet 导入参数和预览对话框。 |
| `shimejiimportwizard.h/.cpp` | Shimeji 素材、动作和行为的事务式导入。 |

## `src/journal`：心情日记

| 文件 | 用途 |
| --- | --- |
| `moodjournal.h/.cpp` | 日记数据、日期提醒和单例提醒调度。 |
| `journalwindow.h/.cpp` | 日记浏览、编辑和保存窗口。 |

## `src/integrations`：外部工具

| 文件 | 用途 |
| --- | --- |
| `toolintegrationmanager.h/.cpp` | 外部程序和本地工具的发现、启动与错误处理。 |

## `src/ui/theme`：通用视觉系统

| 文件 | 用途 |
| --- | --- |
| `apptheme.h/.cpp` | 应用全局 Qt Style Sheet 和主题应用入口。 |
| `themeconstants.h` | 颜色、间距、圆角和控件尺寸常量。 |
| `iconprovider.h/.cpp` | 统一加载应用资源图标和系统图标。 |

## `tests`：自动化测试

| 文件 | 用途 |
| --- | --- |
| `test_support.h/.cpp` | Qt 测试共享的临时目录、项目样本和辅助断言。 |
| `core_project_tests.cpp` | `PetProject` 基础加载、保存和配置兼容性。 |
| `asset_transaction_tests.cpp` | 素材变更事务和失败回滚。 |
| `formal_asset_tests.cpp` | 正式发布素材规则和动作资源完整性。 |
| `sprite_sheet_tests.cpp` | Sprite Sheet 切帧算法。 |
| `shimeji_transaction_tests.cpp` | Shimeji 导入事务和错误回滚。 |
| `petpack_security_tests.cpp` | `.petpack` 路径穿越、归档边界和安全导入。 |
| `provider_credential_tests.cpp` | Provider 档案与凭据服务隔离。 |
| `credential_store_windows_integration_tests.cpp` | Windows Credential Manager 真实集成。 |
| `multi_ai_room_tests.cpp` | 多 AI 房间模型、关系和消息流程。 |
| `multi_ai_lifecycle_tests.cpp` | 多 AI 会话请求取消和生命周期。 |
| `runtime_action_tests.cpp` | 运行时动作、状态优先级和投递结果。 |
| `gui_widget_tests.cpp` | 导入窗口与通用 Widget 行为。 |
| `ui_theme_tests.cpp` | 主题常量、样式和图标。 |
| `platform_smoke_tests.cpp` | 跨模块基础冒烟测试。 |
| `platform_gui_tests.cpp` | 管理中心、气泡、托盘和真实控件 GUI 回归测试入口。 |

## 其他构建文件

| 文件 | 用途 |
| --- | --- |
| `CMakeLists.txt` | 按上述职责分组定义应用和测试目标。 |
| `resources.qrc` | Qt 内嵌品牌资源清单。 |
| `desktop_pet.rc` | Windows EXE 图标和版本资源。 |
