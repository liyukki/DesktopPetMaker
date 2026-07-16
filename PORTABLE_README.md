# Desktop Pet Maker Runtime Package

This is a self-contained Windows runtime package. It is not a no-trace
portable mode: settings remain in Windows application settings and API
credentials remain in Windows Credential Manager.

## First Start

Run `start_desktop_pet.bat`. It starts `pro.exe --control-center`, so the
control center is visible even when no pet project is installed.

This public package intentionally contains no sample character project.
The two projects in the development workspace are not cleared for public
redistribution. Import or create a pet project from the control center.

## Recovery And Exit

- Use the system tray icon to open the control center.
- The tray menu can show all pets, start or stop pets, pause patrol, and
  disable mouse passthrough.
- Use `退出程序` in the tray menu for a complete exit.
- If a saved pet position no longer fits the connected displays, the runtime
  moves it into the current screen's available area and saves the correction.

## Settings And Credentials

- Ordinary settings use Qt's Windows application settings for organization
  `DesktopPetMaker` and application `DesktopPetMaker`.
- API keys are stored in Windows Credential Manager and are never written to
  `pet.json` or this package.
- Remove saved API credentials through Windows Credential Manager.
- Removing this folder does not remove Windows settings or credentials.

## Requirements

- Windows 10 or Windows 11, 64-bit.
- A normal interactive desktop session.
- Network access is required only for configured AI providers.
- Petpack import currently relies on Windows PowerShell or PowerShell 7.

## Common Startup Issues

- SmartScreen may warn because the current Release Candidate is not
  Authenticode signed. Verify the published SHA-256 before running it.
- If no pet appears, open the control center from the system tray and import
  or create a pet project.
- If AI chat is unavailable, open AI Service settings and verify the provider
  profile, model, and Credential Manager entry.

## Live2D Boundary

This release provides an experimental Live2D adapter boundary; it does not
include a real Cubism renderer. The licensed Cubism SDK and a production model
renderer are not bundled. A Live2D project safely falls back to the Sprite
runtime and records the reason.

See `THIRD_PARTY_NOTICES.md`, `licenses/`, and `ASSET_PROVENANCE.md`.
