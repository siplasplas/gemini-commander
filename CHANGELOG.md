# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Encrypted `*.een` files in the editor (qceditor 1.2): the password is asked
  when the file is opened; the formats of EdiQ and the older Editas are read,
  and saving writes EdiQ's. The viewer shows such files as they are on disk,
  without asking for a password
- The viewer of small files searches (Ctrl+F, F3, Shift+F3) and goes to a
  line:column (Ctrl+G), as the editor does
- The editor removes trailing whitespace when saving (qceditor 1.3, **Tools >
  Remove Trailing Whitespace on Save**, `editor.trim_trailing_whitespace`):
  in lines changed since the last save by default, never in the cursor's
  line, as one undo step
- Alt+Enter in the editor shows the spelling suggestions for the word at the
  cursor (qceditor 1.3.1); suggestions come from all forms of the dictionary,
  usually one, three at most (qcodeedit 1.20.1)
- The spelling menu offers first the correction of a space missing or typed in
  the wrong place: "n aoknie" → "na oknie", "mam yz" → "mamy z" (qceditor 1.4,
  qcodeedit 1.21)
- Spelling suggestions put common words before rare ones once the word
  frequencies of the language are downloaded: asked once, then the lists of
  new languages follow when needed (**Tools > Download Word Frequencies for
  Spelling**, `editor.download_word_frequencies`; qceditor 1.5, qcodeedit 1.22)
- **Tools > Review Spelling** (F6) in the editor: the misspelled words of the
  selection or the whole text with their corrections, single ones checked
  already, corrected together as one undo step (qceditor 1.6, qcodeedit 1.23)
- **Tools > Correct Spelling While Typing** in the editor (off by default,
  `editor.correct_while_typing`): a misspelled word with a single correction
  is corrected when a space or punctuation is typed after it; Ctrl+Z brings
  back the word as typed (qceditor 1.7, qcodeedit 1.24)
- Review Spelling checks at first only sure corrections, words with a single
  suggestion, or every word (`editor.review_checked_at_first`), and can show
  only the checked words;
  the spelling menu lists a moved space after the word's suggestions, with
  both words ("mam yz → mamy z"); suggestions rank by hands and touching
  keys, and none are offered when every word is far off (qceditor 1.9,
  qcodeedit 1.26)
- A double click in the editor selects the word (qcodeedit 1.26)
- Review Spelling shows the search time and has **Add to Dictionary** and
  **Ignore**, which take every row of the selected word out of the list;
  suggestions ask hunspell only when the dictionary's forms give none, about
  four times faster with the same results (qceditor 1.12, qcodeedit 1.29)
- Review Spelling shows each word's score and sorts as in the text,
  alphabetically or by score; **Tools > Default Spelling Language** sets
  the language of a new text (`editor.default_spell_language`; qceditor
  1.13, qcodeedit 1.30)
- Review Spelling copies rows, shows the word in bold in its context and
  remembers corrections as your own (score 0, also while typing); **Tools >
  User Dictionary** lists your words and corrections (qceditor 1.14,
  qcodeedit 1.35)
- Review Spelling no longer blocks Gemini Commander: its window is not modal
  and only its tab is read-only (and not reloaded from disk) meanwhile; a
  second review of a long file is at once (qceditor 1.16, qcodeedit 1.38)
- Review Spelling: the selected row stays in sight after sorting; it shows
  the words with several suggestions ("Several", "Several, first close");
  suggestions follow the word's capital and Shift held (also with AltGr) is
  cheap, a capital after a small letter is not (qcodeedit 1.39)
- Review Spelling's "Checked at first" also picks the words with several
  suggestions ("Several", "Several, first close") or without suggestions but
  given an own correction ("Words without suggestions"); Show lists the
  unchecked words of that kind (qcodeedit 1.42)
- Review Spelling: one row at a time, Up/Down between rows, Alt+Down opens
  the corrections; Own Correction starts from the word as typed and can
  replace it with its neighbours (qcodeedit 1.43)
- Review Spelling's list shows the words of the kind chosen in "Checked at
  first" (all, checked, unchecked) or every word, from the start; "None"
  checks nothing (qcodeedit 1.45)
- Words ignored in the editor's spell check stay with their file, also after
  closing it (qceditor 1.20, qcodeedit 1.46); copying (F5), moving or
  renaming (F6, multi-rename) and deleting (to the trash or permanently) a
  file or a whole directory carries them along, one database update per
  selected item
- Spelling suggestions in the editor (qcodeedit 1.47–1.54): the user's own
  words are suggested too; a much smaller forms cache (Polish 124 → 43 MB);
  suggestions from the forms for French, German, Spanish, Czech, Portuguese
  and more (accents typed or not, compound words from hunspell); typographic
  apostrophes and hyphenated words; a key next to the meant one costs less;
  frequency orders words spelled as close but does not hide them, so
  "fiunding" is not corrected surely to "finding". The word frequency list
  of every language with a dictionary installed is downloaded when missing
  (qceditor 1.21)
- Spelling in the editor fitted to real typing mistakes (qcodeedit 1.55–1.63):
  keyboard layouts (AltGr not let go: "któ©re", "zró”"), costs tuned on a
  long hand-typed text, more sure corrections. Review Spelling shows one kind
  of words at a time (sure corrections, a single suggestion not sure, ...),
  checks them with actions (all, the rows selected with Shift), and Correct
  leaves it open for the next kind; a double click shows the word in the
  text. **Tools > Review Spelling of Lines** (Shift+F6) reviews a long text a
  block of lines at a time, Up and Enter going on to the next (qceditor 1.22)
- Grammar in the editor (qcodeedit 1.65, qceditor 1.23), with its data files
  installed: words spelled right that do not fit together are underlined in
  blue ("ze jest": że, "Mam dzwoniła": Mama); the context menu and Alt+Enter
  show the problem and the corrections; **Tools > Review Grammar** (Ctrl+F6)
  and **Review Grammar of Lines** (Ctrl+Shift+F6) list the mistakes to
  correct; **Tools > Check Grammar** turns it off
- Help > About shows the versions of qt-extra, qcodeedit and qceditor, and
  whether each is installed or built in
- The search bar of the editor and the viewer covers the top lines and moves
  them down only when the result is among the first lines; selected text is
  the query also while the bar is open; the placeholder is "Search"
  (qceditor 1.17)
- F3 and Shift+F3 with the search bar closed go to the next or previous
  result without showing the bar; when nothing is found a tooltip says e.g.
  `"abcd" not found` (qceditor 1.18)
- **Tools > Statistics…** in the editor: bytes of the file in its encoding,
  code points (not UTF-16 units) and lines, and the same for the selection
  (qceditor 1.19)
- The viewer of large files has a caret moved with the keyboard, Shift
  selection, double-click word and triple-click line selection, and the
  scrollbar jumps to the clicked point (textviewers 1.1)
- The viewer of large files colours the syntax chosen by the file name, as
  the editor does, and folds e.g. XML elements by a click in the margin; only
  the visible part of the file is scanned (textviewers 1.2)
- The viewer of large files searches (Ctrl+F) with a bar like the editor's:
  results are marked in the visible part only, without a count; F3 and
  Shift+F3 search the whole file in the background with a progress dialog
  that can cancel it (textviewers 1.2)

### Fixed
- Tab in the editor inserts a tab (qcodeedit 1.19.2); Ctrl+Tab still switches
  tabs

## [2.2.0] - 2026-10-18

### Changed
- The editor window and the viewer of small files are QCEditor's editor
  library (qceditor 1.1.1), so both programs have the same editor; its
  settings stay in the `[editor]` section of `config.toml`. The editor's
  menus are File, Search, View, Tools and Help
- A file changed on disk is reloaded as one undo step (Ctrl+Z brings back the
  text before it, the earlier undo history stays) and the cursor follows its
  text; a file with unsaved edits is left as it is unless **Tools > Ask to
  Reload Modified Files** is on (`editor.ask_reload_modified`)
- The configuration is in `~/.config/siplasplas/gemini-commander`; an older
  version's `config.toml` is moved there at start-up, the KTextEditor-era
  `~/.config/gemini-commanderrc` is removed, and the Kate syntax data moves to
  the shared `~/.local/share/kate-data`
- **Configuration > Editor > Languages of files not in UTF-8** takes several
  languages (`editor.legacy_languages`)
- The copy/move progress dialog appears only for operations longer than
  400 ms; windows are activated only when needed (no GNOME busy cursor after
  renames or opening the editor on Wayland)

### Added
- Spell checking in the editor with hunspell (**Tools > Check Spelling**, on
  by default): suggestions, Add to Dictionary, Ignore and the spelling
  language in the context menu
- A notice above the editor when the encoding was detected without
  confidence, with a dialog comparing the plausible encodings
- The editor's status bar shows the code point after the caret (U+0105);
  columns count characters, also in Go to Line/Column
- The editor's context menu has Undo, Redo, Cut, Copy, Paste, Delete and
  Select All; Help > About in the editor

### Fixed
- Files open through xdg-open without an activation token (no busy cursor
  while e.g. Loupe starts)
- Copying or moving a file into a subdirectory keeps the cursor in the
  source panel; the progress dialog no longer covers an Overwrite question
- Alt pressed alone keeps the panel focus; Tab closes the quick search

## [2.1.1] - 2026-10-08

### Fixed
- Enter or double-click on an executable runs the file in the panel (like
  `./name`), not a program of the same name found in PATH; in the branch view
  names with spaces run too

## [2.1.0] - 2026-10-08

### Added
- Show menu: All Files (Ctrl+F10), Programs (Ctrl+F11: directories and
  executables) and Custom (Ctrl+F12: file masks such as `*.cpp;*.h` and/or
  hiding what `.gitignore` files ignore, directories included) for the current
  panel; the branch view (Ctrl+B) and directory sizes (Space, Alt+Shift+Enter)
  follow the filter, which is shown after the path, e.g. `/project/[*.cpp;!gitignore]`
- Find Files: Skip during search, by directory and file names and by `.gitignore`
- Copy (F5) of a directory or several entries can skip what `.gitignore` ignores
- Copy dialog: Count shows the number and size of the files to copy in the
  background; the copy uses it instead of counting again
- `.gitignore` options shared by Show > Custom, Copy and Find Files: start from
  the nearest `.git` or from the highest `.gitignore` below home (one for a group
  of repositories), and whether `.git` directories are skipped
- Editor: the line with the caret has a slightly darker background

### Changed
- Large files are copied in the kernel (copy_file_range, or sendfile between
  filesystems); SHA-256 verification is optional and no longer the default
- Alt+Shift+Enter sorts the panel again when the directory sizes are done
- qcodeedit 1.12.0

### Fixed
- Closing a menu with Escape gives the focus back at once (a second Escape was
  needed)
- Editor: text typed into a new tab is shown, and the caret no longer drifts
  away from the text (qcodeedit 1.12.0)

## [2.0.0] - 2026-10-08

### Changed
- Only Qt6 is needed: KDE Frameworks (KF5/KF6) and the Qt5 build are gone
- The editor and the code view of the viewer use the qcodeedit widget instead of
  KTextEditor, with Kate syntax definitions downloaded on first use
  (Tools > Update Syntax Definitions), folding and per-tab syntax/theme choice
- qt-extra, qcodeedit, toml++ and textviewers are fetched by CMake (FetchContent)
  instead of git submodules or manual installation; installed qt-extra and
  qcodeedit packages are used when present
- .deb packages are built with Qt6 for Ubuntu 24.04 too; `install/deb-qt5` removed

### Added
- Editor: inline find/replace bar (regular expressions, whole words, match case,
  in selection, preserve case, replace all in one undo step), F3/Shift+F3,
  Go to Line/Column (Ctrl+G), Save As, edit context menu
- Editor: Wrap Lines and Show Special Characters are remembered
- Editor: files open in their own encoding, UTF-8/16/32 or a legacy code page
  (cp1250, ISO 8859-2, cp852, ...), detected automatically and saved back in it
  with BOM, line breaks (LF, CRLF or CR) and the final newline; characters the
  code page cannot store ask whether to write `?` or switch to UTF-8; Encoding
  in the context menu reopens or saves in another encoding
- Editor: Configuration > Editor > Language of files not in UTF-8 limits code
  page detection to one language (e.g. Polish)
- Editor status bar: line:column, line breaks (Unix, Windows, Mac, Mixed),
  encoding, the text's language (detected in the background) and tab width
- Editor: Go to Line/Column lists this session's positions
- Editor: Breeze Light/Dark default theme following the desktop, Kate-style
  gutter with wrap arrows, word wrap at the same places as Kate, Ctrl+Backspace
  and Ctrl+Delete delete words, whole-word search uses the editor's word rules

## [1.5.0] - 2026-01-02

### Added
- File Info dialog (Ctrl+L) with recursive size statistics for files and directories
- Quick View mode (Ctrl+Q) shows file preview in opposite panel
  - Updates automatically on arrow keys and selection changes
  - Shows directory size calculation with live progress (yellow bar → blue when done)
- Hex viewer mode in Quick View and Viewer (F3)
  - Context menu to switch between Text and Hex modes
  - Menu bar in Viewer (F3) with View → Text/Hex selection
- KTextEditor integration for small files in viewer
  - Configurable threshold (MB) in Settings with 10% RAM limit
- Virtual file support (/proc files) via readAll() for Text and Hex views

### Changed
- Menu "View" renamed to "Show", File Info moved to File menu
- ViewerFrame (F3) uses ViewerWidget internally with full Text/Hex/KTextEditor support
- View mode (Text/Hex) persists between F3 calls in same session

### Fixed
- Virtual files (size 0 like /proc/*) can now be viewed correctly

## [1.4.2] - 2026-01-01

### Added
- Configurable copy modes for large files with chunk size setting
- Safe 'atomic' file copy using temp file + rename to prevent corruption
- Compare directories "ignore size" option with config dialog checkbox

### Changed
- Wider input dialogs for F5/F6/F7 operations
- Smart cursor positioning after delete (F8/Shift+F8) using selectNameAfterDelete

### Fixed
- Cursor now positions on created directory after F7 (e.g. on "123" when creating "123/456")
- Progress dialog visibility during file operations
- Cursor position after delete operations stays on next file

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