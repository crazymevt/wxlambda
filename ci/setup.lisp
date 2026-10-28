;;; Use only the pinned dependency tree and the Lisp's bundled ASDF/UIOP.
(require :asdf)
(let ((dependencies (merge-pathnames "../.ci-deps/"
                                    (uiop:pathname-directory-pathname *load-truename*))))
  (unless (probe-file (merge-pathnames "cffi/cffi.asd" dependencies))
    (error "Pinned dependencies are missing; run python3 ci/fetch-deps.py first"))
  ;; CFFI-generated callbacks are not compatible across dependency versions.
  ;; Keep pinned FASLs separate from Quicklisp and from other Lisp implementations.
  (asdf:initialize-output-translations
   `(:output-translations
     (t (,(merge-pathnames "fasl/" dependencies) :implementation))
     :ignore-inherited-configuration))
  (asdf:initialize-source-registry
   `(:source-registry (:tree ,dependencies) :ignore-inherited-configuration)))
