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

(defun check-styled-workspace ()
  (let* ((frame (wxlambda:make-frame :width 640 :height 420))
         (book (wxlambda:make-aui-notebook frame))
         (first (wxlambda:make-panel book))
         (second (wxlambda:make-panel book))
         (text (wxlambda:make-styled-text-control first :value "Styled λ 😀")))
    (unwind-protect
         (progn
           (wxlambda:make-column first) (wxlambda:add-child first text :expand t :proportion 1)
           (wxlambda:add-notebook-page book first :label "First")
           (wxlambda:insert-notebook-page book second 0 :label "Second" :select t)
           (assert (= 2 (wxlambda:notebook-page-count book)))
           (wxlambda:move-notebook-page book 0 1)
           (assert (eq second (wxlambda:notebook-page book 1)))
           (wxlambda:remove-notebook-page book 1)
           (assert (wxlambda:handler-alive-p second))
           (wxlambda:show-window frame)
           (wxlambda:replace-stc-range text 0 6 "Native")
           (assert (string= "Native λ 😀" (wxlambda:stc-text text)))
           (setf (wxlambda:stc-style-foreground text 1) #x3366cc)
           (wxlambda:start-stc-styling text 0) (wxlambda:set-stc-styling text 6 1)
           (assert (= 1 (wxlambda:stc-style-at text 0)))
           (wxlambda:stc-undo text)
           (assert (string= "Styled λ 😀" (wxlambda:stc-text text))))
      (wxlambda:destroy-window frame))))

(defun check-thread-delivery ()
  (let* ((receiver (make-instance 'wxlambda:event-handler))
         (gui sb-thread:*current-thread*) (received nil)
         (payload (concatenate 'string "Packaged worker λ" (string #\Null) "😀")))
    (unwind-protect
         (progn
           (wxlambda:bind-event receiver :thread
             (lambda (event)
               (assert (eq gui sb-thread:*current-thread*))
               (setf received (wxlambda:event-text event))))
           (sb-thread:join-thread
            (sb-thread:make-thread (lambda () (wxlambda:queue-thread-event receiver payload))))
           (loop repeat 100 until received do (wxlambda:process-events) (sleep .005))
           (assert (equal payload received)))
      (wxlambda:destroy-event-handler receiver))))

(defun check-report-views ()
  (let* ((frame (wxlambda:make-frame))
         (report (wxlambda:make-list-control frame :virtual-p t
                   :item-text-function (lambda (control row column)
                                         (declare (ignore control))
                                         (format nil "Packaged λ ~D/~D" row column)))))
    (unwind-protect
         (progn
           (wxlambda:insert-list-column report 0 "Value" :width 180)
           (setf (wxlambda:item-count report) 1000000)
           (assert (string= "Packaged λ 999999/0" (wxlambda:list-item-text report 999999 0))))
      (wxlambda:destroy-window frame))))

(defun check-image-resources ()
  (let* ((frame (wxlambda:make-frame))
         (image (wxlambda:make-image :width 1 :height 1
                  :rgb (make-array 3 :element-type '(unsigned-byte 8) :initial-contents '(32 64 128))
                  :alpha (make-array 1 :element-type '(unsigned-byte 8) :initial-element 128)))
         (bitmap (wxlambda:make-bitmap :image image))
         (view (wxlambda:make-static-bitmap frame bitmap)))
    (unwind-protect
         (progn
           (assert (equal '(32 64 128 128) (wxlambda:image-pixel image 0 0)))
           (uiop:with-temporary-file (:pathname path :prefix "wxlambda-image-" :type "png")
             (wxlambda:save-image image path)
             (let ((loaded (wxlambda:make-image :path path)))
               (unwind-protect
                    (assert (equal '(32 64 128 128) (wxlambda:image-pixel loaded 0 0)))
                 (wxlambda:destroy-image loaded))))
           (let ((copy (wxlambda:bitmap-value view)))
             (unwind-protect
                  (assert (equal '(1 1) (wxlambda:bitmap-logical-size copy)))
               (wxlambda:destroy-bitmap copy)))
           (assert (> (wxlambda:display-count) 0)))
      (wxlambda:destroy-window frame)
      (wxlambda:destroy-bitmap bitmap) (wxlambda:destroy-image image))))

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
        (check-image-resources)
        (check-report-views)
        (check-thread-delivery)
        (check-styled-workspace)
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
  (format t "PASS: packaged startup, Unicode callbacks, recreation, drawing PNG, activity, grid, JavaScript messaging, styled text, dynamic tabs, and cleanup~%"))

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
