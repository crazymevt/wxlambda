;;; Verify generated API coverage against the actual loaded Lisp package.
(require :asdf)
(asdf:load-asd (merge-pathnames "../wxlambda.asd" *load-truename*))
(asdf:load-system "wxlambda")
(asdf:load-asd
 (merge-pathnames "docset.asd"
  (uiop:ensure-directory-pathname
   (or (uiop:getenv "WXLAMBDA_DOCSET_SOURCE")
       (merge-pathnames "../../docset/" (uiop:pathname-directory-pathname *load-truename*))))))
(asdf:load-system "docset")
(let* ((directory (merge-pathnames "../docs/docset/" (uiop:pathname-directory-pathname *load-truename*)))
       (set (docset:open-docset directory))
       (count 0))
  (assert (zerop (docset:check-docset directory)))
  (do-external-symbols (symbol (find-package :wxlambda))
    (incf count)
    (assert (= 1 (length (docset:lookup-symbol (list set) (symbol-name symbol) "WXLAMBDA")))))
  (assert (= count (count "WXLAMBDA" (docset:docset-index set)
                         :key #'docset:record-package :test #'equal)))
  (format t "PASS: actual ~D public exports each have exactly one docset API entry~%" count))
