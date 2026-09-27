# 相对 Seeed upstream 的差异记录

用途：记录以后 PhotoPainter 移植相对于 Seeed upstream 修改了哪些现有文件。

当前还没有正式移植代码，因此本文件不列出、也不虚构任何文件修改。

后续每项改动至少记录：

- Seeed upstream commit 或分支；
- 被修改的 upstream 文件；
- 修改原因和安全影响；
- 是否属于 PhotoPainter 专属、可回退的改动；
- 验证状态（静态检查、构建、真机）。

约束：优先新增 PhotoPainter 专属 board/BSP 文件，避免把硬件差异散落到
SenseCraft APP 业务逻辑中；不得无理由修改 Seeed_GFX。
