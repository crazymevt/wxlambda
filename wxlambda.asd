(asdf:defsystem "wxlambda"
  :description "Portable Common Lisp bindings to wxWidgets: lifecycle prototype"
  :version "0.1.0"
  :depends-on ("cffi")
  :serial t
  :components ((:file "lisp/package") (:file "lisp/ffi") (:file "lisp/api")))
