/* Process model C form file: mgen_protolib.pr.c */
/* Portions of this file copyright 1992-2004 by OPNET Technologies, Inc. */



/* This variable carries the header into the object file */
const char mgen_protolib_pr_c [] = "MIL_3_Tfile_Hdr_ 115A 30A modeler 7 437241B0 437241B0 1 apocalypse Jim@Hauser 0 0 none none 0 0 none 0 0 0 0 0 0 0 0 d18 1                                                                                                                                                                                                                                                                                                                                                                                                     ";
#include <string.h>



/* OPNET system definitions */
#include <opnet.h>



/* Header Block */

#include <mgen.h>
#include <oms_pr.h>
#include <oms_tan.h>

/*	Define a transition conditions              	*/

#define	SELF_NOTIF		intrpt_type == OPC_INTRPT_SELF


/* End of Header Block */

#if !defined (VOSD_NO_FIN)
#undef	BIN
#undef	BOUT
#define	BIN		FIN_LOCAL_FIELD(_op_last_line_passed) = __LINE__ - _op_block_origin;
#define	BOUT	BIN
#define	BINIT	FIN_LOCAL_FIELD(_op_last_line_passed) = 0; _op_block_origin = __LINE__;
#else
#define	BINIT
#endif /* #if !defined (VOSD_NO_FIN) */



/* State variable definitions */
typedef struct
	{
	/* Internal state tracking for FSM */
	FSM_SYS_STATE
	/* State Variables */
	Objid	                  		my_id                                           ;
	Objid	                  		my_node_id                                      ;
	Prohandle	              		own_prohandle                                   ;
	OmsT_Pr_Handle	         		own_process_record_handle                       ;
	Stathandle	             		bits_rcvd_stathandle                            ;
	Stathandle	             		bitssec_rcvd_stathandle                         ;
	Stathandle	             		pkts_rcvd_stathandle                            ;
	Stathandle	             		pktssec_rcvd_stathandle                         ;
	Stathandle	             		bits_sent_stathandle                            ;
	Stathandle	             		bitssec_sent_stathandle                         ;
	Stathandle	             		pkts_sent_stathandle                            ;
	Stathandle	             		pktssec_sent_stathandle                         ;
	Stathandle	             		ete_delay_stathandle                            ;
	Stathandle	             		bits_rcvd_gstathandle                           ;
	Stathandle	             		bitssec_rcvd_gstathandle                        ;
	Stathandle	             		pkts_rcvd_gstathandle                           ;
	Stathandle	             		pktssec_rcvd_gstathandle                        ;
	Stathandle	             		bits_sent_gstathandle                           ;
	Stathandle	             		bitssec_sent_gstathandle                        ;
	Stathandle	             		pkts_sent_gstathandle                           ;
	Stathandle	             		pktssec_sent_gstathandle                        ;
	Stathandle	             		ete_delay_gstathandle                           ;
	} mgen_protolib_state;

#define pr_state_ptr            		((mgen_protolib_state*) (OP_SIM_CONTEXT_PTR->_op_mod_state_ptr))
#define my_id                   		pr_state_ptr->my_id
#define my_node_id              		pr_state_ptr->my_node_id
#define own_prohandle           		pr_state_ptr->own_prohandle
#define own_process_record_handle		pr_state_ptr->own_process_record_handle
#define bits_rcvd_stathandle    		pr_state_ptr->bits_rcvd_stathandle
#define bitssec_rcvd_stathandle 		pr_state_ptr->bitssec_rcvd_stathandle
#define pkts_rcvd_stathandle    		pr_state_ptr->pkts_rcvd_stathandle
#define pktssec_rcvd_stathandle 		pr_state_ptr->pktssec_rcvd_stathandle
#define bits_sent_stathandle    		pr_state_ptr->bits_sent_stathandle
#define bitssec_sent_stathandle 		pr_state_ptr->bitssec_sent_stathandle
#define pkts_sent_stathandle    		pr_state_ptr->pkts_sent_stathandle
#define pktssec_sent_stathandle 		pr_state_ptr->pktssec_sent_stathandle
#define ete_delay_stathandle    		pr_state_ptr->ete_delay_stathandle
#define bits_rcvd_gstathandle   		pr_state_ptr->bits_rcvd_gstathandle
#define bitssec_rcvd_gstathandle		pr_state_ptr->bitssec_rcvd_gstathandle
#define pkts_rcvd_gstathandle   		pr_state_ptr->pkts_rcvd_gstathandle
#define pktssec_rcvd_gstathandle		pr_state_ptr->pktssec_rcvd_gstathandle
#define bits_sent_gstathandle   		pr_state_ptr->bits_sent_gstathandle
#define bitssec_sent_gstathandle		pr_state_ptr->bitssec_sent_gstathandle
#define pkts_sent_gstathandle   		pr_state_ptr->pkts_sent_gstathandle
#define pktssec_sent_gstathandle		pr_state_ptr->pktssec_sent_gstathandle
#define ete_delay_gstathandle   		pr_state_ptr->ete_delay_gstathandle

/* These macro definitions will define a local variable called	*/
/* "op_sv_ptr" in each function containing a FIN statement.	*/
/* This variable points to the state variable data structure,	*/
/* and can be used from a C debugger to display their values.	*/
#undef FIN_PREAMBLE_DEC
#undef FIN_PREAMBLE_CODE
#  define FIN_PREAMBLE_DEC	mgen_protolib_state *op_sv_ptr;
#if defined (OPD_PARALLEL)
#  define FIN_PREAMBLE_CODE	\
		op_sv_ptr = ((mgen_protolib_state *)(sim_context_ptr->_op_mod_state_ptr));
#else
#  define FIN_PREAMBLE_CODE	op_sv_ptr = pr_state_ptr;
#endif


/* Function Block */

#if !defined (VOSD_NO_FIN)
enum { _op_block_origin = __LINE__ + 2};
#endif

/* Initialization function */

void mgen_init ()
	{
	
	}

/* End of Function Block */

/* Undefine optional tracing in FIN/FOUT/FRET */
/* The FSM has its own tracing code and the other */
/* functions should not have any tracing.		  */
#undef FIN_TRACING
#define FIN_TRACING

#undef FOUTRET_TRACING
#define FOUTRET_TRACING

#if defined (__cplusplus)
extern "C" {
#endif
	void mgen_protolib (OP_SIM_CONTEXT_ARG_OPT);
	VosT_Obtype _op_mgen_protolib_init (int * init_block_ptr);
	VosT_Address _op_mgen_protolib_alloc (VOS_THREAD_INDEX_ARG_COMMA VosT_Obtype, int);
	void _op_mgen_protolib_diag (OP_SIM_CONTEXT_ARG_OPT);
	void _op_mgen_protolib_terminate (OP_SIM_CONTEXT_ARG_OPT);
	void _op_mgen_protolib_svar (void *, const char *, void **);


	VosT_Obtype Vos_Define_Object_Prstate (const char * _op_name, unsigned int _op_size);
	VosT_Address Vos_Alloc_Object_MT (VOS_THREAD_INDEX_ARG_COMMA VosT_Obtype _op_ob_hndl);
	VosT_Fun_Status Vos_Poolmem_Dealloc_MT (VOS_THREAD_INDEX_ARG_COMMA VosT_Address _op_ob_ptr);
#if defined (__cplusplus)
} /* end of 'extern "C"' */
#endif




/* Process model interrupt handling procedure */


void
mgen_protolib (OP_SIM_CONTEXT_ARG_OPT)
	{
#if !defined (VOSD_NO_FIN)
	int _op_block_origin = 0;
#endif
	FIN_MT (mgen_protolib ());

		{
		/* Temporary Variables */
		Packet*		pkptr;
		double		pk_size;
		double		ete_delay;
		/* End of Temporary Variables */


		FSM_ENTER_NO_VARS ("mgen_protolib")

		FSM_BLOCK_SWITCH
			{
			/*---------------------------------------------------------*/
			/** state (idle) enter executives **/
			FSM_STATE_ENTER_UNFORCED (0, "idle", state0_enter_exec, "mgen_protolib [idle enter execs]")

			/** blocking after enter executives of unforced state. **/
			FSM_EXIT (1,"mgen_protolib")


			/** state (idle) exit executives **/
			FSM_STATE_EXIT_UNFORCED (0, "idle", "mgen_protolib [idle exit execs]")
				FSM_PROFILE_SECTION_IN ("mgen_protolib [idle exit execs]", state0_exit_exec)
				{
				/* Obtain the incoming packet.	*/
				pkptr = op_pk_get (op_intrpt_strm ());
				
				/* Caclulate metrics to be updated.		*/
				pk_size = (double) op_pk_total_size_get (pkptr);
				ete_delay = op_sim_time () - op_pk_creation_time_get (pkptr);
				
				/* Update local statistics.				*/
				op_stat_write (bits_rcvd_stathandle, 		pk_size);
				op_stat_write (pkts_rcvd_stathandle, 		1.0);
				op_stat_write (ete_delay_stathandle, 		ete_delay);
				
				op_stat_write (bitssec_rcvd_stathandle, 	pk_size);
				op_stat_write (bitssec_rcvd_stathandle, 	0.0);
				op_stat_write (pktssec_rcvd_stathandle, 	1.0);
				op_stat_write (pktssec_rcvd_stathandle, 	0.0);
				
				/* Update global statistics.	*/
				op_stat_write (bits_rcvd_gstathandle, 		pk_size);
				op_stat_write (pkts_rcvd_gstathandle, 		1.0);
				op_stat_write (ete_delay_gstathandle, 		ete_delay);
				
				op_stat_write (bitssec_rcvd_gstathandle, 	pk_size);
				op_stat_write (bitssec_rcvd_gstathandle, 	0.0);
				op_stat_write (pktssec_rcvd_gstathandle, 	1.0);
				op_stat_write (pktssec_rcvd_gstathandle, 	0.0);
				
				/* Destroy the received packet.	*/
				op_pk_destroy (pkptr);
				}
				FSM_PROFILE_SECTION_OUT (state0_exit_exec)


			/** state (idle) transition processing **/
			FSM_TRANSIT_FORCE (0, state0_enter_exec, ;, "default", "", "idle", "idle", "mgen_protolib [idle -> idle : default / ]")
				/*---------------------------------------------------------*/



			/** state (init) enter executives **/
			FSM_STATE_ENTER_UNFORCED_NOLABEL (1, "init", "mgen_protolib [init enter execs]")
				FSM_PROFILE_SECTION_IN ("mgen_protolib [init enter execs]", state1_enter_exec)
				{
				
				/* Obtain the object ID of the surrounding mgen processor. 	*/
				my_id = op_id_self ();
				
				/* Also obtain the object ID of the surrounding node.		*/
				my_node_id = op_topo_parent (my_id);
				
				/* Obtain the prohandle for this process.					*/
				own_prohandle = op_pro_self ();
				
				/**	Register the process in the model-wide registry.				**/
				own_process_record_handle = (OmsT_Pr_Handle) oms_pr_process_register 
					(my_node_id, my_id, own_prohandle, "MGEN");
				
				/*	Register the protocol attribute in the registry. No other	*/
				/*	process should use the string "mgen" as the value for its	*/
				/*	"protocol" attribute!										*/
				oms_pr_attr_set (own_process_record_handle, 
					"protocol", 	OMSC_PR_STRING, 	"mgen",
					OPC_NIL);
				
				/*	Initialize the state variable used to keep track of the	*/
				/*	MGEN module object ID and to generate trace/debugging 	*/
				/*	string information. Obtain process ID of this process. 	*/
				my_pro_id = op_pro_id (op_pro_self ());
				
				/* 	Set the process ID string, to be later used for trace	*/
				/*	and debugging information.								*/
				sprintf (pid_string, "MGEN PID (%d)", my_pro_id);
				
				
				/* Initilaize the statistic handles to keep	*/
				/* track of traffic sent and sinked by this process.	*/
				bits_rcvd_stathandle 		= op_stat_reg ("MGEN.Traffic Received (bits)",			OPC_STAT_INDEX_NONE, OPC_STAT_LOCAL);
				bitssec_rcvd_stathandle 	= op_stat_reg ("MGEN.Traffic Received (bits/sec)",		OPC_STAT_INDEX_NONE, OPC_STAT_LOCAL);
				pkts_rcvd_stathandle 		= op_stat_reg ("MGEN.Traffic Received (packets)",		OPC_STAT_INDEX_NONE, OPC_STAT_LOCAL);
				pktssec_rcvd_stathandle 	= op_stat_reg ("MGEN.Traffic Received (packets/sec)",	OPC_STAT_INDEX_NONE, OPC_STAT_LOCAL);
				bits_sent_stathandle 		= op_stat_reg ("MGEN.Traffic Sent (bits)",			OPC_STAT_INDEX_NONE, OPC_STAT_LOCAL);
				bitssec_sent_stathandle 	= op_stat_reg ("MGEN.Traffic Sent (bits/sec)",		OPC_STAT_INDEX_NONE, OPC_STAT_LOCAL);
				pkts_sent_stathandle 		= op_stat_reg ("MGEN.Traffic Sent (packets)",		OPC_STAT_INDEX_NONE, OPC_STAT_LOCAL);
				pktssec_sent_stathandle 	= op_stat_reg ("MGEN.Traffic Sent (packets/sec)",	OPC_STAT_INDEX_NONE, OPC_STAT_LOCAL);
				ete_delay_stathandle		= op_stat_reg ("MGEN.End-to-End Delay (seconds)",		OPC_STAT_INDEX_NONE, OPC_STAT_LOCAL);
				
				bits_rcvd_gstathandle 		= op_stat_reg ("MGEN.Traffic Received (bits)",			OPC_STAT_INDEX_NONE, OPC_STAT_GLOBAL);
				bitssec_rcvd_gstathandle 	= op_stat_reg ("MGEN.Traffic Received (bits/sec)",		OPC_STAT_INDEX_NONE, OPC_STAT_GLOBAL);
				pkts_rcvd_gstathandle 		= op_stat_reg ("MGEN.Traffic Received (packets)",		OPC_STAT_INDEX_NONE, OPC_STAT_GLOBAL);
				pktssec_rcvd_gstathandle 	= op_stat_reg ("MGEN.Traffic Received (packets/sec)",	OPC_STAT_INDEX_NONE, OPC_STAT_GLOBAL);
				bits_sent_gstathandle 		= op_stat_reg ("MGEN.Traffic Sent (bits)",			OPC_STAT_INDEX_NONE, OPC_STAT_GLOBAL);
				bitssec_sent_gstathandle 	= op_stat_reg ("MGEN.Traffic Sent (bits/sec)",		OPC_STAT_INDEX_NONE, OPC_STAT_GLOBAL);
				pkts_sent_gstathandle 		= op_stat_reg ("MGEN.Traffic Sent (packets)",		OPC_STAT_INDEX_NONE, OPC_STAT_GLOBAL);
				pktssec_sent_gstathandle 	= op_stat_reg ("MGEN.Traffic Sent (packets/sec)",	OPC_STAT_INDEX_NONE, OPC_STAT_GLOBAL);
				ete_delay_gstathandle		= op_stat_reg ("MGEN.End-to-End Delay (seconds)",		OPC_STAT_INDEX_NONE, OPC_STAT_GLOBAL);
				
				/* 	Schedule a self interrupt to allow  additional initialization. */ 			   		*/
				op_intrpt_schedule_self (op_sim_time (), 0);
				}
				FSM_PROFILE_SECTION_OUT (state1_enter_exec)

			/** blocking after enter executives of unforced state. **/
			FSM_EXIT (3,"mgen_protolib")


			/** state (init) exit executives **/
			FSM_STATE_EXIT_UNFORCED (1, "init", "mgen_protolib [init exit execs]")
				FSM_PROFILE_SECTION_IN ("mgen_protolib [init exit execs]", state1_exit_exec)
				{
				mgen_init();
				mgen_process->start();
				}
				FSM_PROFILE_SECTION_OUT (state1_exit_exec)


			/** state (init) transition processing **/
			FSM_TRANSIT_FORCE (0, state0_enter_exec, ;, "default", "", "init", "idle", "mgen_protolib [init -> idle : default / ]")
				/*---------------------------------------------------------*/



			}


		FSM_EXIT (1,"mgen_protolib")
		}
	}




void
_op_mgen_protolib_diag (OP_SIM_CONTEXT_ARG_OPT)
	{
	/* No Diagnostic Block */
	}




void
_op_mgen_protolib_terminate (OP_SIM_CONTEXT_ARG_OPT)
	{

	FIN_MT (_op_mgen_protolib_terminate ())


	/* No Termination Block */

	Vos_Poolmem_Dealloc_MT (OP_SIM_CONTEXT_THREAD_INDEX_COMMA pr_state_ptr);

	FOUT
	}


/* Undefine shortcuts to state variables to avoid */
/* syntax error in direct access to fields of */
/* local variable prs_ptr in _op_mgen_protolib_svar function. */
#undef my_id
#undef my_node_id
#undef own_prohandle
#undef own_process_record_handle
#undef bits_rcvd_stathandle
#undef bitssec_rcvd_stathandle
#undef pkts_rcvd_stathandle
#undef pktssec_rcvd_stathandle
#undef bits_sent_stathandle
#undef bitssec_sent_stathandle
#undef pkts_sent_stathandle
#undef pktssec_sent_stathandle
#undef ete_delay_stathandle
#undef bits_rcvd_gstathandle
#undef bitssec_rcvd_gstathandle
#undef pkts_rcvd_gstathandle
#undef pktssec_rcvd_gstathandle
#undef bits_sent_gstathandle
#undef bitssec_sent_gstathandle
#undef pkts_sent_gstathandle
#undef pktssec_sent_gstathandle
#undef ete_delay_gstathandle

#undef FIN_PREAMBLE_DEC
#undef FIN_PREAMBLE_CODE

#define FIN_PREAMBLE_DEC
#define FIN_PREAMBLE_CODE

VosT_Obtype
_op_mgen_protolib_init (int * init_block_ptr)
	{
	VosT_Obtype obtype = OPC_NIL;
	FIN_MT (_op_mgen_protolib_init (init_block_ptr))

	obtype = Vos_Define_Object_Prstate ("proc state vars (mgen_protolib)",
		sizeof (mgen_protolib_state));
	*init_block_ptr = 2;

	FRET (obtype)
	}

VosT_Address
_op_mgen_protolib_alloc (VOS_THREAD_INDEX_ARG_COMMA VosT_Obtype obtype, int init_block)
	{
#if !defined (VOSD_NO_FIN)
	int _op_block_origin = 0;
#endif
	mgen_protolib_state * ptr;
	FIN_MT (_op_mgen_protolib_alloc (obtype))

	ptr = (mgen_protolib_state *)Vos_Alloc_Object_MT (VOS_THREAD_INDEX_COMMA obtype);
	if (ptr != OPC_NIL)
		{
		ptr->_op_current_block = init_block;
#if defined (OPD_ALLOW_ODB)
		ptr->_op_current_state = "mgen_protolib [init enter execs]";
#endif
		}
	FRET ((VosT_Address)ptr)
	}



void
_op_mgen_protolib_svar (void * gen_ptr, const char * var_name, void ** var_p_ptr)
	{
	mgen_protolib_state		*prs_ptr;

	FIN_MT (_op_mgen_protolib_svar (gen_ptr, var_name, var_p_ptr))

	if (var_name == OPC_NIL)
		{
		*var_p_ptr = (void *)OPC_NIL;
		FOUT
		}
	prs_ptr = (mgen_protolib_state *)gen_ptr;

	if (strcmp ("my_id" , var_name) == 0)
		{
		*var_p_ptr = (void *) (&prs_ptr->my_id);
		FOUT
		}
	if (strcmp ("my_node_id" , var_name) == 0)
		{
		*var_p_ptr = (void *) (&prs_ptr->my_node_id);
		FOUT
		}
	if (strcmp ("own_prohandle" , var_name) == 0)
		{
		*var_p_ptr = (void *) (&prs_ptr->own_prohandle);
		FOUT
		}
	if (strcmp ("own_process_record_handle" , var_name) == 0)
		{
		*var_p_ptr = (void *) (&prs_ptr->own_process_record_handle);
		FOUT
		}
	if (strcmp ("bits_rcvd_stathandle" , var_name) == 0)
		{
		*var_p_ptr = (void *) (&prs_ptr->bits_rcvd_stathandle);
		FOUT
		}
	if (strcmp ("bitssec_rcvd_stathandle" , var_name) == 0)
		{
		*var_p_ptr = (void *) (&prs_ptr->bitssec_rcvd_stathandle);
		FOUT
		}
	if (strcmp ("pkts_rcvd_stathandle" , var_name) == 0)
		{
		*var_p_ptr = (void *) (&prs_ptr->pkts_rcvd_stathandle);
		FOUT
		}
	if (strcmp ("pktssec_rcvd_stathandle" , var_name) == 0)
		{
		*var_p_ptr = (void *) (&prs_ptr->pktssec_rcvd_stathandle);
		FOUT
		}
	if (strcmp ("bits_sent_stathandle" , var_name) == 0)
		{
		*var_p_ptr = (void *) (&prs_ptr->bits_sent_stathandle);
		FOUT
		}
	if (strcmp ("bitssec_sent_stathandle" , var_name) == 0)
		{
		*var_p_ptr = (void *) (&prs_ptr->bitssec_sent_stathandle);
		FOUT
		}
	if (strcmp ("pkts_sent_stathandle" , var_name) == 0)
		{
		*var_p_ptr = (void *) (&prs_ptr->pkts_sent_stathandle);
		FOUT
		}
	if (strcmp ("pktssec_sent_stathandle" , var_name) == 0)
		{
		*var_p_ptr = (void *) (&prs_ptr->pktssec_sent_stathandle);
		FOUT
		}
	if (strcmp ("ete_delay_stathandle" , var_name) == 0)
		{
		*var_p_ptr = (void *) (&prs_ptr->ete_delay_stathandle);
		FOUT
		}
	if (strcmp ("bits_rcvd_gstathandle" , var_name) == 0)
		{
		*var_p_ptr = (void *) (&prs_ptr->bits_rcvd_gstathandle);
		FOUT
		}
	if (strcmp ("bitssec_rcvd_gstathandle" , var_name) == 0)
		{
		*var_p_ptr = (void *) (&prs_ptr->bitssec_rcvd_gstathandle);
		FOUT
		}
	if (strcmp ("pkts_rcvd_gstathandle" , var_name) == 0)
		{
		*var_p_ptr = (void *) (&prs_ptr->pkts_rcvd_gstathandle);
		FOUT
		}
	if (strcmp ("pktssec_rcvd_gstathandle" , var_name) == 0)
		{
		*var_p_ptr = (void *) (&prs_ptr->pktssec_rcvd_gstathandle);
		FOUT
		}
	if (strcmp ("bits_sent_gstathandle" , var_name) == 0)
		{
		*var_p_ptr = (void *) (&prs_ptr->bits_sent_gstathandle);
		FOUT
		}
	if (strcmp ("bitssec_sent_gstathandle" , var_name) == 0)
		{
		*var_p_ptr = (void *) (&prs_ptr->bitssec_sent_gstathandle);
		FOUT
		}
	if (strcmp ("pkts_sent_gstathandle" , var_name) == 0)
		{
		*var_p_ptr = (void *) (&prs_ptr->pkts_sent_gstathandle);
		FOUT
		}
	if (strcmp ("pktssec_sent_gstathandle" , var_name) == 0)
		{
		*var_p_ptr = (void *) (&prs_ptr->pktssec_sent_gstathandle);
		FOUT
		}
	if (strcmp ("ete_delay_gstathandle" , var_name) == 0)
		{
		*var_p_ptr = (void *) (&prs_ptr->ete_delay_gstathandle);
		FOUT
		}
	*var_p_ptr = (void *)OPC_NIL;

	FOUT
	}

