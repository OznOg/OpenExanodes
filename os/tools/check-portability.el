;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; check-portability.el - Check that the current buffer is portable
;;
;; Usage: Put this script in your Emacs script directory and add
;;        the following line to your .emacs:
;;
;;           (require 'check-portability)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; FIXME This should not assume that check_portability is in the PATH.
;;       Need to write a function determining the toplevel dir of the
;;       current Exanodes tree.

(defun check-portability ()
  (interactive)
  (let ((saved-compile-cmd compile-command))
    (compile (concat "check_portability -c " buffer-file-name))
    (setq compile-command saved-compile-cmd)))

(provide 'check-portability)
