(require :asdf)
(load (merge-pathnames "../wxlambda.asd" *load-truename*))
(asdf:load-system "wxlambda")
(defpackage #:wxlambda-tree-drag (:use #:cl) (:export #:start #:exercise))
(in-package #:wxlambda-tree-drag)
(defun exercise ()
  (let* ((frame (wxlambda:make-frame)) (tree (wxlambda:make-tree-control frame))
         (root (wxlambda:add-tree-root tree "Root")))
    (unwind-protect
         (progn
           (assert (handler-case (progn (wxlambda:make-event :kind :tree-begin-drag) nil) (error () t)))
           (assert (handler-case (progn (wxlambda:drag-tree-item root) nil) (wxlambda:native-error () t))) t)
      (wxlambda:destroy-window frame) (wxlambda:process-events))))
(defun start ()
  (wxlambda:with-application ()
    (let* ((frame (wxlambda:make-frame :title "Drag tree items"))
           (tree (wxlambda:make-tree-control frame)) (root (wxlambda:add-tree-root tree "Root")))
      ;; MSW needs an image list to initiate the native BEGIN notification.
      (let* ((image (wxlambda:make-image :width 16 :height 16
                     :rgb (make-array (* 16 16 3) :element-type '(unsigned-byte 8) :initial-element 130)))
             (bitmap (wxlambda:make-bitmap :image image)))
        (unwind-protect
             (progn (setf (wxlambda:tree-icons tree) (list bitmap) (wxlambda:tree-item-icon root) 0)
                    (dolist (label '("Source" "Destination"))
                      (setf (wxlambda:tree-item-icon (wxlambda:append-tree-item root label)) 0)))
          (wxlambda:destroy-bitmap bitmap) (wxlambda:destroy-image image)))
      (setf (wxlambda:tree-item-expanded-p root) t)
      (wxlambda:make-column frame) (wxlambda:add-child frame tree :proportion 1)
      (wxlambda:bind-event tree :tree-begin-drag
        (lambda (event context)
          (declare (ignore context))
          (let* ((source (wxlambda:event-tree-item event))
                 (target (wxlambda:drag-tree-item source)))
            ;; Only application mutations defer; native dragging needs this
            ;; mouse event to remain live (in particular on Cocoa).
            (when target
              (wxlambda:defer-ui-action
                (lambda () (format t "Drop on ~S~%" (when (wxlambda:tree-item-alive-p target)
                                                       (wxlambda:tree-item-text target))))))))
        :with-context t)
      (wxlambda:show-window frame) (wxlambda:run))))
