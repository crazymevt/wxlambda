(load (merge-pathnames "../examples/theme-palette.lisp" *load-truename*))
(wxlambda:with-application ()
  (assert (wxlambda-theme-palette:exercise))
  (let ((parent (wxlambda:make-frame)))
    (unwind-protect
         (progn
           (assert (typep (make-instance 'wxlambda:splitter-window :parent parent :sash-style :flat) 'wxlambda:splitter-window))
           (assert (handler-case (progn (wxlambda:make-splitter-window parent :sash-style :unknown) nil) (error () t))))
      (wxlambda:destroy-window parent)))
  (let* ((frame (wxlambda:make-frame))
         (book (wxlambda:make-aui-notebook frame))
         (editor (wxlambda:make-styled-text-control frame))
         (button (wxlambda:make-button frame :label "Check"))
         (refused nil))
    (unwind-protect
         (progn
           (wxlambda:on-click button
             (lambda () (setf refused
               (handler-case (progn (setf (wxlambda:window-colours frame) '((0 0 0) (255 255 255))) nil)
                 (wxlambda:native-error () t)))))
           (wxlambda:queue-click button) (wxlambda:process-events)
           (assert refused)
           (setf (wxlambda:stc-text editor) "unchanged λ"
                 (wxlambda:stc-caret-colour editor) #x123456
                 (wxlambda:stc-selection-colours editor) '(#x112233 #x445566))
           (assert (handler-case (progn (setf (wxlambda:stc-selection-colours editor) '(#xFFFFFF -1)) nil) (error () t)))
           (assert (equal '(#x112233 #x445566) (wxlambda:stc-selection-colours editor)))
           (assert (string= "unchanged λ" (wxlambda:stc-text editor)))
           (assert (handler-case (progn (setf (wxlambda:notebook-tab-colours book) '(#xFFFFFF -1)) nil) (error () t)))
           (assert (null (wxlambda:notebook-tab-colours book))))
      (wxlambda:destroy-window frame) (wxlambda:process-events)
      (assert (not (wxlambda:handler-alive-p editor)))
      (assert (handler-case (progn (wxlambda:stc-caret-colour editor) nil) (error () t))))))
(format t "Theme palette example and callback checks passed.~%")

;;; Selection metadata, validation, callback guards and parent-owned lifetime.
;;; A fresh process/session is used by this test file; all windows are in the
;;; same application session above, so keep additional checks in that body.
