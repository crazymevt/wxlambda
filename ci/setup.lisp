;;; Use only the pinned dependency tree and the Lisp's bundled ASDF/UIOP.
(require :asdf)
(let ((dependencies (merge-pathnames "../.ci-deps/"
                                    (uiop:pathname-directory-pathname *load-truename*))))
  (unless (probe-file (merge-pathnames "cffi/cffi.asd" dependencies))
    (error "Pinned dependencies are missing; run python3 ci/fetch-deps.py first"))
  (asdf:initialize-source-registry
   `(:source-registry (:tree ,dependencies) :ignore-inherited-configuration)))
