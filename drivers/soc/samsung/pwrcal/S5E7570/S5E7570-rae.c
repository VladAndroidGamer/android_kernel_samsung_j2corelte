#include "../pwrcal-rae.h"
#include "S5E7570-sfrbase.h"

#ifdef PWRCAL_TARGET_LINUX

struct v2p_sfr v2psfrmap[] = {
	DEFINE_V2P(CMU_APM_BASE,	0x11CE0000),
	DEFINE_V2P(CMU_MIF_BASE,	0x10460000),
	DEFINE_V2P(CMU_PERI_BASE,	0x101F0000),
	DEFINE_V2P(CMU_G3D_BASE,	0x11460000),
	DEFINE_V2P(CMU_CPUCL0_BASE,	0x10900000),
	DEFINE_V2P(CMU_MFCMSCL_BASE,	0x12CB0000),
	DEFINE_V2P(CMU_FSYS_BASE,	0x13730000),
	DEFINE_V2P(CMU_ISP_BASE,	0x144D0000),
	DEFINE_V2P(CMU_DISPAUD_BASE,	0x148D0000),

	DEFINE_V2P(PMU_CPUCL0_BASE,	0x10920000),
	DEFINE_V2P(PMU_FSYS_BASE,	0x13740000),
	DEFINE_V2P(PMU_G3D_BASE,	0x11470000),
	DEFINE_V2P(PMU_ISP_BASE,	0x144E0000),
	DEFINE_V2P(PMU_MFCMSCL_BASE,	0x12CC0000),
	DEFINE_V2P(PMU_MIF_BASE,	0x10470000),
	DEFINE_V2P(PMU_PERI_BASE,	0x101E0000),
	DEFINE_V2P(PMU_IF_BASE,	0x11C70000),
	DEFINE_V2P(PMU_ALIVE_BASE,	0x11C80000),
	DEFINE_V2P(PMU_APM_BASE,	0x11CF0000),
	DEFINE_V2P(PMU_DISPAUD_BASE,	0x148E0000),

	DEFINE_V2P(DREX0_BASE,	0x10400000),
	DEFINE_V2P(DREX0_PF_BASE,	0x10410000),
	DEFINE_V2P(DREX0_SECURE_BASE,	0x10420000),
	DEFINE_V2P(DREX0_PF_SECURE_BASE,	0x10430000),
	DEFINE_V2P(DREXPHY0_BASE,	0x10440000),

	DEFINE_V2P(SYSREG_CPUCL0_BASE,	0x10910000),
	DEFINE_V2P(SYSREG_G3D_BASE,	0x11450000),
	DEFINE_V2P(SYSREG_FSYS_BASE,	0x13720000),
	DEFINE_V2P(SYSREG_MIF_BASE,	0x10450000),
	DEFINE_V2P(SYSREG_PERI_BASE,	0x101D0000),
	DEFINE_V2P(SYSREG_MFCMSCL_BASE,	0x12CA0000),
	DEFINE_V2P(SYSREG_ISP_BASE,	0x144F0000),
	DEFINE_V2P(SYSREG_DISPAUD_BASE,	0x148F0000),
};

int num_of_v2psfrmap = sizeof(v2psfrmap) / sizeof(v2psfrmap[0]);
void *spinlock_enable_offset = (void *)PMU_CPUCL0_BASE;
#endif
