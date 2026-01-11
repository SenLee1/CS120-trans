# copy_assets.ps1

# 1. 定义要复制的文件列表 (根据你的项目需求，我把 INPUT 和 PING 配置也加进去了)
$filesToCopy = @("config.txt", "INPUT.txt", "configPING.txt")

# 2. 定义目标目录 (根据你的截图路径)
# 注意：如果你编译 Node1 或 Node3，可能还需要把它们的目标路径加进来
$targetDirs = @(
    "build/Node2_artefacts/Debug",
    "build/Node1_artefacts/Debug",
    "build/Node3_artefacts/Debug"
)

Write-Host "=== 开始复制配置文件 ===" -ForegroundColor Cyan

foreach ($dir in $targetDirs) {
    # 检查目标文件夹是否存在
    if (Test-Path $dir) {
        Write-Host "正在复制到: $dir" -ForegroundColor Yellow
        foreach ($file in $filesToCopy) {
            if (Test-Path $file) {
                Copy-Item -Path $file -Destination $dir -Force
                Write-Host "  [OK] $file" -ForegroundColor Green
            } else {
                # 只有文件不存在时才警告，避免某些节点不需要特定文件时的误报
                # Write-Warning "  [SKIP] 源文件不存在: $file" 
            }
        }
    } else {
        # 文件夹不存在通常意味着你还没编译那个节点，跳过即可
    }
}

Write-Host "=== 复制完成 ===" -ForegroundColor Cyan