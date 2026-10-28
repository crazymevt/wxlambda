(load (merge-pathnames "../examples/clipboard.lisp" *load-truename*))
(load (merge-pathnames "../examples/document-form.lisp" *load-truename*))

(defun with-test-definitions (definitions function)
  (let ((originals (mapcar (lambda (entry) (list (first entry) (fdefinition (first entry)))) definitions)))
    (unwind-protect
         (progn (dolist (entry definitions) (setf (fdefinition (first entry)) (second entry)))
                (funcall function))
      (dolist (entry originals) (setf (fdefinition (first entry)) (second entry))))))
(defun clipboard-expect-error (function &optional (type 'error))
  (assert (handler-case (progn (funcall function) nil) (error (condition) (typep condition type)))))

(dolist (function (list #'wxlambda:clipboard-text #'wxlambda:clipboard-text-available-p #'wxlambda:flush-clipboard
                       (lambda () (setf (wxlambda:clipboard-text) "inactive"))))
  (clipboard-expect-error function 'wxlambda:native-error))

(wxlambda:with-application ()
  ;; Only the availability query touches the real clipboard. Never read/write its content.
  (assert (wxlambda-clipboard:exercise))
  (let ((frame (wxlambda:make-frame)))
    (unwind-protect
         (progn
           ;; Keep roots present so CHECK-CALLBACK-ERROR performs native cleanup queries.
           (wxlambda:bind-event frame :click (lambda (event) (declare (ignore event))) :user-data (list :root))
           (cffi:with-foreign-string ((pointer bytes) "λ 日本語 😀" :encoding :utf-8)
             (with-test-definitions
                 (list (list 'wxlambda::%clipboard-text
                             (lambda (length available)
                               (setf (cffi:mem-ref length :size) (1- bytes)
                                     (cffi:mem-ref available :int) 1)
                               pointer)))
               (lambda ()
                 (let ((result (wxlambda:clipboard-text)))
                   (assert (string= result "λ 日本語 😀"))
                   (setf (cffi:mem-aref pointer :unsigned-char 0) 65)
                   (assert (string= result "λ 日本語 😀"))))))
           (cffi:with-foreign-string (pointer "")
             (dolist (present '(0 1))
               (with-test-definitions
                   (list (list 'wxlambda::%clipboard-text
                               (lambda (length available)
                                 (setf (cffi:mem-ref length :size) 0 (cffi:mem-ref available :int) present)
                                 pointer)))
                 (lambda () (assert (equal (if (zerop present) nil "") (wxlambda:clipboard-text)))))))
           (let ((value (copy-seq "Copied λ")) (calls 0))
             (with-test-definitions
                 (list (list 'wxlambda::%set-clipboard-text
                             (lambda (pointer length)
                               (incf calls)
                               (assert (string= value (cffi:foreign-string-to-lisp pointer :count length :encoding :utf-8)))
                               0)))
               (lambda ()
                 (assert (eq value (setf (wxlambda:clipboard-text) value)))
                 (clipboard-expect-error (lambda () (setf (wxlambda:clipboard-text) 42)))
                 (clipboard-expect-error (lambda () (setf (wxlambda:clipboard-text) (string (code-char 0)))))
                 (assert (= calls 1)))))
           ;; Native failure text survives native root-reaping queries.
           (flet ((failure ()
                    (cffi:with-foreign-object (pointer :char 1)
                      (setf (cffi:mem-ref pointer :char) 0)
                      (wxlambda::%set-clipboard-text pointer 1))))
             (dolist (name '(wxlambda::%clipboard-text-available wxlambda::%flush-clipboard))
               (with-test-definitions (list (list name #'failure))
                 (lambda ()
                   (let ((condition
                           (handler-case (progn (if (eq name 'wxlambda::%flush-clipboard)
                                                    (wxlambda:flush-clipboard) (wxlambda:clipboard-text-available-p)) nil)
                             (wxlambda:native-error (condition) condition))))
                     (assert condition)
                     (assert (search "NUL" (princ-to-string condition))))))))
           (dolist (result '(0 1))
             (with-test-definitions
                 (list (list 'wxlambda::%clipboard-text-available (lambda () result))
                       (list 'wxlambda::%flush-clipboard (lambda () result)))
               (lambda ()
                 (assert (eq (plusp result) (wxlambda:clipboard-text-available-p)))
                 (assert (eq (plusp result) (wxlambda:flush-clipboard))))))
           (let ((primary (make-condition 'simple-error :format-control "clipboard callback")))
             (with-test-definitions
                 (list (list 'wxlambda::%flush-clipboard
                             (lambda () (setf wxlambda::*callback-error* primary) -1)))
               (lambda ()
                 (assert (eq primary (handler-case (wxlambda:flush-clipboard) (error (condition) condition))))))))
      (wxlambda:destroy-window frame)
      (wxlambda:process-events)))
  ;; Exercise document Copy/Paste without replacing any system clipboard formats.
  (let ((copied nil) (incoming nil) (errors nil) (form nil))
    (with-test-definitions
        (list (list '(setf wxlambda:clipboard-text) (lambda (value) (setf copied value)))
              (list 'wxlambda:clipboard-text (lambda () incoming))
              (list 'wxlambda:flush-clipboard (lambda () nil)))
      (lambda ()
        (setf form (wxlambda-document-form::open-form
                    :report-error (lambda (form condition) (declare (ignore form)) (push condition errors))))
        (unwind-protect
             (progn
               (assert (wxlambda-document-form::perform-command form :copy-name))
               (assert (string= copied "Ada"))
               (assert (not (wxlambda-document-form::dirty-p form)))
               (assert (not (wxlambda-document-form::perform-command form :paste-name)))
               (setf incoming "Grace λ")
               (assert (wxlambda-document-form::perform-command form :paste-name))
               (assert (wxlambda-document-form::dirty-p form))
               (setf incoming (format nil "bad~%name"))
               (assert (not (wxlambda-document-form::perform-command form :paste-name)))
               (assert (= 1 (length errors)))
               (assert (string= "Grace λ" (wxlambda:text (wxlambda-document-form::document-form-name form))))
               (setf incoming "")
               (assert (wxlambda-document-form::perform-command form :paste-name))
               (assert (string= "" (wxlambda:text (wxlambda-document-form::document-form-name form)))))
          (wxlambda-document-form::dispose-form form))))))
(format t "PASS: clipboard wrapper protocol, error/callback containment and form Copy/Paste; system contents untouched~%")
