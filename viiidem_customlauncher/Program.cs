using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

namespace viiidem_customlauncher_min
{
    internal static class Program
    {
        // ============================================================
        // CONFIGURE THE CHILD'S CONSOLE AT PROCESS CREATION
        // The Demaster console remains available for logs without window polling.
        // ============================================================
        private const uint UseStartupWindowVisibility = 0x00000001;

        private static int Main(string[] args)
        {
            // Base directory where this launcher is running
            string baseDir = AppDomain.CurrentDomain.BaseDirectory;

            // Resolve FFVIII.exe (current dir, fallback: parent dir)
            string ff8Path = Path.Combine(baseDir, "FFVIII.exe");
            if (!File.Exists(ff8Path))
            {
                string parent = Directory.GetParent(baseDir.TrimEnd(Path.DirectorySeparatorChar))?.FullName ?? baseDir;
                string alt = Path.Combine(parent, "FFVIII.exe");
                if (File.Exists(alt)) ff8Path = alt;
            }

            // ============================================================
            // REJECT MISSING GAME FILES WITHOUT AN UNHANDLED CLR EXCEPTION
            // The bootstrap is distributed separately from the Steam game.
            // ============================================================
            if (!File.Exists(ff8Path))
                return 3;
            if (!IsPortableExecutable(ff8Path))
                return 4;

            // Default behavior: NORMAL. Only hide when we receive --silent.
            bool silent = false;
            if (args != null)
            {
                foreach (var a in args)
                {
                    if (string.Equals(a, "--silent", StringComparison.OrdinalIgnoreCase))
                    {
                        silent = true;
                        break;
                    }
                }
            }

            // ============================================================
            // REPORT PROCESS-START FAILURES THROUGH A CONTROLLED EXIT CODE
            // Invalid or blocked installations must not trigger Windows Error Reporting.
            // ============================================================
            try
            {
                return StartGame(ff8Path, silent);
            }
            catch (Exception exception) when (
                exception is System.ComponentModel.Win32Exception ||
                exception is IOException ||
                exception is UnauthorizedAccessException ||
                exception is InvalidOperationException ||
                exception is System.Security.SecurityException)
            {
                return 4;
            }
        }

        // ============================================================
        // START THE FIXED EXECUTABLE WITH AN EXPLICIT APPLICATION PATH
        // Startup visibility also applies to the console later allocated by Demaster.
        // ============================================================
        private static int StartGame(string gameExecutablePath, bool silent)
        {
            string absoluteExecutablePath = Path.GetFullPath(gameExecutablePath);
            var startupInformation = new StartupInformation
            {
                Size = Marshal.SizeOf(typeof(StartupInformation)),
                Flags = UseStartupWindowVisibility,
                ShowWindow = (short)(silent ? 0 : 1)
            };
            var commandLine = new StringBuilder("\"" + absoluteExecutablePath + "\"");
            if (!CreateProcessW(absoluteExecutablePath, commandLine,
                IntPtr.Zero, IntPtr.Zero, false, 0,
                IntPtr.Zero, Path.GetDirectoryName(absoluteExecutablePath),
                ref startupInformation, out ProcessInformation processInformation))
                return 4;

            // ============================================================
            // RELEASE ONLY THE HANDLES RETURNED FOR THE NEW CHILD PROCESS
            // The game continues independently; no other process is opened or altered.
            // ============================================================
            CloseHandle(processInformation.Thread);
            CloseHandle(processInformation.Process);
            return 0;
        }

        // ============================================================
        // CHECK COMPLETE PE HEADERS BEFORE WINDOWS ATTEMPTS EXECUTION
        // Reject truncated headers; this is not a signature or trust check.
        // ============================================================
        private static bool IsPortableExecutable(string executablePath)
        {
            try
            {
                using (var stream = File.OpenRead(executablePath))
                using (var reader = new BinaryReader(stream))
                {
                    if (stream.Length < 64 || reader.ReadUInt16() != 0x5A4D)
                        return false;
                    stream.Position = 60;
                    long headerOffset = reader.ReadUInt32();
                    if (headerOffset < 64 || headerOffset > stream.Length - 24)
                        return false;
                    stream.Position = headerOffset;
                    if (reader.ReadUInt32() != 0x00004550)
                        return false;
                    // ============================================================
                    // BOUND THE COFF OPTIONAL HEADER AND SECTION TABLE
                    // Use wide offsets to avoid wrapping file positions above 4 GiB.
                    // ============================================================
                    stream.Position = headerOffset + 6;
                    ushort sectionCount = reader.ReadUInt16();
                    stream.Position = headerOffset + 20;
                    ushort optionalHeaderSize = reader.ReadUInt16();
                    ushort characteristics = reader.ReadUInt16();
                    long optionalHeaderOffset = headerOffset + 24;
                    if (sectionCount == 0 || sectionCount > 96 ||
                        (characteristics & 0x0002) == 0 || (characteristics & 0x2000) != 0 ||
                        optionalHeaderSize < 2 || optionalHeaderOffset > stream.Length - optionalHeaderSize)
                        return false;
                    ushort optionalHeaderMagic = reader.ReadUInt16();
                    int minimumOptionalHeaderSize;
                    if (optionalHeaderMagic == 0x010B)
                        minimumOptionalHeaderSize = 96;
                    else if (optionalHeaderMagic == 0x020B)
                        minimumOptionalHeaderSize = 112;
                    else
                        return false;
                    long sectionTableSize = (long)sectionCount * 40;
                    return optionalHeaderSize >= minimumOptionalHeaderSize &&
                        optionalHeaderOffset + optionalHeaderSize <= stream.Length - sectionTableSize;
                }
            }
            catch (IOException) { return false; }
            catch (UnauthorizedAccessException) { return false; }
        }

        // ============================================================
        // DECLARE PROCESS CREATION INTEROP WITHOUT DESKTOP WINDOW OPERATIONS
        // Structures match the Unicode Win32 STARTUPINFO and PROCESS_INFORMATION ABI.
        // ============================================================

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        private struct StartupInformation
        {
            public int Size;
            public IntPtr Reserved;
            public IntPtr Desktop;
            public IntPtr Title;
            public uint X, Y, Width, Height, BufferWidth, BufferHeight, FillAttributes;
            public uint Flags;
            public short ShowWindow;
            public short ReservedLength;
            public IntPtr ReservedBytes;
            public IntPtr StandardInput, StandardOutput, StandardError;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct ProcessInformation
        {
            public IntPtr Process, Thread;
            public uint ProcessId, ThreadId;
        }

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, ExactSpelling = true, SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool CreateProcessW(string applicationName, StringBuilder commandLine,
            IntPtr processAttributes, IntPtr threadAttributes,
            [MarshalAs(UnmanagedType.Bool)] bool inheritHandles, uint creationFlags,
            IntPtr environment, string currentDirectory, ref StartupInformation startupInformation,
            out ProcessInformation processInformation);

        [DllImport("kernel32.dll", ExactSpelling = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool CloseHandle(IntPtr handle);
    }
}
