# Folder dialogs and tree icons (ABI66)

`make-directory-dialog`/`directory-dialog` supports :parent, :title and :path.
The native generic chooser accepts existing directories only and never changes
process working directory. Show it with `show-modal`, read `directory-dialog-path`
after :ok, then destroy it. :cancel leaves application state to the caller.
Normal modal parent lifetime/callback guards, title properties, subclass slots
and rollback apply. No native pointer adoption or arbitrary style flags.

SETF `tree-icons` copies0..32 managed16x16-logical-pixel bitmaps into a tree-owned
image list, preserving alpha. Dispose source bitmaps after copying. A replacement
cannot remove an index used by any existing item icon state. The reader returns
new owned bitmap copies; callers must destroy every returned bitmap. The same
copy is available through `make-bitmap :source tree :icon-index n`.

`tree-item-icon` reads an index/NIL. SETF updates normal and selected states,
accepting NIL or an in-range index0..31. Expanded artwork variants and image-list
objects remain outside the public API. Existing canonical item/parent lifetime
and tree operation guards remain in force. Icon writes and resource-allocating
reads reject native callbacks; use deferred actions. Index reads are callback-safe.
All require the main GUI thread. Tree destruction owns image-list cleanup.

The library provides presentation and chooser primitives, not filesystem/Git
policy. KryptonEdit supplies lazy background folder reads and Git ignore filtering
in its separate backend. See examples/folder-tree.lisp and tests/folder-tree.lisp.
Windows/Linux physical rendering remains unverified.
