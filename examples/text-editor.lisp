;;; Bounded multiline editor primitives; document IO remains Lisp application code.
(require :asdf)
(load (merge-pathnames "../wxlambda.asd" *load-truename*))
(asdf:load-system "wxlambda")
(defpackage #:wxlambda-text-editor (:use #:cl) (:export #:start #:exercise))
(in-package #:wxlambda-text-editor)

(defun exercise ()
  (let* ((frame (wxlambda:make-frame))
         (text (wxlambda:make-text frame :multiline t :value (format nil "First λ~%Second 日本語 😀")))
         (changes 0))
    (unwind-protect
         (progn
           (assert (wxlambda:text-multiline-p text))
           (wxlambda:on-text-change text (lambda (event) (declare (ignore event)) (incf changes)))
           (setf (wxlambda:text-selection text) (list 0 (wxlambda:text-last-position text)))
           (multiple-value-bind (from to) (wxlambda:text-selection text)
             (wxlambda:replace-text text from to (format nil "Replaced λ~%Still multiline")))
           (assert (plusp changes))
           (assert (string= (wxlambda:text text) (format nil "Replaced λ~%Still multiline")))
           (setf (wxlambda:text-editable-p text) nil)
           (assert (not (wxlambda:text-editable-p text)))
           t)
      (wxlambda:destroy-window frame)
      (wxlambda:process-events))))

(defun start ()
  (wxlambda:with-application ()
    (let* ((frame (wxlambda:make-frame :title "Multiline Lisp editor" :width 640 :height 480))
           (panel (wxlambda:make-panel frame))
           (text (wxlambda:make-text panel :multiline t :value (format nil "Edit this text.~%Unicode: λ 日本語 😀")))
           (append (wxlambda:make-button panel :label "Append a line"))
           (replace (wxlambda:make-button panel :label "Replace selection with λ"))
           (editable (wxlambda:make-checkbox panel :label "Allow typing" :checked t))
           (status (wxlambda:make-label panel :label "Ready")))
      (wxlambda:make-column frame)
      (wxlambda:add-child frame panel :border 0 :proportion 1)
      (wxlambda:make-column panel)
      (wxlambda:add-child panel text :proportion 1)
      (dolist (child (list append replace editable status)) (wxlambda:add-child panel child))
      (wxlambda:on-text-change text
        (lambda (event)
          (declare (ignore event))
          (setf (wxlambda:label-text status)
                (format nil "Changed; native end offset ~D" (wxlambda:text-last-position text)))))
      ;; Read-only controls still accept these explicit programmatic changes.
      (wxlambda:on-click append
        (lambda () (let ((end (wxlambda:text-last-position text)))
                     (wxlambda:replace-text text end end (format nil "~%Appended λ")))))
      (wxlambda:on-click replace
        (lambda () (multiple-value-bind (from to) (wxlambda:text-selection text)
                     (wxlambda:replace-text text from to "λ"))))
      (wxlambda:on-toggle editable
        (lambda (event) (setf (wxlambda:text-editable-p text) (wxlambda:event-checked-p event))))
      (unwind-protect
           (progn (wxlambda:show-window frame) (wxlambda:run))
        (when (wxlambda:handler-alive-p frame) (wxlambda:destroy-window frame))
        (wxlambda:process-events)))))
