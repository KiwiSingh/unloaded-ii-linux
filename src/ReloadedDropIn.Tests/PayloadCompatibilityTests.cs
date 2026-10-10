using System.Reflection.PortableExecutable;
using ReloadedDropIn.Core.Platform;

namespace ReloadedDropIn.Tests;

public sealed class PayloadCompatibilityTests : IDisposable
{
    private readonly TempDirectory temp = new();
    public void Dispose() => temp.Dispose();

    private string Native(Machine machine)
    {
        var path = Path.Combine(temp.Path, $"{machine}.dll");
        return WriteNative(path, machine);
    }

    internal static string WriteNative(string path, Machine machine)
    {
        // Minimal PE32+ image; no CLR header. PEReader must parse the real COFF machine.
        var bytes = new byte[512];
        using var stream = new MemoryStream(bytes);
        using var writer = new BinaryWriter(stream);
        writer.Write((ushort)0x5a4d);
        stream.Position = 0x3c; writer.Write(0x80);
        stream.Position = 0x80; writer.Write(0x00004550);
        writer.Write((ushort)machine); writer.Write((ushort)0);
        stream.Position = 0x94; writer.Write((ushort)240); writer.Write((ushort)0x2022);
        writer.Write((ushort)0x20b);
        stream.Position = 0x104; writer.Write((uint)16);
        File.WriteAllBytes(path, bytes);
        return path;
    }

    [Theory]
    [InlineData(Machine.Amd64, WindowsPayload.X64)]
    [InlineData(Machine.Arm64, WindowsPayload.Arm64)]
    public void ReadsGameMachineRatherThanHost(Machine machine, WindowsPayload expected) =>
        Assert.Equal(expected, PayloadCompatibility.ReadGame(Native(machine)));

    [Fact]
    public void RejectsArm64LibraryForX64Game() =>
        Assert.Throws<InvalidDataException>(() => PayloadCompatibility.RequireLibrary(Native(Machine.Arm64), WindowsPayload.X64, true));

    [Fact]
    public void AcceptsX64NativeLibrary() =>
        PayloadCompatibility.RequireLibrary(Native(Machine.Amd64), WindowsPayload.X64, true);

    [Fact]
    public void RejectsNativeImageAsManagedPayload() =>
        Assert.Throws<InvalidDataException>(() => PayloadCompatibility.RequireLibrary(Native(Machine.Amd64), WindowsPayload.X64, false));

    [Fact]
    public void AcceptsAnyCpuIlLibraryForX64Game() =>
        PayloadCompatibility.RequireLibrary(typeof(PayloadCompatibility).Assembly.Location, WindowsPayload.X64, false);

    [Fact]
    public void RejectsManagedDllAsNative() =>
        Assert.Throws<InvalidDataException>(() => PayloadCompatibility.RequireLibrary(typeof(PayloadCompatibility).Assembly.Location, WindowsPayload.X64, true));

    [Fact]
    public void RejectsMalformedGame()
    {
        var path = Path.Combine(temp.Path, "bad.exe"); File.WriteAllText(path, "MZ");
        Assert.Throws<BadImageFormatException>(() => PayloadCompatibility.ReadGame(path));
    }
}
