;;; Load and call (wxlambda-event-routing:start).
(require :asdf)
(load (merge-pathnames "../wxlambda.asd" *load-truename*))
(asdf:load-system "wxlambda")
(defpackage #:wxlambda-event-routing (:use #:cl) (:export #:start #:exercise))
(in-package #:wxlambda-event-routing)
(defun exercise ()
  "Rewrite a command ID before parent delivery; the child's snapshot stays unchanged."
  (wxlambda:register-event-type :routed-command :family :command)
  (let* ((frame (wxlambda:make-frame))
         (panel (wxlambda:make-panel frame))
         (child (wxlambda:make-button panel))
         (before nil) (after nil)
         (parent-binding (wxlambda:bind-event panel :routed-command
                           (lambda (event) (setf after event)) :id 42))
         (child-binding (wxlambda:bind-event child :routed-command
                          (lambda (event context)
                            (setf before event)
                            (setf (wxlambda:event-id context) 42
                                  (wxlambda:event-skipped-p context) t)) :id 10 :with-context t)))
    (unwind-protect
         (progn
           (wxlambda:process-event child :routed-command :id 10)
           (assert (= 10 (wxlambda:event-id before)))
           (assert (= 42 (wxlambda:event-id after)))
           (assert (eq panel (wxlambda:event-source after))))
      (wxlambda:unbind-event child-binding)
      (wxlambda:unbind-event parent-binding)
      (wxlambda:destroy-window frame))))
(defun start () (wxlambda:with-application () (exercise)))
