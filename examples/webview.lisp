(require :asdf)
(asdf:load-asd (merge-pathnames "../wxlambda.asd" *load-truename*))
(asdf:load-system "wxlambda")
(defpackage #:wxlambda-webview (:use #:cl) (:export #:start #:exercise))
(in-package #:wxlambda-webview)
;; Same page protocol on WKWebView and opt-in experimental Linux WebKitGTK.
;; See docs/WEB-VIEW.md for the pinned Linux build; Windows is not enabled yet.
;; Windows SDK/creation-patch prerequisites are in docs/WEB-VIEW-PORTABILITY.md;
;; applying them alone does not make this example runnable on Windows.
(defparameter *page*
  "<!doctype html><meta charset='utf-8'><title>Development λ</title>
<style>body{font:18px system-ui;background:#172033;color:#e4edf8;padding:24px}button{padding:12px}</style>
<h1>Editor web preview λ</h1><p id='output'>JavaScript is enabled.</p>
<button onclick=\"window.webkit.messageHandlers.wxlambda.postMessage({type:'click',value:document.title})\">Send a result to Lisp</button>
<script>window.webkit.messageHandlers.wxlambda.postMessage({type:'ready',value:'λ 😀'});</script>")
(defun wait-for (predicate)
  ;; Keep native dispatch active while waiting for asynchronous application readiness.
  (unless (funcall predicate)
    (let ((deadline (+ (get-internal-real-time) (* 12 internal-time-units-per-second)))
          (timer nil))
      (unwind-protect
           (progn
             (setf timer (wxlambda:start-ui-timer
               (lambda ()
                 (cond ((funcall predicate) (wxlambda:stop))
                       ((> (get-internal-real-time) deadline) (error "Timed out waiting for WebView")))) 10))
             (wxlambda:run))
        (when timer (wxlambda:cancel-ui-timer timer)))))
  (assert (funcall predicate)))
(defun build-window ()
  (let* ((frame (wxlambda:make-frame :title "Editor web development" :width 900 :height 650))
         (panel (wxlambda:make-panel frame))
         (code (wxlambda:make-text panel :multiline t
                 :value "document.getElementById('output').textContent = 'Changed from Lisp λ'; ({passed:true, count:3})"))
         (run (wxlambda:make-button panel :label "Run JavaScript"))
         (view (wxlambda:make-web-view panel))
         (result (wxlambda:make-text panel :multiline t :editable-p nil)))
    (wxlambda:make-column panel) (wxlambda:add-child panel code :expand t)
    (wxlambda:add-child panel run) (wxlambda:add-child panel view :proportion 1 :expand t)
    (wxlambda:add-child panel result :expand t)
    (wxlambda:on-click run
      (lambda () (wxlambda:defer-ui-action
        (lambda () (wxlambda:run-web-script view (wxlambda:text code))) :owner view)))
    (dolist (kind '(:web-message :web-script-result :web-error))
      (wxlambda:bind-event view kind
        (lambda (event) (setf (wxlambda:text result)
          (format nil "~A request=~D success=~D~%~A"
            (wxlambda:event-kind event) (wxlambda:event-extra-long event)
            (wxlambda:event-integer event) (wxlambda:event-text event))))))
    (wxlambda:set-web-page view *page*) (wxlambda:show-window frame)
    (values frame view)))
(defun exercise ()
  (multiple-value-bind (frame view) (build-window)
    (let ((messages nil) (results nil) (loaded nil))
      (unwind-protect
           (progn
             (wxlambda:bind-event view :web-message (lambda (event) (push event messages)))
             (wxlambda:bind-event view :web-loaded (lambda (event) (declare (ignore event))
               (setf loaded t)))
             (wxlambda:bind-event view :web-script-result (lambda (event) (push event results)))
             (wait-for (lambda () (and loaded messages (string= "Development λ" (wxlambda:web-view-title view)) (not (wxlambda:web-view-busy-p view)))))
             (let ((ticket (wxlambda:run-web-script view "({sum:2+3,unicode:'λ 😀'})")))
               (wait-for (lambda () (find ticket results :key #'wxlambda:event-extra-long)))
               (let ((event (find ticket results :key #'wxlambda:event-extra-long)))
                 (assert (= 1 (wxlambda:event-integer event)))
                 (assert (search "sum" (wxlambda:event-text event)))
                 (assert (search "5" (wxlambda:event-text event)))))
             (assert (search "ready" (wxlambda:event-text (first messages))))
             t)
        (wxlambda:destroy-window frame)))))
(defun start () (wxlambda:with-application () (build-window) (wxlambda:run)))
