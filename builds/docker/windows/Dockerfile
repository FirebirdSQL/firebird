# escape=`

FROM mcr.microsoft.com/dotnet/framework/sdk:4.8-windowsservercore-ltsc2022

SHELL ["cmd", "/S", "/C"]

# set visualstudio2019buildtools --version and --installChannelUri.
#
# Open https://docs.microsoft.com/en-us/visualstudio/releases/2019/history#installing-an-earlier-release
# Download BuildTools for a specific version.
# Open (extract) the downloaded file.
# Open file vs_setup_bootstrapper.json and extract the installChannelUri value.
#
# 16.11.6.0 - https://aka.ms/vs/16/release/201528995_-1285443981/channel
# 16.11.7.0 - https://aka.ms/vs/16/release/152566872_220409660/channel
# 16.11.16 - https://aka.ms/vs/16/release/377566269_-1382739058/channel

RUN `
    set chocolateyUseWindowsCompression='false' && `
    powershell -NoProfile -ExecutionPolicy Bypass -Command "iex ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))" && `
    set "PATH=%PATH%;%ALLUSERSPROFILE%\chocolatey\bin" && `
    `
    choco install --no-progress --yes visualstudio2019buildtools --version=16.11.16 --package-parameters " `
        --installChannelUri https://aka.ms/vs/16/release/377566269_-1382739058/channel `
        --quiet --wait --norestart --nocache `
        --locale en-US `
        --add Microsoft.Component.MSBuild `
        --add Microsoft.VisualStudio.Component.VC.ATLMFC `
        --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        --add Microsoft.VisualStudio.Component.VC.CMake.Project `
        --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        --add Microsoft.VisualStudio.Component.Windows10SDK.19041 `
        --add Microsoft.VisualStudio.ComponentGroup.NativeDesktop.Core `
        --add Microsoft.VisualStudio.Workload.NativeDesktop `
        --remove Microsoft.VisualStudio.Component.Windows10SDK.10240 `
        --remove Microsoft.VisualStudio.Component.Windows10SDK.10586 `
        --remove Microsoft.VisualStudio.Component.Windows10SDK.14393 `
        --remove Microsoft.VisualStudio.Component.Windows81SDK `
        " && `
    choco install --no-progress --yes git --version=2.33.1 && `
    choco install --no-progress --yes 7zip.install --version=19.0 && `
    choco install --no-progress --yes innosetup --version=6.1.2 && `
    `
    refreshenv && `
    setx PATH "%PATH%;C:\Program Files\Git\usr\bin"

ENV SEVENZIP='"C:\Program Files\7-Zip"'
ENV INNO6_SETUP_PATH='"C:\Program Files (x86)\Inno Setup 6"'

COPY scripts\* C:\fbscripts\
