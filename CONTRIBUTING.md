# 参与贡献

感谢关注 Desktop Pet Maker。

## 开始之前

1. 使用 UTF-8 无 BOM 保存文本文件。
2. 不要提交 API Key、Token、用户凭据或绝对用户目录。
3. 不要提交未经许可的角色素材、字体、模型或第三方图标。
4. 保留 `RuntimePetWindow` 作为正式 Qt/C++ 运行时，不在旧 Python 原型中新增行为。
5. 修改运行时状态时保持物理、巡逻、AI 动画和 Overlay 的优先级约束。

## 提交检查

```powershell
python tools/check_text_encoding.py --root .
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

提交应聚焦一个主题，并说明：

- 改动目的
- 用户可见行为
- 已执行的测试
- 仍需人工验证的部分
- 新增素材的来源与许可证

## UI 贡献

- 复用 `ui/theme` 中的主题常量和 `IconProvider`。
- 用户可见中文与内部稳定 ID 分离。
- 不使用文本按钮替代已有通用图标。
- 不把源码静态检查描述成 GUI 视觉通过。
