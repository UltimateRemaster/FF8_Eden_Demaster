

# ff8_demaster
FF8_Demaster is unofficial patch for Final Fantasy VIII Remastered game by SquareEnix. It works as DLL-dependency and has custom launcher with brand new functions. Patch makes it possible to load new textures, use non-fixed-resolution textures and will aim in tweaking issues

## Fork:
This fork of FF8_Demaster changes `viiidem_customlauncher/Program.cs` to provide a custom launcher executable (built from the `viiidem_customlauncher` project) that can run silently in the background. The goal is to avoid any visible user interface while it does its job, meaning no pop-ups, no taskbar window, and no tray icon. In practice, this makes it ideal for being called by an external tool (such as a mod manager or game launcher) without interrupting the user’s workflow.

- For more information, visit the main repository:

https://github.com/MaKiPL/FF8_demaster
