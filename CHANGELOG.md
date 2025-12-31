# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.3.0] - 2025-12-31

### Added
- Comprehensive toolbar configuration system with TOML persistence
  - Configurable position (top/bottom/left/right), visibility, order, line breaks
  - Context menu on toolbars and menu bar for quick visibility toggling
  - "Reset Toolbar Layout" button in Settings dialog
- Vertical toolbar support with rotated text (CLion-style)
- FunctionBar supports vertical orientation when docked to left/right
- Storage info toolbar showing free/total space for active panel's mount
- Configurable size format with separate settings for file sizes and storage sizes
  - Precise (1'234'567), Decimal (1.2 M), Binary (1.2 Mi)

### Changed
- Toolbar layout (positions, visibility, order, line breaks) now persists between sessions
- Removed external tool button from toolbar

### Fixed
- Toolbars can no longer float outside the application window
- Emergency safeguard ensures menu or main toolbar always remains visible
- Selection changed signal now emitted for bulk selection operations
- Adjusted default column widths (Size wider, Ext thinner)

## [1.2.0] - 2025-12-30

### Added
- Proportional column widths that scale with window resize
- Configuration dialog with Wayland support
- Column configuration UI (add, remove, reorder columns)
- Configurable tab limit with directory persistence across sessions
- Platform-specific file attributes display (Linux/Windows)

### Changed
- Panel sorting settings now persist between sessions

## [1.1.0] - 2025-12-29

### Added
- Pack dialog (Alt+F5) for creating zip/7z archives
- Archive browser - navigate inside archives as directories
- Extract files (Alt+F9) for archive extraction
- Multi-volume RAR archive support

## [1.0.0] - 2025-12-29

### Added
- Dual-pane file manager with tabs
- Block devices toolbar (UDisks integration)
- "Other Mounts" toolbar for mounted filesystems
- Unmount option in context menu
- Qt5/KF5 and Qt6/KF6 support
- Windows platform support (drives toolbar, terminal)
- Calculate directory sizes (Shift+Alt+Enter)
- Folder icons (Yaru style)
- Dark theme compatibility
- About dialog with version and git SHA