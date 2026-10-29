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
                                (wxlambda:label-text wxlambda-form:*status*)))
               (wxlambda-form:close-window)
               (when (= calls 1)
                 (wxlambda-form:open-window)
                 (assert (string= "λ 日本語 😀" (wxlambda:text wxlambda-form:*name*)))
                 (wxlambda-form:set-submit-action #'submitted)
                 (wxlambda:queue-click wxlambda-form:*submit*))))
      (wxlambda:with-application ()
        (let* ((frame (wxlambda:make-frame))
               (canvas (wxlambda:make-drawing-canvas frame
                         :content '(:background (255 255 255)
                                    :items ((:rect 2 2 12 10 (20 80 140))
                                            (:text 2 18 "Packaged λ" 12 (0 0 0))))))
               (activity (wxlambda:make-activity-indicator frame)))
          (unwind-protect
               (progn
                 (setf (wxlambda:activity-running-p activity) t)
                 (assert (wxlambda:activity-running-p activity))
                 (setf (wxlambda:activity-running-p activity) nil)
                 (assert (not (wxlambda:activity-running-p activity)))
                 (uiop:with-temporary-file (:pathname path :prefix "wxlambda-png-" :type "png")
                   (wxlambda:save-canvas-image canvas path :width 96 :height 48)
                   (with-open-file (input path :element-type '(unsigned-byte 8))
                     (assert (equal '(137 80 78 71 13 10 26 10)
                                    (loop repeat 8 collect (read-byte input)))))))
            (wxlambda:destroy-window frame)))
        (wxlambda-form:open-window)
        (setf (wxlambda:text wxlambda-form:*name* :notify t) "λ 日本語 😀")
        (setf (wxlambda:selection wxlambda-form:*style* :notify t) 1)
        (wxlambda-form:set-submit-action #'submitted)
        (wxlambda:queue-click wxlambda-form:*submit*)
        (wxlambda:run)))
    (assert (= calls 2)))
  (format t "PASS: packaged startup, Unicode callbacks, recreation, drawing PNG, activity, and cleanup~%"))

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
