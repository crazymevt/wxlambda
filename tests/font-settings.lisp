(load (merge-pathnames "../examples/font-settings.lisp" *load-truename*))
(wxlambda:with-application ()
  (assert (wxlambda-font-settings:exercise))
  (let* ((frame (wxlambda:make-frame)) (button (wxlambda:make-button frame :label "Font"))
         (control (wxlambda:make-styled-text-control frame))
         (choice (wxlambda:make-choice frame :items '("One" "Two") :selection 1)) (refused nil))
    (unwind-protect
         (progn
           (setf (wxlambda:window-font frame :recursive-p t) (wxlambda:default-font))
           (assert (= 1 (wxlambda:selection choice)))
           (wxlambda:on-click button
             (lambda () (setf refused (handler-case (progn (setf (wxlambda:window-font frame) nil) nil) (wxlambda:native-error () t)))))
           (wxlambda:queue-click button) (wxlambda:process-events) (assert refused)
           (assert (handler-case (progn (setf (wxlambda:stc-tab-width control) 0) nil) (error () t)))
           (assert (handler-case (progn (setf (wxlambda:window-font frame) '("" 12)) nil) (error () t))))
      (wxlambda:destroy-window frame) (wxlambda:process-events))
    (assert (handler-case (progn (wxlambda:window-font frame) nil) (error () t)))))
(format t "Font enumeration, defaults, styles and guard checks passed.~%")
