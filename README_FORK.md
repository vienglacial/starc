# Jenny Script

Jenny Script is a fork of [Story Architect (STARC)](https://github.com/story-apps/starc) maintained by
Jenny Pixel Studio. It adds a Google Docs / Google Drive bridge on top of the open-source part of STARC.

Based on Story Architect by Story Apps. Licensed under GPL-3.0, see [LICENSE](LICENSE).

The app still shows the name "Story Architect" for now, but this is not an official Story Apps
release: please report problems here, not to the Story Apps team.

Downloads for Windows and macOS: [Releases](https://github.com/vienglacial/starc/releases). Credits and third party
licences: [NOTICE.txt](NOTICE.txt).

Roadmap: [docs/FORK_PLAN.md](docs/FORK_PLAN.md).

## Differences from upstream

- Built from public sources only: the private `src/cloud` submodule and the private PRO plugins are not available.
- Crashpad / BugSplat crash reporting can be disabled with `CONFIG+=no_crashpad` (the fork CI always does this).
- Vietnamese UI translation: `src/core/translations/translation_vi.ts`. The `.qm` is not committed; CI builds it with
  `lrelease src/core/translations/translation_vi.ts -qm src/core/translations/translation_vi.qm`
  (run the same command before `qmake` when building locally).
- Own CI workflow: [.github/workflows/build_fork.yml](.github/workflows/build_fork.yml).
  Upstream workflows need private submodules and secrets; disable them in the GitHub UI
  (Actions → workflow → Disable workflow) instead of deleting them.

## Build

Qt 6.11.x (6.8 minimum), modules `qt5compat qtimageformats qtmultimedia qtspeech qtwebsockets qtnetworkauth`.

Init **only** the public submodules:

```bash
git submodule update --init --recursive src/3rd_party/pdfhummus src/3rd_party/pdftextextraction src/3rd_party/qtkeychain
```

Linux / macOS:

```bash
cd src && qmake starc.pro CONFIG+=release CONFIG+=no_crashpad && make -j8
```

Windows (x64 Native Tools Command Prompt for VS 2022, Qt `msvc2022_64` in `PATH`,
OpenSSL 3 at a path without spaces in `OPENSSL_PREFIX`):

```bat
cd src && qmake starc.pro CONFIG+=release CONFIG+=no_crashpad && jom -j4
```

macOS package: build with `QMAKE_APPLE_DEVICE_ARCHS="x86_64 arm64"`, then run
`bash build/mac/make_fork_dmg.sh`. It has an ad-hoc signature only, not an Apple Developer ID.

## Releasing

1. Raise `kVersion` in `src/core/management_layer/content/update/update_info.h`, push, wait for CI.
2. Pack the Windows artifact as `JennyScript-v<version>-windows-x64.zip` with one root folder.
   The macOS artifact already holds `JennyScript-v<version>-macos.dmg`.
3. Tag the commit `v<version>` and publish it as a normal GitHub release (not a prerelease), with
   both files.

On Windows the app looks at the latest release when it starts and offers it if the version is
newer. The macOS version does not check for updates. To test
the update flow, run the app as an older version in PowerShell:
`$env:JENNY_SCRIPT_TEST_VERSION='0.8.4-jenny.0'; .\starcapp.exe`

## Syncing with upstream

Remotes: `origin` = this fork, `upstream` = `https://github.com/story-apps/starc`.
Once a month or after an upstream release:

```bash
git fetch upstream && git checkout master && git merge upstream/master
```

## Upstream files modified by the fork

Keep this list up to date: these are the files to check when merging upstream.

| File | Change |
|---|---|
| `src/core/core.pro` | Crashpad block wrapped in `!no_crashpad` |
| `src/core/management_layer/application_manager.cpp` | Crashpad includes, client, `initCrashpad()` and `sendCrashInfo()` guarded by `NO_CRASHPAD`; startup telemetry (`telemetry.storyapps.dev`) disabled in `sendStartupStatistics()` |
| `src/core/management_layer/application_manager.cpp` | `QLocale::Vietnamese` → `translation_vi.qm` |
| `src/core/ui/settings/language_dialog.cpp`, `src/core/ui/settings/settings_view.cpp` | "Tiếng Việt" in the language list |
| `src/core/ui/onboarding/onboarding_navigator.cpp`, `src/core/ui/onboarding/onboarding_view.cpp` | "Tiếng Việt" in the first-run language list |
| `src/starc.pro`, `src/core/translations.qrc` | `translation_vi.ts` / `translation_vi.qm` registered |
| `src/corelib/business_layer/import/abstract_fountain_importer.cpp` | input goes through `FountainTextNormalizer` (scene numbers, Vietnamese headings, Google Docs characters); a leading ellipsis (`..`) is no longer a forced scene heading |
| `src/corelib/corelib.pro` | `fountain_text_normalizer.{h,cpp}`, `screenplay_google_doc_importer.{h,cpp}` and `screenplay_dossier.{h,cpp}` registered |
| `src/corelib/business_layer/model/structure/structure_proxy_model.{h,cpp}` | `setHiddenDocumentTypes()`: documents without an editor are hidden in the navigator (statistics, characters, locations, worlds, mind map, images gallery, presentation, series: their plugins are private) |
| `src/core/management_layer/plugins_builder.{h,cpp}` | editors whose plugin library is missing are left out of the toolbar (`editorsInfoFor`); `documentMimeTypesWithoutEditors()`; the plugin folder lookup is shared by `pluginsDirectory()` |
| `src/core/ui/project/create_document_dialog.{h,cpp}` | `setHiddenDocumentTypes()`: document types without an editor and "Split to episodes" are not offered |
| `src/core/ui/projects/create_project_dialog.cpp` | without `CLOUD_SERVICE_MANAGER` the story is always local and the notes about the cloud are hidden |
| `src/core/management_layer/content/projects/projects_manager.cpp` | "Move to the cloud" in the project context menu only with `CLOUD_SERVICE_MANAGER` |
| `application_manager.cpp`, `onboarding_navigator.cpp`, `onboarding_view.cpp`, `language_dialog.cpp` | Vietnamese is the default and first language; onboarding saves the chosen language instead of the system one (upstream bug) |
| `src/corelib/utils/helpers/color_helper.cpp`, `src/corelib/ui/modules/comments/comments_toolbar.cpp` | industry standard revision colors (Blue, Pink, Yellow, Green, Goldenrod, Buff, Salmon, Cherry, Tan) as highlight with a dark text tone; upstream colors still map to the same level |
| `src/core/core.pro` | `include(management_layer/content/google/google.pri)` (Google integration, QtKeychain) |
| `src/core/management_layer/content/project/project_manager.{h,cpp}` | Google Docs items in the navigator context menu and the toolbar button (`googleDocsActionRequested`), `modelForDocument()`, `screenplayDocuments()`, `siblingModelForDocument()`, `addDocumentDraft()`; `hiddenDocumentTypes` computed from the available plugins |
| `src/core/ui/project/project_tool_bar.{h,cpp}` | Google Docs button after the view icons (`setGoogleDocsVisible()`, `googleDocsPressed()`) |
| `src/core/management_layer/application_manager.cpp` | `GoogleDocsManager` created and connected under `GOOGLE_INTEGRATION` |
| `src/corelib/business_layer/document/text/text_document.cpp` | revision marks are applied as highlight (`addReviewMark`) |
| `src/corelib/data_layer/storage/settings_storage.cpp` | default screenplay / simple text templates use Courier New (Courier Prime has no Vietnamese letters) |
| `src/core/management_layer/plugins/screenplay_text/text/screenplay_text_edit.cpp` | light band under scene headings in the editor (`paintEvent`, not exported); pasted plain text goes through `FountainTextNormalizer::formatPlainText`; "Auto format" in the context menu (`autoFormatText()`) |
| `src/core/ui/onboarding/onboarding_navigator.{h,cpp}`, `src/core/management_layer/content/onboarding/onboarding_manager.{h,cpp}` | optional Google sign in on the onboarding sign in page (`showGoogleAccount()`) |
| `src/core/ui/settings/settings_view.{h,cpp}` | `addCardOnTop()` for the Google account card |
| `src/corelib/ui/widgets/text_edit/scalable_wrapper/scalable_wrapper.cpp`, `src/corelib/corelib.pro` | middle mouse button auto scroll in the text editors (`MiddleButtonAutoScroll`, new file) |
| `src/core/core.pro`, `src/core/management_layer/application_manager.cpp` | update check at startup on Windows (`AppUpdateManager`, new folder `content/update`) |
| `src/core/core.pro`, `src/core/management_layer/application_manager.cpp`, `src/core/management_layer/content/settings/settings_manager.{h,cpp}`, `src/core/ui/settings/settings_view.{h,cpp}`, `src/corelib/data_layer/storage/settings_storage.{h,cpp}` | choice of typing sounds (`TypingSoundPlayer`, new folder `content/typing_sounds`); the sounds are made by `build/sounds/generate_typing_sounds.py` |
| `src/core/ui/about_application_dialog.cpp`, `src/core/ui/menu_view.cpp` | show the fork version (`AppUpdate::kVersion`) instead of the upstream one |
| `src/corelib/data_layer/storage/settings_storage.cpp`, `src/core/ui/onboarding/onboarding_navigator.cpp`, `src/core/ui/settings/settings_view.cpp`, `src/core/ui/settings/settings_navigator.cpp` | AI assistant hidden (it needs the private cloud service): the setting always reads as off, which hides the editor toolbar buttons; no onboarding page and no settings item |
| `src/corelib/utils/helpers/quotes_generator.cpp` | Joss Whedon quote removed: without its context it reads like an insult to the user |
| `src/core/ui/about_application_dialog.cpp` | fork notice with the source links; the invitation to contact the upstream team is hidden; the version links to the fork releases |
| `src/corelib/ui/widgets/scroll_bar/scroll_bar.cpp` | overlay scroll bar is resized before it is moved, so it stays at the edge and can be dragged (upstream bug candidate) |
