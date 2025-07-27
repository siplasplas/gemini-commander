* Basic file and directory operations: Missing support for copying (F5), moving (F6), deleting (F8),
creating directories/files (F7), multi-rename (Ctrl+M), or directory synchronization. 
Current code only navigates directories without file modifications.
* View and edit files: Missing file preview (F3), editing (F4), or quick view (Ctrl+Q). 
Code does not handle opening files in external editors or built-in viewers.
* Multi-select and batch operations: Extended selection exists, but no actions on selected files
(e.g., size summation, packing, comparing).
* Sorting and views: Sorting only by name (directories first); missing sorting by size, date, 
extension. Missing views: brief (list), full (details), thumbnails, tree view, or custom columns.
* Search and filtering: Missing file search (Alt+F7), quick search (typing letters to focus), 
or filters (e.g., by extension, date). Code lacks dynamic panel filtering.
* Command line (commandLineEdit): Exists but does not execute commands (e.g., cd, dir, copy, 
external commands). Shortcuts like Ctrl+P/Enter partially work, but missing shell integration
(e.g., cmd.exe/bash) or command history.
* Keyboard shortcuts and usability: Some shortcuts exist (Tab, Ctrl+P), but missing many standards
(e.g., F2 rename, Alt+Enter properties, Ctrl+A select all, Ctrl+Z undo).
* User interface elements: Missing status bar (with free space info, file count), drag-and-drop 
between panels, mouse support for operations (e.g., right-click menu).
* History and bookmarks: Missing navigation history (Alt+Down), bookmarks (Ctrl+D), or quick access
to favorite directories.
* Configuration and personalization: Missing menus, toolbars, configurable shortcuts, colors for 
file types, file icons, or settings (e.g., hidden files, case sensitivity).
* Compare and synchronization: Missing file/directory comparison (Ctrl+F3) or synchronization (Alt+S).
* Performance and advanced features: Missing handling of large directories 
(lazy loading), multi-threading for operations, symbolic links, or file attributes (permissions, hidden).
* Archives and protocols support: Missing treatment of archives (ZIP, RAR, TAR) as directories, 
* Plugins and extensions: Missing support for plugins (e.g., lister plugins for file format previews).