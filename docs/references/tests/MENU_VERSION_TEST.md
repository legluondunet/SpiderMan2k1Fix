# Main-menu version label — PR test

Test an experimental CI build. Main-menu version rendering must not alter HUD behavior.

1. Open this pull request's **Checks** tab and select the successful
   **Build SpiderMan2k1Fix (32-bit MSVCRT)** job.
2. Download the **SpiderMan2k1Fix-Windows-x86** artifact from its workflow run.
3. Extract the downloaded artifact and the included ZIP if nested.
4. Back up, then replace **winmm.dll** and **SpiderMan2k1Fix.dll** next to
   **SpideyPC.exe**. Keep your existing INI.
5. Start the game in 1920x1080; check the bottom-right corner of the centered
   4:3 main menu for **SpiderMan2k1Fix v0.0.0-<short Git SHA>**.
6. Confirm it does not appear during the intro movies or gameplay.
7. Repeat in 4:3; verify menus, controls and Alt+Tab are unaffected.

Optionally set [Debug] Log=1 and submit MENU-VERSION lines plus a screenshot.
The PR must not be merged until the in-game rendering has been validated.
