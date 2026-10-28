;;; Load and call (wxlambda-close-events:start).
(require :asdf)
(load (merge-pathnames "../wxlambda.asd" *load-truename*))
(asdf:load-system "wxlambda")
(defpackage #:wxlambda-close-events
  (:use #:cl)
  (:export #:start #:exercise))
(in-package #:wxlambda-close-events)

(defun open-window ()
  (let* ((frame (wxlambda:make-frame :title "Veto-capable close events"))
         (panel (wxlambda:make-panel frame))
         (allow (wxlambda:make-checkbox panel :label "Allow closing"))
         (request (wxlambda:make-button panel :label "Request close"))
         (caption (wxlambda:make-label panel :label "Closing is currently blocked"))
         (blocked 0)
         (binding
           (wxlambda:bind-event frame :close
             (lambda (snapshot context)
               (assert (eq :close (wxlambda:event-family snapshot)))
               (if (and (wxlambda:event-can-veto-p context) (not (wxlambda:checked-p allow)))
                   (progn
                     (setf (wxlambda:event-vetoed-p context) t)
                     (incf blocked)
                     (setf (wxlambda:label-text caption) (format nil "Blocked ~D close requests" blocked)))
                   (progn
                     ;; Release the convenience callback before native default closing.
                     (wxlambda:off-click request)
                     (setf (wxlambda:event-skipped-p context) t))))
             :with-context t)))
    (wxlambda:make-column panel)
    (dolist (child (list allow request caption)) (wxlambda:add-child panel child))
    (wxlambda:on-click request (lambda () (wxlambda:close-window frame)))
    (wxlambda:show-window frame)
    (values frame allow request binding (lambda () blocked))))

(defun exercise ()
  (multiple-value-bind (frame allow request binding blocked) (open-window)
    (wxlambda:queue-click request)
    (wxlambda:process-events)
    (assert (= 1 (funcall blocked)))
    (assert (wxlambda:handler-alive-p frame))
    (setf (wxlambda:checked-p allow) t)
    (wxlambda:queue-click request)
    (wxlambda:process-events)
    (assert (not (wxlambda:handler-alive-p frame)))
    (assert (not (wxlambda:binding-active-p binding)))))

(defun start ()
  (wxlambda:with-application () (open-window) (wxlambda:run)))
