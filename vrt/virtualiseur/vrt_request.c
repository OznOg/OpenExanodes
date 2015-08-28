/*!\file vrt_request.c
\brief définit la fonction make_request qui gère les lectures et les écritures faites sur notre BDD

Le virtualiseur n'utilise pas de queue car il n'a pas besoin du clustering : il ne fait que rediriger les 
requêtes d'E/S faites sur les zones (VOLUMES LOGIQUES == ZONES), vers les rdevs (qui contiennent effectivement les données). 
Donc pas de fonction request (comme c'est le cas pour le scimapdev) mais une fonction make_request.
*/
#include <linux/blk.h>
#include "virtualiseur.h"
#include <linux/time.h>
#include "vrt_layout.h"


#ifdef PERF_STATS
static double somme_durees_aiguillages=0.0;
static long nb_aiguillages=0;
#endif




//! fonction qui transpose les requêtes sur les zones en requêtes sur les rdev
/*!
	on modifie le descripteur du buffer head qui décrit la requête, 
	on change :
		- le rdev accédé (ce n'est plus une zone mais un rdev)
		- la position de la requête sur ce rdev (offset)
	
	on envoie la requête, puis on attend qu'elle se termine
	et on vérifie si la requête a été correctement traitée
*/
void aiguiller_req_sur_rdev(int cmd, 
			    struct buffer_head *bh,  
			    storzone_t *z) {
  realdev_t *rd;
  unsigned long sec_rd; // normalement, c'est u64 quand le noyau linux sera 64 bits

  PDEBUG("cmd = %d : tzone = %s, sect = %ld \n", 
	 cmd, 
	 z->name,
	 bh->b_rsector);
  
  // appel à la fonction d'aiguillage (pointeur de fonction
  // car l'aiguillage dépend de la politique de placement - layout -
  // utilisée pour le groupe de devices)
  (*(z->g->zone2rdev))(z, bh->b_rsector, &rd, &sec_rd);
  
  bh->b_rdev = MKDEV(rd->major, rd->minor); 
  bh->b_rsector = (u32) sec_rd;

  PDEBUG("routed to rdev = %s [%d,%d] sect = %ld \n", 
	 rd->name,
	 rd->major, rd->minor,
	 bh->b_rsector);
}

int vrt_make_request(request_queue_t *q, int rw, struct buffer_head *bh) {
  
  storzone_t *target_zone;
  u8 minor; 

#ifdef PERF_STATS
  struct timeval debut_aiguillage,fin_aiguillage; // stats
#endif 
  
  // pour les stats
#ifdef PERF_STATS
  do_gettimeofday(&debut_aiguillage);
#endif
  vs.request_nb++;
  
  // on extrait le numéro de la zone concernée par cette requête	
  minor = DEVICE_NR(bh->b_rdev);
  target_zone = vs.minor2zone[minor];
  if (target_zone == NULL) {
    PDEBUG("bad minor number %d (inactive or nonexistent zone)\n",
	   minor);
    return 0;
  }
  
  aiguiller_req_sur_rdev(rw, bh, target_zone);

  // une fois le bh modifié pour qu'il indique le rdev qu'il faut accéder pour
  // effecter l'I/O : on sort du traitement de la requête en retournant 1 =>
  // le kernel va retraiter cette requête mais cette fois, ce sera le rdev qui
  // la recevra et non la zone.
  return 1;
}
