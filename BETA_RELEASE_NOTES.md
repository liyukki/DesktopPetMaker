# Desktop Pet Maker 1.0.0-beta

这是 Desktop Pet Maker 的首个公开测试版。

## 本版内容

- Qt/C++ 桌宠运行时
- 桌宠管理中心
- PNG、GIF、Sprite Sheet、Shimeji 和 petpack 素材工作流
- 动作编辑、画布标准化、锚点与帧偏移
- 拖拽、掉落、弹跳、巡逻、跟随、随机动作和自动睡眠
- OpenAI-compatible AI 服务配置
- 每桌宠独立 AI 角色提示词
- 主动聊天气泡和完整聊天窗口
- Multi-AI 多角色对话台
- Windows 托盘恢复与运行控制
- Debug/Release 自动化测试基线

## 使用方式

1. 下载 `DesktopPetMaker-1.0.0-beta-windows-x64.zip`。
2. 解压到普通可写目录。
3. 运行 `start_desktop_pet.bat`。
4. 在管理中心创建、导入或打开桌宠项目。

公开运行包不包含未经授权的角色素材。

## 已知限制

- 当前二进制未进行 Authenticode 签名，Windows SmartScreen 可能提示风险。
- Live2D 仅提供适配边界，不包含 Cubism SDK 渲染器。
- 未实现跨进程持久聊天记忆。
- 真实多显示器、高 DPI、干净 Windows 虚拟机和主观动画质量仍需更多人工测试。
- API 服务、模型和额度由用户自行配置。

## 验证

发布目录同时提供 SHA-256 校验文件。下载后可以运行：

```powershell
Get-FileHash .\DesktopPetMaker-1.0.0-beta-windows-x64.zip -Algorithm SHA256
```

## 反馈建议

提交问题时请包含：

- Windows 版本
- 操作步骤
- 预期行为与实际行为
- 脱敏后的日志或截图

不要在公开 Issue 中发送 API Key、Token、聊天隐私或个人路径。
