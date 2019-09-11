/* Obtain the incoming packet.	*/
pkptr = op_pk_get (op_intrpt_strm ());

/* Caclulate metrics to be updated.		*/
double pk_size = (double) op_pk_total_size_get (pkptr);
double ete_delay = op_sim_time () - op_pk_creation_time_get (pkptr);

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

char* buffer;
MgenMsg msg;
op_pk_fd_get_ptr(pkptr,0,(void**)&buffer);
msg.Unpack(buffer,pk_size,false);

unsigned int flowId = msg.GetFlowId();
if (flowId<MAXSTATFLOWS)
	{
	op_stat_write (bitssec_rcvd_flow_stathandle[flowId], 	pk_size);
//	op_stat_write (pktssec_rcvd_flow_stathandle[flowId], 	1.0);
	op_stat_write (ete_delay_flow_stathandle[flowId], 		ete_delay);
	}

ProtoAddress srcAddr;
int src_addr;
Ici* ici_ptr = op_intrpt_ici ();
if (ici_ptr == OPC_NIL)
	mgen_fatal_error ("Did not receive ICI with packet from UDP.");
/* Extract the source address. */
if (op_ici_attr_exists (ici_ptr, "src_addr"))
	{
	if (op_ici_attr_get (ici_ptr, "src_addr", &src_addr) ==
		OPC_COMPCODE_FAILURE)
		{
		mgen_fatal_error ("Unable to get source address field from ICI.");
		}
	}
srcAddr.SetRawHostAddress(ProtoAddress::SIM,(char*)&src_addr,4);
	
mgen_proc.HandleMgenMessage(buffer,pk_size,srcAddr);
