# MUST-HAVE (Core File Manager Functionality)

* **Basic file and directory operations**

    * Implement copying (F5), moving (F6), deleting (F8).
    * Implement creating directories/files (F7).
    * Implement renaming (at least single rename).
    * Add file selection support.

* **File selection mechanics (essential)**

    * Space / Insert: toggle selection of the current item and move cursor down.
    * Gray Plus (+): select files matching a mask (e.g., `*.cpp`).
    * Gray Minus (–): deselect files matching a mask.
    * Gray Asterisk (*): invert selection.

* **Essential editor and viewer features**

    * Implement reliable file saving in the editor.
    * Add a “save / discard / cancel” dialog when exiting with unsaved changes.
    * Implement Quick View (Ctrl+Q) with basic text/image support.
    * Ensure F4 opens a file in the editor or assigned viewer.

* **Information and status display**

    * Add a status bar (free space, file count, size of selected items).
    * Implement F3 “properties” for files.

* **Keyboard usability**

    * Add standard shortcuts:

        * Shift+F6 rename
        * Ctrl+A select all
    * Fix Ctrl+P / Ctrl+Enter / Ctrl+Shift+Enter so inserted paths end with a space and are quoted when containing spaces.

* **Command line basics**

    * Make the command line execute essential commands (cd, external programs).
    * Minimal shell integration (history not required initially).

* **Essential filtering**

    * Implement visibility filtering by mask/pattern (e.g., `*.cpp`).
    * Implement show/hide hidden files.

* **Basic configuration UI**

    * Add a simple configuration dialog (Ctrl+D).
    * Add minimal menu/toolbars (File / View / Options).

---

# IMPORTANT BUT NOT URGENT (Second Priority)

* **Quick navigation improvements**

    * Add navigation history (Alt+Down).
    * Improve behavior of F4 on folders (disable or define expected behavior).
    * Improve behavior of F3 on folders (show total size).

* **Editor and viewer extensions**

    * Support “view as image” mode more fully.
    * Add “view as hex” widget.
    * Implement folder preview or info panels.

* **Visual/UX improvements**

    * Optional status line modes.
    * Separate appearance for sudo mode.
    * Right-click context menu and better mouse support.

---

# LATER (Nice-to-Have / Advanced)

* **Advanced operations**

    * Multi-rename (Ctrl+M).
    * Directory comparison and synchronization.
    * File attribute editing (permissions, hidden).
    * Proper handling of symbolic links.

* **Command line enhancements**

    * Full shell integration (history, completion).

* **Filtering enhancements**

    * Filter by type (folders/hidden/others) as separate modes.

* **Archive and protocol support**

    * Treat ZIP, RAR, TAR, etc. as directories.

* **Plugin system**

    * Add plugin support (e.g., lister/viewer plugins).
