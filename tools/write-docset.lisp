;;; Build/check through the same library Cadre uses; no GUI or Quicklisp needed.
(require :asdf)
(asdf:load-asd (merge-pathnames "docset.asd"
                      (uiop:ensure-directory-pathname (uiop:getenv "WXLAMBDA_DOCSET_SOURCE"))))
(asdf:load-system "docset")

(defun read-input (name)
  (with-open-file (stream (merge-pathnames name (uiop:ensure-directory-pathname
                                                (uiop:getenv "WXLAMBDA_DOCSET_INPUT")))
                         :external-format :utf-8)
    (let ((*read-eval* nil)) (read stream))))

(let* ((directory (uiop:ensure-directory-pathname (uiop:getenv "WXLAMBDA_DOCSET_OUTPUT")))
       (manifest (read-input "manifest.sexp"))
       (entries (read-input "entries.sexp")))
  (when (string= (uiop:getenv "WXLAMBDA_DOCSET_MODE") "build")
    (docset:write-docset directory manifest entries))
  (assert (zerop (docset:check-docset directory)))
  (let* ((sets (docset:find-docsets :directories (list (uiop:pathname-parent-directory-pathname directory))
                                    :systems nil :user nil))
         (set (find "wxlambda" sets :key #'docset:docset-id :test #'string=)))
    (assert set)
    (loop for (key value) on manifest by #'cddr
          do (assert (equal value (getf (docset:docset-manifest set) key))))
    (assert (= 1 (getf (docset:docset-manifest set) :format)))
    (assert (= (length entries) (getf (docset:docset-manifest set) :entry-count)))
    (assert (= (length entries) (length (docset:docset-index set))))
    (assert (equal '("WXLAMBDA") (docset:docset-packages set)))
    (dolist (entry entries)
      ;; Check every API lookup and every guide/example is read at its indexed offset.
      (let* ((id (second entry))
             (record (docset:entry-record set id))
             (actual (docset:docset-entry set id)))
        (assert record)
        (loop for (key reader) on
              (list :names #'docset:record-names :kind #'docset:record-kind
                    :kind-name #'docset:record-kind-name :package #'docset:record-package
                    :parent #'docset:record-parent :summary #'docset:record-summary)
                by #'cddr
              do (assert (equal (getf (cddr entry) key) (funcall reader record))))
        (assert (docset:search-names set (first (getf (cddr entry) :names))))
        (assert (equal entry actual))
        (when (getf (cddr entry) :package)
          (dolist (name (getf (cddr entry) :names))
            (let ((hits (docset:lookup-symbol (list set) name "WXLAMBDA")))
              (assert (= 1 (length hits)))
              (assert (equal id (docset:record-id (cdar hits)))))))))
    (assert (= 3 (length (docset:docset-toc set))))
    (format t "PASS: ~D entries; zero checker problems; discovery, symbol lookup and guide/example reading~%"
            (length entries))))
