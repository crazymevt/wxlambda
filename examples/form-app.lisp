;;; Reusable form application. FORM.LISP launches it; START-LIVE enables editing.
(require :asdf)
(load (merge-pathnames "../wxlambda.asd" *load-truename*))
(asdf:load-system "wxlambda")

(defpackage #:wxlambda-form
  (:use #:cl)
  (:export #:start #:start-live #:open-window #:close-window #:unbind-handlers
           #:submit-form #:set-submit-action #:*name* #:*greeting* #:*style*
           #:*status* #:*submit* #:*reopen*))
(in-package #:wxlambda-form)

(defvar *frame* nil)
(defvar *root* nil)
(defvar *name* nil)
(defvar *greeting* nil)
(defvar *style* nil)
(defvar *status* nil)
(defvar *submit* nil)
(defvar *reopen* nil)
(defvar *name-value* "Ada")
(defvar *greeting-value* t)
(defvar *style-value* 0)

(defun remember-input ()
  ;; Cleanup can also run after only part of the form was constructed.
  (when (and *name* *greeting* *style*)
    (setf *name-value* (wxlambda:get-text *name*)
          *greeting-value* (wxlambda:checked-p *greeting*)
          *style-value* (wxlambda:selection *style*))))

(defun edited ()
  (remember-input)
  (wxlambda:set-label *status* "Edited; submit to preview")
  (wxlambda:layout-window *root*))

(defun submit-form ()
  (remember-input)
  (wxlambda:set-label *status*
    (format nil (cond ((not *greeting-value*) "Name: ~A")
                      ((eql *style-value* 1) "Good day, ~A.")
                      (t "Hello, ~A!"))
            *name-value*))
  (wxlambda:layout-window *root*))

(defun set-submit-action (&optional (action 'submit-form))
  "Replace the current submit handler. Symbol actions pick up function redefinitions."
  (wxlambda:on-click *submit* (lambda () (funcall action))))

(defun unbind-handlers ()
  "Release all registered closures before destroying this form."
  (loop for (control unbind) in
        (list (list *name* #'wxlambda:off-text-change)
              (list *greeting* #'wxlambda:off-toggle)
              (list *style* #'wxlambda:off-selection)
              (list *submit* #'wxlambda:off-click)
              (list *reopen* #'wxlambda:off-click))
        when control do
          ;; If the user already closed the native frame, stale handles cannot
          ;; be unbound. Those closures remain rooted until session cleanup.
          (handler-case (funcall unbind control)
            (wxlambda:native-error () nil))))

(defun close-window ()
  (when *frame*
    (handler-case (remember-input)
      (wxlambda:native-error () nil))
    (unbind-handlers)
    (handler-case (wxlambda:destroy-window *frame*)
      (wxlambda:native-error () nil))
    (setf *frame* nil *root* nil *name* nil *greeting* nil *style* nil
          *status* nil *submit* nil *reopen* nil)))

(defun open-window ()
  "Recreate the form within the current session, retaining its input values."
  (close-window)
  (setf *frame* (wxlambda:make-frame :title "A Lisp form" :width 560 :height 400)
        *root* (wxlambda:make-panel *frame*))
  (let* ((name-row (wxlambda:make-panel *root*))
         (name-label (wxlambda:make-label name-row :label "Name"))
         (style-row (wxlambda:make-panel *root*))
         (style-label (wxlambda:make-label style-row :label "Style"))
         (actions (wxlambda:make-panel *root*)))
    (setf *name* (wxlambda:make-text name-row :value *name-value*)
          *greeting* (wxlambda:make-checkbox *root* :label "Include a greeting"
                                           :checked *greeting-value*)
          *style* (wxlambda:make-choice style-row :items '("Friendly" "Formal")
                                      :selection *style-value*)
          *status* (wxlambda:make-label *root* :label "Ready")
          *submit* (wxlambda:make-button actions :label "Submit")
          *reopen* (wxlambda:make-button actions :label "Reopen form"))
    (wxlambda:make-column *frame*)
    (wxlambda:add-child *frame* *root* :border 0 :proportion 1)
    (wxlambda:make-column *root*)
    (wxlambda:make-row name-row)
    (wxlambda:add-child name-row name-label :border 0 :expand nil :align :center)
    (wxlambda:add-spacer name-row 12)
    (wxlambda:add-child name-row *name* :border 0 :proportion 1)
    (wxlambda:make-row style-row)
    (wxlambda:add-child style-row style-label :border 0 :expand nil :align :center)
    (wxlambda:add-spacer style-row 12)
    (wxlambda:add-child style-row *style* :border 0 :proportion 1)
    (wxlambda:make-row actions)
    (wxlambda:add-stretch actions)
    (wxlambda:add-child actions *reopen* :border 0 :expand nil)
    (wxlambda:add-spacer actions 12)
    (wxlambda:add-child actions *submit* :border 0 :expand nil)
    (dolist (child (list name-row *greeting* style-row *status*))
      (wxlambda:add-child *root* child :border 12))
    (wxlambda:add-stretch *root*)
    (wxlambda:add-child *root* actions :border 12)
    (wxlambda:on-text-change *name* (lambda (event) (declare (ignore event)) (edited)))
    (wxlambda:on-toggle *greeting* (lambda (event) (declare (ignore event)) (edited)))
    (wxlambda:on-selection *style* (lambda (event) (declare (ignore event)) (edited)))
    (set-submit-action)
    (wxlambda:on-click *reopen* (lambda () (open-window)))
    (wxlambda:show-window *frame*)))

(defun start ()
  "Run normally until the last frame closes. Restart Lisp for another session."
  (wxlambda:with-application ()
    (unwind-protect (progn (open-window) (wxlambda:run))
      (close-window))))

(defun start-live (&key (input *standard-input*) (output *standard-output*) (on-error :debug))
  "Enter the terminal application REPL. STOP/EOF ends this one GUI session."
  (wxlambda:with-repl-application (:input input :output output :on-error on-error)
    (open-window)))
