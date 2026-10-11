using System.Runtime.InteropServices;
using System.Text.Json;
using ReloadedDropIn.Core.Platform;
using ReloadedDropIn.Adapter.Abstractions;
using ReloadedDropIn.Core.Configuration;
using ReloadedDropIn.Core.Dependencies;
using ReloadedDropIn.Core.Discovery;

namespace ReloadedDropIn.Cli;

public sealed class Commands(
    IReadOnlyList<IGameAdapter> adapters,
    AdapterContext context,
    TextWriter output)
{
    /// <summary>Read-only preparation contract. Static layout is never launch attestation.</summary>
    public int ProtonPlan()
    {
        var (adapter, detection) = DetectAdapter();
        if (adapter is null || detection?.ExecutablePath is null) return 1;
        WindowsPayload target;
        try { target = PayloadCompatibility.ReadGame(detection.ExecutablePath); }
        catch (Exception ex) when (ex is IOException or BadImageFormatException)
        {
            output.WriteLine(JsonSerializer.Serialize(new { SchemaVersion = 1, Status = "blocked", Reason = ex.Message }));
            return 1;
        }
        var supported = target == WindowsPayload.X64;
        var scan = new ModScanner().Scan(context.ModsDirectory);
        var resolution = new DependencyResolver().Resolve(scan.Mods);
        output.WriteLine(JsonSerializer.Serialize(new {
            SchemaVersion = 1,
            Adapter = adapter.Id,
            SteamAppIds = adapter.SteamAppIds,
            HostArchitecture = RuntimeInformation.OSArchitecture.ToString().ToLowerInvariant(),
            CoordinatorRid = OperatingSystem.IsLinux() ? $"linux-{RuntimeInformation.ProcessArchitecture.ToString().ToLowerInvariant()}" : null,
            PayloadRid = target switch { WindowsPayload.X64 => "win-x64", WindowsPayload.X86 => "win-x86", _ => "win-arm64" },
            Executable = detection.ExecutablePath,
            RequiredMods = adapter.GetRequiredMods().Select(m => new { m.Id, m.UpdateRepo, m.UpdateAssetPrefix, m.PayloadRid }),
            MissingRequiredMods = adapter.GetRequiredMods().Where(m => !scan.Mods.Any(d => d.ModId.Equals(m.Id, StringComparison.OrdinalIgnoreCase))).Select(m => m.Id),
            MissingDependencies = resolution.MissingDependencies,
            Status = "blocked",
            Reason = supported ? "Windows x64 payloads required under Proton. The bridge does not attest loader hosting or injection; modded launch remains disabled." : "No supported loader stack for this Windows game architecture.",
            CanPrepareDependencies = supported,
            CanLaunchModded = false,
        }, new JsonSerializerOptions { WriteIndented = true }));
        return supported ? 0 : 1;
    }

    /// <summary>Host-native download preparation only; does not write Wine AppData or launch the game.</summary>
    public int FetchBaseMods()
    {
        var (adapter, detection) = DetectAdapter();
        if (adapter is null || detection?.ExecutablePath is null) return 1;
        try
        {
            var target = PayloadCompatibility.ReadGame(detection.ExecutablePath);
            if (target != WindowsPayload.X64) { output.WriteLine("Unsupported Windows game payload architecture."); return 1; }
            using var deadline = new CancellationTokenSource(TimeSpan.FromSeconds(60));
            using var feed = new ReloadedDropIn.Bootstrap.Update.GitHubReleaseFeed(Path.Combine(context.DropInDirectory, "state"));
            var installer = new ReloadedDropIn.Bootstrap.Update.BaseModInstaller(context, feed, target);
            foreach (var line in installer.RunAsync(adapter.GetRequiredMods(), deadline.Token).GetAwaiter().GetResult()) output.WriteLine(line);
            var scan = new ModScanner().Scan(context.ModsDirectory);
            var present = scan.Mods.Select(m => m.ModId).ToHashSet(StringComparer.OrdinalIgnoreCase);
            if (!adapter.GetRequiredMods().All(m => present.Contains(m.Id))) return 1;
            foreach (var mod in scan.Mods.Where(m => adapter.GetRequiredMods().Any(r => r.Id.Equals(m.ModId, StringComparison.OrdinalIgnoreCase))))
                ReloadedDropIn.Bootstrap.Update.BaseModInstaller.ValidateInstalledPayload(mod.Directory, target);
            return 0;
        }
        catch (Exception ex) when (ex is IOException or BadImageFormatException or OperationCanceledException)
        { output.WriteLine($"Preparation failed: {ex.Message}"); return 1; }
    }

    public int Detect()
    {
        var (adapter, detection) = DetectAdapter();
        if (adapter is null)
        {
            output.WriteLine($"No supported game detected in {context.GameDirectory}");
            return 1;
        }

        output.WriteLine($"Game: {adapter.DisplayName}");
        output.WriteLine($"Adapter: {adapter.Id}");
        output.WriteLine($"Executable: {detection!.MatchedExecutable}");
        return 0;
    }

    public int ListMods()
    {
        var scan = new ModScanner().Scan(context.ModsDirectory);

        foreach (var mod in scan.Mods)
            output.WriteLine($"{mod.ModId} {mod.Manifest.ModVersion} ({mod.Manifest.ModName})");

        foreach (var issue in scan.Issues)
            output.WriteLine($"ignored: {issue.Path} — {issue.Reason}");

        output.WriteLine($"{scan.Mods.Count} mod(s) discovered");
        return 0;
    }

    public int Validate()
    {
        var (adapter, _) = DetectAdapter();
        if (adapter is null)
        {
            output.WriteLine($"No supported game detected in {context.GameDirectory}");
            return 1;
        }

        var result = adapter.ValidateInstallation(context);
        foreach (var message in result.Messages)
            output.WriteLine($"[{message.Severity.ToString().ToLowerInvariant()}] {message.Check}: {message.Message}");

        output.WriteLine(result.IsValid ? "Validation passed" : "Validation failed");
        return result.IsValid ? 0 : 1;
    }

    public int Doctor()
    {
        var (adapter, detection) = DetectAdapter();
        if (adapter is null)
        {
            output.WriteLine($"Game: not detected in {context.GameDirectory}");
            output.WriteLine("Status: not ready");
            return 1;
        }

        var scan = new ModScanner().Scan(context.ModsDirectory);
        var resolution = new DependencyResolver().Resolve(scan.Mods);
        var validation = adapter.ValidateInstallation(context);
        var injection = adapter.GetInjectionConfiguration(context);
        var requiredMods = adapter.GetRequiredMods();
        var discoveredIds = scan.Mods.Select(m => m.ModId).ToHashSet(StringComparer.OrdinalIgnoreCase);

        output.WriteLine($"Game: {adapter.DisplayName}");
        output.WriteLine($"Adapter: {adapter.Id}");
        output.WriteLine($"Steam App ID: {string.Join(", ", adapter.SteamAppIds)}");
        output.WriteLine($"Executable: {(detection!.IsDetected ? "found" : "missing")}");
        output.WriteLine($"Proxy DLL: {injection.PreferredProxyDll}");
        output.WriteLine($"Wine override: {injection.WineDllOverride}");

        var missingRequired = new List<string>();
        foreach (var required in requiredMods)
        {
            var found = discoveredIds.Contains(required.Id);
            if (!found)
                missingRequired.Add(required.Id);
            output.WriteLine($"Required mod: {required.Id} {(found ? "found" : "MISSING")}");
        }

        output.WriteLine($"Discovered mods: {scan.Mods.Count}");
        output.WriteLine($"Missing dependencies: {resolution.MissingDependencies.Count}");
        foreach (var missing in resolution.MissingDependencies)
            output.WriteLine($"  {missing.ModId} requires {missing.MissingDependencyId}");

        foreach (var message in validation.Messages.Where(m => m.Severity != ValidationSeverity.Info))
            output.WriteLine($"[{message.Severity.ToString().ToLowerInvariant()}] {message.Check}: {message.Message}");

        var ready = validation.IsValid && missingRequired.Count == 0 && resolution.IsComplete;
        output.WriteLine($"Status: {(ready ? "ready" : "not ready")}");
        return ready ? 0 : 1;
    }

    public int Restore()
    {
        var (adapter, _) = DetectAdapter();
        if (adapter is null)
        {
            output.WriteLine($"No supported game detected in {context.GameDirectory}");
            return 1;
        }

        var lines = adapter.RestoreAsync(context, CancellationToken.None).GetAwaiter().GetResult();
        foreach (var line in lines)
            output.WriteLine(line);
        output.WriteLine("Restore complete — game files are back to their pre-mod state.");
        return 0;
    }

    public int Sync(bool dryRun)
    {
        if (!OperatingSystem.IsWindows() && !dryRun)
        {
            output.WriteLine("Native host sync cannot write Windows loader paths. Use fetch-base-mods for host preparation and the Windows in-process bootstrap for sync.");
            return 1;
        }
        var (adapter, detection) = DetectAdapter();
        if (adapter is null)
        {
            output.WriteLine($"No supported game detected in {context.GameDirectory}");
            return 1;
        }

        var validation = adapter.ValidateInstallation(context);
        if (!validation.IsValid)
        {
            foreach (var message in validation.Messages.Where(m => m.Severity == ValidationSeverity.Error))
                output.WriteLine($"[error] {message.Check}: {message.Message}");
            output.WriteLine("Sync aborted: installation validation failed");
            return 1;
        }

        var scan = new ModScanner().Scan(context.ModsDirectory);
        var resolution = new DependencyResolver().Resolve(scan.Mods);

        foreach (var missing in resolution.MissingDependencies)
            output.WriteLine($"[warning] {missing.ModId} requires missing dependency {missing.MissingDependencyId}");

        var requiredEnabled = adapter.GetRequiredMods()
            .Where(m => m.Enabled)
            .Select(m => m.Id)
            .ToList();

        var plan = new ConfigGenerator().Plan(
            context.GeneratedDirectory,
            appId: detection!.MatchedExecutable!,
            appName: adapter.DisplayName,
            appLocation: detection.ExecutablePath!,
            orderedMods: resolution.OrderedMods,
            requiredEnabledModIds: requiredEnabled);

        output.WriteLine($"Enabled mods ({plan.EnabledMods.Length}): {string.Join(", ", plan.EnabledMods)}");
        output.WriteLine($"Target: {plan.TargetPath}");

        if (dryRun)
        {
            output.WriteLine(plan.WouldChange ? "Dry run: config would change" : "Dry run: no changes");
            return 0;
        }

        var changed = new ConfigGenerator().Apply(plan);
        output.WriteLine(changed ? "Config written" : "Config already up to date");
        return 0;
    }

    private (IGameAdapter? Adapter, GameDetectionResult? Detection) DetectAdapter()
    {
        foreach (var adapter in adapters)
        {
            var detection = adapter.Detect(context);
            if (detection.IsDetected)
                return (adapter, detection);
        }

        return (null, null);
    }
}
