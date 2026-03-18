THE ENJIN GAME ENGINE (TEGE)
============================

SYSTEM REQUIREMENTS
  - Windows 10 or 11 (64-bit)
  - Vulkan-capable GPU with up-to-date drivers
  - Visual C++ Redistributable 2022

HOW TO RUN
  Double-click bin\EnjinEditor.exe to launch the editor.
  To open a project, drag a .enjin file onto EnjinEditor.exe.

TROUBLESHOOTING

  App won't start or crashes immediately:
    Install the Visual C++ Redistributable 2022 (x64):
    https://aka.ms/vs/17/release/vc_redist.x64.exe

  Black screen or Vulkan error:
    Update your GPU drivers:
      NVIDIA:  https://www.nvidia.com/drivers
      AMD:     https://www.amd.com/en/support
      Intel:   https://www.intel.com/content/www/us/en/download-center

  Missing DLL error:
    Install the Visual C++ Redistributable linked above.

  Poor performance:
    Make sure your system is using the discrete GPU, not integrated
    graphics. Check your GPU control panel or Windows Graphics Settings.

HELP & DOCUMENTATION
  https://www.marty64.net/enjin
