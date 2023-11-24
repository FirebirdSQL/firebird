# escape=`

FROM mcr.microsoft.com/dotnet/framework/sdk:4.8-windowsservercore-ltsc2022

SHELL ["cmd", "/S", "/C"]

# Installation of specific Visual Studio build tools version:
# - Open https://learn.microsoft.com/en-us/visualstudio/releases/2022/release-history
# - Download BuildTools for a specific version.
# - Open (extract) the downloaded file, for example using 7-Zip.
# - Open file vs_setup_bootstrapper.json and extract the installChannelUri to VS_INSTALL_CHANNEL_URI.

# 17.7.4
ARG VS_INSTALL_CHANNEL_URI=https://aka.ms/vs/17/release/134599118_118780349/channel

ADD https://aka.ms/vs/17/release/vs_buildtools.exe C:/temp/vs_BuildTools.exe

RUN `
    (start /w C:\temp\vs_BuildTools.exe `
        --installPath C:\VisualStudio `
        --channelUri %VS_INSTALL_CHANNEL_URI% `
        --installChannelUri %VS_INSTALL_CHANNEL_URI% `
        --quiet --wait --norestart --nocache --noUpdateInstaller `
        --locale en-US `
        --add Microsoft.Component.MSBuild `
        --add Microsoft.VisualStudio.Component.VC.ATLMFC `
        --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        --add Microsoft.VisualStudio.Component.VC.CMake.Project `
        --add Microsoft.VisualStudio.Component.VC.Redist.14.Latest `
        --add Microsoft.VisualStudio.Component.VC.Redist.MSM `
        --add Microsoft.VisualStudio.Component.Windows10SDK.19041 `
        --add Microsoft.VisualStudio.ComponentGroup.NativeDesktop.Core `
        --add Microsoft.VisualStudio.Workload.NativeDesktop `
        --remove Microsoft.VisualStudio.Component.Windows10SDK.10240 `
        --remove Microsoft.VisualStudio.Component.Windows10SDK.10586 `
        --remove Microsoft.VisualStudio.Component.Windows10SDK.14393 `
        --remove Microsoft.VisualStudio.Component.Windows81SDK `
        || if "%ERRORLEVEL%"=="3010" exit 0) && `
    `
    set chocolateyUseWindowsCompression='false' && `
    powershell -NoProfile -ExecutionPolicy Bypass -Command "iex ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))" && `
    set "PATH=%PATH%;%ALLUSERSPROFILE%\chocolatey\bin" && `
    `
    choco install --no-progress --yes git --version=2.33.1 && `
    choco install --no-progress --yes 7zip.install --version=19.0 && `
    choco install --no-progress --yes innosetup --version=6.1.2 && `
    choco install --no-progress --yes wixtoolset --version=3.11.2 && `
    `
    refreshenv && `
    setx PATH "%PATH%;C:\Program Files\Git\usr\bin"

ENV SEVENZIP='C:\Program Files\7-Zip'
ENV INNO6_SETUP_PATH='C:\Program Files (x86)\Inno Setup 6'
ENV WIX='C:\Program Files (x86)\WiX Toolset v3.11'

COPY scripts\* C:\fbscripts\
