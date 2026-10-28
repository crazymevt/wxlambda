;;; Fail early on native loading/ABI errors and retain actual runtime metadata.
(require :asdf)
(load (merge-pathnames "../wxlambda.asd" *load-truename*))
(asdf:load-system "wxlambda")
(wxlambda:load-native-library)
(format t "~&WXLAMBDA_PLATFORM ~S~%"
        (list :lisp (lisp-implementation-type)
              :lisp-version (lisp-implementation-version)
              :os (software-type) :os-version (software-version)
              :machine (machine-type) :abi (wxlambda::%abi-version)
              :cffi (namestring (asdf:system-source-directory "cffi"))))
