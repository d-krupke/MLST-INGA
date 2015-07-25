/**
 * An example for the usage of the public variable neighborhood (./public_variable_neighborhood.h). 
 * It calculates the maximum ID of all the nodes in the overall network.
 * If the node with the maximum ID vanishes from the network, the value is not reset.
 *
 * @author Dominik Krupke, d.krupke@tu-bs.de
 */

#include "contiki.h"
#include "net/rime/rime.h"
#include <stdio.h>
#include <stdlib.h>
#include "public_variable_neighborhood.h"
#include "lib/random.h"

//The public variable
struct maxIdPubVar{
	uint16_t maxId;
};
struct maxIdPubVar pv;
struct PVN pvn;

//--Callbacks--
// These functions are called when something in the public variable neighborhood changes. Set later.
void onNew(struct Nbr* n){ printf("NEW %d\n", n->id); }
void onChange(struct Nbr* n){ printf("CHANGE %d\n", n->id); }
void onDelete(struct Nbr* n){ printf("DELETE %d\n", n->id); }

struct PVN_callbacks cbs = {onNew, onChange, onDelete};


/*---------------------------------------------------------------------------*/
PROCESS(example_pvn_process, "PVN example");
AUTOSTART_PROCESSES(&example_pvn_process);
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(example_pvn_process, ev, data)
{
	static struct etimer et;

		PROCESS_BEGIN();

	//Set up PVN
	pvn_init(&pvn, 123, &pv, sizeof(pv), 10); //Public Variable Neighborhood Struct, Port, own public variable, size of it, max age of neighborhood entries in seconds
	pvn_setCallbacks(&pvn, cbs); //Set the above callbacks

	pv.maxId = (linkaddr_node_addr.u8[0]<<8) | linkaddr_node_addr.u8[1]; //set max id to own id
	random_init(pv.maxId); //init the random generator for random sleep time (important in Cooja)

	while(1) {
		//Sleep random time (0,2) seconds
		etimer_set(&et, 2*CLOCK_SECOND*((float)random_rand()/RANDOM_RAND_MAX));
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		//Remove outdated entries
		pvn_remove_old_neighbor_information(&pvn);

		//Iterating neighbors
		struct Nbr* nbr = pvn_getNbrs(&pvn);
		for(; nbr!=0; nbr=pvn_getNextNbr(nbr)){
			struct maxIdPubVar* n_pv = (struct maxIdPubVar*)(nbr->public_var); 
			if(n_pv->maxId > pv.maxId) pv.maxId = n_pv->maxId;
			printf("Nbr %d: MaxId:%d\n", nbr->id, n_pv->maxId);
		}

		//broadcast new state
		pvn_broadcast(&pvn);
		
		//some output
		printf("MAX ID: %d\n", pv.maxId);
	}

	PROCESS_END();
}
/*---------------------------------------------------------------------------*/

