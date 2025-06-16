icu_windows.zip is a pre-built (by us) IBM ICU 77.1 library.

The sources was downloaded from https://github.com/unicode-org/icu/releases/tag/release-77-1.

The build was done using VS 2022 (17.13.6) in a way that ICU data was not included in icudt DLL.

To do it, follow these steps:
- Download icu4c-77_1-src.zip and icu4c-77_1-data.zip
- Unzip icu4c-77_1-src.zip
- Delete source\data directory from the extracted icu4c-77_1-src.zip
- Unzip data from icu4c-77_1-data.zip into source\data from the extracted icu4c-77_1-src.zip
- Create a file named filter.json with the content: {"strategy": "additive"}
- Set environment variable ICU_DATA_FILTER_FILE to full path of created filter.json file
- Open Visual Studio with the environment variable set
- Build solution

To update data, download icu4c-77_1-data-bin-l.zip, extract it and add the data file to icudt.zip.

---

tzdata is automatically updated (pull request created) by GitHub Actions tzdata-update.yml.
