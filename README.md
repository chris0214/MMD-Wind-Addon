WindTool 是面向 MikuMikuDance 9.31 x64 的实时风力与物理控制插件

## 构建与测试

```powershell
cmake -S . -B build -G Ninja
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

一键构建、测试并生成 `dist` 安装目录：

```powershell
.\scripts\Build-WindTool.ps1
```

构建过程会运行四项 CTest，并把 DLL、MME 转发器和 PMX 风源安装到 `dist`。

生成带安装脚本、说明文档和 SHA-256 清单的 Release：

```powershell
.\scripts\Package-WindTool.ps1
```

## 使用方法

1. 启动受支持 MMD 目录中的 `MikuMikudance.exe`。
2. 加载带动态刚体的 PMD/PMX 模型。
3. 如需视口局部风源，再加载 `WindTool\WindTool-WindSource.pmx`。
4. 打开 `WindTool > 打开 WindTool`。
5. 在风力系统页启用风力，选择环境预设、风场类型和噪波类型。
6. 使用方向预设选择 `±X / ±Y / ±Z`，或直接输入自定义 XYZ。
7. `全局风场` 会对目标刚体直接使用当前风场；如需局部风源，在 `风源` 下拉框选择 `PMX 局部`。
8. PMX 模式下球形笼就是作用范围，移动/旋转 `风源控制` 骨骼即可在视口定位和转向；选中风源模型不会中断角色受风。
9. PMX 表情 `風場範囲` 映射 `2 - 80`，`風力倍率` 映射 `0 - 4` 倍并乘在面板基础强度之上，`減衰核心` 映射 `0.05 - 0.95`；可先用约 `23% / 25% / 33%`，其中倍率 `25% = 1x`、`0% = 0x`。
10. 需要多个局部风源时，可重复加载同一个 PMX；面板底部会显示 `PMX已连接 N`。每个副本的控制骨骼和三个表情均可独立 K 帧。
11. 两个风源覆盖同一刚体时会同时参与计算：同方向相加，反方向抵消；超过安全范围后统一限幅，不会重复写入 Bullet。
12. 三个 PMX 表情和控制骨骼都可使用 MMD 原生注册按钮 K 帧；其余风力参数继续使用面板底部 JSON 轨道。
13. 分别调整风力强度、阵风幅度、噪波强度和变化频率。
14. 涡旋风使用方向作为旋转轴，使用中心作为旋涡中心；径向风、热上升气流、下击暴流和风切变会使用中心坐标。
15. 切换到目标页，先选择 `风力层 / 阻尼层 / 重力层`，再复选该层使用的碰撞组或刚体；按住 `Shift` 点击可连续选择一段刚体。
16. 切换其他作用层并设置不同目标；输入分组名称可以保存和复用当前选择。
17. 切换到刚体页，可覆盖目标的线性阻尼、角阻尼与重力。
18. 移动 MMD 时间轴到目标帧，调整参数后点击设置关键帧；在底部轨道点击菱形标记可选择或删除关键帧。

建议第一次测试先使用 `Strength 15-35`、`Gust 10-25%`，方向设为 `1, 0, 0`。阻尼可从 `5-20%` 开始；较高阻尼会快速压制头发或裙摆摆动。`狂暴风`与接近 `1000` 的手动强度用于夸张镜头，轻质刚体会被迅速加速；PMX 倍率全开时组合强度最高可到 `4000`。

## 安装与恢复

先运行构建脚本生成 `dist`，然后安装实时加载链：

```powershell
pwsh -File .\scripts\Install-WindTool.ps1 -MmdDirectory "D:\MikuMikuDance"
```

恢复原始 MMEffect：

```powershell
pwsh -File .\scripts\Uninstall-WindTool.ps1 -MmdDirectory "D:\MikuMikuDance"
```

原始 MMEffect 会以 `MMEffect.original.dll` 保留，卸载过程可逆。

部署脚本还会安装可视化风源：

```text
MikuMikuDance -2\WindTool\WindTool-WindSource.pmx
```
