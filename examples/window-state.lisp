;;; Reusable state example. Run (wxlambda-window-state:start), or use the
;;; noninteractive exercise called by tests/window-state.lisp.
(require :asdf)
(load (merge-pathnames "../wxlambda.asd" *load-truename*))
(asdf:load-system "wxlambda")

(defpackage #:wxlambda-window-state
  (:use #:cl)
  (:export #:start #:open-window #:exercise))
(in-package #:wxlambda-window-state)

(defun open-window ()
  (let* ((frame (wxlambda:make-frame :title "Window state"))
         (panel (wxlambda:make-panel frame))
         (text (wxlambda:make-text panel :value "Hide/show and disable/enable me"))
         (visibility (wxlambda:make-button panel :label "Toggle text visibility"))
         (input (wxlambda:make-button panel :label "Toggle text input")))
    (wxlambda:make-column panel)
    (dolist (child (list text visibility input)) (wxlambda:add-child panel child))
    (wxlambda:on-click visibility
      (lambda ()
        (wxlambda:show-window text (not (wxlambda:shown-p text)))
        (wxlambda:layout-window panel)))
    (wxlambda:on-click input
      (lambda () (wxlambda:enable-window text (not (wxlambda:enabled-p text)))))
    (wxlambda:show-window frame)
    (values frame panel text visibility input)))

(defun exercise ()
  "Run the example's actual callbacks, then release handlers and close it."
  (multiple-value-bind (frame panel text visibility input) (open-window)
    (declare (ignore panel))
    (assert (wxlambda:shown-p text))
    (assert (wxlambda:enabled-p text))
    (wxlambda:queue-click visibility)
    (wxlambda:queue-click input)
    (wxlambda:process-events)
    (assert (not (wxlambda:shown-p text)))
    (assert (not (wxlambda:enabled-p text)))
    (wxlambda:queue-click visibility)
    (wxlambda:queue-click input)
    (wxlambda:process-events)
    (assert (wxlambda:shown-p text))
    (assert (wxlambda:enabled-p text))
    (wxlambda:off-click visibility)
    (wxlambda:off-click input)
    (wxlambda:destroy-window frame)))

(defun start ()
  (wxlambda:with-application ()
    (open-window)
    (wxlambda:run)))
