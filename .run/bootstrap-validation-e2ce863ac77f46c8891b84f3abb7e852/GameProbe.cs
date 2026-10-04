using System;
using System.IO;
using System.Threading;
internal static class GameProbe {
    private static int Main() {
        File.WriteAllText(Path.Combine(Environment.CurrentDirectory, "started.txt"), Environment.CurrentDirectory);
        Thread.Sleep(100);
        return 0;
    }
}
