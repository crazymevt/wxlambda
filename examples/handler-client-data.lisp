;;; Load and call (wxlambda-handler-client-data:start).
(require :asdf)
(load (merge-pathnames "../wxlambda.asd" *load-truename*))
(asdf:load-system "wxlambda")
(defpackage #:wxlambda-handler-client-data (:use #:cl) (:export #:start #:exercise))
(in-package #:wxlambda-handler-client-data)

(defun exercise ()
  (let* ((handler (wxlambda:make-event-handler))
         (state (list :clicks 0))
         (binding nil))
    (unwind-protect
         (progn
           (setf (wxlambda:event-handler-client-data handler) state)
           (setf binding
                 (wxlambda:bind-event handler :click
                   (lambda (event)
                     (let ((data (wxlambda:event-handler-client-data
                                  (wxlambda:event-source event))))
                       (incf (getf data :clicks))))))
           (wxlambda:process-event handler :click)
           (assert (eq state (wxlambda:event-handler-client-data handler)))
           (assert (= 1 (getf state :clicks)))
           (setf (wxlambda:event-handler-client-data handler) nil)
           (assert (null (wxlambda:event-handler-client-data handler)))
           t)
      (when binding (wxlambda:unbind-event binding))
      (wxlambda:destroy-event-handler handler))))

(defun start () (wxlambda:with-application () (exercise)))
