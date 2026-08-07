# Security Policy

## Supported Version

当前仅维护仓库主分支，并且只支持 README 中散列完全匹配的 MikuMikuDance 9.31 x64。

## Reporting a Vulnerability

请不要公开提交可能导致任意内存写入、DLL 劫持、路径穿越或宿主崩溃的细节。请使用 GitHub 仓库的 **Security > Report a vulnerability** 私下报告，并提供：

- WindTool 提交版本；
- Windows 与 MMD 版本及 SHA-256；
- 最小复现步骤；
- 崩溃地址或日志，但不要附带受版权保护的 MMD/MME 文件。

## Safety Notes

- 安装脚本只接受显式 MMD 路径，并验证宿主与原始 MME 散列。
- 不受支持的宿主版本不会执行物理内存写入。
- 下载 Release 时请核对发布页提供的 SHA-256，并保留原始 `MMEffect.dll` 备份。
