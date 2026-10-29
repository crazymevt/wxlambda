(load (merge-pathnames "../examples/tree-drag.lisp" *load-truename*))
(wxlambda:with-application () (assert (wxlambda-tree-drag:exercise)))
(format t "Tree drag validation checks passed.~%")
