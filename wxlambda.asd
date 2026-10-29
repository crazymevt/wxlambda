(asdf:defsystem "wxlambda"
  :description "Portable Common Lisp bindings to wxWidgets: lifecycle prototype"
  :version "0.1.0"
  :depends-on ("cffi")
  :serial t
  :components ((:file "lisp/package") (:file "lisp/ffi") (:file "lisp/api")
               (:file "lisp/drawing-canvas-ffi") (:file "lisp/drawing-canvas")
               (:file "lisp/activity-indicator-ffi") (:file "lisp/activity-indicator")
               (:file "lisp/grid-ffi") (:file "lisp/grid")
               (:file "lisp/webview-ffi") (:file "lisp/webview")
               (:file "lisp/stc-ffi") (:file "lisp/stc") (:file "lisp/stc-generated")
               (:file "lisp/input-events-ffi") (:file "lisp/input-events")
               (:file "lisp/aui-ffi") (:file "lisp/aui") (:file "lisp/splitter-extra")
               (:file "lisp/thread-events-ffi") (:file "lisp/thread-events")
               (:file "lisp/views-ffi") (:file "lisp/views")))
