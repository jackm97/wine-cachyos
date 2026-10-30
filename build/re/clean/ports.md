# Port table: photolab-clean

Branch photolab-clean on Valve 0509a0b64a4. Order is commit order; skipped rows have no commit.

| Order | Item | Kind | Result | Method | Reason |
|---|---|---|---|---|---|
| 1 | tkg preset (valve-exp-bleeding) | tkg | clean | wine-tkg 04ddf86c968 non-makepkg-build.sh, tracked-file diff | preset result; configure/untracked generated files excluded |
| 2 | import-msicu.nu (50 wine-mirror commits) | msicu | clean | cherry-pick --no-commit with asserted conflict lists | all 50 commits and 7 blobs were local; all conflict lists matched |
| 3 | Proton wineopenxr b7a763327f0c | openxr | clean | cp of sparse checkout | import-wineopenxr.sh equivalent |
| 4 | advapi32-preserve-username | soda patch | clean | git apply |  |
| 5 | appx-package-queries | soda patch | clean | git apply |  |
| 6 | arm64-disable-aarch32 | soda patch | clean | git apply |  |
| 7 | combase-free-threaded-marshaler-agile | soda patch | clean | git apply |  |
| 8 | combase-winrt-support | soda patch | clean | git apply |  |
| 9 | comdlg32-shell-internet-url-items | soda patch | clean | git apply |  |
| 10 | coremessaging-user-properties | soda patch | clean | git apply |  |
| 11 | d2d1-cache-shape-shaders | soda patch | clean | git apply |  |
| 12 | d2d1-office-rendering | soda patch | clean | git apply |  |
| 13 | dcomp-device | soda patch | clean | git apply |  |
| 14 | dcomp-target-window-for-swap-chains | soda patch | clean | git apply |  |
| 15 | devices-input | soda patch | clean | git apply |  |
| 16 | dispatcher-queue-runtime | soda patch | clean | git apply |  |
| 17 | dwrite-scalar-alpha-texture-blit | soda patch | clean | git apply |  |
| 18 | dwrite-thai-font-fallback | soda patch | clean | git apply |  |
| 19 | dwrite-wine-data-fonts | soda patch | clean | git apply |  |
| 20 | gdi-thai-font-selection | soda patch | clean | git apply |  |
| 21 | kernel32-office-compat | soda patch | clean | git apply |  |
| 22 | loader-preserve-split-layout | soda patch | clean | git apply |  |
| 23 | media-closed-captioning | soda patch | clean | git apply |  |
| 24 | mod-loader-prefer-native | soda patch | clean | git apply |  |
| 25 | msi-c2r-product-code | soda patch | clean | git apply |  |
| 26 | msicu-cxx-cross-compiler | soda patch | clean | git apply |  |
| 27 | msicu-drop-unavailable-headers | soda patch | clean | git apply |  |
| 28 | msicu-libunwind-config-order | soda patch | clean | git apply |  |
| 29 | msicu-toolchain-compat | soda patch | clean | git apply |  |
| 30 | msicu-ucrt-i386-math | soda patch | clean | git apply |  |
| 31 | ntdll-adaptive-launch-profile | soda patch | clean | git apply |  |
| 32 | ntdll-handle-unmapped-unix-paths | soda patch | clean | git apply |  |
| 33 | ntdll-heroes-vii-cpu-topology | soda patch | clean | git apply |  |
| 34 | ntdll-remove-stale-registry-temp | soda patch | clean | git apply |  |
| 35 | ntdll-transparent-huge-pages | soda patch | clean | git apply |  |
| 36 | office-runtime-apis | soda patch | clean | git apply |  |
| 37 | office-winappsdk-webview2 | soda patch | clean | git apply |  |
| 38 | oleacc-implement-standard-accessible-proxy | soda patch | clean | git apply |  |
| 39 | onlineid-web-account-manager | soda patch | clean | git apply |  |
| 40 | prntvpt-null-print-ticket | soda patch | clean | git apply |  |
| 41 | registry-hive-codec | soda patch | clean | git apply |  |
| 42 | server-token-group-file-mode | soda patch | clean | git apply |  |
| 43 | software-licensing | soda patch | clean | git apply |  |
| 44 | steam-client-compatibility | soda patch | clean | git apply |  |
| 45 | storage-applicationdata | soda patch | clean | git apply |  |
| 46 | system-profile-systemid | soda patch | clean | git apply |  |
| 47 | twinapi-declarative-factory | soda patch | clean | git apply |  |
| 48 | uiautomationcore-office | soda patch | clean | git apply |  |
| 49 | urlmon-iertutil-uri | soda patch | clean | git apply |  |
| 50 | user32-popup-window-position | soda patch | clean | git apply |  |
| 51 | virtdisk-reject-non-virtual-storage | soda patch | clean | git apply |  |
| 52 | webservices-any-attributes-field-mapping | soda patch | clean | git apply |  |
| 53 | widl-winmd-image-size | soda patch | clean | git apply |  |
| 54 | win32u-fill-depth-only-display-mode | soda patch | clean | git apply |  |
| 55 | win32u-fix-office-font-rendering | soda patch | clean | git apply |  |
| 56 | win32u-gdi-flush-window-surfaces | soda patch | clean | git apply |  |
| 57 | win32u-reusable-client-surfaces | soda patch | clean | git apply |  |
| 58 | win32u-surface-for-client-windows | soda patch | clean | git apply |  |
| 59 | windows-applicationmodel-core | soda patch | clean | git apply |  |
| 60 | windows-globalization-words-segmenter | soda patch | clean | git apply |  |
| 61 | windows-networking | soda patch | clean | git apply |  |
| 62 | windows-security-enterprisedata | soda patch | clean | git apply |  |
| 63 | windows-ui-composition | soda patch | clean | git apply |  |
| 64 | windows-web-http-json | soda patch | clean | git apply |  |
| 65 | winebrowser-portal | soda patch | clean | git apply |  |
| 66 | winegstreamer-detect-kirikiri-media-reader | soda patch | clean | git apply |  |
| 67 | winegstreamer-release-buffer-after-flush | soda patch | clean | git apply |  |
| 68 | winegstreamer-skip-unconfigured-media-converter | soda patch | clean | git apply |  |
| 69 | wineopenxr-host-runtime | soda patch | clean | git apply |  |
| 70 | wineps-print-preview | soda patch | clean | git apply |  |
| 71 | winewayland-10-decorated-window-not-fullscreen | soda patch | clean | git apply |  |
| 72 | winewayland-20-input-language-to-focus | soda patch | clean | git apply |  |
| 73 | winewayland-30-office-surfaces | soda patch | clean | git apply |  |
| 74 | winewayland-40-ownerless-tool-popups-stay-attached | soda patch | clean | git apply |  |
| 75 | winewayland-50-pointer-skips-disabled-children | soda patch | clean | git apply |  |
| 76 | winewayland-60-show-gdi-children | soda patch | clean | git apply |  |
| 77 | winewayland-70-windows-on-monitors | soda patch | clean | git apply |  |
| 78 | winewayland-80-foreign-child-follows-parent | soda patch | clean | git apply |  |
| 79 | winex11-office-popups-reveal | soda patch | clean | git apply |  |
| 80 | winhttp-content-length-unknown-sentinel | soda patch | clean | git apply |  |
| 81 | winhttp-open-connection-release-on-cert-failure | soda patch | clean | git apply |  |
| 82 | winhttp-send-request-null-optional | soda patch | clean | git apply |  |
| 83 | winhttp-wininet-options | soda patch | clean | git apply |  |
| 84 | wininet-follow-absolute-redirects | soda patch | clean | git apply |  |
| 85 | xinput-guard-missing-steam-game-id | soda patch | clean | git apply |  |
| 86 | xmllite-reader-large-utf16 | soda patch | clean | git apply |  |
| 87 | xmllite-writer-explicit-xmlns-declaration | soda patch | clean | git apply |  |
| 88 | zz-d2d1-command-list-rebase | soda patch | clean | git apply |  |
| 89 | zz-d2d1-office-rendering-completion | soda patch | clean | git apply |  |
| 90 | zz-dispatcher-queue-current-thread | soda patch | clean | git apply |  |
| 91 | zz-mapi-outlook-startup | soda patch | clean | git apply |  |
| 92 | zz-office-composition-graphics-device2 | soda patch | clean | git apply |  |
| 93 | zz-office-composition-source-parameter | soda patch | clean | git apply |  |
| 94 | zz-office-display-mode-refresh | soda patch | clean | git apply |  |
| 95 | zz-office-native-storage-lock | soda patch | clean | git apply |  |
| 96 | zz-office-scissor-clamp | soda patch | clean | git apply |  |
| 97 | zz-office-shared-mode-settings | soda patch | clean | git apply |  |
| 98 | zz-office-vector3-keyframe-animation | soda patch | clean | git apply |  |
| 99 | zz-office-web-token-results | soda patch | clean | git apply |  |
| 100 | zz-office-window-surface-initial-paint | soda patch | clean | git apply |  |
| 101 | zz-office-window-surface-represent | soda patch | clean | git apply |  |
| 102 | zz-services-pipe-timeout | soda patch | clean | git apply |  |
| 103 | zz-soda-registration | soda patch | clean | git apply |  |
| 104 | zz-sppc-application-policies | soda patch | clean | git apply |  |
| 105 | zz-wineopenxr-long-types | soda patch | clean | git apply |  |
| 106 | zzz-d2d1-flush | soda patch | clean | git apply |  |
| 107 | zzz-eagle-debug-providers | soda patch | clean | git apply |  |
| 108 | zzz-office-interaction-tracker-close | soda patch | clean | git apply |  |
| 109 | zzzz-combase-mapi-client-classes | soda patch | clean | git apply |  |
| 110 | zzzz-combase-mapi-mime | soda patch | clean | git apply |  |
| 111 | zzzz-control-panel-host | soda patch | clean | git apply |  |
| 112 | zzzz-crypt32-hash-oid-cng | soda patch | clean | git apply |  |
| 113 | zzzz-d2d1-supported | soda patch | clean | git apply |  |
| 114 | zzzz-eagle-thread-status | soda patch | clean | git apply |  |
| 115 | zzzz-eagle-wait-close-filter | soda patch | clean | git apply |  |
| 116 | zzzz-msi-c2r-components | soda patch | clean | git apply |  |
| 117 | zzzz-office-virtual-surface-alignment | soda patch | clean | git apply |  |
| 118 | zzzzz-d2d1-image-transform | soda patch | clean | git apply |  |
| 119 | zzzzz-msi-c2r-component-clients | soda patch | clean | git apply |  |
| 120 | zzzzz-office-virtual-surface-tiles | soda patch | clean | git apply |  |
| 121 | zzzzz-wldap32-empty-host-list | soda patch | clean | git apply |  |
| 122 | zzzzzz-d2d1-arithmetic-image | soda patch | clean | git apply |  |
| 123 | zzzzzz-d2d1-wic-presentation-errors | soda patch | clean | git apply |  |
| 124 | zzzzzz-dxgi-present-regions | soda patch | clean | git apply |  |
| 125 | zzzzzz-msi-feature-handles | soda patch | clean | git apply |  |
| 126 | zzzzzz-oleaut32-typelib-registration | soda patch | clean | git apply |  |
| 127 | zzzzzz-twinapi-haptics-waveforms | soda patch | clean | git apply |  |
| 128 | zzzzzz-windows-ui-effect-factory | soda patch | clean | git apply |  |
| 129 | zzzzzz-windows-ui-update-offset | soda patch | clean | git apply |  |
| 130 | zzzzzz-windows-ui-vector-expressions | soda patch | clean | git apply |  |
| 131 | zzzzzz-windows-ui-visual-transform | soda patch | clean | git apply |  |
| 132 | zzzzzz-wldap32-host-whitespace | soda patch | clean | git apply |  |
| 133 | zzzzzzz-d2d1-gradient-modes | soda patch | clean | git apply |  |
| 134 | zzzzzzz-dcomp-window-styles | soda patch | clean | git apply |  |
| 135 | zzzzzzz-onlineid-long-socket-path | soda patch | clean | git apply |  |
| 136 | zzzzzzz-windows-ui-01-gradients | soda patch | clean | git apply |  |
| 137 | zzzzzzz-windows-ui-02-redirect-visual | soda patch | clean | git apply |  |
| 138 | zzzzzzz-windows-ui-03-visual-object2 | soda patch | clean | git apply |  |
| 139 | zzzzzzz-windows-ui-04-animation-group | soda patch | clean | git apply |  |
| 140 | zzzzzzz-windows-ui-05-implicit-collection | soda patch | clean | git apply |  |
| 141 | zzzzzzz-windows-ui-06-collection-close | soda patch | clean | git apply |  |
| 142 | zzzzzzz-windows-ui-07-collection-object2 | soda patch | clean | git apply |  |
| 143 | zzzzzzz-windows-ui-08-brush-lifetime | soda patch | clean | git apply |  |
| 144 | zzzzzzz-windows-ui-09-surface-commit | soda patch | clean | git apply |  |
| 145 | zzzzzzzz-eagle-identity-stages | soda patch | clean | git apply |  |
| 146 | zzzzzzzz-wined3d-native-shared-textures | soda patch | adapted | git apply --exclude generated + tools/make_requests | failed only on server_protocol.h/request_handlers.h/request_trace.h; regenerated, SERVER_PROTOCOL_VERSION 932 |
| 147 | zzzzzzzzz-ink-desktop-host-queue | soda patch | clean | git apply |  |
| 148 | zzzzzzzzz-opengl-vulkan-application-info | soda patch | clean | git apply |  |
| 149 | zzzzzzzzz-registry-app-hive | soda patch | clean | git apply |  |
| 150 | zzzzzzzzz-web-token-prompt | soda patch | clean | git apply |  |
| 151 | zzzzzzzzz-windows-ui-scalar-expressions | soda patch | clean | git apply |  |
| 152 | zzzzzzzzz-windows-ui-tracker-position | soda patch | clean | git apply |  |
| 153 | zzzzzzzzz-windows-ui-tracker-scale | soda patch | clean | git apply |  |
| 154 | zzzzzzzzz-windows-ui-tracker-scroll | soda patch | clean | git apply |  |
| 155 | zzzzzzzzz-windows-ui-tracker-values | soda patch | clean | git apply |  |
| 156 | zzzzzzzzz-windows-ui-tracker-z-clamping | soda patch | clean | git apply |  |
| 157 | zzzzzzzzz-windows-ui-tracker-zz-overloads | soda patch | clean | git apply |  |
| 158 | zzzzzzzzz-windows-ui-tracker-zzz-default-scale | soda patch | clean | git apply |  |
| 159 | zzzzzzzzz-windows-ui-tracker-zzzz-inertia | soda patch | clean | git apply |  |
| 160 | zzzzzzzzz-windows-ui-tracker-zzzzz-close-animation | soda patch | clean | git apply |  |
| 161 | zzzzzzzzz-windows-ui-tracker-zzzzzz-close-binding | soda patch | clean | git apply |  |
| 162 | zzzzzzzzz-windows-ui-z-target-layer | soda patch | clean | git apply |  |
| 163 | zzzzzzzzz-wined3d-shared-instance | soda patch | clean | git apply |  |
| 164 | zzzzzzzzzz-ink-point-drawing-attributes | soda patch | clean | git apply |  |
| 165 | zzzzzzzzzzz-core-ink-host-support | soda patch | clean | git apply |  |
| 166 | zzzzzzzzzzzz-eagle-d2d-results | soda patch | clean | git apply |  |
| 167 | zzzzzzzzzzzz-ink-guid-archive | soda patch | clean | git apply |  |
| 168 | zzzzzzzzzzzzz-d2d1-pixel-units | soda patch | clean | git apply |  |
| 169 | zzzzzzzzzzzzz-eagle-loader-results | soda patch | clean | git apply |  |
| 170 | zzzzzzzzzzzzzz-d2d1-effects | soda patch | clean | git apply |  |
| 171 | zzzzzzzzzzzzzz-dwrite-glyph-origin | soda patch | clean | git apply |  |
| 172 | zzzzzzzzzzzzzz-eagle-native-loader-results | soda patch | clean | git apply |  |
| 173 | zzzzzzzzzzzzzzz-d2d1-affine-transform | soda patch | clean | git apply |  |
| 174 | zzzzzzzzzzzzzzz-opengl-framebuffer-height | soda patch | clean | git apply |  |
| 175 | zzzzzzzzzzzzzzz-x11-client-resize | soda patch | clean | git apply |  |
| 176 | zzzzzzzzzzzzzzzz-d2d1-bitmap-units | soda patch | clean | git apply |  |
| 177 | zzzzzzzzzzzzzzzz-web-token-personal-response | soda patch | clean | git apply |  |
| 178 | zzzzzzzzzzzzzzzzz-d2d1-brush-units | soda patch | clean | git apply |  |
| 179 | zzzzzzzzzzzzzzzzz-web-account-personal-id | soda patch | clean | git apply |  |
| 180 | zzzzzzzzzzzzzzzzzz-composition-graphics-device | soda patch | clean | git apply |  |
| 181 | zzzzzzzzzzzzzzzzzz-composition-inertia-resting-value | soda patch | clean | git apply |  |
| 182 | zzzzzzzzzzzzzzzzzz-d2d1-dashes | soda patch | clean | git apply |  |
| 183 | zzzzzzzzzzzzzzzzzz-http-header-abi | soda patch | clean | git apply |  |
| 184 | zzzzzzzzzzzzzzzzzzz-http-header-values | soda patch | clean | git apply |  |
| 185 | zzzzzzzzzzzzzzzzzzzz-control-applet-machine | soda patch | clean | git apply |  |
| 186 | 27519b4920c dxcore: Add support for DedicatedAdapterMemory property. | carry | clean | cherry-pick -n + commit -C (re-authored) |  |
| 187 | aa63ca6901d dxcore: Add support for SharedSystemMemory property. | carry | clean | cherry-pick -n + commit -C (re-authored) |  |
| 188 | 56de43d8484 dxcore: Return stub value for DedicatedSystemMemory property. | carry | clean | cherry-pick -n + commit -C (re-authored) |  |
| 189 | bbb0e5c4603 include: Add missing DXCore property definitions. | carry | clean | cherry-pick -n + commit -C (re-authored) |  |
| 190 | 67642f82637 include: Add DXCore hardware-type and generic ML/media attribute GUIDs. | carry | clean | cherry-pick -n + commit -C (re-authored) |  |
| 191 | f156ba4c66b dxcore: Implement IsPropertySupported() and IsValid(). | carry | clean | cherry-pick -n + commit -C (re-authored) |  |
| 192 | a53e441977f dxcore: Implement IsAttributeSupported() for the D3D11 and D3D12 attributes. | carry | clean | cherry-pick -n + commit -C (re-authored) |  |
| 193 | 89bba20ec4d dxcore: Filter adapters by attributes in CreateAdapterList(). | carry | clean | cherry-pick -n + commit -C (re-authored) |  |
| 194 | 8482121574e dxcore: Implement Sort() and IsAdapterPreferenceSupported(). | carry | clean | cherry-pick -n + commit -C (re-authored) |  |
| 195 | f9a19535083 dxcore: Handle hardware-type and generic attributes; derive core compute from graphics support. | carry | clean | cherry-pick -n + commit -C (re-authored) | applied after a53e441977f/89bba20ec4d/8482121574e (dependency order, not plan order) |
| 196 | aa905cb665f dxcore: Support the remaining adapter properties. | carry | clean | cherry-pick -n + commit -C (re-authored) |  |
| 197 | bee3603c5d6 dxcore: Implement QueryState() and IsQueryStateSupported() for AdapterMemoryBudget. | carry | clean | cherry-pick -n + commit -C (re-authored) |  |
| 198 | d3552799fef dxcore: Implement SetState() and IsSetStateSupported() for AdapterMemoryBudget. | carry | clean | cherry-pick -n + commit -C (re-authored) |  |
| 199 | d67f859b9c2 mscms: Implement enough of the WCS default-profile queries for display profile lookup. | carry | clean | cherry-pick -n + commit -C (re-authored) |  |
| 200 | 48c371b5bcb kernel32: Add UnregisterApplicationRecoveryCallback stub. | carry | clean | cherry-pick -n + commit -C (re-authored) |  |
| 201 | c82c7f91793 wineboot, winecfg: Use build 26100 (24H2) for Windows 11. | carry | adapted | hand re-expression | wineboot.c hunk dropped: Valve update_win_version has no win11 branch (writes win10 19045); winecfg win11 entry applied |
| 202 | 9cec63ae073 ntdll, loader: Report Windows 11 24H2 (build 26100) for win11. | carry | adapted | hand re-expression | wine.inf.in conflicted with Valve 19045/5796; commit lines 26100/1742 taken on Valve file state; ProductName untouched |
| 203 | 5ccd9d1746b Add photolab-wine source package (pixi-build-rattler-build) | carry | clean | cherry-pick -n + commit -C (re-authored) | ordered before 4a0c44f5760 (pixi.toml/recipe must exist first) |
| 204 | a4847a6a0d3 recipe: run the fork's autogen.sh | carry | clean | cherry-pick -n + commit -C (re-authored) | ordered before 4a0c44f5760 (pixi.toml/recipe must exist first) |
| 205 | 65a0a01a8c1 recipe: add llvm-tools (winebuild needs llvm-dlltool) | carry | clean | cherry-pick -n + commit -C (re-authored) | ordered before 4a0c44f5760 (pixi.toml/recipe must exist first) |
| 206 | e6546deee99 Add dxvk, vkd3d-proton and winetricks packages as runtime deps of photolab-wine | carry | clean | cherry-pick -n + commit -C (re-authored) | ordered before 4a0c44f5760 (pixi.toml/recipe must exist first) |
| 207 | 4a0c44f5760 Add windows-winmd package; install Microsoft's combined Windows.winmd instead of Wine's per-namespace winmds. | carry | clean | cherry-pick -n + commit -C (re-authored) |  |
| 208 | b80387d2f8b windows-winmd: use windows-rs 0.62.0; later tags strip runtime-class members | carry | clean | cherry-pick -n + commit -C (re-authored) |  |
| 209 | 20b3c8633b5 recipe: build for x86-64-v3 (conda microarch level 3, PE side via x86_64_CFLAGS) | carry | clean | cherry-pick -n + commit -C (re-authored) |  |
| 210 | 2941df865c6 recipe: microarch level needs an exact spec | carry | clean | cherry-pick -n + commit -C (re-authored) |  |
| 211 | edbfde314a1 mscoree, appwiz.cpl, mshtml: Drop compile-time INSTALL_DATADIR lookups. | carry | clean | cherry-pick -n + commit -C (re-authored) |  |
| 212 | 2e311d3da7f tools: find perl through env instead of /usr/bin/perl | carry | clean | cherry-pick -n + commit -C (re-authored) |  |
| 213 | 3fa3a2deeac gitignore: ignore files generated by autogen.sh | carry | adapted | hand re-expression | .gitignore created from photolab-soda .gitignore (Valve tree has none) |
| 214 | a7369403d51 ntdll: Use the standard loader when Proton's i386-unix/wine64 is absent. | carry | adapted | hand re-expression | fallback re-expressed inside Valve PROTON_EOS_EAC branch of preloader_exec |
| 215 | 1e211c14e3f include: IPackageManager9 idl | carry | skipped | compared with soda IPackageManager9 | redundant: soda declares the identical IPackageManager9 (only param names differ); IAddPackageOptions2/IStagePackageOptions2/IRegisterPackageOptions2/IPackageVolume2 are unused in the tree |
| 216 | 71b342f8b5f VRAM clamp | drop | skipped | not carried | not on DxO path |
| 217 | 277fc4e2a70 windows.storage | drop | skipped | not carried | unused without App Runtime |
| 218 | 19c7fa37886 windows.storage | drop | skipped | not carried | unused without App Runtime |
| 219 | cf3a7daa668 windows.storage | drop | skipped | not carried | unused without App Runtime |
| 220 | e828af921b3 | drop | skipped | not carried | superseded/reverted |
| 221 | 3b9ffa34eb9 | drop | skipped | not carried | superseded/reverted |
| 222 | eeaa86e7e0e | drop | skipped | not carried | superseded/reverted |
| 223 | 7516afa5ac3 | drop | skipped | not carried | reverted |
| 224 | 6ebf450eaac winewayland fix | drop | skipped | not carried | repairs the hand-adapted 11.20 winewayland driver this branch lacks |
| 225 | 4b110eded37 winewayland fix | drop | skipped | not carried | repairs the hand-adapted 11.20 winewayland driver this branch lacks |
| 226 | 957f2b38822 winewayland fix | drop | skipped | not carried | repairs the hand-adapted 11.20 winewayland driver this branch lacks |
| 227 | b8ad4f69429 winewayland fix | drop | skipped | not carried | repairs the hand-adapted 11.20 winewayland driver this branch lacks |
| 228 | 1556aa22077 winewayland fix | drop | skipped | not carried | repairs the hand-adapted 11.20 winewayland driver this branch lacks |
| 229 | local NTLM series (upstream 11.15 msv1_0/lsass) | drop | skipped | not carried | Bottles' platform ships ntlm_auth; trusted until shown otherwise |

Notes: arm64-disable-aarch32 applied cleanly on x86_64. The 5a dxcore commits were carried because the DedicatedAdapterMemory/SharedSystemMemory/DedicatedSystemMemory cases were absent after the soda patches (grep of dlls/dxcore and include/dxcore_interface.h: enum members only, no cases). aa905cb665f applied without the expected dxcore.c conflict. dlls/dxcore/Makefile.in keeps Valve's EXTRADLLFLAGS = -Wb,--prefer-native, which photolab-soda lacks.
