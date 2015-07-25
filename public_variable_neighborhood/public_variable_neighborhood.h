/**
 * Public Variable Neighborhood
 * =================================
 *
 * This file implements a public variable system. Public variables are variables that can be seen by neighbors.
 * This allows a more mathematical implementation of algorithms as messages would allow, e.g. finding the maximum
 * id: max_id = max{own_id, max{n.max_id | n in Neighborhood}}
 * 
 * Of course we cannot operate on sets but iterators are very close to it (at least much closer as implementing everything
 * with single messages).
 *
 * See the example in ./public_variable_neighborhood_example.c
 *
 *
 * User Functions:
 * ---------------------------
 *  void pvn_setCallbacks(struct PVN* pvn, struct PVN_callbacks callbacks); //Sets callbacks for 'new neighbor'/'neighbor removed'/'neighbor changed' events
 *  void pvn_set_comparison_function(struct PVN* pvn, uint8_t (*cmp)(void*, void*)); //Sets an optimal comparison function to check if neighbor has changed
 *  struct Nbr* pvn_getNbrs(struct PVN* pvn); //Returns first neighbor
 *  struct Nbr* pvn_getNextNbr(struct Nbr* n); //Returns next neighbor or 0
 * 	uint16_t pvn_neighborhood_size(struct PVN* pvn); //Returns size of neighborhood
 * 	void pvn_print_state(struct PVN* pvn); //Prints some info about the PVN (Neighbors, etc.)
 *
 *
 * @author Dominik Krupke, d.krupke@tu-bs.de
 * @year 2015
 * @licence MIT
 */

#ifndef PUBLIC_VARIABLE_NEIGHBORHOOD_H
#define PUBLIC_VARIABLE_NEIGHBORHOOD_H

#include "contiki.h"
#include "net/rime/rime.h"
#include <stdio.h>
#include <stdlib.h>
#include "sys/ctimer.h"

#ifndef CHECK_ALLOCATION
#define CHECK_ALLOCATION(x)	if( (x) == 0 ) { printf("MEMORY ALLOCATION FAILED. EXPECT THE UNEXPECTED!\n"); }
#endif 


/**
 * This structure manages a neighbor. It contains the identifier, the age of this entry (since it has been updated last), the 
 * public variable, and possible some user data.
 */
struct Nbr;
struct Nbr{
	uint16_t id;
	linkaddr_t addr;
	void *public_var;
	struct Nbr* nextNbr;
	unsigned long timestamp;
};

/**
 * This methods are called if the neighborhood or the public variables changes
 */
struct PVN_callbacks {
	void (*onChange)(struct Nbr*);
	void (*onNew)(struct Nbr*);
	void (*onDelete)(struct Nbr*);
};

/**
 * This struct manages the PVN, similiar to e.g. struct broadcast_conn
 * Do not change any values on your own but use the corresponding functions
 */
struct PVN;
struct PVN {
	//Public variable
	void* variable;
	uint8_t size_of_variable;

	//Neighborhood
	uint8_t maximum_age_of_neighbor_information;
	struct Nbr* nbrList;
	uint16_t neighborhood_size;

	//Messaging
	uint16_t port;
	struct broadcast_conn broadcast; 
	uint8_t online;

	//UDFs
	uint8_t (*cmp)(void*, void*);
	struct PVN_callbacks callbacks;

	//Make the neighborhoods to a list for managing them.
	struct PVN* next;
};
//linked list of all open public variable neighborhoods
struct PVN* list_of_all_public_variable_neighborhoods = 0;

void pvn_setCallbacks(struct PVN* pvn, struct PVN_callbacks callbacks)
{
	pvn->callbacks = callbacks;
}

/**
 * Sets the comparison function that gets the old and the new public variable as input and has to decide whether there
 * is a change or not. Some public variables might contain a sequence number that should not be considered.
 * If it is not set, the default memcmp function is used.
 * The size of the two bitfields is obviously the size of the public variable (PVN.size_of_variable).
 *
 * cmp should return !=0 for change
 */
void pvn_set_comparison_function(struct PVN* pvn, uint8_t (*cmp)(void*, void*))
{
	pvn->cmp = cmp;
}

/**
 * Returns the next neighbor. Be careful to not input 0
 */
struct Nbr* pvn_getNextNbr(struct Nbr* n)
{
	return n->nextNbr;
}

/**
 * Use this function to obtain the first neighbor element in the neighbors linked list
 */
struct Nbr* pvn_getNbrs(struct PVN* pvn)
{
	return pvn->nbrList;
}

/**
 * Finds the neighbor entry for a specific id.
 */
struct Nbr* pvn_getNbr(struct PVN* pvn, uint16_t id)
{
	struct Nbr* nbr = pvn_getNbrs(pvn);
	for(; nbr!=0; nbr=pvn_getNextNbr(nbr)) {
		if(nbr->id == id) return nbr;
	}
	return 0;
}


/**
 * Is called if new neighbor information arrive on one of the communication channels.
 * Unfortunately we can not automatically generate one function per neighborhood.
 */
void on_new_neighbor_information(struct broadcast_conn *c, const linkaddr_t *from)
{
	uint16_t id = from->u8[0]<<8 | from->u8[1]; //decode id
	struct PVN* tmp = list_of_all_public_variable_neighborhoods;
	while(tmp!=0) {
		if(&(tmp->broadcast) == c) {
			//find nbr or create it
			if(tmp->nbrList == 0) { //No neighbors yet
				//create entry for this robot with empty data
				tmp->nbrList = (struct Nbr*) calloc(1, sizeof(struct Nbr));
				CHECK_ALLOCATION( tmp->nbrList );
				tmp->nbrList->id = id;
				linkaddr_copy(&(tmp->nbrList->addr), from);
			}
			struct Nbr* nbr = pvn_getNbrs(tmp);
			for(; nbr; nbr=nbr->nextNbr) {
				if(nbr->id == id) { //found
					//update entry
					nbr->timestamp = clock_seconds();
					if(nbr->public_var==0) {//No old public variable
						nbr->public_var = calloc(1, tmp->size_of_variable);
						CHECK_ALLOCATION( nbr->public_var );
						memcpy(nbr->public_var, packetbuf_dataptr(), tmp->size_of_variable);
						if(tmp->callbacks.onNew!=0) {
							(*(tmp->callbacks.onNew))(nbr);
							tmp->neighborhood_size++;
						}
					} else {
						//check if public variable has changed
						if((tmp->cmp == 0 && memcmp(nbr->public_var, packetbuf_dataptr(), tmp->size_of_variable)!=0) 
								|| (tmp->cmp!=0 && (*(tmp->cmp))(nbr->public_var, packetbuf_dataptr())!=0)) {
							//Change happened
							if(tmp->callbacks.onChange!=0) {
								(*(tmp->callbacks.onChange))(nbr);
							}
						}
						memcpy(nbr->public_var, packetbuf_dataptr(), tmp->size_of_variable);
					}
					return;
				} else if(nbr->nextNbr == 0) {//not in list
					//Create an empty entry only with the base informations. Further informations are added in the next for-iteration (which finds this entry)
					nbr->nextNbr = (struct Nbr*) calloc(1, sizeof(struct Nbr));
					CHECK_ALLOCATION( nbr->nextNbr );
					nbr->nextNbr->id = id;
					linkaddr_copy(&(nbr->addr), from);
				}
			}
		}
		tmp=tmp->next;
	}
	printf("ERROR: Received neighbor informations that could not be assigned\n");
}
static struct broadcast_callbacks pvn_broadcast_callbacks = {on_new_neighbor_information};

/**
 * Returns 1 iff it is online, otherwise 0
 */
uint8_t pvn_is_online(struct PVN* pvn)
{
	return pvn->online;
}

/**
 * Switches the networking channel of the PVN on
 */
void pvn_set_online(struct PVN* pvn)
{
	if(pvn->online==0) {
		broadcast_open(&(pvn->broadcast), pvn->port, &pvn_broadcast_callbacks);
		pvn->online = 1;
	}
}

/**
 * Switches the networking channel of the PVN off
 */
void pvn_set_offline(struct PVN* pvn)
{
	if(pvn->online!=0) {
		broadcast_close(&(pvn->broadcast));
		pvn->online = 0;
	}
}

/**
 * Initializes the PVN. Also switches is online.
 * @param pvn 	The PVN to be initialized. Has to be new.
 * @param port 	The broadcast port to be used internally
 * @param variable 	A reference to the public variable
 * @param size 	The size of the public variable (to interpret the variable as a bitfield of this length)
 * @param maxAge 	The age in seconds with which an neighbor entry is outdated.
 *
 */
void pvn_init(struct PVN* pvn, uint16_t port, void* variable, uint8_t size, uint8_t maxAge)
{
	if(pvn->port != 0) printf("WARNING: Possible multiple creation!\n");
	pvn->port = port;
	pvn->variable = variable;
	pvn->size_of_variable = size;
	pvn->maximum_age_of_neighbor_information = maxAge;
	pvn->neighborhood_size = 0;

	//add to list
	if(list_of_all_public_variable_neighborhoods==0) {
		list_of_all_public_variable_neighborhoods = pvn;
	} else {
		struct PVN* tmp = list_of_all_public_variable_neighborhoods;
		while(tmp->next != 0) {
			tmp = tmp->next;
		}
		tmp->next = pvn;
	}
	pvn_set_online(pvn);
}	

/**
 * Broadcasts the own state.
 * Why isn't this done automatically? Because the broadcast is strongly connected to the neighborhood iteration. Also you
 * gain more power over the frequency and it saves an extra thread.
 */
void pvn_broadcast(struct PVN* pvn)
{
	//open the channel temporarily if closed
	if(pvn->online==0) {
		broadcast_open(&(pvn->broadcast), pvn->port, &pvn_broadcast_callbacks);
	}

	//Send
	if(pvn->variable!=0) {
		packetbuf_copyfrom(pvn->variable, pvn->size_of_variable);
		broadcast_send(&(pvn->broadcast));
	}

	//close channel again if pvn is offline
	if(pvn->online==0) {
		broadcast_close(&(pvn->broadcast));
	}
}


/**
 * Artificially increases the age of a neighbor such that it will outdate sooner. 
 * Can be used to punish missing acknowledgements.
 */
void pvn_increaseNbrAge(struct Nbr* n, uint8_t seconds)
{
	if(seconds>n->timestamp) n->timestamp = 0;
	else n->timestamp -= seconds;
}




/**
 * Goes through the neighborhood entries and removes all entries that are above the maximum age of neighbor informations.
 * Has to be called frequently
 */
void pvn_remove_old_neighbor_information(struct PVN* pvn)
{
	unsigned long oldest_timestamp_allowed = clock_seconds()-pvn->maximum_age_of_neighbor_information;
	if(clock_seconds()<=pvn->maximum_age_of_neighbor_information) oldest_timestamp_allowed = 0;

	struct Nbr* nbr = pvn_getNbrs(pvn);
	//remove the outdated entries in the beginning such that the list is either empty or the first entry is not outdated
	while(nbr!=0) {
		if(nbr->timestamp<oldest_timestamp_allowed) {
			//delete nbr
			if(pvn->callbacks.onDelete!=0) {
				(*(pvn->callbacks.onDelete))(nbr);//Notify
				pvn->neighborhood_size--;
			}
			pvn->nbrList = pvn_getNextNbr(nbr);
			free(nbr->public_var);
			free(nbr);
			nbr = pvn_getNbrs(pvn);
		} else {
			break;
		}
	}
	//remove the outdated entries further behind
	for(; nbr!=0 && pvn_getNextNbr(nbr)!=0; nbr= pvn_getNextNbr(nbr)) {
		if(nbr->nextNbr->timestamp<oldest_timestamp_allowed) {
			struct Nbr* tmp = nbr->nextNbr;
			if(pvn->callbacks.onDelete!=0) {
				(*(pvn->callbacks.onDelete))(tmp);//Notify
				pvn->neighborhood_size--;
			}
			nbr->nextNbr = nbr->nextNbr->nextNbr;
			free(tmp->public_var);
			free(tmp);
		}
	}
	return;
}

/**
 * Frees all connection and memory of the pvn (except the memory of the PVN struct).
 */
void pvn_destroy(struct PVN* pvn)
{
	//close network
	pvn_set_offline(pvn);

	//find entry in list and remove it
	if(list_of_all_public_variable_neighborhoods == pvn) { 
		list_of_all_public_variable_neighborhoods = pvn->next; 
	} else {
		struct PVN* tmp = list_of_all_public_variable_neighborhoods;
		while(tmp->next != pvn) {
			tmp = tmp->next;
		} 
		tmp->next = pvn->next;
	}

	//free all neighbor entries
	pvn->maximum_age_of_neighbor_information = 0;
	pvn_remove_old_neighbor_information(pvn);
}

/**
 * Returns the size of the neighborhood
 */
uint16_t pvn_neighborhood_size(struct PVN* pvn)
{
	return pvn->neighborhood_size;	
}

/**
 * Prints some informations to the serial port for debugging.
 */
void pvn_print_state(struct PVN* pvn)
{
	printf("PVN-Info: SIZE=%u, {", pvn_neighborhood_size(pvn));
	struct Nbr* nbr = pvn_getNbrs(pvn);
	for(; nbr!=0; nbr=pvn_getNextNbr(nbr)) {
		printf("[ID=%u](age=%u)", nbr->id,  (unsigned int)(clock_seconds() - nbr->timestamp));
		if(pvn_getNextNbr(nbr) != 0) {
			printf(", ");
		}
	}
	if(pvn->online == 0){
	printf(", offline\n");
	} else {
	printf(", online\n");
	}
}


#endif
