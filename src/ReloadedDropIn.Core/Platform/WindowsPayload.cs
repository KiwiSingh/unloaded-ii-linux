using System.Reflection.PortableExecutable;

namespace ReloadedDropIn.Core.Platform;

/// <summary>The injected ABI follows the Windows game, never the Linux host.</summary>
public enum WindowsPayload { X64, X86, Arm64 }

public static class PayloadCompatibility
{
    public static WindowsPayload ReadGame(string path)
    {
        using var stream = File.OpenRead(path);
        using var pe = new PEReader(stream);
        if (pe.PEHeaders.PEHeader is null || pe.PEHeaders.CorHeader is not null)
            throw new InvalidDataException("Expected a native Windows game executable.");
        return pe.PEHeaders.CoffHeader.Machine switch
        {
            Machine.Amd64 => WindowsPayload.X64,
            Machine.I386 => WindowsPayload.X86,
            Machine.Arm64 => WindowsPayload.Arm64,
            _ => throw new InvalidDataException("Unsupported Windows game machine."),
        };
    }

    public static void RequireLibrary(string path, WindowsPayload target, bool native)
    {
        using var stream = File.OpenRead(path);
        using var pe = new PEReader(stream);
        var headers = pe.PEHeaders;
        if (headers.PEHeader is null)
            throw new InvalidDataException($"Invalid Windows payload: {path}");
        var expected = target switch
        {
            WindowsPayload.X64 => Machine.Amd64,
            WindowsPayload.X86 => Machine.I386,
            WindowsPayload.Arm64 => Machine.Arm64,
            _ => throw new InvalidDataException("Unsupported payload target."),
        };
        // Pure IL AnyCPU assemblies use I386 even in an x64 process. R2R and
        // native images must match the game ABI; 32-bit-required IL is not AnyCPU.
        var cor = headers.CorHeader;
        var anyCpu = !native && cor is not null &&
            cor.Flags.HasFlag(CorFlags.ILOnly) &&
            !cor.Flags.HasFlag(CorFlags.Requires32Bit) &&
            cor.ManagedNativeHeaderDirectory.Size == 0 &&
            headers.CoffHeader.Machine == Machine.I386;
        if (native && cor is not null || !native && cor is null || !anyCpu && headers.CoffHeader.Machine != expected)
            throw new InvalidDataException($"Payload architecture does not match Windows {target}: {path}");
    }
}
