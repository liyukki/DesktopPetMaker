# Current Project State

This is the only current architecture fact source. Older reports are historical snapshots and are not current pass certificates.

Last updated: 2026-07-16 (UI and brand system redesign)

## Runtime

- Official runtime: `RuntimePetWindow` (Qt/C++). Python prototypes are legacy only.
- Normal mode lifecycle: `RuntimePetManager` and `PetRuntimeInstance`; multiple registered pets may be restored when multi-pet mode is enabled.
- CLI debug mode: multiple `pet.json` arguments are supported.
- Movement/physics priority remains Dragging/Falling/Bouncing, Follow/Patrol, AI generation, random idle, Idle.
- Ground collision and first placement use the project anchor. Visually confirmed walk direction is unchanged.
- Runtime persistence uses field-level `RuntimeStatePatch` with `QSaveFile`; full project save also uses `QSaveFile`.
- Saved runtime placement is clamped to a connected screen's available geometry before display. Corrected placement is persisted through `RuntimeStatePatch`.
- Release sample `pet.json` files do not carry a developer-machine `runtime.anchorScreen`.

## Runtime Actions

- AI action dispatch returns structured Executed/Queued/Rejected/Failed results.
- Public AI actions currently support only `Normal` runtime state. Non-Normal declarations are rejected during validation instead of being advertised as implemented.
- Immediate actions require every referenced frame to be readable and cached before execution is reported.
- Busy actions are queued, superseded, reloaded, revalidated, and executed only when runtime state is safe.
- Unknown parameters are rejected. `additionalProperties: true` is unsupported and rejected.
- `mirror` is accepted only when the action declares `mirrorSupported`.
- Random idle interval: 12-30 seconds; trigger window 60%; random sleep window 3%.
- Automatic sleep: 4 minutes idle, 60 seconds duration, at least 120 seconds cooldown.

## Rendering

- `IRenderBackend` is the runtime rendering boundary.
- `SpriteRenderBackend` is the production backend and owns sprite transform/render behavior.
- `RenderBackendFactory` selects the project backend; tests can inject an `IRenderBackend` into the real runtime.
- `Live2DRenderBackend` owns a non-sprite model lifecycle boundary and explicit failure result. `DESKTOP_PET_ENABLE_LIVE2D` defaults to OFF; unavailable projects record the reason and safely fall back to Sprite.
- `PetProject` persists `render.backend` and `render.live2dModelMetadata`; the editor exposes backend and model path fields.
- Release wording: provides an experimental Live2D adapter boundary; does not include a real Cubism renderer.

## Assets

- PNG/GIF, sprite-sheet, procedural, Shimeji, and petpack paths retain their current editors and import flows.
- Shimeji import records source metadata without absolute external paths.
- Shimeji batch commit uses staged files, an on-disk transaction journal, injected failure seams, rollback, and startup recovery.
- Sprite sheet parser and widget behavior have separate Qt tests. Full visual acceptance remains `MANUAL TEST`.

## AI And Multi-AI

- Provider credentials are stored by `CredentialStore`, not in `pet.json` or room files.
- DeepSeek V4 requests use `thinking.type=disabled`; provider-specific request logic remains in `AIProvider`.
- Per-pet AI metadata: `characterName`, `systemPrompt`, and provider profile reference.
- AI animation fallback: talk, otherwise nod, otherwise idle; transient physics/movement states have priority.
- Multi-AI room persistence uses versioned `QSaveFile` records, typed request/delivery/action outcomes, cancellation handles, and per-turn identity.

## Overlay And UI

- The application uses the centralized `ui/theme` design system. `AppTheme` owns the palette and QSS entry point, `ThemeConstants` owns reusable tokens, and `IconProvider` owns cached original line icons.
- The visual language is a light creation-tool interface with neutral surfaces, teal primary actions, coral secondary emphasis, status chips, visible focus states, and card radii no larger than 8 px.
- Control Center has a branded header, icon navigation, runtime summary, project cards, empty states, and grouped AI/runtime/general settings.
- Multi-AI Console, Action Material, Sprite Sheet Import, chat, journal, AI settings, tray menu, runtime menu, and proactive bubble use the same theme.
- Brand assets live under `resources/branding/` and are connected to the EXE, Qt window icon, Control Center, tray, About dialog, README, and release provenance. The public beta uses original geometric application artwork with documented provenance and redistribution permission.
- Automated snapshots cover Control Center, Multi-AI, Action Material, and Sprite Sheet at normal scaling. A separate 150% run is retained as DPI evidence.
- Bubble presentation is not an overlay and does not alter physics.
- Chat/settings/journal overlays block new patrol/random-idle/automatic-sleep starts without rewriting active physics or sleep state.
- Runtime menus remain Chinese. GUI visual correctness is never inferred from source-only checks.
- Journal reminders defer opening the journal until the modal reminder has unwound; choosing Yes is covered by an automated runtime regression.
- Normal startup shows the Control Center on first run. `--control-center` provides an explicit recovery entry point.
- `SystemTrayController` owns the process-level `QSystemTrayIcon` recovery menu: open Control Center, show pets, start/stop pets, pause patrol, disable mouse passthrough, and exit.
- If the system tray is unavailable, startup forces the Control Center visible even after first run.

## Release Package

- Windows version metadata is compiled from `desktop_pet.rc` and reports `Desktop Pet Maker 1.0.0-beta`.
- The current application artwork is embedded as the EXE, window, and tray icon; provenance and SHA-256 values are recorded. The public beta artwork is cleared for redistribution with Desktop Pet Maker.
- Runtime ZIP creation is atomic and uses standard forward-slash entry names.
- The inner source manifest excludes itself and its verification log, preventing a circular or stale count.
- The release runtime contains only required Qt/runtime files and Chinese, Japanese, and English Qt translations.
- Existing sample character projects are excluded from the public runtime package because their redistribution rights are not verified. They remain untouched in the development workspace.
- A separately distributed original robot sample petpack is available and is not embedded in the runtime ZIP.
- Qt, GPL, LLVM runtime, and Microsoft runtime notices are inventoried under `licenses/`.
- Authenticode signing automation exists in `tools/sign_windows_artifacts.ps1`, but the current executable is `NOT_SIGNED` because no signing certificate is configured.
- The package is a self-contained runtime, not a no-trace portable mode. Settings remain in `QSettings`; credentials remain in Windows Credential Manager.

## Tests And Evidence

- CTest is the standard runner. The public source Release baseline passes 28/28 tests. Domain tests carry asset, petpack, credential, Multi-AI, runtime, theme, and visual snapshot assertions; the platform smoke target is below 500 lines and is limited to fast cross-module checks.
- Qt widget tests use the Windows platform plugin because the installed Qt package has no offscreen platform plugin.
- Runtime action tests use a real `RuntimePetWindow` test seam and verify state, queue, timer, and rendered-frame effects.
- UI and DPI snapshot coverage is part of the public CTest suite; generated local JUnit evidence is intentionally excluded from the repository.
- CTest verifies the source manifest without generating it. Manifest updates require the explicit `update_source_manifest` target.
- GUI automation covers the sprite-sheet dialog plus real Control Center, Multi-AI, Action Material, runtime bubble/options, Multi-AI-to-runtime interactions, theme roles, and 100/125/150/200% rendered snapshots. Human visual, real multi-monitor, and clean-VM acceptance remain outstanding.
- Synthetic screen-layout tests cover removed monitors, negative coordinates, small screens, and enlarged/high-DPI pet windows. Real multi-monitor and DPI acceptance remains manual.

## External Or Not Implemented

- Real Live2D Cubism SDK and production model rendering: `BLOCKED_EXTERNAL`.
- Full GUI automation for every editor/control-center workflow: `IN_PROGRESS`.
- Persistent cross-process conversational memory: `NOT IMPLEMENTED`.
- Authenticode certificate/signing execution: `BLOCKED_EXTERNAL`.
- Licensed public sample character assets: `NOT CONFIGURED`; current samples are excluded from the public runtime package.
- Clean Windows 10/11 VM launch and human visual acceptance: `MANUAL TEST`.
- Final release-owner license-route approval: `MANUAL TEST`.
