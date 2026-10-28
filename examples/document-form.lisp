;;; START is interactive; EXERCISE runs with an already active application.
(require :asdf)
(load (merge-pathnames "../wxlambda.asd" *load-truename*))
(asdf:load-system "wxlambda")
(defpackage #:wxlambda-document-form
  (:use #:cl) (:export #:start #:exercise))
(in-package #:wxlambda-document-form)

(defstruct document-form
  frame name greeting style status path baseline busy
  choose-path confirm-changes report-error items)

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
            (wxlambda:label-text (document-form-status form)) "Saved")
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
                (setf (wxlambda:label-text (document-form-status form))
                      (if (wxlambda:flush-clipboard) "Copied name" "Copied name; persistence unavailable"))
                t)
               (:paste-name
                (let ((value (wxlambda:clipboard-text)))
                  (when value
                    (when (find-if (lambda (char) (find char '(#\Newline #\Return))) value)
                      (error "The form name must fit on one line."))
                    (setf (wxlambda:text (document-form-name form)) value)
                    (refresh form)
                    (setf (wxlambda:label-text (document-form-status form)) "Pasted name")
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
                        (setf (wxlambda:label-text (document-form-status form)) "Opened")
                        t)))))
               (:close (may-discard-p form)))
           (error (condition)
             (funcall (document-form-report-error form) form condition)
             nil))
      (setf (document-form-busy form) nil))))

(defun open-form (&key (choose-path #'choose-path) (confirm-changes #'confirm-changes)
                       (report-error #'report-error))
  (let* ((frame (wxlambda:make-frame :title "Untitled — Lisp form" :width 580 :height 320))
         (form (make-document-form :frame frame :choose-path choose-path
                                  :confirm-changes confirm-changes :report-error report-error)))
    (handler-case
        (let* ((panel (wxlambda:make-panel frame))
               (bar (wxlambda:make-menu-bar frame))
               (menu (wxlambda:make-menu bar :label "&File")))
          (setf (document-form-name form) (wxlambda:make-text panel :value "Ada")
                (document-form-greeting form) (wxlambda:make-checkbox panel :label "Include greeting" :checked t)
                (document-form-style form) (wxlambda:make-choice panel :items '("Friendly" "Formal") :selection 0)
                (document-form-status form) (wxlambda:make-label panel :label "Ready")
                (document-form-items form)
                (loop for (id label) in '((41001 "&Open…") (41002 "&Save") (41003 "Save &As…") (41004 "&Close"))
                      collect (wxlambda:make-menu-item menu :id id :label label)))
          (let ((edit (wxlambda:make-menu bar :label "&Edit")))
            (wxlambda:make-menu-item edit :id 41005 :label "Copy name")
            (wxlambda:make-menu-item edit :id 41006 :label "Paste name"))
          (wxlambda:make-column frame)
          (wxlambda:add-child frame panel :proportion 1 :border 0)
          (wxlambda:make-column panel)
          (dolist (child (list (document-form-name form) (document-form-greeting form)
                              (document-form-style form) (document-form-status form)))
            (wxlambda:add-child panel child :border 12))
          (setf (document-form-baseline form) (form-values form))
          (wxlambda:on-text-change (document-form-name form) (lambda (event) (declare (ignore event)) (refresh form)))
          (wxlambda:on-toggle (document-form-greeting form) (lambda (event) (declare (ignore event)) (refresh form)))
          (wxlambda:on-selection (document-form-style form) (lambda (event) (declare (ignore event)) (refresh form)))
          (wxlambda:bind-event frame :menu
            (lambda (event context)
              (declare (ignore context))
              (case (wxlambda:event-id event)
                (41001 (perform-command form :open)) (41002 (perform-command form :save))
                (41003 (perform-command form :save-as)) (41004 (wxlambda:close-window frame))
                (41005 (perform-command form :copy-name)) (41006 (perform-command form :paste-name))))
            :id 41001 :last-id 41006 :with-context t)
          (wxlambda:bind-event frame :close
            (lambda (event context)
              ;; Forced/non-vetoable closure cannot safely promise an interactive
              ;; save opportunity. Leave its native default policy intact.
              (if (and (wxlambda:event-can-veto-p event)
                       (not (perform-command form :close)))
                  (setf (wxlambda:event-vetoed-p context) t)
                  (setf (wxlambda:event-skipped-p context) t)))
            :with-context t)
          (wxlambda:show-window frame)
          form)
      (error (condition) (wxlambda:destroy-window frame) (error condition)))))

(defun dispose-form (form)
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
           (assert (not (perform-command form :save)))
           (assert (dirty-p form))
           t)
      (dispose-form form))))

(defun start ()
  (wxlambda:with-application ()
    (let ((form (open-form)))
      (unwind-protect (wxlambda:run) (dispose-form form)))))
