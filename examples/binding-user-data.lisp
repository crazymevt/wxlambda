;;; Load and call (wxlambda-binding-user-data:start).
(require :asdf)
(load (merge-pathnames "../wxlambda.asd" *load-truename*))
(asdf:load-system "wxlambda")
(defpackage #:wxlambda-binding-user-data (:use #:cl) (:export #:start #:exercise))
(in-package #:wxlambda-binding-user-data)

(defun exercise ()
  (let* ((receiver (wxlambda:make-event-handler))
         (state (list :count 0))
         (saved nil) (binding nil))
    (unwind-protect
         (progn
           (setf binding
                 (wxlambda:bind-event receiver :click
                   (lambda (event context)
                     (setf saved event)
                     (let ((data (wxlambda:event-user-data context)))
                       (incf (getf data :count)))
                     (wxlambda:unbind-event binding)
                     ;; The active callback retains its datum after unbinding.
                     (assert (eq state (wxlambda:event-user-data context))))
                   :with-context t :user-data state))
           (wxlambda:process-event receiver :click)
           (assert (= 1 (getf state :count)))
           (assert (eq state (wxlambda:event-user-data saved)))
           t)
      (when binding (wxlambda:unbind-event binding))
      (wxlambda:destroy-event-handler receiver))))

(defun start () (wxlambda:with-application () (exercise)))
