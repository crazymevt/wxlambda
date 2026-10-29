(require :asdf)
(load (merge-pathnames "../wxlambda.asd" *load-truename*))
(asdf:load-system "wxlambda")
(defpackage #:wxlambda-theme-palette (:use #:cl) (:export #:start #:exercise))
(in-package #:wxlambda-theme-palette)
(defun exercise ()
  (let* ((frame (wxlambda:make-frame :title "Theme palette"))
         (panel (wxlambda:make-panel frame))
         (book (wxlambda:make-aui-notebook panel))
         (page (wxlambda:make-panel book))
         (editor (wxlambda:make-styled-text-control page)))
    (unwind-protect
         (progn
           (wxlambda:make-column frame) (wxlambda:add-child frame panel :proportion 1)
           (wxlambda:make-column panel) (wxlambda:add-child panel book :proportion 1)
           (wxlambda:make-column page) (wxlambda:add-child page editor :proportion 1)
           (wxlambda:add-notebook-page book page :label "Palette")
           (setf (wxlambda:window-colours frame :recursive-p t) '((230 230 230) (25 28 35))
                 (wxlambda:notebook-tab-colours book) '(#x17191E #x303440)
                 (wxlambda:stc-text editor) "Theme palette λ"
                 (wxlambda:stc-style-foreground editor 32) #xE1E5ED
                 (wxlambda:stc-style-background editor 32) #x1E2026)
           (wxlambda:clear-stc-styles editor)
           (setf (wxlambda:stc-caret-colour editor) #xFFFFFF
                 (wxlambda:stc-selection-colours editor) '(#xFFFFFF #x36587E))
           (assert (= #xFFFFFF (wxlambda:stc-caret-colour editor)))
           (assert (equal '(#xFFFFFF #x36587E) (wxlambda:stc-selection-colours editor)))
           (assert (equal '(#x17191E #x303440) (wxlambda:notebook-tab-colours book)))
           (assert (equal '(230 230 230) (wxlambda:window-colours panel)))
           (assert (equal "Theme palette λ" (wxlambda:stc-text editor)))
           (wxlambda:system-colours)
           (setf (wxlambda:stc-selection-colours editor) nil
                 (wxlambda:notebook-tab-colours book) nil
                 (wxlambda:window-colours frame :recursive-p t) nil)
           (assert (null (wxlambda:stc-selection-colours editor)))
           (assert (null (wxlambda:notebook-tab-colours book)))
           t)
      (wxlambda:destroy-window frame) (wxlambda:process-events))))
(defun start () (wxlambda:with-application () (exercise)))
