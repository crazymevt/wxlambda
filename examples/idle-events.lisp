;;; Load and call (wxlambda-idle-events:start).
(require :asdf)
(load (merge-pathnames "../wxlambda.asd" *load-truename*))
(asdf:load-system "wxlambda")
(defpackage #:wxlambda-idle-events (:use #:cl) (:export #:start #:exercise))
(in-package #:wxlambda-idle-events)

(defun exercise ()
  (let* ((previous-mode (wxlambda:idle-mode))
         (window (wxlambda:make-frame :title "Bounded idle work"))
         (remaining (list :first :second))
         (completed 0) (binding nil))
    (unwind-protect
         (progn
           (setf (wxlambda:idle-mode) :specified
                 (wxlambda:window-process-idle-p window) t)
           (setf binding
                 (wxlambda:bind-event (wxlambda:current-application-handler) :idle
                   (lambda (snapshot context)
                     (declare (ignore snapshot))
                     ;; Do one small chunk, then yield control to the GUI.
                     (when remaining (pop remaining) (incf completed))
                     (setf (wxlambda:event-more-requested-p context)
                           (not (null remaining))))
                   :with-context t))
           (wxlambda:wake-up-idle)
           (loop repeat 4 while remaining do (wxlambda:process-events))
           (assert (= 2 completed))
           t)
      (when binding (wxlambda:unbind-event binding))
      (setf (wxlambda:idle-mode) previous-mode)
      (wxlambda:destroy-window window)
      (wxlambda:process-events))))

(defun start () (wxlambda:with-application () (exercise)))
