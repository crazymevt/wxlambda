(in-package #:wxlambda)
(cffi:defcstruct native-canvas-item
  (kind :int) (x :int) (y :int) (width :int) (height :int)
  (rgb :uint32) (option :int) (text :pointer) (text-length :size) (pixels :pointer) (pixel-length :size))
(cffi:defcfun ("wxl_drawing_canvas" %drawing-canvas) :uint64 (parent :uint64))
(cffi:defcfun ("wxl_set_canvas_content" %set-canvas-content) :int (canvas :uint64) (background :uint32) (items :pointer) (count :size))
(cffi:defcfun ("wxl_canvas_client_size" %canvas-client-size) :int (canvas :uint64) (width :pointer) (height :pointer))
(cffi:defcfun ("wxl_save_canvas_image" %save-canvas-image) :int (canvas :uint64) (path :pointer) (length :size) (width :int) (height :int))
