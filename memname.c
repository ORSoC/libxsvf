/*
 *  Lib(X)SVF  -  A library for implementing SVF and XSVF JTAG players
 *
 *  Copyright (C) 2009  RIEGL Research ForschungsGmbH
 *  Copyright (C) 2009  Clifford Wolf <clifford@clifford.at>
 *  
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *    3. The names of the authors may not be used to endorse or promote
 *       products derived from this software without specific prior
 *       written permission.
 *  
 *  THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 *  IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#include "libxsvf.h"

const char *libxsvf_mem2str(enum libxsvf_mem which)
{
#define X(_w, _t) if (which == LIBXSVF_MEM_ ## _w) return #_t;
	X(XSVF_TDI_DATA, xsvf_tdi_data)
	X(XSVF_TDO_DATA, xsvf_tdo_data)
	X(XSVF_TDO_MASK, xsvf_tdo_mask)
	X(XSVF_ADDR_MASK, xsvf_addr_mask)
	X(XSVF_DATA_MASK, xsvf_data_mask)
	X(SVF_COMMANDBUF, svf_commandbuf)
	X(SVF_HDR_TDI_DATA, svf_hdr_tdi_data)
	X(SVF_HDR_TDI_MASK, svf_hdr_tdi_mask)
	X(SVF_HDR_TDO_DATA, svf_hdr_tdo_data)
	X(SVF_HDR_TDO_MASK, svf_hdr_tdo_mask)
	X(SVF_HDR_RET_MASK, svf_hdr_ret_mask)
	X(SVF_HIR_TDI_DATA, svf_hir_tdi_data)
	X(SVF_HIR_TDI_MASK, svf_hir_tdi_mask)
	X(SVF_HIR_TDO_DATA, svf_hir_tdo_data)
	X(SVF_HIR_TDO_MASK, svf_hir_tdo_mask)
	X(SVF_HIR_RET_MASK, svf_hir_ret_mask)
	X(SVF_TDR_TDI_DATA, svf_tdr_tdi_data)
	X(SVF_TDR_TDI_MASK, svf_tdr_tdi_mask)
	X(SVF_TDR_TDO_DATA, svf_tdr_tdo_data)
	X(SVF_TDR_TDO_MASK, svf_tdr_tdo_mask)
	X(SVF_TDR_RET_MASK, svf_tdr_ret_mask)
	X(SVF_TIR_TDI_DATA, svf_tir_tdi_data)
	X(SVF_TIR_TDI_MASK, svf_tir_tdi_mask)
	X(SVF_TIR_TDO_DATA, svf_tir_tdo_data)
	X(SVF_TIR_TDO_MASK, svf_tir_tdo_mask)
	X(SVF_TIR_RET_MASK, svf_tir_ret_mask)
	X(SVF_SDR_TDI_DATA, svf_sdr_tdi_data)
	X(SVF_SDR_TDI_MASK, svf_sdr_tdi_mask)
	X(SVF_SDR_TDO_DATA, svf_sdr_tdo_data)
	X(SVF_SDR_TDO_MASK, svf_sdr_tdo_mask)
	X(SVF_SDR_RET_MASK, svf_sdr_ret_mask)
	X(SVF_SIR_TDI_DATA, svf_sir_tdi_data)
	X(SVF_SIR_TDI_MASK, svf_sir_tdi_mask)
	X(SVF_SIR_TDO_DATA, svf_sir_tdo_data)
	X(SVF_SIR_TDO_MASK, svf_sir_tdo_mask)
	X(SVF_SIR_RET_MASK, svf_sir_ret_mask)
#undef X
	return (void*)0;
}

