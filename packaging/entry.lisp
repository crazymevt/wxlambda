(defpackage #:wxlambda-packaged
  (:use #:cl))
(in-package #:wxlambda-packaged)

(defun check-web-grid ()
  (let* ((frame (wxlambda:make-frame :width 640 :height 480))
         (panel (wxlambda:make-panel frame))
         (grid (wxlambda:make-grid panel :rows 2 :columns 2))
         (view (wxlambda:make-web-view panel))
         (ready nil) (ticket nil) (received nil) (timer nil) (ticks 0))
    (unwind-protect
         (progn
           (wxlambda:make-column panel)
           (wxlambda:add-child panel grid :expand t)
           (wxlambda:add-child panel view :proportion 1 :expand t)
           (setf (wxlambda:grid-cell-text grid 0 0) "Packaged grid λ")
           (assert (string= "Packaged grid λ" (wxlambda:grid-cell-text grid 0 0)))
           (wxlambda:bind-event view :web-message
             (lambda (event) (when (search "ready" (wxlambda:event-text event)) (setf ready t))))
           (wxlambda:bind-event view :web-script-result
             (lambda (event)
               (when (= ticket (wxlambda:event-extra-long event))
                 (assert (= 1 (wxlambda:event-integer event)))
                 (assert (string= "42" (wxlambda:event-text event)))
                 (setf received t) (wxlambda:stop))))
           (wxlambda:set-web-page view
             "<!doctype html><meta charset='utf-8'><title>Packaged WebView λ</title><script>window.webkit.messageHandlers.wxlambda.postMessage({ready:true});</script>")
           (wxlambda:show-window frame)
           (setf timer (wxlambda:start-ui-timer
             (lambda ()
               (incf ticks)
               (when (> ticks 1000) (error "Packaged WebView timed out"))
               (when (and ready (not ticket))
                 (setf ticket (wxlambda:run-web-script view "6*7")))) 10 :owner frame))
           (wxlambda:run)
           (assert received))
      (when timer (wxlambda:cancel-ui-timer timer))
      (wxlambda:destroy-window frame))))

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
        (check-web-grid)
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
  (format t "PASS: packaged startup, Unicode callbacks, recreation, drawing PNG, activity, grid, JavaScript messaging, and cleanup~%"))

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
