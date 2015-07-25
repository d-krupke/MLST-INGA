/**
 * Maximum Leaf Spanning Tree
 * ================================
 *
 * This header implements the MLST algorithm and sleeping of leaves. It uses the public variable neighborhood for the MLST
 * determination and rsunicast for messaging.
 * 
 *
 * Maximum Leaf Spanning Tree
 * --------------------------------
 * The MLST is the Maximum Leaf Spanning Tree, thus the spanning tree with a maximum amount of leaves. The MLST-Problem is 
 * unfortunately NP-hard but we are able to get quite good results with the used heuristic of Habibi and McLurkin.
 * The leaves are not part of the communication backbone to the root and thus can be switched of if they have nothing to say.
 * A large amount of leaves thus means, that a lot of nodes can switch offline and save a lot of energy.
 * 
 * Dynamic and Self Stabilizing
 * --------------------------------------
 * Also if the leaves go offline, they go online after some time to check if the network has changed. This implementation is
 * self stabilizing and can be used in dynamic networks. Of course the MLST is of no use in quickly changing networks as
 * the leaves will only go offline if the network has stabilized. The network thus should be stable for most of the time.
 *
 * Messaging
 * -------------------------------------
 * Messages are 'reliable' sent to the root/sink of the tree. The root needs special code that is activated by `#define ROOT'.
 * The messages are copied into a queue and some attempts to sent it to the parent are made. The MLST has not to be defined
 * during the call. The amount of attempts made can be defined in "./rsunicast.h".
 *
 *
 * User Functions:
 * ------------------------------------
 * void mlst_init(); //Initializes the MLST. Has to be called before the MLST is used.
 * void mlst_send(void *msg, uint16_t size); //Sends a message to the root. No guarantee but there a local acknowledgements for each hop.
 * void mlst_print_state(); //Prints the MLST state for debugging.
 * uint8_t mlst_is_undefined(); // 1 iff the MLST is not defined. Possibly there is no need for reading sensors as long as the MLST is not established.
 *
 *
 * @author Dominik Krupke, d.krupke@tu-bs.de
 * @year 2015
 * @licence MIT
 */


#ifndef MLST_NETWORK_H
#define MLST_NETWORK_H

#include "contiki.h"
#include "net/rime/rime.h"
#include <stdio.h>
#include <stdlib.h>
#include "lib/random.h"
#include "./public_variable_neighborhood/public_variable_neighborhood.h"
#include "leds.h"
#include "./rsunicast/rsunicast.h"
#include "./auxiliary.h"

//The Port for the public variable system
#define MLST_PVN_PORT 154
//After this time without refreshment neighbor-entries are deleted
#define MAX_AGE_OF_MLST_NBR_IN_SECONDS 15
//The length of a period in the calculation. In each period a while-loop with the MLST Calculation is executed as well as the state broadcasted. It will be randomized a little.
#define MLST_PERIOD_LENGTH_IN_SECONDS 1
//If there has been a change, the node is not going to sleep even if it is a leaf for this amount of periods
#define IF_CHANGE_STAY_ACTIVE_FOR_N_PERIODS 3
//the maximal age of the parent neighbor entry in seconds. It will stay awake if it is too old until the entry is updated.
#define MAX_AGE_OF_PARENT 5

//Do not change. Used internally
#define WAIT_ONE_PERIOD etimer_set(&mlst_period_timer, MLST_PERIOD_LENGTH_IN_SECONDS*CLOCK_SECOND*getRandomFloat(0.8,1.0)/divide_period_time_by); PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&mlst_period_timer));
#define RIME_ID ((linkaddr_node_addr.u8[0]<<8) | linkaddr_node_addr.u8[1])

//**Variables**
struct Nbr* mlst_parent = 0; //The neighbor entry for the parent
uint8_t mlst_stay_active_for_next_n_periods = 0; //Stay active for some rounds even if leaf if there is action (see also #IF_CHANGE_STAY_ACTIVE_FOR_N_PERIODS)
uint8_t divide_period_time_by = 1; //Used to shorten the period length during busy phases (for a quick convergence)
struct PVN mlst_pvn; //The public variable neighborhood system
struct etimer mlst_period_timer; //Timer used for the period delays
//--Variables--



//***********************************************************************************
// Public Variable
//***********************************************************************************

//This is the data field used as variable for the public variable neighborhood
struct mlst_public_variable{
	uint8_t distance_to_root;
	uint16_t parent_id;
	uint8_t children_count;
};
struct mlst_public_variable own_mlst_public_variable;//The own public variable

//Called if the public variable of a neighbor changes
static void onPvnChange(struct Nbr* n)
{
#ifdef DEBUG
	printf("CHANGE %d\n", n->id);
#endif
	mlst_stay_active_for_next_n_periods = IF_CHANGE_STAY_ACTIVE_FOR_N_PERIODS;
}

//called if there is a new neighbor
static void onPvnNew(struct Nbr* n)
{
#ifdef DEBUG 
	printf("NEW %d\n", n->id);
#endif
	mlst_stay_active_for_next_n_periods = IF_CHANGE_STAY_ACTIVE_FOR_N_PERIODS;
}

//called if a neighbor is removed
static void onPvnDelete(struct Nbr* n)
{
#ifdef DEBUG
	printf("DELETE %d\n", n->id);
#endif
	mlst_stay_active_for_next_n_periods = IF_CHANGE_STAY_ACTIVE_FOR_N_PERIODS;
	if(n == mlst_parent){ //If parent is deleted, reset state
		mlst_parent = 0;
		own_mlst_public_variable.parent_id = 0;
		own_mlst_public_variable.distance_to_root = 0xff;
		own_mlst_public_variable.children_count = 0;
	}
}

//used by PVN to check if the public variable has changed
static uint8_t pvnCmp(void* a, void* b)
{
	struct mlst_public_variable* av = (struct mlst_public_variable*) a;
	struct mlst_public_variable* bv = (struct mlst_public_variable*) b;
	if(av->parent_id!=bv->parent_id || av->children_count != bv->children_count) return 1;
	return 0;
}

struct PVN_callbacks mlst_pvn_callbacks = {onPvnChange, onPvnNew, onPvnDelete}; //The callbacks for the public variable neighborhood. Called on specific changes
//--Public Variable-----------------------------------------------------------------------



/**
 * Sends a message to the sink of the MLST using multiple hops. Can also be used if the parent is not determined yet.
 * The message is copied and put into a message queue.
 */
void mlst_send(void *msg, uint16_t size){
	rsunicast_send(msg, size);
}

/**
 * Returns 1 iff the parent is not determined yet
 **/
uint8_t mlst_is_undefined(){
	return mlst_parent == 0 || own_mlst_public_variable.parent_id == 0;
}

//is called when the node is not allowed to sleep
static void mlst_online(){
	pvn_set_online(&mlst_pvn);
	leds_off(LEDS_GREEN);
}

//is called when the node is allowed to sleep
static void mlst_offline(){
	pvn_set_offline(&mlst_pvn);
	leds_on(LEDS_GREEN);
}

//Return 1 iff this node is a leaf in this iteration of the MLST algorithm.
static uint8_t mlst_is_leaf(){
	if(mlst_is_undefined() != 0){
		return 0;
	}
	return own_mlst_public_variable.children_count==0;
}


//*****************************************************************
// MLST Calculation
//*****************************************************************

//Here is a feedback loop round of the MLST algorithm
static void mlst_recalculate(){
#ifdef ROOT
	own_mlst_public_variable.distance_to_root = 0;
	own_mlst_public_variable.parent_id = 0xffff;
	own_mlst_public_variable.children_count = 0xff;
#else 
	uint8_t children_count = 0;
	uint8_t distance_to_root = 0xff;
	uint8_t number_of_potential_parents = 0;

	struct Nbr* best_parent = 0;
	struct mlst_public_variable* best_parent_pv = 0;

	//Iterating neighbors
	struct Nbr* n = pvn_getNbrs(&mlst_pvn);
	for(; n!=0; n=pvn_getNextNbr(n)){
		struct mlst_public_variable* n_pv = (struct mlst_public_variable*)(n->public_var);
		if(n_pv->parent_id == 0){//Neighbor has undefined state;
			mlst_stay_active_for_next_n_periods = IF_CHANGE_STAY_ACTIVE_FOR_N_PERIODS;
			children_count++;
			continue; 
		} else {
			if( n_pv->parent_id == (RIME_ID) ){ //is defined child
				children_count++;
				continue;
			} else { //potential parent
				if(n_pv->distance_to_root+1 < distance_to_root){//closer than current best parent
					distance_to_root = n_pv->distance_to_root+1;
					number_of_potential_parents = 1;
					best_parent = n;
					best_parent_pv = n_pv;
				} else if(n_pv->distance_to_root+1 == distance_to_root){
					if(best_parent_pv->children_count < n_pv->children_count){//more children than current best parent
						number_of_potential_parents = 1;
						best_parent = n;
						best_parent_pv = n_pv;
					} else if(best_parent_pv->children_count == n_pv->children_count){//has same values as current best parent
						number_of_potential_parents++;
						if(best_parent->id > n->id){ //If multiple choices, choose parent with lowest id
							best_parent = n;
							best_parent_pv = n_pv;
						}
					}
				}
			}
		}
	}

	//set state
	if(best_parent!=0){
		//if parent is not unique, you may want to wait one round
		if(number_of_potential_parents>1 && random_rand()<0.5*RANDOM_RAND_MAX){
			//stay undefined
#ifdef DEBUG
			printf("CANNOT DECIDE\n");
#endif
			own_mlst_public_variable.parent_id = 0;
			own_mlst_public_variable.distance_to_root = 0xff;
			own_mlst_public_variable.children_count = children_count;
		} else {
			//check if something has changed and you should stay online for some rounds
			if(own_mlst_public_variable.parent_id==0 ||
					own_mlst_public_variable.parent_id!=best_parent->id ||
					own_mlst_public_variable.distance_to_root!=distance_to_root ||
					own_mlst_public_variable.children_count != children_count
			  ){

				mlst_stay_active_for_next_n_periods = IF_CHANGE_STAY_ACTIVE_FOR_N_PERIODS;
				divide_period_time_by = 3;
			}

			//set new state
			own_mlst_public_variable.parent_id = best_parent->id;
			own_mlst_public_variable.distance_to_root = distance_to_root;
			own_mlst_public_variable.children_count = children_count;
			mlst_parent = best_parent;
		}
	} else {//undefined
		own_mlst_public_variable.parent_id = 0;
		own_mlst_public_variable.distance_to_root = 0xff;
		own_mlst_public_variable.children_count = children_count;
	}

#endif
}

//--MLST_CALCULATION-----------------------------------------------------------------




//*****************************************************************************
// THREAD 
//*****************************************************************************

/**
 * The MLST has its own thread that runs in the background and updates the MLST. It automatically switches off leave nodes
 * and switches them on again after some time. An alternative implementation via ctimer would be possible but possibly harder
 * to read and debug.
 */
PROCESS(mlst_process, "MLST Process");
PROCESS_THREAD(mlst_process, ev, data)
{

	PROCESS_BEGIN();
	leds_init();

	while(1) {
		//Clean up the neighborhood data (e.g. remove outdated neighbor entries)
		pvn_remove_old_neighbor_information(&mlst_pvn);

		//INIT - Get defined
		if(mlst_is_undefined()!=0){	
			mlst_online();
			rsunicast_disallowSleeping();
			WAIT_ONE_PERIOD;
			mlst_recalculate();
		} else {

			if(mlst_is_leaf()>0){
				rsunicast_allowSleeping();
				//is allowed to sleep
				if(mlst_stay_active_for_next_n_periods>0 || clock_seconds()-mlst_parent->timestamp > MAX_AGE_OF_PARENT){
					//stay awake to fetch some news before sleeping again
					mlst_online();
					WAIT_ONE_PERIOD;
					mlst_recalculate();
				} else {
					//Sleep for one period
					mlst_offline();
					WAIT_ONE_PERIOD;
					mlst_recalculate();
				}
			} else {
				//is backbone and has to stay online
				mlst_online(); 
				rsunicast_disallowSleeping();
				WAIT_ONE_PERIOD;
				mlst_recalculate();
			}
		}
			
		//set parent in messaging
		rsunicast_setparent(own_mlst_public_variable.parent_id);

		pvn_broadcast(&mlst_pvn);
		if(mlst_stay_active_for_next_n_periods>0){ 
			mlst_stay_active_for_next_n_periods--;
		}
		if(divide_period_time_by>1){
			divide_period_time_by--;
		}
	}

	PROCESS_END();
}

//-- THREAD -------------------------------------------------------------------------------------



/**
 * Initializes the MLST. Has to be called once in the beginning. You can also call it multiple times without harm but at least
 * once before you use it. The is no receiving or sending or anything else otherwise.
 */
void mlst_init(){
	static uint8_t is_initialized = 0;
	if(is_initialized == 0){
		//init pvn
		pvn_init(&mlst_pvn, MLST_PVN_PORT, &own_mlst_public_variable, sizeof(struct mlst_public_variable), MAX_AGE_OF_MLST_NBR_IN_SECONDS);
		pvn_set_comparison_function(&mlst_pvn, pvnCmp);
		pvn_setCallbacks(&mlst_pvn, mlst_pvn_callbacks);
		is_initialized = 1;
		rsunicast_init();

		//start process
		process_start(&mlst_process,0);
	}
}

/**
 * For Debugging. Prints the state of the MLST to the serial port.
 * It contains ID of the parent and the number of children.
 */
void mlst_print_state(){
	printf("MLST[Parent:%d, #Children:%d]\n", own_mlst_public_variable.parent_id, own_mlst_public_variable.children_count);
	pvn_print_state(&mlst_pvn);
}


#endif
