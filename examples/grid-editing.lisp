(require :asdf)
(asdf:load-asd (merge-pathnames "../wxlambda.asd" *load-truename*))
(asdf:load-system "wxlambda")
(defpackage #:wxlambda-grid-editing (:use #:cl) (:export #:start #:exercise))
(in-package #:wxlambda-grid-editing)
(defun build-window ()
  (let* ((frame (wxlambda:make-frame :title "Native grid validation" :width 650 :height 400))
         (panel (wxlambda:make-panel frame))
         (grid (wxlambda:make-grid panel :rows 4 :columns 3))
         (add (wxlambda:make-button panel :label "Append row"))
         (remove (wxlambda:make-button panel :label "Delete last row"))
         (reset (wxlambda:make-button panel :label "Reset active editor, then disable"))
         (status (wxlambda:make-status-bar frame)))
    (wxlambda:make-column panel)
    (wxlambda:add-child panel grid :proportion 1 :expand t :border 8)
    (dolist (button (list add remove reset)) (wxlambda:add-child panel button :expand t :border 4))
    (setf (wxlambda:grid-column-label grid 0) "Required name"
          (wxlambda:grid-column-label grid 1) "Notes"
          (wxlambda:grid-column-label grid 2) "Read only"
          (wxlambda:grid-cell-text grid 0 0) "Edit this λ"
          (wxlambda:grid-cell-text grid 0 2) "Protected"
          (wxlambda:grid-cell-read-only-p grid 0 2) t
          (wxlambda:grid-cell-background grid 0 2) #xeeeeee)
    (wxlambda:bind-event grid :grid-cell-changing
      (lambda (event context)
        (when (and (zerop (wxlambda:event-grid-column event))
                   (zerop (length (wxlambda:event-text event))))
          (setf (wxlambda:event-vetoed-p context) t
                (wxlambda:status-text status) "Name cannot be empty."))
        (setf (wxlambda:event-skipped-p context) t)) :with-context t)
    (wxlambda:bind-event grid :grid-cell-changed
      (lambda (event)
        (setf (wxlambda:status-text status)
          (format nil "(~D,~D): ~S → ~S" (wxlambda:event-grid-row event)
            (wxlambda:event-grid-column event) (wxlambda:event-text event)
            (wxlambda:event-grid-cell-value event)))))
    (wxlambda:on-click add
      (lambda () (wxlambda:defer-ui-action
        (lambda () (unless (wxlambda:grid-editing-p grid) (wxlambda:append-grid-rows grid))) :owner grid)))
    (wxlambda:on-click remove
      (lambda () (wxlambda:defer-ui-action
        (lambda () (when (and (not (wxlambda:grid-editing-p grid)) (plusp (wxlambda:grid-rows grid)))
                     (wxlambda:delete-grid-rows grid (1- (wxlambda:grid-rows grid))))) :owner grid)))
    (wxlambda:on-click reset
      (lambda () (wxlambda:defer-ui-action
        (lambda () (when (wxlambda:grid-editing-p grid)
                     (wxlambda:reset-grid-cell-editor grid)
                     (wxlambda:disable-grid-cell-edit-control grid))) :owner grid)))
    (setf (wxlambda:status-text status) "Double-click to edit. Reset restores the native editor baseline.")
    (values frame grid add remove)))
(defun exercise ()
  (multiple-value-bind (frame grid add remove) (build-window)
    (unwind-protect
         (progn
           (wxlambda:show-window frame) (wxlambda:layout-window frame)
           (wxlambda:queue-click add)
           (loop repeat 30 until (= 5 (wxlambda:grid-rows grid)) do (wxlambda:process-events))
           (assert (= 5 (wxlambda:grid-rows grid)))
           (wxlambda:queue-click remove)
           (loop repeat 30 until (= 4 (wxlambda:grid-rows grid)) do (wxlambda:process-events))
           (assert (= 4 (wxlambda:grid-rows grid)))
           (assert (wxlambda:grid-cell-read-only-p grid 0 2)) t)
      (wxlambda:destroy-window frame))))
(defun start ()
  (wxlambda:with-application ()
    (let ((frame (build-window))) (wxlambda:show-window frame) (wxlambda:run))))
