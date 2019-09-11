/* Process model C++ form file: ns2_node_movement.pr.cpp */
/* Portions of this file copyright 1992-2006 by OPNET Technologies, Inc. */



/* This variable carries the header into the object file */
const char ns2_node_movement_pr_cpp [] = "MIL_3_Tfile_Hdr_ 115A 30A modeler 7 44EDE482 44EDE482 1 apocalypse Jim@Hauser 0 0 none none 0 0 none 0 0 0 0 0 0 0 0 d50 3                                                                                                                                                                                                                                                                                                                                                                                                     ";
#include <string.h>



/* OPNET system definitions */
#include <opnet.h>



/* Header Block */

#include	<string.h>
#include	<stdio.h>
#include 	<stdlib.h>

// #define OP_DEBUG2 1 /* LP 4-12-04 - added to test.  Should be removed later */


/* Global variables declaration. */

const int 		MAX_NUM_NODE = 120;
const int 		MAX_NODE_NAME = 128;
List *			Movement_data[MAX_NUM_NODE];


struct Movement_coord {
	
		double lat;
		double lon;
		double alt;
		double spd;
		double move_time;
	};

class Tval
	{
	double s;
	int h, m;
	
	public:
	Tval (){}
	~Tval (){}
	void settime (double seconds)
		{
		int time = seconds;
		h = time/3600;
		m = (time - h*60)/60;
		s = seconds - (h*60 + m)*60;
		}
	int hr(void) {return h;}
	int mn(void) {return m;}
	double sec(void) {return s;}
	};
	
/* Functions */
	int read_movement_file(void);
	void build_trj_file(int);
	void print_header (FILE*, int);

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
class ns2_node_movement_state
	{
	public:
		ns2_node_movement_state (void);

		/* Destructor contains Termination Block */
		~ns2_node_movement_state (void);

		/* State Variables */
		Objid	                  		own_objid                                       ;
		char	                   		movement_data_file[256]                         ;
		char	                   		path[256]                                       ;
		char	                   		nodename[MAX_NUM_NODE][MAX_NODE_NAME]           ;
		double	                 		sim_end                                         ;
		char	                   		units[12]                                       ;

		/* FSM code */
		void ns2_node_movement (OP_SIM_CONTEXT_ARG_OPT);
		/* Diagnostic Block */
		void _op_ns2_node_movement_diag (OP_SIM_CONTEXT_ARG_OPT);

#if defined (VOSD_NEW_BAD_ALLOC)
		void * operator new (size_t) throw (VOSD_BAD_ALLOC);
#else
		void * operator new (size_t);
#endif
		void operator delete (void *);

		/* Memory management */
		static VosT_Obtype obtype;

	private:
		/* Internal state tracking for FSM */
		FSM_SYS_STATE
	};

VosT_Obtype ns2_node_movement_state::obtype = (VosT_Obtype)OPC_NIL;

#define pr_state_ptr            		((ns2_node_movement_state*) (OP_SIM_CONTEXT_PTR->_op_mod_state_ptr))
#define own_objid               		pr_state_ptr->own_objid
#define movement_data_file      		pr_state_ptr->movement_data_file
#define path                    		pr_state_ptr->path
#define nodename                		pr_state_ptr->nodename
#define sim_end                 		pr_state_ptr->sim_end
#define units                   		pr_state_ptr->units

/* These macro definitions will define a local variable called	*/
/* "op_sv_ptr" in each function containing a FIN statement.	*/
/* This variable points to the state variable data structure,	*/
/* and can be used from a C debugger to display their values.	*/
#undef FIN_PREAMBLE_DEC
#undef FIN_PREAMBLE_CODE
#  define FIN_PREAMBLE_DEC	ns2_node_movement_state *op_sv_ptr;
#if defined (OPD_PARALLEL)
#  define FIN_PREAMBLE_CODE	\
		op_sv_ptr = ((ns2_node_movement_state *)(sim_context_ptr->_op_mod_state_ptr));
#else
#  define FIN_PREAMBLE_CODE	op_sv_ptr = pr_state_ptr;
#endif


/* Function Block */

#if !defined (VOSD_NO_FIN)
enum { _op_block_origin = __LINE__ + 2};
#endif

int read_movement_file(void)
	
{
	List* 	fieldlist;
	List* 	movement_data_ptr;
	Movement_coord * coord_;
	int num_nodes = 0;
	
	for (int i=0; i<MAX_NUM_NODE; i++)
		{
		//nodename[i] = (char*)op_prg_mem_alloc(MAX_NODE_NAME);
		nodename[i][0] = '\0';
		}

	op_ima_obj_attr_get_str(own_objid,"OLSR_move_file", 256, movement_data_file);
	op_ima_obj_attr_get_str(own_objid,"Path", 256, path);
	op_ima_obj_attr_get_str(own_objid,"Units", 12, units);
	movement_data_ptr = op_prg_gdf_read ( (const char *) movement_data_file );
	int num_lines = op_prg_list_size ( movement_data_ptr );
	for (int line_nbr = 0; line_nbr < num_lines; line_nbr++ )
		{
		int node_id;
		coord_ = (Movement_coord *) op_prg_mem_alloc (sizeof (Movement_coord));
		char* line = (char *)op_prg_list_access ( movement_data_ptr, line_nbr );
		
		// Remove double quotes from line so op_prg_str_decomp will work.
		for (int i=0; line[i]!='\0'; i++)
			if (line[i] == '\"')
				line[i] = ' ';
		
		fieldlist = op_prg_str_decomp ( line , " " );
		
		// First uncommented line gives simulation end time.
		if (line_nbr == 0)
			{
			sim_end = atof ((const char *)op_prg_list_access ( fieldlist, 2));
			continue;
			}
		
		// Skip lines that don't begin with $ns_
		if (strcmp("$ns_",(const char *)op_prg_list_access ( fieldlist, 0)))
			continue;
		
		coord_->move_time = atof ((const char *)op_prg_list_access ( fieldlist, 2));
		coord_->lon = atof ((const char *)op_prg_list_access ( fieldlist, 5));			
		coord_->lat = atof ((const char *)op_prg_list_access ( fieldlist, 6));
		coord_->alt = 0.0;
		coord_->spd = atof ((const char *)op_prg_list_access ( fieldlist, 7));
		const char* name = (const char *)op_prg_list_access ( fieldlist, 3);
		
		// If node name matches existing name, use the existing node_id, otherwise create a new node_id for name.
		for (int i=0; i<=num_nodes; i++)
			{
			if (nodename[i][0] != '\0')
				{
				if (strcmp(nodename[i],name) == 0)
					{
					node_id = i;
					break;
					}					
				}
			else
				{
				strcpy(nodename[i],name);
				node_id = i;
				num_nodes++;
				op_prg_list_insert (Movement_data[node_id], nodename[i], OPC_LISTPOS_TAIL);
				break;
				}
			}
				
		op_prg_list_insert (Movement_data[node_id], coord_, OPC_LISTPOS_TAIL);
		op_prg_list_free ( fieldlist ); 
		op_prg_mem_free ( fieldlist );
		}
	op_prg_list_free ( movement_data_ptr ); 
	op_prg_mem_free ( movement_data_ptr );
	return num_nodes;
}

void build_trj_file(int NODE_ID)
	{
	int list_size;
	Movement_coord * coord_, * next_coord;
	
	list_size = op_prg_list_size(Movement_data[NODE_ID]);
	
	// First list element contains name of node.
	const char *node = (const char *)op_prg_list_access (Movement_data[NODE_ID], 0);
	char outfile[256];
	
	// Concatenate input file name and node name to create output trj file name.
	printf("\n%s\\%s%s.trj\n", path, movement_data_file, node);	
	sprintf(outfile, "%s\\%s%s.trj", path, movement_data_file, node);
	FILE* fp = fopen(outfile, "w");
	
	// Handle special case where only one coord is given, i.e., no movement.
	// Must have at least two entries for valid trj file.
	if (list_size == 2)
		{
		print_header(fp, list_size);
		coord_ = (Movement_coord *) op_prg_list_access (Movement_data[NODE_ID], 1);
		Tval tt;
		tt.settime(coord_->move_time);
		printf("%f   ,%f   ,%f   ,0h0m0.0s   ,0h0m0.0s   ,Autocomputed   ,Autocomputed   ,Unspecified\n",
			coord_->lon,coord_->lat,coord_->alt);
		printf("%f   ,%f   ,%f   ,%ih%im%fs   ,0h0m0.0s   ,Autocomputed   ,Autocomputed   ,Unspecified\n",
			coord_->lon,coord_->lat,coord_->alt, tt.hr(), tt.mn(), tt.sec());
		fprintf(fp,"%f   ,%f   ,%f   ,0h0m0.0s   ,0h0m0.0s   ,Autocomputed   ,Autocomputed   ,Unspecified\n",
			coord_->lon,coord_->lat,coord_->alt);
		fprintf(fp,"%f   ,%f   ,%f   ,%ih%im%fs   ,0h0m0.0s   ,Autocomputed   ,Autocomputed   ,Unspecified\n",
			coord_->lon,coord_->lat,coord_->alt, tt.hr(), tt.mn(), tt.sec());
		return;
		}
	
	// Need to look at current and next movement event pairs to compute traversal times.
	// Travesal time is the time to travel from the previous point to the current point.
	// Wait times are accommodated by traversing to the same point and, therefore, set to zero.
	print_header(fp, list_size-2);
	for (int i = 1; i < list_size-1; i++)
		{
		coord_ = (Movement_coord *) op_prg_list_access (Movement_data[NODE_ID], i);
		next_coord = (Movement_coord *) op_prg_list_access (Movement_data[NODE_ID], i+1);
		Tval t;
		t.settime(next_coord->move_time - coord_->move_time);
		// first point special case
		if (i == 1)
			{
			Tval tt;
			tt.settime(coord_->move_time);
			printf("%f   ,%f   ,%f   ,%ih%im%fs   ,%ih%im%fs   ,Autocomputed   ,Autocomputed   ,Unspecified\n",
				coord_->lon, coord_->lat, coord_->alt, tt.hr(), tt.mn(), tt.sec(), t.hr(), t.mn(), t.sec());
			fprintf(fp,"%f   ,%f   ,%f   ,%ih%im%fs   ,%ih%im%fs   ,Autocomputed   ,Autocomputed   ,Unspecified\n",
				coord_->lon, coord_->lat, coord_->alt, tt.hr(), tt.mn(), tt.sec(), t.hr(), t.mn(), t.sec());
			}
		// Last point is special case.
		else if (i == list_size-2)
			{
			t.settime(sim_end - next_coord->move_time);
			printf("%f   ,%f   %f   ,%ih%im%fs   ,0h0m0.0s   ,Autocomputed   ,Autocomputed   ,Unspecified\n",
				next_coord->lon, next_coord->lat, coord_->alt, t.hr(), t.mn(), t.sec());
			fprintf(fp,"%f   ,%f   ,%f   ,%ih%im%fs   ,0h0m0.0s   ,Autocomputed   ,Autocomputed   ,Unspecified\n",
				next_coord->lon, next_coord->lat, coord_->alt, t.hr(), t.mn(), t.sec());
			}
		else
			{
			printf("%f   ,%f   ,%f   ,%ih%im%fs   ,0h0m0.0s   ,Autocomputed   ,Autocomputed   ,Unspecified\n",
				coord_->lon, coord_->lat, coord_->alt, t.hr(), t.mn(), t.sec());
			fprintf(fp,"%f   ,%f   ,%f   ,%ih%im%fs   ,0h0m0.0s   ,Autocomputed   ,Autocomputed   ,Unspecified\n",
				coord_->lon, coord_->lat, coord_->alt, t.hr(), t.mn(), t.sec());
			}
		} /* end for i */
	
	// 
	}

void print_header (FILE* fp, int num_coords)
	{
	printf("Version: 3\n");
	printf("Position_Unit: %s\n",units);
	printf("Altitude_Unit: Meters\n");
	printf("Coordinate_Method: fixed\n");
	printf("Altitude_Method: absolute\n");
	printf("locale: C\n");
	printf("Calendar_Start: unused\n");
	printf("Coordinate_Count: %i\n", num_coords);
	printf("# X Position        ,Y Position          ,Altitude            ,Traverse Time       ,Wait Time           ,Pitch               ,Yaw              ,Roll\n");	
	fprintf(fp,"Version: 3\n");
	fprintf(fp,"Position_Unit: %s\n",units);
	fprintf(fp,"Altitude_Unit: Meters\n");
	fprintf(fp,"Coordinate_Method: fixed\n");
	fprintf(fp,"Altitude_Method: absolute\n");
	fprintf(fp,"locale: C\n");
	fprintf(fp,"Calendar_Start: unused\n");
	fprintf(fp,"Coordinate_Count: %i\n", num_coords);
	fprintf(fp,"# X Position        ,Y Position          ,Altitude            ,Traverse Time       ,Wait Time           ,Pitch               ,Yaw             ,Roll\n");	
	}

/* End of Function Block */

/* Undefine optional tracing in FIN/FOUT/FRET */
/* The FSM has its own tracing code and the other */
/* functions should not have any tracing.		  */
#undef FIN_TRACING
#define FIN_TRACING

#undef FOUTRET_TRACING
#define FOUTRET_TRACING

/* Undefine shortcuts to state variables because the */
/* following functions are part of the state class */
#undef own_objid
#undef movement_data_file
#undef path
#undef nodename
#undef sim_end
#undef units

/* Access from C kernel using C linkage */
extern "C"
{
	VosT_Obtype _op_ns2_node_movement_init (int * init_block_ptr);
	VosT_Address _op_ns2_node_movement_alloc (VOS_THREAD_INDEX_ARG_COMMA VosT_Obtype, int);
	void ns2_node_movement (OP_SIM_CONTEXT_ARG_OPT)
		{
		((ns2_node_movement_state *)(OP_SIM_CONTEXT_PTR->_op_mod_state_ptr))->ns2_node_movement (OP_SIM_CONTEXT_PTR_OPT);
		}

	void _op_ns2_node_movement_svar (void *, const char *, void **);

	void _op_ns2_node_movement_diag (OP_SIM_CONTEXT_ARG_OPT)
		{
		((ns2_node_movement_state *)(OP_SIM_CONTEXT_PTR->_op_mod_state_ptr))->_op_ns2_node_movement_diag (OP_SIM_CONTEXT_PTR_OPT);
		}

	void _op_ns2_node_movement_terminate (OP_SIM_CONTEXT_ARG_OPT)
		{
		/* The destructor is the Termination Block */
		delete (ns2_node_movement_state *)(OP_SIM_CONTEXT_PTR->_op_mod_state_ptr);
		}


	VosT_Obtype Vos_Define_Object_Prstate (const char * _op_name, unsigned int _op_size);
	VosT_Address Vos_Alloc_Object_MT (VOS_THREAD_INDEX_ARG_COMMA VosT_Obtype _op_ob_hndl);
	VosT_Fun_Status Vos_Poolmem_Dealloc_MT (VOS_THREAD_INDEX_ARG_COMMA VosT_Address _op_ob_ptr);
} /* end of 'extern "C"' */




/* Process model interrupt handling procedure */


void
ns2_node_movement_state::ns2_node_movement (OP_SIM_CONTEXT_ARG_OPT)
	{
#if !defined (VOSD_NO_FIN)
	int _op_block_origin = 0;
#endif
	FIN_MT (ns2_node_movement_state::ns2_node_movement ());
	try
		{


		FSM_ENTER_NO_VARS ("ns2_node_movement")

		FSM_BLOCK_SWITCH
			{
			/*---------------------------------------------------------*/
			/** state (init) enter executives **/
			FSM_STATE_ENTER_FORCED_NOLABEL (0, "init", "ns2_node_movement [init enter execs]")
				FSM_PROFILE_SECTION_IN ("ns2_node_movement [init enter execs]", state0_enter_exec)
				{
				// Obtain the surrounding node's objid.
				own_objid = op_id_self();
				
				if (op_ima_obj_attr_exists(own_objid,"OLSR_move_file"))
					{
				
					for (int i = 0; i < MAX_NUM_NODE; i++)
						{
						Movement_data[i] = op_prg_list_create();
						}
					
					int num_nodes = read_movement_file();
				
					for (int i=0; i<num_nodes; i++)
						build_trj_file(i);
					}
				else
					printf("No OLSR_move_file exists.  Node mover will not be used.");
				}
				FSM_PROFILE_SECTION_OUT (state0_enter_exec)

			/** state (init) exit executives **/
			FSM_STATE_EXIT_FORCED (0, "init", "ns2_node_movement [init exit execs]")


			/** state (init) transition processing **/
			FSM_TRANSIT_FORCE (1, state1_enter_exec, ;, "default", "", "init", "idle", "ns2_node_movement [init -> idle : default / ]")
				/*---------------------------------------------------------*/



			/** state (idle) enter executives **/
			FSM_STATE_ENTER_UNFORCED (1, "idle", state1_enter_exec, "ns2_node_movement [idle enter execs]")

			/** blocking after enter executives of unforced state. **/
			FSM_EXIT (3,"ns2_node_movement")


			/** state (idle) exit executives **/
			FSM_STATE_EXIT_UNFORCED (1, "idle", "ns2_node_movement [idle exit execs]")


			/** state (idle) transition processing **/
			FSM_TRANSIT_FORCE (1, state1_enter_exec, ;, "default", "", "idle", "idle", "ns2_node_movement [idle -> idle : default / ]")
				/*---------------------------------------------------------*/



			}


		FSM_EXIT (0,"ns2_node_movement")
		}
	catch (...)
		{
		Vos_Error_Print (VOSC_ERROR_ABORT,
			(const char *)VOSC_NIL,
			"Unhandled C++ exception in process model (ns2_node_movement)",
			(const char *)VOSC_NIL, (const char *)VOSC_NIL);
		}
	}




void
ns2_node_movement_state::_op_ns2_node_movement_diag (OP_SIM_CONTEXT_ARG_OPT)
	{
	/* No Diagnostic Block */
	}

void
ns2_node_movement_state::operator delete (void* ptr)
	{
	FIN (ns2_node_movement_state::operator delete (ptr));
	Vos_Poolmem_Dealloc_MT (OP_SIM_CONTEXT_THREAD_INDEX_COMMA ptr);
	FOUT
	}

ns2_node_movement_state::~ns2_node_movement_state (void)
	{

	FIN (ns2_node_movement_state::~ns2_node_movement_state ())


	/* No Termination Block */


	FOUT
	}


#undef FIN_PREAMBLE_DEC
#undef FIN_PREAMBLE_CODE

#define FIN_PREAMBLE_DEC
#define FIN_PREAMBLE_CODE

void *
ns2_node_movement_state::operator new (size_t)
#if defined (VOSD_NEW_BAD_ALLOC)
		throw (VOSD_BAD_ALLOC)
#endif
	{
	void * new_ptr;

	FIN_MT (ns2_node_movement_state::operator new ());

	new_ptr = Vos_Alloc_Object_MT (VOS_THREAD_INDEX_UNKNOWN_COMMA ns2_node_movement_state::obtype);
#if defined (VOSD_NEW_BAD_ALLOC)
	if (new_ptr == VOSC_NIL) throw VOSD_BAD_ALLOC();
#endif
	FRET (new_ptr)
	}

/* State constructor initializes FSM handling */
/* by setting the initial state to the first */
/* block of code to enter. */

ns2_node_movement_state::ns2_node_movement_state (void) :
		_op_current_block (0)
	{
#if defined (OPD_ALLOW_ODB)
		_op_current_state = "ns2_node_movement [init enter execs]";
#endif
	}

VosT_Obtype
_op_ns2_node_movement_init (int * init_block_ptr)
	{
	FIN_MT (_op_ns2_node_movement_init (init_block_ptr))

	ns2_node_movement_state::obtype = Vos_Define_Object_Prstate ("proc state vars (ns2_node_movement)",
		sizeof (ns2_node_movement_state));
	*init_block_ptr = 0;

	FRET (ns2_node_movement_state::obtype)
	}

VosT_Address
_op_ns2_node_movement_alloc (VOS_THREAD_INDEX_ARG_COMMA VosT_Obtype, int)
	{
#if !defined (VOSD_NO_FIN)
	int _op_block_origin = 0;
#endif
	ns2_node_movement_state * ptr;
	FIN_MT (_op_ns2_node_movement_alloc ())

	/* New instance will have FSM handling initialized */
#if defined (VOSD_NEW_BAD_ALLOC)
	try {
		ptr = new ns2_node_movement_state;
	} catch (const VOSD_BAD_ALLOC &) {
		ptr = VOSC_NIL;
	}
#else
	ptr = new ns2_node_movement_state;
#endif
	FRET ((VosT_Address)ptr)
	}



void
_op_ns2_node_movement_svar (void * gen_ptr, const char * var_name, void ** var_p_ptr)
	{
	ns2_node_movement_state		*prs_ptr;

	FIN_MT (_op_ns2_node_movement_svar (gen_ptr, var_name, var_p_ptr))

	if (var_name == OPC_NIL)
		{
		*var_p_ptr = (void *)OPC_NIL;
		FOUT
		}
	prs_ptr = (ns2_node_movement_state *)gen_ptr;

	if (strcmp ("own_objid" , var_name) == 0)
		{
		*var_p_ptr = (void *) (&prs_ptr->own_objid);
		FOUT
		}
	if (strcmp ("movement_data_file" , var_name) == 0)
		{
		*var_p_ptr = (void *) (prs_ptr->movement_data_file);
		FOUT
		}
	if (strcmp ("path" , var_name) == 0)
		{
		*var_p_ptr = (void *) (prs_ptr->path);
		FOUT
		}
	if (strcmp ("nodename" , var_name) == 0)
		{
		*var_p_ptr = (void *) (prs_ptr->nodename);
		FOUT
		}
	if (strcmp ("sim_end" , var_name) == 0)
		{
		*var_p_ptr = (void *) (&prs_ptr->sim_end);
		FOUT
		}
	if (strcmp ("units" , var_name) == 0)
		{
		*var_p_ptr = (void *) (prs_ptr->units);
		FOUT
		}
	*var_p_ptr = (void *)OPC_NIL;

	FOUT
	}

