# Introduction
Win11 right click menu option, copy path and replace backslash(`'\'`) to slash(`'/'`), friendly to program
# Install
- Set infomation in cmake and build
- Copy binary file(context_menu.dll, installer.exe and uninstaller.exe) to `./sparse_package`
- Run cmd in `./sparse_package` to generate msix file
    ```
    makeappx pack /d . /p CopyPath.msix
    ```
    makeappx is in windows sdk, typically `C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\makeappx.exe`
- Sign package. 

    First, create a certificate, see `https://learn.microsoft.com/en-us/windows/msix/package/create-certificate-package-signing`. The result is a pfx file.

    Click the pfx file, add to `Trusted Root Certification Authorities`.

    Run this cmd to sign msix
    ```
    signtool sign /f <your .pfx file> /p <your certificate password> /t http://timestamp.digicert.com /fd SHA256 CopyPath.msix
    ```
    signtool is also in windows sdk.
- Run `installer.exe`(in administrator mode) to install, **not the msix file**


# Uninstall
Run `unstaller.exe` in administrator mode. If the directory is in used, restart resource manager.