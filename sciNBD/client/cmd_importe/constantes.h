#ifndef _constantes_h
#define _constantes_h

#define RUNNING_DIR				"/tmp"
#define LOCK_FILE					"smd_exportd.lock"
#define LOG_FILE					"smd_exportd.log"
#define DEVICE_EXPORT 		"smd_exportd.export" // ce fichier contient la liste des devices export�s par cette brique
#define SMDD2EXPORTE 			"/dev/smdd2exporte" // tube nomm� pour transfert messages smdd -> smd_exporte
#define EXPORTE2SMDD 			"/dev/exporte2smdd" // tube nomm� pour transfert message smdd <- smd_exporte
#define NB_MAX_EXPORTED_DEVICES 16 // nombre max de devices export�s par une brique
#define BUFFER_MAX_SIZE 	16*1024	// ces 2 valeurs d�signent..
#define LINE_MAX_SIZE 		256	// ..des tailles max de cha�nes de car pour le parsing des msgs entre les processus
#define SOCKET_PORT 			0x1234
#define OK 								0
#define ERROR 						-1
#define PROC_SCIMAPDEV		"/proc/scimapdev/ndevs_enregistres"


// constantes pour SCI
#define CV_NODE_ID 16 // n� SCI du controleur virtualiseur
#define B_MEM_CTL_ID 15 // n� indentifiant le n� du "segment pour contr�le" sur la brique
#define B_MEM_CTL_SIZE 128*4096 // octets
#define B_MEM_DATA_ID 16 // n� indentifiant le n� du "segment pour les donn�es" sur la brique
#define B_MEM_DATA_SIZE 4096 // octets
#define ADAPTER_NO 0 // n� de l'adapter SCI utilis� en local et en remote (meme num�ro dans mon programme)
#define INT_DEB_ECR_NO 12345 // no de l'interruption pour un d�but de lecture
#define INT_FIN_LEC_NO 12346 // no de l'interruption pour une fin d'�criture
#define NO_FLAGS 0 
#define NO_ARG 0
#define NO_CALLBACK 0

#endif
