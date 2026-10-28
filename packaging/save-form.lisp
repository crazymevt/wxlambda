;;; Run in a fresh SBCL with ci/setup.lisp loaded; never initialize wxWidgets here.
(load (merge-pathnames "../examples/form-app.lisp" *load-truename*))
(load (merge-pathnames "entry.lisp" *load-truename*))
(assert (not wxlambda::*native-loaded*))
(assert (null (cffi:list-foreign-libraries :loaded-only t)))
(assert (not (find-package "QL")))
;; Make source-tree fallback impossible in the delivered image. All application
;; functions and CFFI callbacks are already present; ASDF will not load them again.
(asdf:clear-system "wxlambda")
(asdf:initialize-source-registry '(:source-registry :ignore-inherited-configuration))
(sb-ext:save-lisp-and-die (uiop:getenv "WXLAMBDA_PACKAGE_CORE")
                        :toplevel #'wxlambda-packaged::main :compression t)
