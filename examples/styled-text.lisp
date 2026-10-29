(require :asdf)
(load (merge-pathnames "../wxlambda.asd" *load-truename*))
(asdf:load-system "wxlambda")
(defpackage #:wxlambda-styled-text (:use #:cl) (:export #:start #:exercise))
(in-package #:wxlambda-styled-text)
(defun build-window ()
  (let* ((frame (wxlambda:make-frame :title "Native styled text" :width 700 :height 420))
         (control (wxlambda:make-styled-text-control frame :value (format nil "alpha λ~%beta 😀~%"))))
    ;; These styling decisions are application code, not a wxlambda language service.
    (setf (wxlambda:stc-margin-type control 0) 1
          (wxlambda:stc-margin-width control 0) 48
          (wxlambda:stc-style-foreground control 1) #x3366cc
          (wxlambda:stc-style-bold-p control 1) t)
    (wxlambda:start-stc-styling control 0)
    (wxlambda:set-stc-style-bytes control (make-array 5 :element-type '(unsigned-byte 8) :initial-element 1))
    (assert (>= (wxlambda:stc-end-styled control) 5))
    (wxlambda:define-stc-marker control 0 2) (wxlambda:add-stc-marker control 0 0)
    (setf (wxlambda:stc-indicator-style control 0) 1
          (wxlambda:stc-indicator-foreground control 0) #xe09020
          (wxlambda:stc-current-indicator control) 0)
    (wxlambda:fill-stc-indicator-range control 6 2)
    (wxlambda:bind-event control :stc-style-needed
      (lambda (event)
        (let* ((end (wxlambda:event-stc-position event)) (prefix (min 5 end)))
          (wxlambda:start-stc-styling control 0)
          (wxlambda:set-stc-styling control prefix 1)
          (wxlambda:set-stc-styling control (- end prefix) 0))))
    (wxlambda:show-window frame) (values frame control)))
(defun exercise ()
  (multiple-value-bind (frame control) (build-window)
    (unwind-protect
         (progn
           (assert (= 1 (wxlambda:stc-style-at control 0)))
           (assert (= #x3366cc (wxlambda:stc-style-foreground control 1)))
           (assert (logbitp 0 (wxlambda:stc-markers-at-line control 0)))
           (wxlambda:begin-stc-undo-action control)
           (unwind-protect
                (progn (wxlambda:replace-stc-range control 0 5 "first")
                       (wxlambda:replace-stc-range control 9 13 "next"))
             (wxlambda:end-stc-undo-action control))
           (wxlambda:stc-undo control)
           (assert (string= (format nil "alpha λ~%beta 😀~%") (wxlambda:stc-text control)))
           t)
      (wxlambda:destroy-window frame))))
(defun start () (wxlambda:with-application () (build-window) (wxlambda:run)))
