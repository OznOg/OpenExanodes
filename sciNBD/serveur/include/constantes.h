#ifndef _constantes_h
#define _constantes_h

#define SMDEXPORT_VER     "1.14"
#define RUNNING_DIR	  "/tmp"
#define EXPORTD_LOCKFILE  "/var/lock/smd_exportd.lock"
#define DEVICE_EXPORT 	  "smd_exportd.export" // ce fichier contient la liste des dev exportés par cette brique
#define SMDD2EXPORTE 	  "/dev/smdd2exporte"  // tube nommé pour transfert messages exportd -> exporte
#define EXPORTE2SMDD 	  "/dev/exporte2smdd"  // tube nommé pour transfert messages exportd <- exporte
#define NEWDEVICE2TR      "/dev/newdevice2tr"  // tube nommé pour que exportd indique le dev à prendre en charge
#define NBMAX_EDEVS       16                   // nombre max de devices exportés par ce serveur
#define MAXSIZE_BUFFER 	  16*1024	       
#define MAXSIZE_LINE 	  256	               
#define MAXSIZE_DEVNAME   16	               //! taille max des nom des devices
#define SOCKET_PORT 	  0x1234
#define SUCCESS           0
#define ERROR             1

#define NO_FLAGS          0 

#define TRUE              1
#define FALSE             0

#ifndef NULL
#define NULL              0
#endif

// signaux pour communiquer avec serverd
#define SIGNEWIMPORT      (SIGRTMAX-3)
#define SIGENDIMPORT      (SIGRTMAX-4)
#define SIGNEWDEVICE      (SIGRTMAX-5)
#define SIGENDNEWDEV      (SIGRTMAX-6)

#endif
