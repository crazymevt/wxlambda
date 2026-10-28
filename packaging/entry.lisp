(defpackage #:wxlambda-packaged
  (:use #:cl))
(in-package #:wxlambda-packaged)

(defun self-test ()
  "Exercise real GUI events, Unicode, recreation, and cleanup in the saved image."
  (let ((calls 0))
    (labels ((submitted ()
               (incf calls)
               (wxlambda-form:submit-form)
               (assert (string= "Good day, λ 日本語 😀."
                                (wxlambda:get-label wxlambda-form:*status*)))
               (wxlambda-form:close-window)
               (when (= calls 1)
                 (wxlambda-form:open-window)
                 (assert (string= "λ 日本語 😀" (wxlambda:get-text wxlambda-form:*name*)))
                 (wxlambda-form:set-submit-action #'submitted)
                 (wxlambda:queue-click wxlambda-form:*submit*))))
      (wxlambda:with-application ()
        (wxlambda-form:open-window)
        (wxlambda:set-text wxlambda-form:*name* "λ 日本語 😀" :notify t)
        (wxlambda:set-selection wxlambda-form:*style* 1 :notify t)
        (wxlambda-form:set-submit-action #'submitted)
        (wxlambda:queue-click wxlambda-form:*submit*)
        (wxlambda:run)))
    (assert (= calls 2)))
  (format t "PASS: packaged startup, Unicode callbacks, recreation, and cleanup~%"))

(defun main ()
  (handler-case
      (restart-case
          (let* ((runtime-directory
                   (uiop:pathname-directory-pathname (truename sb-ext:*runtime-pathname*)))
                 (bridge (merge-pathnames "../lib/libwxlambda.dylib" runtime-directory))
                 (arguments (cdr sb-ext:*posix-argv*)))
            ;; No ASDF system or development loader fallback is used at runtime.
            (wxlambda:load-native-library (truename bridge))
            (cond ((null arguments) (wxlambda-form:start))
                  ((equal arguments '("--repl"))
                   ;; The build process is non-interactive. Do not retain its
                   ;; disabled debugger when the user selects interactive use.
                   (sb-ext:enable-debugger)
                   (wxlambda-form:start-live))
                  ((equal arguments '("--self-test")) (self-test))
                  (t (error "Usage: wxlambda-form [--repl | --self-test]"))))
        (abort () :report "Exit the packaged application." (sb-ext:exit :code 1)))
    (error (condition)
      (format *error-output* "~&Application failed: ~A~%" condition)
      (sb-ext:exit :code 1))))
