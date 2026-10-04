fix:
  * count directory total size by space, second time not works
  * viewer F3 get old contenct if changed
  
add:
 * compare files/directories by content 
 

# MUST-HAVE (Core File Manager Functionality)

* **Editor on qcodeedit — follow-ups after leaving KTextEditor**
    * Marker for modified documents on the tab (now only " *" in the window title).
    * Line:column and encoding in a status bar.
    * Encoding choice; now UTF-8 (with BOM kept) and Latin-1 fallback for invalid UTF-8.
    * Syntax detection by MIME type / shebang, not only by file name.
    * Remember the chosen color theme (now per tab, default palette on start).
    * Find bar (Ctrl+F, F3) in the code view of the viewer (F3 / Quick View).

* **Essential editor and viewer features**
    * Implement search text in text viewer
    * Implement Quick View (Ctrl+Q) with basic text/image support.

* **Information and status display**
    * Add a status bar (free space, file count, size of selected items).
    * Implement F3 “properties” for files.
* 
* **Essential filtering**
    * Implement visibility filtering by mask/pattern (e.g., `*.cpp`).
    * Implement show/hide hidden files.

* **Basic configuration UI**
    * Add a simple configuration dialog.

---

# IMPORTANT BUT NOT URGENT (Second Priority)

* **Editor and viewer extensions**
    * Support “view as image” mode more fully.
    * Add “view as hex” widget.
    * Implement folder preview or info panels.

* **Visual/UX improvements**
    * Optional status line modes.
    * Separate appearance for sudo mode.

---

# LATER (Nice-to-Have / Advanced)

* **Advanced operations**
    * File attribute editing (permissions, hidden).
    * Proper handling of symbolic links.
* 
* **Filtering enhancements**
    * Filter by type (folders/hidden/others) as separate modes.

* **Plugin system**
    * Add plugin support (e.g., lister/viewer plugins).
