# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.4.1] - 2026-01-01

### Added
- Symbolic link handling for F5/F6 operations:
  - Same filesystem: symlinks are copied/moved as links (not followed)
  - Cross-filesystem: symlinks are skipped with info message (exFAT compatibility)
- Tab switching now refreshes directory contents automatically

### Changed
- Lazy tab loading: only active tabs are loaded at startup (2 instead of 8)
- Directory watcher uses 350ms debounce to reduce redundant refreshes

### Fixed
- Focus correctly set to FilePanel after tab switch

## [1.4.0] - 2026-01-01

### Added
- Progress dialogs for F5 (Copy) and F6 (Move) operations with cancel support
- Multi-Rename Tool (Ctrl+M) for batch file renaming
- Compare directories (Shift+F2) in Mark menu

### Changed
- File operations (copy/move) extracted to separate FileOperations module
- Selection is restored after file operations (cursor moves to copied/moved file)
- Shift+F5/F6 for in-place copy/rename operations

### Fixed
- Correct destination path handling when copying/moving single files
- Selection no longer lost after copy/move (QFileSystemWatcher conflict resolved)
- Modal dialogs (overwrite confirmation) now properly handle keyboard input
- Toolbar visibility safeguard works correctly at startup

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