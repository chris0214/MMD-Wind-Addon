# WindTool

WindTool 是面向 **MikuMikuDance 9.31 x64** 的实时风场与局部刚体物理控制扩展。它在 MMD 内提供中文叠加面板、可视化目标多选、命名分组、阻尼/重力覆盖，以及独立 JSON 物理关键帧轨道。

## 兼容性

当前公开版本只支持：

| 项目 | 要求 |
| --- | --- |
| MMD | MikuMikuDance 9.31 x64 |
| `MikuMikudance.exe` SHA-256 | `2C9414C21619B4AD85D9C2EF76836F3C34DB7A8ABD07BD6C6176D385F7EFDFB4` |
| 原始 `MMEffect.dll` SHA-256 | `A20D77FB6C6919B7894EEADCFB852F5EA6D56E93C5A65142BC2DAE75C6F54D25` |
| 系统 | Windows x64 |

散列不匹配时，安装脚本会拒绝修改文件。不要通过删除校验来强行安装；不同构建的结构偏移可能不同。

## 构建

需要 CMake 3.20+，以及 Visual Studio 2022 x64 工具链或 MinGW-w64。仓库已包含构建所需的版本锁定接口头文件，不依赖仓库外的 SDK。

```powershell
pwsh -File .\scripts\Build-WindTool.ps1
```

也可以手动构建：

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix dist
```

MinGW/Ninja 示例：

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --install build --prefix dist
```

## 安装

安装脚本必须显式传入 MMD 目录。它会先验证散列，将原始 `MMEffect.dll` 保存为 `MMEffect.original.dll`，再安装转发器与 WindTool DLL。

```powershell
pwsh -File .\scripts\Install-WindTool.ps1 -MmdDirectory "D:\MikuMikuDance"
```

卸载并恢复原始 MME：

```powershell
pwsh -File .\scripts\Uninstall-WindTool.ps1 -MmdDirectory "D:\MikuMikuDance"
```

启动 MMD 后，从 `WindTool > 打开 WindTool` 打开面板。轨道默认保存在：

```text
<MMD目录>\PhysicsControlStudio\PhysicsControlStudio.json
```

## License

WindTool 源码以 [MIT License](LICENSE) 发布。


By 克里斯提亚娜 2026.8.7
