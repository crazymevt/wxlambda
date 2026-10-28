;;; Load and call (wxlambda-application-handler:start).
(require :asdf)
(load (merge-pathnames "../wxlambda.asd" *load-truename*))
(asdf:load-system "wxlambda")
(defpackage #:wxlambda-application-handler
  (:use #:cl) (:export #:start #:exercise))
(in-package #:wxlambda-application-handler)

(defun exercise ()
  "Dispatch an application-level message and release only its binding."
  (wxlambda:register-event-type :application-message :family :command)
  (let* ((app (wxlambda:current-application-handler))
         (received nil)
         (binding (wxlambda:bind-event app :application-message
                    (lambda (event) (push (wxlambda:event-text event) received)))))
    (unwind-protect
         (progn
           (wxlambda:queue-event app :application-message :text "Application message")
           (wxlambda:process-pending-events)
           (assert (equal received '("Application message")))
           (assert (eq app (wxlambda:current-application-handler)))
           app)
      (wxlambda:unbind-event binding))))

(defun start ()
  (wxlambda:with-application () (exercise)))
