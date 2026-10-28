(in-package #:wxlambda)

(cffi:define-foreign-library wxlambda-native
  (:darwin "libwxlambda.dylib")
  (:unix "libwxlambda.so")
  (:windows "wxlambda.dll"))

(cffi:defcfun ("wxl_abi_version" %abi-version) :int)
(cffi:defcfun ("wxl_last_error" %last-error) :string)
(cffi:defcfun ("wxl_start" %start) :int (callback :pointer))
(cffi:defcfun ("wxl_run" %run) :int)
(cffi:defcfun ("wxl_stop" %stop) :int)
(cffi:defcfun ("wxl_finish" %finish) :int)
(cffi:defcfun ("wxl_frame" %frame) :uint64 (title (:string :encoding :utf-8)) (width :int) (height :int))
(cffi:defcfun ("wxl_panel" %panel) :uint64 (parent :uint64))
(cffi:defcfun ("wxl_button" %button) :uint64 (parent :uint64) (label (:string :encoding :utf-8)))
(cffi:defcfun ("wxl_text" %text) :uint64 (parent :uint64) (value (:string :encoding :utf-8)))
(cffi:defcfun ("wxl_column" %column) :int (parent :uint64))
(cffi:defcfun ("wxl_add" %add) :int (parent :uint64) (child :uint64) (proportion :int))
(cffi:defcfun ("wxl_show" %show) :int (window :uint64))
(cffi:defcfun ("wxl_destroy" %destroy) :int (window :uint64))
(cffi:defcfun ("wxl_set_text" %set-text) :int
  (control :uint64) (value (:string :encoding :utf-8)) (notify :int))
(cffi:defcfun ("wxl_get_text" %get-text) :pointer (control :uint64))
(cffi:defcfun ("wxl_on_text_change" %on-text-change) :int (control :uint64) (token :uint64))
(cffi:defcfun ("wxl_off_text_change" %off-text-change) :int (control :uint64))
(cffi:defcfun ("wxl_off_click" %off-click) :int (button :uint64))
(cffi:defcfun ("wxl_on_click" %on-click) :int (button :uint64) (token :uint64))
(cffi:defcfun ("wxl_click" %click) :int (button :uint64))

(defvar *native-loaded* nil)
(defun load-native-library (&optional path)
  "Load PATH, or the bridge in build/, falling back to the system loader."
  (unless *native-loaded*
    (let* ((name #+darwin "libwxlambda.dylib" #+win32 "wxlambda.dll"
                 #-(or darwin win32) "libwxlambda.so")
           (local (asdf:system-relative-pathname "wxlambda" (concatenate 'string "build/" name))))
      (cond (path (cffi:load-foreign-library path))
            ((probe-file local) (cffi:load-foreign-library local))
            (t (cffi:use-foreign-library wxlambda-native)))
      (unless (= 3 (%abi-version)) (error "Unsupported wxlambda native ABI"))
      (setf *native-loaded* t)))
  t)
