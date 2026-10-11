using System.Reflection.PortableExecutable;
using System.Text.Json;
using ReloadedDropIn.Adapter.Abstractions;
using ReloadedDropIn.Adapter.DSTS;
using ReloadedDropIn.Cli;

namespace ReloadedDropIn.Tests;

public sealed class ProtonPlanTests : IDisposable
{
    private readonly TempDirectory temp = new();
    public void Dispose() => temp.Dispose();

    private Commands CommandsFor(Machine machine, StringWriter output)
    {
        PayloadCompatibilityTests.WriteNative(Path.Combine(temp.Path, "Digimon Story Time Stranger.exe"), machine);
        return new Commands([new DstsAdapter()], new AdapterContext {
            GameDirectory = temp.Path, ModsDirectory = Path.Combine(temp.Path, "mods"),
            DropInDirectory = Path.Combine(temp.Path, "reloaded-dropin") }, output);
    }

    [Fact]
    public void X64GamePlanDoesNotGrantLaunchOrWriteState()
    {
        using var output = new StringWriter();
        Assert.Equal(0, CommandsFor(Machine.Amd64, output).ProtonPlan());
        using var json = JsonDocument.Parse(output.ToString());
        Assert.Equal("win-x64", json.RootElement.GetProperty("PayloadRid").GetString());
        Assert.False(json.RootElement.GetProperty("CanLaunchModded").GetBoolean());
        Assert.True(json.RootElement.GetProperty("CanPrepareDependencies").GetBoolean());
        Assert.Equal(7, json.RootElement.GetProperty("RequiredMods").GetArrayLength());
        Assert.False(Directory.Exists(Path.Combine(temp.Path, "reloaded-dropin")));
        Assert.False(Directory.Exists(Path.Combine(temp.Path, "mods")));
    }

    [Fact]
    public void Arm64WindowsGameIsBlockedDespiteArm64Coordinator()
    {
        using var output = new StringWriter();
        Assert.Equal(1, CommandsFor(Machine.Arm64, output).ProtonPlan());
        using var json = JsonDocument.Parse(output.ToString());
        Assert.False(json.RootElement.GetProperty("CanPrepareDependencies").GetBoolean());
        Assert.False(json.RootElement.GetProperty("CanLaunchModded").GetBoolean());
    }

    [Fact]
    public void NativeSyncDoesNotWriteWindowsConfiguration()
    {
        if (OperatingSystem.IsWindows()) return;
        using var output = new StringWriter();
        Assert.Equal(1, CommandsFor(Machine.Amd64, output).Sync(false));
        Assert.False(Directory.Exists(Path.Combine(temp.Path, "reloaded-dropin")));
    }

    [Fact]
    public void UnsupportedFetchDoesNotWriteState()
    {
        using var output = new StringWriter();
        Assert.Equal(1, CommandsFor(Machine.Arm64, output).FetchBaseMods());
        Assert.False(Directory.Exists(Path.Combine(temp.Path, "reloaded-dropin")));
    }
}
