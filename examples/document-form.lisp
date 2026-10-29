;;; START is interactive; EXERCISE runs with an already active application.
(require :asdf)
(load (merge-pathnames "../wxlambda.asd" *load-truename*))
(asdf:load-system "wxlambda")
(defpackage #:wxlambda-document-form
  (:use #:cl) (:export #:start #:exercise))
(in-package #:wxlambda-document-form)

(defstruct document-form
  frame name greeting style status path baseline busy
  choose-path confirm-changes report-error items toolbar pending-action action-active close-approved disposed)

(defun form-values (form)
  (list (wxlambda:text (document-form-name form))
        (not (null (wxlambda:checked-p (document-form-greeting form))))
        (wxlambda:selection (document-form-style form))))

(defun dirty-p (form)
  (not (equal (form-values form) (document-form-baseline form))))

(defun native-basename (path)
  (uiop:native-namestring (make-pathname :host nil :device nil :directory nil :defaults path)))

(defun refresh (form)
  (setf (wxlambda:frame-title (document-form-frame form))
        (format nil "~A~A — Lisp form"
                (if (dirty-p form) "* " "")
                (if (document-form-path form)
                    (native-basename (document-form-path form)) "Untitled"))))

(defun install-values (form values path)
  ;; Non-notifying setters avoid treating a loaded document as a user edit.
  (setf (wxlambda:text (document-form-name form)) (first values)
        (wxlambda:checked-p (document-form-greeting form)) (second values)
        (wxlambda:selection (document-form-style form)) (third values)
        (document-form-path form) path
        (document-form-baseline form) (copy-tree values))
  (refresh form))

(defun read-document (path)
  ;; Deliberately no Lisp reader: files are data, never executable forms.
  (with-open-file (stream path :external-format :utf-8)
    (let ((header (read-line stream nil nil))
          (name (read-line stream nil nil))
          (greeting (read-line stream nil nil))
          (style (read-line stream nil nil)))
      (unless (and (equal header "WXLAMBDA-FORM-1") name
                   (not (find-if (lambda (char) (find char '(#\Newline #\Return))) name))
                   (member greeting '("yes" "no") :test #'equal)
                   (member style '("friendly" "formal") :test #'equal)
                   (eq :eof (read-line stream nil :eof)))
        (error "Invalid form document: ~A" path))
      (list name (equal greeting "yes") (if (equal style "formal") 1 0)))))

(defun write-document (path values)
  ;; Finish writing a unique sibling before replacing the target. No durability,
  ;; metadata-preservation or cross-platform atomic-replacement guarantee.
  (when (find-if (lambda (char) (find char '(#\Newline #\Return))) (first values))
    (error "A form name must fit on one line."))
  (let ((temporary nil) (stream nil))
    (unwind-protect
         (progn
           (loop repeat 100
                 for candidate = (make-pathname :name (format nil ".wxlambda-~36R" (random (expt 36 12)))
                                                :type "tmp" :defaults path)
                 do (setf stream (open candidate :direction :output :external-format :utf-8
                                      :if-exists nil :if-does-not-exist :create))
                 when stream do (setf temporary candidate) (return))
           (unless stream (error "Unable to create a temporary sibling of ~A" path))
           (format stream "WXLAMBDA-FORM-1~%~A~%~A~%~A~%" (first values)
                   (if (second values) "yes" "no") (if (= 1 (third values)) "formal" "friendly"))
           (close stream)
           (setf stream nil)
           (uiop:rename-file-overwriting-target temporary path)
           (setf temporary nil)
           t)
      (when stream (close stream :abort t))
      (when (and temporary (probe-file temporary)) (delete-file temporary)))))

(defun choose-path (form mode)
  (let* ((old (document-form-path form))
         (dialog (wxlambda:make-file-dialog
                  :parent (document-form-frame form) :mode mode
                  :directory (if old (uiop:pathname-directory-pathname old) "")
                  :filename (if old (native-basename old) "")
                  :wildcard "Lisp form documents|*.wxform|All files|*")))
    (unwind-protect
         (when (eq :ok (wxlambda:show-modal dialog))
           (uiop:parse-native-namestring (wxlambda:file-dialog-path dialog)))
      (wxlambda:destroy-window dialog))))

(defun confirm-changes (form)
  (let ((dialog (wxlambda:make-message-dialog
                 :parent (document-form-frame form) :title "Unsaved changes"
                 :message "Save changes first? Yes saves, No discards, Cancel keeps this form."
                 :buttons :yes-no-cancel :icon :question)))
    (unwind-protect (wxlambda:show-modal dialog)
      (wxlambda:destroy-window dialog))))

(defun report-error (form condition)
  (let ((dialog (wxlambda:make-message-dialog :parent (document-form-frame form)
                 :title "Document operation failed" :message (princ-to-string condition) :icon :error)))
    (unwind-protect (wxlambda:show-modal dialog)
      (wxlambda:destroy-window dialog))))

(defun save-document (form &optional save-as)
  (let ((path (if (and (not save-as) (document-form-path form))
                  (document-form-path form)
                  (funcall (document-form-choose-path form) form :save)))
        (values (form-values form)))
    (when path
      (setf path (merge-pathnames path))
      (write-document path values)
      (setf (document-form-path form) path
            (document-form-baseline form) (copy-tree values)
            (wxlambda:status-text (document-form-status form)) "Saved")
      (refresh form)
      t)))

(defun may-discard-p (form)
  (or (not (dirty-p form))
      (case (funcall (document-form-confirm-changes form) form)
        (:yes (save-document form)) (:no t) (otherwise nil))))

(defun perform-command (form command)
  ;; Nested native loops can deliver other commands. A second command must not
  ;; replace the document underneath a modal action already in progress.
  (unless (document-form-busy form)
    (setf (document-form-busy form) t)
    (unwind-protect
         (handler-case
             (ecase command
               (:copy-name
                (setf (wxlambda:clipboard-text) (wxlambda:text (document-form-name form)))
                (setf (wxlambda:status-text (document-form-status form))
                      (if (wxlambda:flush-clipboard) "Copied name" "Copied name; persistence unavailable"))
                t)
               (:paste-name
                (let ((value (wxlambda:clipboard-text)))
                  (when value
                    (when (find-if (lambda (char) (find char '(#\Newline #\Return))) value)
                      (error "The form name must fit on one line."))
                    (setf (wxlambda:text (document-form-name form)) value)
                    (refresh form)
                    (setf (wxlambda:status-text (document-form-status form)) "Pasted name")
                    t)))
               (:save (save-document form))
               (:save-as (save-document form t))
               (:open
                (when (may-discard-p form)
                  (let ((path (funcall (document-form-choose-path form) form :open)))
                    (when path
                      (setf path (merge-pathnames path))
                      (let ((values (read-document path)))
                        (install-values form values path)
                        (setf (wxlambda:status-text (document-form-status form)) "Opened")
                        t)))))
               (:close (may-discard-p form)))
           (error (condition)
             (funcall (document-form-report-error form) form condition)
             nil))
      (setf (document-form-busy form) nil))))

(defparameter +form-commands+
  '((12101 :open "&Open…" :open "Open document (Ctrl/Command+O)")
    (12102 :save "&Save" :save "Save (Ctrl/Command+S)")
    (12103 :save-as "Save &As…" :save "Save As (Ctrl/Command+Shift+S)")
    (12104 :close "&Close" :quit "Close (Ctrl/Command+W)")
    (12105 :copy-name "Copy name" :copy "Copy whole name (Ctrl/Command+Shift+C)")
    (12106 :paste-name "Paste name" :paste "Paste whole name (Ctrl/Command+Shift+V)")))

(defun cancel-request (form)
  (when (document-form-pending-action form)
    (wxlambda:cancel-ui-action (document-form-pending-action form))
    (setf (document-form-pending-action form) nil)))

(defun command-views-enabled (form enabled)
  (when (wxlambda:handler-alive-p (document-form-frame form))
    (dolist (item (document-form-items form)) (setf (wxlambda:enabled-p item) enabled))
    (dolist (entry +form-commands+)
      (setf (wxlambda:tool-enabled-p (document-form-toolbar form) (first entry)) enabled))))

(defun close-approved-form (form)
  ;; Approval applies only to this synchronous replay, not future close requests.
  (unwind-protect
       (progn (setf (document-form-close-approved form) t)
              (wxlambda:close-window (document-form-frame form)))
    (setf (document-form-close-approved form) nil)))

(defun request-command (form command)
  ;; First request wins until delivery completes. Coalesce even different commands
  ;; while pending/active, including those arriving from nested modal callbacks.
  (unless (or (document-form-disposed form) (document-form-busy form)
              (document-form-action-active form)
              (not (wxlambda:handler-alive-p (document-form-frame form))))
    (let ((pending (document-form-pending-action form)))
      (when (and pending (wxlambda:ui-action-pending-p pending))
        (return-from request-command nil)))
    (setf (document-form-pending-action form)
          (wxlambda:defer-ui-action
            (lambda ()
              (setf (document-form-pending-action form) nil
                    (document-form-action-active form) t)
              (unwind-protect
                   (progn
                     (command-views-enabled form nil)
                     (when (perform-command form command)
                       (when (eq command :close) (close-approved-form form))))
                (setf (document-form-action-active form) nil)
                (command-views-enabled form t)))
            :owner (document-form-frame form)))))

(defun drain-form-action (form)
  ;; Bounded exercise/test helper; interactive delivery is driven by RUN.
  (loop repeat 40
        while (and (document-form-pending-action form)
                   (wxlambda:ui-action-pending-p (document-form-pending-action form)))
        do (wxlambda:process-events))
  (let ((pending (document-form-pending-action form)))
    (assert (or (null pending) (not (wxlambda:ui-action-pending-p pending))))))

(defun open-form (&key (choose-path #'choose-path) (confirm-changes #'confirm-changes)
                       (report-error #'report-error))
  (let* ((frame (wxlambda:make-frame :title "Untitled — Lisp form" :width 640 :height 360
                                     :accelerators '((#\o 12101 :ctrl) (#\s 12102 :ctrl)
                                                     (#\s 12103 :ctrl :shift) (#\w 12104 :ctrl)
                                                     (#\c 12105 :ctrl :shift) (#\v 12106 :ctrl :shift))))
         (form (make-document-form :frame frame :choose-path choose-path
                                  :confirm-changes confirm-changes :report-error report-error)))
    (handler-case
        (let* ((panel (wxlambda:make-panel frame))
               (bar (wxlambda:make-menu-bar frame))
               (menu (wxlambda:make-menu bar :label "&File"))
               (edit (wxlambda:make-menu bar :label "&Edit")))
          (setf (document-form-name form) (wxlambda:make-text panel :value "Ada")
                (document-form-greeting form) (wxlambda:make-checkbox panel :label "Include greeting" :checked t)
                (document-form-style form) (wxlambda:make-choice panel :items '("Friendly" "Formal") :selection 0)
                (document-form-status form) (wxlambda:make-status-bar frame)
                (document-form-toolbar form) (wxlambda:make-toolbar frame)
                (document-form-items form)
                (loop for entry in +form-commands+ for index from 0
                      collect (wxlambda:make-menu-item (if (< index 4) menu edit)
                                :id (first entry) :label (third entry))))
          (let ((toolbar (document-form-toolbar form)))
            (dolist (index '(0 1 2 :separator 4 5 :separator 3))
              (if (eq index :separator) (wxlambda:add-toolbar-separator toolbar)
                  (destructuring-bind (id command label art help) (nth index +form-commands+)
                    (declare (ignore command))
                    (wxlambda:add-toolbar-tool toolbar id (remove #\& label) :art art :help help))))
            (wxlambda:realize-toolbar toolbar))
          (setf (wxlambda:status-text (document-form-status form)) "Ready")
          (wxlambda:make-column frame)
          (wxlambda:add-child frame panel :proportion 1 :border 0)
          (wxlambda:make-column panel)
          (dolist (child (list (document-form-name form) (document-form-greeting form)
                              (document-form-style form)))
            (wxlambda:add-child panel child :border 12))
          (setf (document-form-baseline form) (form-values form))
          (wxlambda:on-text-change (document-form-name form) (lambda (event) (declare (ignore event)) (refresh form)))
          (wxlambda:on-toggle (document-form-greeting form) (lambda (event) (declare (ignore event)) (refresh form)))
          (wxlambda:on-selection (document-form-style form) (lambda (event) (declare (ignore event)) (refresh form)))
          (labels ((request (event context)
                     (declare (ignore context))
                     (let ((entry (assoc (wxlambda:event-id event) +form-commands+)))
                       (when entry (request-command form (second entry))))))
            ;; Consume at the toolbar before propagation; keyboard/menu commands
            ;; share the frame receiver and portable action IDs.
            (wxlambda:bind-event (document-form-toolbar form) :menu #'request
              :id 12101 :last-id 12106 :with-context t)
            (wxlambda:bind-event frame :menu #'request :id 12101 :last-id 12106 :with-context t))
          (wxlambda:bind-event frame :close
            (lambda (event context)
              (cond
                ;; Forced/non-vetoable close cannot promise a save opportunity.
                ((not (wxlambda:event-can-veto-p event))
                 (cancel-request form) (setf (wxlambda:event-skipped-p context) t))
                ((wxlambda:event-vetoed-p context)
                 ;; A preceding handler may veto and still propagate. Do not
                 ;; let native default destruction override that decision.
                 (setf (wxlambda:event-skipped-p context) nil))
                ((document-form-close-approved form)
                 (setf (document-form-close-approved form) nil
                       (wxlambda:event-skipped-p context) t))
                (t
                 (setf (wxlambda:event-vetoed-p context) t)
                 (request-command form :close))))
            :with-context t)
          (wxlambda:show-window frame)
          form)
      (error (condition) (wxlambda:destroy-window frame) (error condition)))))

(defun dispose-form (form)
  (setf (document-form-disposed form) t)
  (cancel-request form)
  (when (wxlambda:handler-alive-p (document-form-frame form))
    (wxlambda:destroy-window (document-form-frame form)))
  (wxlambda:process-events))

(defun exercise ()
  (let ((form (open-form :choose-path (lambda (form mode) (declare (ignore form mode)) nil))))
    (unwind-protect
         (progn
           (assert (not (dirty-p form)))
           (setf (wxlambda:text (document-form-name form) :notify t) "Grace λ")
           (assert (dirty-p form))
           (assert (wxlambda:activate-toolbar-tool (document-form-toolbar form) 12102))
           (assert (document-form-pending-action form))
           (drain-form-action form)
           (assert (dirty-p form))
           t)
      (dispose-form form))))

(defun start ()
  (wxlambda:with-application ()
    (let ((form (open-form)))
      (unwind-protect (wxlambda:run) (dispose-form form)))))
