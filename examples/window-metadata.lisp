;;; Load this file and call (wxlambda-window-metadata:start).
(require :asdf)
(load (merge-pathnames "../wxlambda.asd" *load-truename*))
(asdf:load-system "wxlambda")

(defpackage #:wxlambda-window-metadata
  (:use #:cl)
  (:export #:start #:open-window #:exercise))
(in-package #:wxlambda-window-metadata)

(defclass tracking-frame (wxlambda:frame)
  ((title-updates :initform 0 :accessor title-updates)))

(defmethod (setf wxlambda:frame-title) :after (value (frame tracking-frame))
  (declare (ignore value))
  (incf (title-updates frame)))

(defun open-window ()
  (let* ((frame (make-instance 'tracking-frame :title "Window metadata"))
         (panel (wxlambda:make-panel frame))
         (button (wxlambda:make-button panel :label "Rename window and update title"))
         (changes 0))
    (setf (wxlambda:window-name frame) "metadata-example")
    (wxlambda:make-column panel)
    (wxlambda:add-child panel button)
    (wxlambda:on-click button
      (lambda ()
        (incf changes)
        (setf (wxlambda:window-name frame) (format nil "metadata-~D" changes))
        (setf (wxlambda:frame-title frame) (format nil "Updated ~D — λ 日本語 😀" changes))))
    (wxlambda:show-window frame)
    (values frame button)))

(defun exercise ()
  "Exercise the same callback as the interactive example, then clean up."
  (multiple-value-bind (frame button) (open-window)
    (assert (string= "metadata-example" (wxlambda:window-name frame)))
    (wxlambda:queue-click button)
    (wxlambda:process-events)
    (assert (string= "metadata-1" (wxlambda:window-name frame)))
    (assert (string= "Updated 1 — λ 日本語 😀" (wxlambda:frame-title frame)))
    (assert (= 1 (title-updates frame)))
    (wxlambda:off-click button)
    (wxlambda:destroy-window frame)))

(defun start ()
  (wxlambda:with-application ()
    (open-window)
    (wxlambda:run)))
