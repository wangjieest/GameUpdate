// Copyright Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using System;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using Microsoft.Extensions.Logging;

using static AutomationTool.CommandUtils;

/// <summary>
/// 在 Staging 完成后，将 pakchunk1+ 的 pak/bin 文件从 staging 目录移至 DownloadCache 子目录，
/// 使得最终打包（APK/OBB/安装包）中只保留 pakchunk0（首包基础资源）。
/// pakchunk1+ 用于后续通过 CDN 下载分发。
/// </summary>
public class StripExtraPakChunksHandler : CustomStagingHandler
{
    /// <summary>
    /// 所有平台都生效
    /// </summary>
    protected override bool TryInitialize(ProjectParams Params, DeploymentContext SC)
    {
        return true;
    }

    public override void PostStagingFileCopy(ProjectParams Params, DeploymentContext SC)
    {
        // 检查是否传入 -MinimalPackage 参数
        bool bMinimalPackage = Environment.GetCommandLineArgs()
            .Any(arg => arg.Equals("-MinimalPackage", StringComparison.OrdinalIgnoreCase));

        if (!bMinimalPackage)
        {
            return;
        }

        // 解析 -HotUpdateOutputDir 参数（热更资源输出目录）
        string? HotUpdateOutputDir = null;
        var args = Environment.GetCommandLineArgs();
        for (int i = 0; i < args.Length; i++)
        {
            if (args[i].StartsWith("-HotUpdateOutputDir=", StringComparison.OrdinalIgnoreCase))
            {
                HotUpdateOutputDir = args[i].Substring("-HotUpdateOutputDir=".Length).Trim('"');
                break;
            }
        }

        // 未指定输出目录则不移动
        if (string.IsNullOrEmpty(HotUpdateOutputDir))
        {
            Logger.LogWarning("StripExtraPakChunks: -HotUpdateOutputDir not specified, skipping.");
            return;
        }

        string StageDir = SC.StageDirectory.FullName;
        if (!Directory.Exists(StageDir))
        {
            return;
        }

        // 递归搜索 staging 目录下的所有 pakchunk*.pak
        var pakFiles = Directory.GetFiles(StageDir, "pakchunk*.pak", SearchOption.AllDirectories);
        var chunkRegex = new Regex(@"pakchunk(\d+)", RegexOptions.IgnoreCase | RegexOptions.Compiled);

        int movedCount = 0;

        foreach (string pakPath in pakFiles)
        {
            Match match = chunkRegex.Match(Path.GetFileName(pakPath));
            if (!match.Success)
            {
                continue;
            }

            int chunkIndex = int.Parse(match.Groups[1].Value);
            if (chunkIndex == 0)
            {
                // pakchunk0 保留不动
                continue;
            }

            // 直接移动到 HotUpdateOutputDir（由参数指定完整路径）
            string destDir = HotUpdateOutputDir;
            Directory.CreateDirectory(destDir);
            string destPath = Path.Combine(destDir, Path.GetFileName(pakPath));
            if (File.Exists(destPath))
            {
                File.Delete(destPath);
            }
            File.Move(pakPath, destPath);
            movedCount++;

            // 移动对应的附属文件（.bin, .ucas, .utoc）
            foreach (string ext in new[] { ".bin", ".ucas", ".utoc" })
            {
                string sidecarPath = Path.ChangeExtension(pakPath, ext);
                if (File.Exists(sidecarPath))
                {
                    string sidecarDestPath = Path.Combine(destDir, Path.GetFileName(sidecarPath));
                    if (File.Exists(sidecarDestPath))
                    {
                        File.Delete(sidecarDestPath);
                    }
                    File.Move(sidecarPath, sidecarDestPath);
                }
            }

            Logger.LogInformation("Moved {0} -> {1}", Path.GetFileName(pakPath), destDir);
        }

        if (movedCount > 0)
        {
            Logger.LogInformation("StripExtraPakChunks: moved {0} pak file(s) to {1}", movedCount, HotUpdateOutputDir);
        }
    }
}
