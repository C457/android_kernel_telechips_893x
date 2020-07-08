
/****************************************************************************
 * linux/drivers/video/tca_fb_output.c
 *
 * Author:  <linux@telechips.com>
 * Created: March 18, 2012
 * Description: TCC display Driver
 *
 * Copyright (C) 20010-2011 Telechips 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *****************************************************************************/
/*****************************************************************************
*
* Header Files Include
*
******************************************************************************/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/irq.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/div64.h>
#include <asm/mach/map.h>
#include <asm/mach-types.h>
#ifdef CONFIG_PM
#include <linux/pm.h>
#endif

#include <mach/bsp.h>
#include <mach/tca_ckc.h>
#include <mach/tca_lcdc.h>

#include <linux/cpufreq.h>
#include <linux/wait.h>
#include <linux/kthread.h>

#include <plat/pmap.h>
#include <mach/tccfb_ioctrl.h>
#include <mach/tcc_grp_ioctrl.h>
#include <mach/tcc_scaler_ctrl.h>
#include <mach/tcc_wmixer_ioctrl.h>
#include <mach/tca_fb_output.h>
#include <mach/tcc_composite_ioctl.h>
#include <mach/tcc_component_ioctl.h>
#include <mach/tca_fb_hdmi.h>
#include <mach/tccfb_ioctrl.h>

#include <mach/vioc_outcfg.h>
#include <mach/vioc_rdma.h>
#include <mach/vioc_wdma.h>
#include <mach/vioc_wmix.h>
#include <mach/vioc_disp.h>
#include <mach/vioc_global.h>
#include <mach/vioc_config.h>
#include <mach/vioc_scaler.h>
#include <mach/vioc_api.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include "../tccfb.h"

#if 0
#define dprintk(msg...)	 { printk( "tca_fb_output: " msg); }
#else
#define dprintk(msg...)	 
#endif


#define FB_SCALE_MAX_WIDTH		1280
#define FB_SCALE_MAX_HEIGHT		720


#define RDMA_UVI_MAX_WIDTH             3072

void grp_rotate_ctrl(G2D_BITBLIT_TYPE *g2d_p);

static char *fb_scaler_pbuf0;
static char *fb_scaler_pbuf1;
static char *fb_g2d_pbuf0;
static char *fb_g2d_pbuf1;

static char *output_attach_pbuf0;
static char *output_attach_pbuf1;
static char *output_attach_pbuf2;
static char *output_attach_pbuf3;

#define MOUSE_CURSOR_MAX_WIDTH			200
#define MOUSE_CURSOR_MAX_HEIGHT			200

#define MOUSE_CURSOR_BUFF_SIZE		(MOUSE_CURSOR_MAX_WIDTH*MOUSE_CURSOR_MAX_HEIGHT*4)

static char *pMouseBuffer1;
static char *pMouseBuffer2;
static char MouseBufIdx = 0;
static unsigned int mouse_cursor_width = 0;
static unsigned int mouse_cursor_height = 0;

dma_addr_t		Gmap_dma;	/* physical */
u_char *		Gmap_cpu;	/* virtual */

dma_addr_t		FB_Init_dma;	/* physical */
u_char *			FB_Init_cpu;	/* virtual */

#if defined(CONFIG_HIBERNATION) 
extern unsigned int do_hibernation;
#endif

/*****************************************************************************
*
* Defines
*
******************************************************************************/
//#define TCC_FB_UPSCALE

/*****************************************************************************
*
* structures
*
******************************************************************************/

typedef struct {
	unsigned int output;
	unsigned int ctrl;
	unsigned int width;
	unsigned int height;
	unsigned int interlace;
} output_video_img_info;

struct output_struct {
	spinlock_t lock;
	wait_queue_head_t waitq;
	char state;
};
static struct output_struct Output_struct;
static struct output_struct Output_lcdc0_struct;
static struct output_struct Output_lcdc1_struct;

typedef struct {
	unsigned int wmix_num;
	unsigned int src0_fmt;
	unsigned int src0_wd;
	unsigned int src0_ht;
	unsigned int dst0_sx;
	unsigned int dst0_sy;
	unsigned int src1_fmt;
	unsigned int src1_wd;
	unsigned int src1_ht;
	unsigned int dst1_sx;
	unsigned int dst1_sy;
	unsigned int dst_fmt;
	unsigned int dst_wd;
	unsigned int dst_ht;
	char *src0_addr;
	char *src1_addr;
	char *dst_addr;
} output_wmixer_info;

/*****************************************************************************
*
* Variables
*
******************************************************************************/

/*****************************************************************************
*
* Functions
*
******************************************************************************/
	
typedef struct {
	unsigned int LCDC_N;
	VIOC_DISP *pVIOC_DispBase;
	VIOC_WMIX* pVIOC_WMIXBase;
	VIOC_RDMA* pVIOC_RDMA_FB;
#ifdef MVC_PROCESS
	VIOC_RDMA* pVIOC_RDMA_FB_3D;
#endif
	VIOC_RDMA* pVIOC_RDMA_Video;
	VIOC_RDMA* pVIOC_RDMA_Mouse;
}DisplayOutPut_S;

static DisplayOutPut_S pDISP_OUTPUT[TCC_OUTPUT_MAX];

static struct clk *lcdc0_output_clk;
static struct clk *lcdc1_output_clk;

static char output_lcdc[TCC_OUTPUT_MAX];
char output_lcdc_onoff = 0;
static char buf_index = 0;
static unsigned int FBimg_buf_addr;
static unsigned int output_layer_ctrl[TCC_OUTPUT_MAX] = {0, 0};

static unsigned char OutputType = 0;
static unsigned char UseVSyncInterrupt = 0;
static tcc_display_resize uiOutputResizeMode;
static lcdc_chroma_params output_chroma;
static output_video_img_info output_video_img;

static char output_3d_ui_flag = 0;
static char output_3d_ui_mode = 0;
static char output_3d_ui_enable = 0;
static char output_3d_ui_plugin = 0;
static char output_3d_ui_index = 0;
static char output_ui_resize_count = 0;
struct tccfb_info *fbInfo = NULL;

struct inode scaler_inode;
struct file scaler_filp;
struct inode scaler_3d_inode;
struct file scaler_3d_filp;
struct inode scaler_attach_inode;
struct file scaler_attach_filp;

int (*scaler_ioctl) (struct file *, unsigned int, unsigned long);
int (*scaler_open) (struct inode *, struct file *);
int (*scaler_release) (struct inode *, struct file *);

int tccxxx_scaler_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
int tccxxx_scaler_release(struct inode *inode, struct file *filp);
int tccxxx_scaler_open(struct inode *inode, struct file *filp);
int tccxxx_scaler0_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
int tccxxx_scaler0_release(struct inode *inode, struct file *filp);
int tccxxx_scaler0_open(struct inode *inode, struct file *filp);
int tccxxx_scaler1_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
int tccxxx_scaler1_release(struct inode *inode, struct file *filp);
int tccxxx_scaler1_open(struct inode *inode, struct file *filp);
int tccxxx_scaler2_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
int tccxxx_scaler2_release(struct inode *inode, struct file *filp);
int tccxxx_scaler2_open(struct inode *inode, struct file *filp);

struct inode g2d_inode;
struct file g2d_filp;

int tccxxx_grp_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
int tccxxx_grp_release(struct inode *inode, struct file *filp);
int tccxxx_grp_open(struct inode *inode, struct file *filp);

int (*g2d_ioctl) ( struct file *, unsigned int, unsigned long);
int (*g2d_open) (struct inode *, struct file *);
int (*g2d_release) (struct inode *, struct file *);

struct inode wmixer_3d_inode;
struct file wmixer_3d_filp;
struct inode wmixer_attach_inode;
struct file wmixer_attach_filp;

long tccxxx_wmixer1_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
int tccxxx_wmixer1_release(struct inode *inode, struct file *filp);
int tccxxx_wmixer1_open(struct inode *inode, struct file *filp);

#if defined(CONFIG_TCC_CLOCK_TABLE)
extern struct tcc_freq_table_t gtHdmiClockLimitTable;
#endif//CONFIG_CPU_FREQ
extern unsigned int tca_get_lcd_lcdc_num(viod);
extern unsigned int tca_get_output_lcdc_num(viod);

static char tcc_output_attach_flag = 0;
static char tcc_output_attach_index = 0;
static char tcc_output_attach_index_memory = 0;
static char tcc_output_attach_index_display = 0;
static char tcc_output_attach_lcdc = 0;
static char tcc_output_attach_output = 0;
static char tcc_output_attach_resolution = 0;
static char tcc_output_attach_update = 0;
static char tcc_output_attach_starter = 0;
static char tcc_output_attach_plugin = 0;
static char tcc_output_attach_state = 1;
static char tcc_output_attach_update_count = 0;
static char tcc_output_attach_force_disabled = 0;
static struct work_struct tcc_output_attach_work_q;
extern struct display_platform_data tcc_display_data;
#if defined(CONFIG_FB_TCC_COMPOSITE)
extern void tcc_composite_attach(int lcdc_num, char mode, char onoff);
#endif
#if defined(CONFIG_FB_TCC_COMPONENT)
extern void tcc_component_attach(int lcdc_num, char mode, char onoff);
#endif

#ifdef CONFIG_VIOC_FIFO_UNDER_RUN_COMPENSATION
extern void hdmi_start(void);
extern void hdmi_stop(void);
#endif//

#if defined(MVC_PROCESS)
extern int hdmi_get_VBlank(void);
#endif

static irqreturn_t TCC_OUTPUT_LCDC_Handler(int irq, void *dev_id)
{
	static int VIOCFifoUnderRun = 0;
	unsigned int DD_Status = 0;

	if(pDISP_OUTPUT[OutputType].pVIOC_DispBase == NULL)
	{
		printk("%s - Err: Output LCDC is not valid, OutputType=%d\n", __func__, OutputType);
		return IRQ_HANDLED;
	}

	DD_Status = pDISP_OUTPUT[OutputType].pVIOC_DispBase->uLSTATUS.nREG;
	
 	dprintk("%s lcdc_struct.state:%x STATUS:0x%x OutputType:%d \n",__func__, Output_struct.state, DD_Status, OutputType);

#ifdef CONFIG_VIOC_FIFO_UNDER_RUN_COMPENSATION
	if(DD_Status & VIOC_DISP_IREQ_FU_MASK)
	{
		if(OutputType == TCC_OUTPUT_HDMI)
			hdmi_stop();
		
		pDISP_OUTPUT[OutputType].pVIOC_DispBase->uLSTATUS.nREG = VIOC_DISP_IREQ_FU_MASK;
		VIOC_DISP_TurnOff(pDISP_OUTPUT[OutputType].pVIOC_DispBase);
		VIOCFifoUnderRun++;
		dprintk("%s VIOC_DISP_IREQ_FU_MASK :%d STATUS:0x%x LCDC_NUM:%d \n",__func__,VIOCFifoUnderRun, DD_Status, pDISP_OUTPUT[OutputType].LCDC_N);				
	}

	if(DD_Status & VIOC_DISP_IREQ_DD_MASK)
	{
		pDISP_OUTPUT[OutputType].pVIOC_DispBase->uLSTATUS.nREG = VIOC_DISP_IREQ_DD_MASK;

		printk("%s DISABEL DONE:%d STATUS:0x%x LCDC_NUM:%d \n",__func__,VIOCFifoUnderRun, DD_Status, pDISP_OUTPUT[OutputType].LCDC_N);	

		if(VIOCFifoUnderRun)		
		{
			// reset vioc display path
			vioc_display_device_reset(pDISP_OUTPUT[OutputType].LCDC_N);

			VIOC_DISP_TurnOn(pDISP_OUTPUT[OutputType].pVIOC_DispBase);

			VIOCFifoUnderRun = 0;	

			if(OutputType == TCC_OUTPUT_HDMI)
				hdmi_start();
		}
	}
#endif//CONFIG_VIOC_FIFO_UNDER_RUN_COMPENSATION

	if(DD_Status & VIOC_DISP_IREQ_RU_MASK)
	{
		pDISP_OUTPUT[OutputType].pVIOC_DispBase->uLSTATUS.nREG = VIOC_DISP_IREQ_RU_MASK;
		
		#if defined(CONFIG_TCC_OUTPUT_ATTACH) && (defined(CONFIG_HDMI_DISPLAY_LASTFRAME) && defined(CONFIG_ARCH_TCC892X))
			if((DD_Status & 0x3F) == 0x00)
				return IRQ_HANDLED;
		#endif

		struct tccfb_info *fbdev = dev_id;
		if(fbdev!=NULL && fbInfo == fbdev ){
			fbdev->vsync_timestamp = ktime_get();
			if(fbdev->active_vsync)
			{
				schedule_work(&fbdev->vsync_work);
			}
		}

		if(tcc_output_attach_flag)
		{
			#if defined(CONFIG_HDMI_DISPLAY_LASTFRAME) && defined(CONFIG_ARCH_TCC892X)
				if(tcc_output_attach_update_count)
				{
					if(schedule_work(&tcc_output_attach_work_q) == 0)
					{
						//printk("attach error: cannot schedule work!!!\n");
					}

					tcc_output_attach_index = tcc_output_attach_index_display;

					tcc_output_attach_update_count--;
				}

				if(tcc_output_attach_update)
				{
					tcc_output_attach_update_count++;
					tcc_output_attach_update = 0;

					tcc_output_attach_index_display = tcc_output_attach_index_memory;
					tcc_output_attach_index_memory = !tcc_output_attach_index_memory;
				}
			#else
				if(tcc_output_attach_update)
				{
					TCC_OUTPUT_FB_AttachUpdateScreen(pDISP_OUTPUT[OutputType].LCDC_N);
					tcc_output_attach_update = 0;
				}
			#endif
		}
		
		if(Output_struct.state == 0){
			Output_struct.state = 1;
			wake_up_interruptible(&Output_struct.waitq);
		}

	}

	return IRQ_HANDLED;
}

static irqreturn_t TCC_OUTPUT_LCDC0_Handler(int irq, void *dev_id)
{
	unsigned int DD_Status;
	VIOC_DISP *pVIOC = (VIOC_DISP *)tcc_p2v(HwVIOC_DISP0);

	if((pVIOC->uCTRL.nREG & HwDISP_LEN) == 0)
	{
		printk("%s - Err: Output LCDC is not valid\n", __func__);
		return IRQ_HANDLED;
	}

	DD_Status = pVIOC->uLSTATUS.nREG;
	
	struct tccfb_info *fbdev = dev_id;
	if(fbdev!=NULL && fbInfo == fbdev ){
		fbdev->vsync_timestamp = ktime_get();
		if(fbdev->active_vsync)
		{
			schedule_work(&fbdev->vsync_work);
		}
	}

 	dprintk("%s lcdc_struct.state:%d STATUS:0x%x\n", __func__, Output_lcdc0_struct.state, DD_Status);

	BITCSET(pVIOC->uLSTATUS.nREG, 0xFFFFFFFF, 0xFFFFFFFF);

	if(Output_lcdc0_struct.state == 0){
		Output_lcdc0_struct.state = 1;
		wake_up_interruptible(&Output_lcdc0_struct.waitq);
	}

	return IRQ_HANDLED;
}

static irqreturn_t TCC_OUTPUT_LCDC1_Handler(int irq, void *dev_id)
{
	unsigned int DD_Status;
	VIOC_DISP *pVIOC = (VIOC_DISP *)tcc_p2v(HwVIOC_DISP1);

	if((pVIOC->uCTRL.nREG & HwDISP_LEN) == 0)
	{
		printk("%s - Err: Output LCDC is not valid\n", __func__);
		return IRQ_HANDLED;
	}

	DD_Status = pVIOC->uLSTATUS.nREG;
	
	struct tccfb_info *fbdev = dev_id;
	if(fbdev!=NULL && fbInfo == fbdev ){
		fbdev->vsync_timestamp = ktime_get();
		if(fbdev->active_vsync)
		{
			schedule_work(&fbdev->vsync_work);
		}
	}

 	dprintk("%s lcdc_struct.state:%d STATUS:0x%x\n", __func__, Output_lcdc1_struct.state, DD_Status);

	BITCSET(pVIOC->uLSTATUS.nREG, 0xFFFFFFFF, 0xFFFFFFFF);

	if(Output_lcdc1_struct.state == 0){
		Output_lcdc1_struct.state = 1;
		wake_up_interruptible(&Output_lcdc1_struct.waitq);
	}

	return IRQ_HANDLED;
}

void TCC_OUTPUT_LCDC_Init(void)
{
	unsigned int lcd_lcdc_num;
	unsigned int output_lcdc_num;
	pmap_t pmap;
	VIOC_WMIX* pWIXBase;

	dprintk(" %s \n", __func__);

	lcdc0_output_clk = clk_get(0, "lcdc0");
	if (IS_ERR(lcdc0_output_clk))
		lcdc0_output_clk = NULL;

	lcdc1_output_clk = clk_get(0, "lcdc1");
	if (IS_ERR(lcdc1_output_clk))
		lcdc1_output_clk = NULL;

	pmap_get_info("fb_scale0", &pmap);
	fb_scaler_pbuf0 = (char *) pmap.base;
	pmap_get_info("fb_scale1", &pmap);
	fb_scaler_pbuf1 = (char *) pmap.base;

	pmap_get_info("fb_g2d0", &pmap);
	fb_g2d_pbuf0 = (char *) pmap.base;
	pmap_get_info("fb_g2d1", &pmap);
	fb_g2d_pbuf1 = (char *) pmap.base;

	#if defined(CONFIG_TCC_OUTPUT_ATTACH_HDMI_CVBS) || defined(CONFIG_TCC_OUTPUT_ATTACH_DUAL_AUTO)
	pmap_get_info("output_attach", &pmap);
	#else
	pmap_get_info("viqe", &pmap);
	#endif
	output_attach_pbuf0 = (char *) pmap.base;
	output_attach_pbuf1 = (char *) output_attach_pbuf0 + pmap.size/3;
	output_attach_pbuf2 = (char *) output_attach_pbuf1 + pmap.size/3;
	output_attach_pbuf3 = (char *) output_attach_pbuf2 + pmap.size/6;

	#if defined(CONFIG_ARCH_TCC893X)
	
#if 0	// just temp!!!
		pDISP_OUTPUT[TCC_OUTPUT_NONE].pVIOC_DispBase = (VIOC_DISP *)tcc_p2v(HwVIOC_DISP0);
		pDISP_OUTPUT[TCC_OUTPUT_NONE].pVIOC_RDMA_FB= (VIOC_RDMA *)tcc_p2v(HwVIOC_RDMA04);
		pDISP_OUTPUT[TCC_OUTPUT_NONE].pVIOC_WMIXBase= (VIOC_WMIX *)tcc_p2v(HwVIOC_WMIX1);

#else

		// just temp!!!
		#if defined(CONFIG_CHIP_TCC8930)
		
		pDISP_OUTPUT[TCC_OUTPUT_NONE].pVIOC_DispBase = (VIOC_DISP *)tcc_p2v(HwVIOC_DISP1);
		pDISP_OUTPUT[TCC_OUTPUT_NONE].pVIOC_RDMA_FB= (VIOC_RDMA *)tcc_p2v(HwVIOC_RDMA04);
		pDISP_OUTPUT[TCC_OUTPUT_NONE].pVIOC_WMIXBase= (VIOC_WMIX *)tcc_p2v(HwVIOC_WMIX1);			

		#else
		pDISP_OUTPUT[TCC_OUTPUT_NONE].pVIOC_DispBase = (VIOC_DISP *)tcc_p2v(HwVIOC_DISP0);
		pDISP_OUTPUT[TCC_OUTPUT_NONE].pVIOC_RDMA_FB= (VIOC_RDMA *)tcc_p2v(HwVIOC_RDMA00);
		pDISP_OUTPUT[TCC_OUTPUT_NONE].pVIOC_WMIXBase= (VIOC_WMIX *)tcc_p2v(HwVIOC_WMIX0);	

		#endif
#endif
	

	#endif


	output_lcdc_num = tca_get_output_lcdc_num();
	if(output_lcdc_num)
		pWIXBase = (VIOC_WMIX*)tcc_p2v(HwVIOC_WMIX1);
	else
		pWIXBase = (VIOC_WMIX*)tcc_p2v(HwVIOC_WMIX0);

	#if defined(CONFIG_ARCH_TCC893X)
		#if defined(CONFIG_CHIP_TCC8935S) && defined(CONFIG_STB_BOARD_DONGLE_TCC893X)
		VIOC_WMIX_SetOverlayPriority(pWIXBase, 9);		//Image2 - Image1 - Image0 - Image3
		#else
		VIOC_WMIX_SetOverlayPriority(pWIXBase, 8);		//Image2 - Image0 - Image1 - Image3
		#endif
	#else
		VIOC_WMIX_SetOverlayPriority(pWIXBase, 0);		//Image3 - Image0 - Image1 - Image2
	#endif

	memset((void *)&output_chroma, 0x00, sizeof((void *)&output_chroma));

	init_waitqueue_head(&Output_struct.waitq);
	Output_struct.state = 1;

	init_waitqueue_head(&Output_lcdc0_struct.waitq);
	Output_lcdc0_struct.state = 1;

	init_waitqueue_head(&Output_lcdc1_struct.waitq);
	Output_lcdc1_struct.state = 1;

	#if defined(CONFIG_TCC_OUTPUT_DUAL_UI)
		scaler_ioctl = tccxxx_scaler_ioctl;
		scaler_open = tccxxx_scaler_open;
		scaler_release = tccxxx_scaler_release;
	#else
		
			scaler_ioctl = tccxxx_scaler1_ioctl;
			scaler_open = tccxxx_scaler1_open;
			scaler_release = tccxxx_scaler1_release;

	#endif

	#if defined(CONFIG_TCC_GRAPHIC)
	g2d_ioctl = tccxxx_grp_ioctl;
	g2d_open = tccxxx_grp_open;
	g2d_release = tccxxx_grp_release;
	#endif//
	{
		unsigned int size = MOUSE_CURSOR_BUFF_SIZE;
		Gmap_cpu = dma_alloc_writecombine(0, size, &Gmap_dma, GFP_KERNEL);
		memset(Gmap_cpu, 0x00, MOUSE_CURSOR_BUFF_SIZE);

		pMouseBuffer1 = (char *)Gmap_dma;
		pMouseBuffer2 = (char *)(Gmap_dma + MOUSE_CURSOR_BUFF_SIZE/2);
	}

	FB_Init_cpu = dma_alloc_writecombine(0, TCC_FB_OUT_MAX_WIDTH * 4, &FB_Init_dma, GFP_KERNEL);
	memset(FB_Init_cpu, 0x00, TCC_FB_OUT_MAX_WIDTH * 4);

}

void TCC_OUTPUT_UPDATE_OnOff(char onoff, char type)
{
	unsigned int lcdc_num, vioc_scaler_plug_in;

	#if defined(CONFIG_MACH_TCC8920ST) || defined(CONFIG_MACH_TCC8930ST)
	lcdc_num = pDISP_OUTPUT[type].LCDC_N;
	#elif defined(CONFIG_TCC8925S_DISP_PORT_CHANGE)
	if( type >= TCC_OUTPUT_HDMI)
		lcdc_num = 0;
	else	
		lcdc_num = 1;
	#else
	lcdc_num = tca_get_output_lcdc_num();
	#endif

#if defined(CONFIG_ARCH_TCC893X)
	vioc_scaler_plug_in = 1;
#else
	if( pDISP_OUTPUT[type].pVIOC_DispBase == (VIOC_DISP*)tcc_p2v(HwVIOC_DISP0))
	{
		if( !(pDISP_OUTPUT[type].pVIOC_DispBase->uCTRL.nREG & HwDISP_NI ))//interlace mode
			vioc_scaler_plug_in = 0;
		else
			vioc_scaler_plug_in = 1;
	}
	else
		vioc_scaler_plug_in = 1;
#endif /* CONFIG_ARCH_TCC893X */

	#if defined(CONFIG_TCC_COMPOSITE_SIGNAL_QUALITY)
		if(type == TCC_OUTPUT_COMPOSITE)
			vioc_scaler_plug_in = 0;
#endif /* CONFIG_ARCH_TCC893X */

	if(onoff)	
	{
		if(output_lcdc_onoff == 0)
		{
			if( vioc_scaler_plug_in )
			{
				if(lcdc_num) {
					VIOC_CONFIG_PlugIn (VIOC_SC2, VIOC_SC_RDMA_04);
				#ifdef MVC_PROCESS
					if(TCC_OUTPUT_FB_GetMVCStatus())
						VIOC_CONFIG_PlugIn (VIOC_SC0, VIOC_SC_RDMA_05);
				#endif
				} else {
					VIOC_CONFIG_PlugIn (VIOC_SC2, VIOC_SC_RDMA_00);
				#ifdef MVC_PROCESS
					if(TCC_OUTPUT_FB_GetMVCStatus())
						VIOC_CONFIG_PlugIn (VIOC_SC0, VIOC_SC_RDMA_01);
				#endif
				}
			}
			else
			{
				printk("%s, M-to-M Scaler path is used for displaying UI!!\n", __func__);
	 			scaler_open((struct inode *)&scaler_inode, (struct file *)&scaler_filp);
			}

			#if defined(CONFIG_TCC_GRAPHIC)
			g2d_open((struct inode *)&g2d_inode, (struct file *)&g2d_filp);
			#endif//
			output_lcdc_onoff = 1;
		}
	}
	else 
	{
		if(output_lcdc_onoff == 1)
		{
			if( vioc_scaler_plug_in )
			{
				#if defined(CONFIG_MACH_TCC8920ST) || defined(CONFIG_MACH_TCC8930ST)
					if(VIOC_CONFIG_PlugOut(VIOC_SC2) != VIOC_PATH_DISCONNECTED)
					{
						if(lcdc_num) {
							VIOC_SC_SetSWReset(VIOC_SC2, VIOC_SC_RDMA_04, 0xFF);
						} else {
							VIOC_SC_SetSWReset(VIOC_SC2, VIOC_SC_RDMA_00, 0xFF);
						}
						dprintk("%s, scaler path is reset while plug-out!!\n");
					}
				#ifdef MVC_PROCESS
					if(TCC_OUTPUT_FB_GetMVCStatus()) {
						if(VIOC_CONFIG_PlugOut(VIOC_SC0) != VIOC_PATH_DISCONNECTED)
						{
							if(lcdc_num) {
								VIOC_SC_SetSWReset(VIOC_SC0, VIOC_SC_RDMA_05, 0xFF);
							} else {
								VIOC_SC_SetSWReset(VIOC_SC0, VIOC_SC_RDMA_01, 0xFF);
							}
						}
						dprintk("%s, scaler path is reset while plug-out!!\n");
					}
				#endif
				#else
					VIOC_CONFIG_PlugOut (VIOC_SC2);
				#endif
			}
			else
			{
				scaler_release((struct inode *)&scaler_inode, (struct file *)&scaler_filp);
			}

			#if defined(CONFIG_TCC_GRAPHIC)
			g2d_release((struct inode *)&g2d_inode, (struct file *)&g2d_filp);
			#endif//
			output_lcdc_onoff = 0;
		}
	}
}

void TCC_OUTPUT_LCDC_OnOff(char output_type, char output_lcdc_num, char onoff)
{
	int i;

	dprintk("%s output_type:%d lcdc_reg:0x%08x output_lcdc_num:%d onoff:%d \n",__func__, (unsigned int)output_type, pDISP_OUTPUT[output_type].pVIOC_DispBase, output_lcdc_num, (unsigned int)onoff);
	
	if(onoff)
	{
		OutputType = output_type;
				
		if(output_lcdc_num) 
		{
			// hdmi , composite					
			clk_enable(lcdc1_output_clk);

			pDISP_OUTPUT[output_type].pVIOC_DispBase = (VIOC_DISP*)tcc_p2v(HwVIOC_DISP1);
			pDISP_OUTPUT[output_type].pVIOC_WMIXBase= (VIOC_WMIX *)tcc_p2v(HwVIOC_WMIX1);	
			pDISP_OUTPUT[output_type].pVIOC_RDMA_FB = (VIOC_RDMA*)tcc_p2v(HwVIOC_RDMA04);
			#ifdef MVC_PROCESS
			if(TCC_OUTPUT_FB_GetMVCStatus())
			pDISP_OUTPUT[output_type].pVIOC_RDMA_FB_3D = (VIOC_RDMA*)tcc_p2v(HwVIOC_RDMA05);
			#endif

			#if defined(CONFIG_ARCH_TCC893X)
			pDISP_OUTPUT[output_type].pVIOC_RDMA_Video = (VIOC_RDMA*)tcc_p2v(HwVIOC_RDMA07);
			pDISP_OUTPUT[output_type].pVIOC_RDMA_Mouse = (VIOC_RDMA*)tcc_p2v(HwVIOC_RDMA06);
	
			#else
			pDISP_OUTPUT[output_type].pVIOC_RDMA_Video = (VIOC_RDMA*)tcc_p2v(HwVIOC_RDMA06);
			pDISP_OUTPUT[output_type].pVIOC_RDMA_Mouse = (VIOC_RDMA*)tcc_p2v(HwVIOC_RDMA07);
			#endif
		}
		else
		{	// LCD
			clk_enable(lcdc0_output_clk);

			pDISP_OUTPUT[output_type].pVIOC_DispBase = (VIOC_DISP*)tcc_p2v(HwVIOC_DISP0);
			pDISP_OUTPUT[output_type].pVIOC_WMIXBase= (VIOC_WMIX *)tcc_p2v(HwVIOC_WMIX0);	
			pDISP_OUTPUT[output_type].pVIOC_RDMA_FB = (VIOC_RDMA*)tcc_p2v(HwVIOC_RDMA00);
			#ifdef MVC_PROCESS
			if(TCC_OUTPUT_FB_GetMVCStatus())
			pDISP_OUTPUT[output_type].pVIOC_RDMA_FB_3D = (VIOC_RDMA*)tcc_p2v(HwVIOC_RDMA01);
			#endif
			#if defined(CONFIG_ARCH_TCC893X)
			pDISP_OUTPUT[output_type].pVIOC_RDMA_Video = (VIOC_RDMA*)tcc_p2v(HwVIOC_RDMA03);
			#if defined(CONFIG_CHIP_TCC8935S) && defined(CONFIG_STB_BOARD_DONGLE_TCC893X)
			pDISP_OUTPUT[output_type].pVIOC_RDMA_Mouse = (VIOC_RDMA*)tcc_p2v(HwVIOC_RDMA01);
			#else
			pDISP_OUTPUT[output_type].pVIOC_RDMA_Mouse = (VIOC_RDMA*)tcc_p2v(HwVIOC_RDMA02);
			#endif
			#else
			pDISP_OUTPUT[output_type].pVIOC_RDMA_Video = (VIOC_RDMA*)tcc_p2v(HwVIOC_RDMA02);	
			pDISP_OUTPUT[output_type].pVIOC_RDMA_Mouse = (VIOC_RDMA*)tcc_p2v(HwVIOC_RDMA03);
			#endif
		}
		
		BITCSET(pDISP_OUTPUT[output_type].pVIOC_DispBase->uCTRL.nREG,0xFFFFFFFF, 0);
		
		VIOC_RDMA_SetImageDisable( pDISP_OUTPUT[output_type].pVIOC_RDMA_FB);
		#ifdef MVC_PROCESS
		if(TCC_OUTPUT_FB_GetMVCStatus() && pDISP_OUTPUT[output_type].pVIOC_RDMA_FB_3D!=NULL){
		VIOC_RDMA_SetImageDisable( pDISP_OUTPUT[output_type].pVIOC_RDMA_FB_3D);
		}
		#endif
		VIOC_RDMA_SetImageDisable( pDISP_OUTPUT[output_type].pVIOC_RDMA_Video);
		
		if(pDISP_OUTPUT[output_type].pVIOC_RDMA_Mouse != NULL)
		VIOC_RDMA_SetImageDisable( pDISP_OUTPUT[output_type].pVIOC_RDMA_Mouse);
		
			pDISP_OUTPUT[output_type].LCDC_N = output_lcdc_num;
	
		#if defined(CONFIG_TCC_CLOCK_TABLE)
		tcc_cpufreq_set_limit_table(&gtHdmiClockLimitTable, TCC_FREQ_LIMIT_HDMI, 1);
		#endif//CONFIG_TCC_CLOCK_TABLE

		//TCC_OUTPUT_UPDATE_OnOff(1, output_type);

		tca_lcdc_interrupt_onoff(0, output_lcdc_num);
		
		#if defined(CONFIG_TCC_OUTPUT_DUAL_UI)
			if(pDISP_OUTPUT[output_type].LCDC_N) {
				request_irq(INT_VIOC_DEV1, TCC_OUTPUT_LCDC1_Handler,	IRQF_SHARED,
				"TCC_LCD1",	(fbInfo!=NULL) ? fbInfo:TCC_OUTPUT_LCDC1_Handler);
			}
			else 	{
				request_irq(INT_VIOC_DEV0, TCC_OUTPUT_LCDC0_Handler,	IRQF_SHARED,
				"TCC_LCD0",	(fbInfo!=NULL) ? fbInfo:TCC_OUTPUT_LCDC0_Handler);
			}

		#elif defined(CONFIG_TCC8925S_DISP_PORT_CHANGE)		
		
			int ret;
			tca_lcdc_interrupt_onoff(0, 0);				

			ret = request_irq(INT_VIOC_DEV0, TCC_OUTPUT_LCDC_Handler, IRQ_TYPE_EDGE_FALLING|IRQF_SHARED,
							"TCC_LCD_0",		(fbInfo!=NULL) ? fbInfo:TCC_OUTPUT_LCDC_Handler);

			tca_lcdc_interrupt_onoff(1, 0);
		#else
			if(pDISP_OUTPUT[output_type].LCDC_N) {
				request_irq(INT_VIOC_DEV1, TCC_OUTPUT_LCDC_Handler, IRQF_SHARED,
				"TCC_LCD1",	(fbInfo!=NULL) ? fbInfo:TCC_OUTPUT_LCDC_Handler);
			}
			else 	{
				request_irq(INT_VIOC_DEV0, TCC_OUTPUT_LCDC_Handler, IRQF_SHARED,
				"TCC_LCD0",	(fbInfo!=NULL) ? fbInfo:TCC_OUTPUT_LCDC_Handler);
			}
		#endif
	}
	else
	{		
		if(pDISP_OUTPUT[output_type].pVIOC_DispBase != NULL)
		{
			TCC_OUTPUT_FB_DetachOutput(0);				

		
				VIOC_RDMA_SetImageDisable( pDISP_OUTPUT[output_type].pVIOC_RDMA_FB);
				#ifdef MVC_PROCESS
				if(pDISP_OUTPUT[output_type].pVIOC_RDMA_FB_3D!=NULL)
				VIOC_RDMA_SetImageDisable( pDISP_OUTPUT[output_type].pVIOC_RDMA_FB_3D);
				#endif
				VIOC_RDMA_SetImageDisable( pDISP_OUTPUT[output_type].pVIOC_RDMA_Video);
				
				if(pDISP_OUTPUT[output_type].pVIOC_RDMA_Mouse != NULL)
				VIOC_RDMA_SetImageDisable( pDISP_OUTPUT[output_type].pVIOC_RDMA_Mouse);

				VIOC_DISP_TurnOff(pDISP_OUTPUT[output_type].pVIOC_DispBase);
				VIOC_DISP_Wait_DisplayDone(pDISP_OUTPUT[output_type].pVIOC_DispBase);
		
		

			#if defined(CONFIG_TCC_CLOCK_TABLE) && !defined(CONFIG_TCC_OUTPUT_DUAL_UI)
			tcc_cpufreq_set_limit_table(&gtHdmiClockLimitTable, TCC_FREQ_LIMIT_HDMI, 0);
			#endif//CONFIG_CPU_FREQ

			TCC_OUTPUT_UPDATE_OnOff(0, output_type);
			dprintk("lcd disable time %d \n", i);
			
			if(pDISP_OUTPUT[output_type].LCDC_N){						
				clk_disable(lcdc1_output_clk);
			}
			else {
				clk_disable(lcdc0_output_clk);
			}

			#if defined(CONFIG_TCC_OUTPUT_DUAL_UI)
				if(pDISP_OUTPUT[output_type].LCDC_N)
					free_irq(INT_VIOC_DEV1, (fbInfo!=NULL) ? fbInfo:TCC_OUTPUT_LCDC1_Handler);
				else
					free_irq(INT_VIOC_DEV0, (fbInfo!=NULL) ? fbInfo:TCC_OUTPUT_LCDC0_Handler);

			#elif defined(CONFIG_TCC8925S_DISP_PORT_CHANGE)		
					free_irq(INT_VIOC_DEV0, (fbInfo!=NULL) ? fbInfo:TCC_OUTPUT_LCDC_Handler);
			#else
				if(pDISP_OUTPUT[output_type].LCDC_N)
					free_irq(INT_VIOC_DEV1, (fbInfo!=NULL) ? fbInfo:TCC_OUTPUT_LCDC_Handler);
				else
					free_irq(INT_VIOC_DEV0, (fbInfo!=NULL) ? fbInfo:TCC_OUTPUT_LCDC_Handler);
			#endif

			pDISP_OUTPUT[output_type].pVIOC_DispBase= NULL;
			pDISP_OUTPUT[output_type].pVIOC_WMIXBase= NULL;	
			pDISP_OUTPUT[output_type].pVIOC_RDMA_FB = NULL;
			#ifdef MVC_PROCESS
			pDISP_OUTPUT[output_type].pVIOC_RDMA_FB_3D = NULL;
			#endif
			pDISP_OUTPUT[output_type].pVIOC_RDMA_Video = NULL;
			pDISP_OUTPUT[output_type].pVIOC_RDMA_Mouse = NULL;

		}
	}

	memset((void *)output_layer_ctrl, 0x00, sizeof(output_layer_ctrl));
}

void TCC_OUTPUT_LCDC_CtrlLayer(char output_type, char interlace, char format)
{
	dprintk(" %s interlace:%d format:%d  \n", __func__, interlace, format);
	
	output_layer_ctrl[output_type] = HwDMA_IEN | format;

	if(interlace)
		output_layer_ctrl[output_type] |= HwDMA_INTL;
}

void TCC_OUTPUT_LCDC_CtrlChroma(lcdc_chroma_params lcdc_chroma)
{
	dprintk("%s\n", __func__);
	
	output_chroma.enable = lcdc_chroma.enable;
	output_chroma.color = lcdc_chroma.color;
}

void TCC_OUTPUT_LCDC_CtrlWinMixer(output_wmixer_info output_wmixer, unsigned int onoff)
{
	PVIOC_WMIX pVIOC_WMIX = NULL;
	PVIOC_RDMA pVIOC_RDMA0 = NULL;
	PVIOC_RDMA pVIOC_RDMA1 = NULL;
	PVIOC_WDMA pVIOC_WDMA = NULL;

	switch(output_wmixer.wmix_num)
	{
		case 0:
		case 1:
			break;

		case 3:
			pVIOC_WMIX = (PVIOC_WMIX)tcc_p2v(HwVIOC_WMIX3);	
			pVIOC_RDMA0 = (PVIOC_RDMA)tcc_p2v(HwVIOC_RDMA12);
			pVIOC_RDMA1 = (PVIOC_RDMA)tcc_p2v(HwVIOC_RDMA13);	
			pVIOC_WDMA = (PVIOC_WDMA)tcc_p2v(HwVIOC_WDMA03);
			VIOC_CONFIG_RDMA12PathCtrl(0 /* RDMA14 */);
			VIOC_CONFIG_WMIXPath(WMIX30, 1 /* Mixing */);
			break;

		case 4:
			pVIOC_WMIX = (PVIOC_WMIX)tcc_p2v(HwVIOC_WMIX4);	
			pVIOC_RDMA0 = (PVIOC_RDMA)tcc_p2v(HwVIOC_RDMA14);
			pVIOC_RDMA1 = (PVIOC_RDMA)tcc_p2v(HwVIOC_RDMA15);	
			pVIOC_WDMA = (PVIOC_WDMA)tcc_p2v(HwVIOC_WDMA04);
			VIOC_CONFIG_RDMA14PathCtrl(0 /* RDMA14 */);
			VIOC_CONFIG_WMIXPath(WMIX40, 1 /* Mixing */);
			break;
	}

	if(pVIOC_WMIX == NULL)
		return;

	dprintk("%s\n", __func__);

	dprintk("s0_wd=%d, s0_ht=%d, d0_sx=%d, d0_sy=%d, s1_wd=%d, s1_ht=%d, d1_sx=%d, d1_sy=%d\n",
			output_wmixer.src0_wd, output_wmixer.src0_ht, output_wmixer.dst0_sx, output_wmixer.dst0_sy,
			output_wmixer.src1_wd, output_wmixer.src1_ht, output_wmixer.dst1_sx, output_wmixer.dst1_sy);
	
	if(onoff)
	{
		// set to VRDMA switch path
		//VIOC_CONFIG_RDMA14PathCtrl(0 /* RDMA14 */);

		// set to RDMA0(VRDMA)
		VIOC_RDMA_SetImageAlphaEnable(pVIOC_RDMA0, 1);
		VIOC_RDMA_SetImageFormat(pVIOC_RDMA0, output_wmixer.src0_fmt);
		VIOC_RDMA_SetImageSize(pVIOC_RDMA0, output_wmixer.src0_wd, output_wmixer.src0_ht);
		VIOC_RDMA_SetImageOffset(pVIOC_RDMA0, output_wmixer.src0_fmt, output_wmixer.src0_wd);
		VIOC_RDMA_SetImageBase(pVIOC_RDMA0, output_wmixer.src0_addr, NULL, NULL);

		// set to RDMA1(GRDMA)
		VIOC_RDMA_SetImageAlphaEnable(pVIOC_RDMA1, 1);
		VIOC_RDMA_SetImageFormat(pVIOC_RDMA1, output_wmixer.src1_fmt);
		VIOC_RDMA_SetImageSize(pVIOC_RDMA1, output_wmixer.src1_wd, output_wmixer.src1_ht);
		VIOC_RDMA_SetImageOffset(pVIOC_RDMA1, output_wmixer.src1_fmt, output_wmixer.src1_wd);
		VIOC_RDMA_SetImageBase(pVIOC_RDMA1, output_wmixer.src1_addr, NULL, NULL);

		// set to WMIX 
		//VIOC_CONFIG_WMIXPath(WMIX40, 0 /* by-pass */);
		VIOC_WMIX_SetSize(pVIOC_WMIX, output_wmixer.dst_wd, output_wmixer.dst_ht);
		VIOC_WMIX_SetPosition(pVIOC_WMIX, 0, output_wmixer.dst0_sx, output_wmixer.dst0_sy);
		VIOC_WMIX_SetPosition(pVIOC_WMIX, 1, output_wmixer.dst1_sx, output_wmixer.dst1_sy);
		VIOC_WMIX_SetUpdate(pVIOC_WMIX);

		VIOC_RDMA_SetImageEnable(pVIOC_RDMA0);
		VIOC_RDMA_SetImageEnable(pVIOC_RDMA1);

		// set to VWRMA
		VIOC_WDMA_SetImageFormat(pVIOC_WDMA, output_wmixer.dst_fmt);
		VIOC_WDMA_SetImageSize(pVIOC_WDMA, output_wmixer.dst_wd, output_wmixer.dst_ht);
		VIOC_WDMA_SetImageOffset(pVIOC_WDMA, output_wmixer.dst_fmt, output_wmixer.dst_wd);
		VIOC_WDMA_SetImageBase(pVIOC_WDMA, output_wmixer.dst_addr, NULL, NULL);
		VIOC_WDMA_SetImageEnable(pVIOC_WDMA, 0 /* OFF */);
		pVIOC_WDMA->uIRQSTS.nREG = 0xFFFFFFFF; // wdma status register all clear.
	}
	else
	{
		VIOC_RDMA_SetImageDisable(pVIOC_RDMA0);
		VIOC_RDMA_SetImageDisable(pVIOC_RDMA1);
	}
}

char TCC_FB_G2D_FmtConvert(unsigned int width, unsigned int height, unsigned int g2d_rotate, unsigned int Sfmt, unsigned int Tfmt, unsigned int Saddr, unsigned int Taddr)
{
	G2D_BITBLIT_TYPE g2d_p;
	
	memset(&g2d_p, 0x00, sizeof(G2D_BITBLIT_TYPE));

	g2d_p.responsetype = G2D_POLLING;
	g2d_p.src0 = (unsigned int)Saddr;
	
	if(Sfmt == TCC_LCDC_IMG_FMT_RGB888)
		g2d_p.srcfm.format = GE_ARGB8888;
	else if(Sfmt == TCC_LCDC_IMG_FMT_YUYV)
		g2d_p.srcfm.format = GE_YUV422_sq;		
	else
		g2d_p.srcfm.format = GE_RGB565;

	g2d_p.srcfm.data_swap = 0;
	g2d_p.src_imgx = width;
	g2d_p.src_imgy = height;

	g2d_p.ch_mode = g2d_rotate;

	g2d_p.crop_offx = 0;
	g2d_p.crop_offy = 0;
	g2d_p.crop_imgx = width;
	g2d_p.crop_imgy = height;

	g2d_p.tgt0 = (unsigned int)Taddr;	// destination image address

	if(Tfmt == TCC_LCDC_IMG_FMT_RGB888)
		g2d_p.tgtfm.format = GE_ARGB8888;
	else if(Tfmt == TCC_LCDC_IMG_FMT_YUYV)
		g2d_p.tgtfm.format = GE_YUV422_sq;		
	else
		g2d_p.tgtfm.format = GE_RGB565;

	// destinaion f
	g2d_p.tgtfm.data_swap = 0;
	if((g2d_rotate == ROTATE_270) || (g2d_rotate == ROTATE_90)) 	{
		g2d_p.dst_imgx = height;
		g2d_p.dst_imgy = width;
	}
	else		{
		g2d_p.dst_imgx = width;
		g2d_p.dst_imgy = height;
	}
	
	g2d_p.dst_off_x = 0;
	g2d_p.dst_off_y = 0;
	g2d_p.alpha_value = 255;

	g2d_p.alpha_type = G2d_ALPHA_NONE;

	#if defined(CONFIG_TCC_GRAPHIC)
	#if defined(CONFIG_TCC_EXCLUSIVE_UI_LAYER)
		grp_rotate_ctrl(&g2d_p);
	#else
		g2d_ioctl( (struct file *)&g2d_filp, TCC_GRP_ROTATE_IOCTRL_KERNEL, &g2d_p);
	#endif
	#endif//
	return 1;
}

int TCC_OUTPUT_SetOutputResizeMode(tcc_display_resize mode)
{
	printk("%s : mode_x = %d, mode_y = %d\n", __func__, mode.resize_x, mode.resize_y);
	
	uiOutputResizeMode = mode;

	return 1;
}

void TCC_OUTPUT_LCDC_OutputEnable(char output_lcdc, unsigned int onoff)
{
	VIOC_DISP *pDISP;

	if(output_lcdc)	{
		pDISP = (VIOC_DISP *)tcc_p2v(HwVIOC_DISP1);
	}
	else		{
		pDISP = (VIOC_DISP *)tcc_p2v(HwVIOC_DISP0);
	}
	
	if(onoff)
		VIOC_DISP_TurnOn(pDISP);
	else
		VIOC_DISP_TurnOff(pDISP);
}


char TCC_OUTPUT_FB_init( unsigned int type)
{
	unsigned int regl = 0, lcd_width = 0, lcd_height = 0, img_width = 0, img_height = 0;
	VIOC_SC *pSC;

	pSC = (VIOC_SC *)tcc_p2v(HwVIOC_SC2);


	if(pDISP_OUTPUT[type].pVIOC_DispBase == NULL)
	{
		printk("%s - Err: Output LCDC is not valid, type=%d\n", __func__, type);
		return 0;
	}
	
	regl = pDISP_OUTPUT[type].pVIOC_DispBase->uLSIZE.nREG; // get LCD size 
	lcd_width = (regl & 0xFFFF);
	lcd_height = ((regl>>16) & 0xFFFF);

	if(((!lcd_width) || (!lcd_height)) || ((lcd_width > TCC_FB_OUT_MAX_WIDTH) || (lcd_height > TCC_FB_OUT_MAX_HEIGHT)))
	{
		printk(" %s ERROR  type=%d LCD W:%d H:%d \n", __func__, type, lcd_width, lcd_height);
		return 0;
	}
	
	VIOC_SC_SetBypass (pSC, OFF);
	VIOC_SC_SetSrcSize(pSC, lcd_width, lcd_height);
	VIOC_SC_SetDstSize (pSC, lcd_width, lcd_height);			// set destination size in scaler
	VIOC_SC_SetOutSize (pSC, lcd_width, lcd_height);			// set output size in scaer
	VIOC_SC_SetUpdate (pSC);								// Scaler update
	VIOC_WMIX_SetUpdate (pDISP_OUTPUT[type].pVIOC_WMIXBase);			// WMIX update
	
	// size
	VIOC_RDMA_SetImageSize(pDISP_OUTPUT[type].pVIOC_RDMA_FB, lcd_width, lcd_height);

	// format
	VIOC_RDMA_SetImageFormat(pDISP_OUTPUT[type].pVIOC_RDMA_FB, TCC_LCDC_IMG_FMT_RGB888);

	
	VIOC_RDMA_SetImageIntl(pDISP_OUTPUT[type].pVIOC_RDMA_FB, 0);

	//offset
	VIOC_RDMA_SetImageOffset(pDISP_OUTPUT[type].pVIOC_RDMA_FB, TCC_LCDC_IMG_FMT_RGB888, img_width);

	// alpha & chroma key color setting
	VIOC_RDMA_SetImageAlphaSelect(pDISP_OUTPUT[type].pVIOC_RDMA_FB, TRUE);
	VIOC_RDMA_SetImageAlphaEnable(pDISP_OUTPUT[type].pVIOC_RDMA_FB, TRUE);
	
	VIOC_RDMA_SetImageBase(pDISP_OUTPUT[type].pVIOC_RDMA_FB, FB_Init_dma, 0, 0);

	VIOC_RDMA_SetImageEnable(pDISP_OUTPUT[type].pVIOC_RDMA_FB);

	VIOC_WMIX_SetUpdate(pDISP_OUTPUT[type].pVIOC_WMIXBase);
	

	return 1;
}




char TCC_OUTPUT_FB_Process(unsigned int width, unsigned int height, unsigned int bits_per_pixel, unsigned int addr, unsigned int type)
{
	unsigned int regl = 0, g2d_rotate_need = 0, g2d_format_need = 0, scaler_need= 0, m2m_onthefly_use = 0, interlace_output = 0;
	unsigned int lcd_width = 0, lcd_height = 0, lcd_h_pos = 0, lcd_w_pos = 0;
	unsigned int img_width = 0, img_height = 0, ifmt = 0, iPrevfmt = 0;
	unsigned int  chromaR = 0, chromaG = 0, chromaB = 0;
	unsigned int  chroma_en = 0, alpha_blending_en = 0, alpha_type = 0;
	unsigned int vioc_scaler_plug_in = 0;
	char mode_3d = 0;

	SCALER_TYPE fbscaler;
	VIOC_SC *pSC;

	pSC = (VIOC_SC *)tcc_p2v(HwVIOC_SC2);

	memset(&fbscaler, 0x00, sizeof(SCALER_TYPE));

	if(pDISP_OUTPUT[type].pVIOC_DispBase == NULL)
	{
		printk("%s - Err: Output LCDC is not valid, type=%d\n", __func__, type);
		return 0;
	}
	
	dprintk(" %s width=%d, height=%d, bpp=%d, type=%d\n", __func__, width, height, bits_per_pixel, type);

	regl = pDISP_OUTPUT[type].pVIOC_DispBase->uLSIZE.nREG; // get LCD size 
	lcd_width = (regl & 0xFFFF);
	lcd_height = ((regl>>16) & 0xFFFF);

	if(((!lcd_width) || (!lcd_height)) || ((lcd_width > TCC_FB_OUT_MAX_WIDTH) || (lcd_height > TCC_FB_OUT_MAX_HEIGHT)))
	{
		printk(" ##### %s ERROR width=%d, height=%d, bpp=%d, type=%d LCD W:%d H:%d \n", __func__, width, height, bits_per_pixel, type, lcd_width, lcd_height);
		return 0;
	}

	#if	defined(CONFIG_HDMI_FB_ROTATE_90)||defined(CONFIG_HDMI_FB_ROTATE_180)||defined(CONFIG_HDMI_FB_ROTATE_270)
	g2d_rotate_need = 1;
	#endif//

	#if 0//defined(CONFIG_TCC_COMPOSITE_SIGNAL_QUALITY)
	if(type == TCC_OUTPUT_COMPOSITE)
		g2d_format_need = 1;
	#endif

	if(lcd_width != width || lcd_height != height || uiOutputResizeMode.resize_x || uiOutputResizeMode.resize_y)
		scaler_need = 1;

	if(bits_per_pixel == 32 )	{
		chroma_en = 0;
		alpha_type = 1;
		alpha_blending_en = 1;
		ifmt = TCC_LCDC_IMG_FMT_RGB888;		
	}
	else  {
		chroma_en = 1;
		alpha_type = 0;
		alpha_blending_en = 0;
		ifmt = TCC_LCDC_IMG_FMT_RGB565;
	}

#if defined(CONFIG_ARCH_TCC893X)
	vioc_scaler_plug_in = 1;
#else
	if( pDISP_OUTPUT[type].pVIOC_DispBase == (VIOC_DISP*)tcc_p2v(HwVIOC_DISP0))
	{
		if( !(pDISP_OUTPUT[type].pVIOC_DispBase->uCTRL.nREG & HwDISP_NI ))//interlace mode
			vioc_scaler_plug_in = 0;
		else
			vioc_scaler_plug_in = 1;
	}
	else
		vioc_scaler_plug_in = 1;
#endif /* CONFIG_ARCH_TCC893X */

	#if defined(CONFIG_TCC_COMPOSITE_SIGNAL_QUALITY)
		if(type == TCC_OUTPUT_COMPOSITE)
			vioc_scaler_plug_in = 0;
	#endif

	if( vioc_scaler_plug_in == 1 )
	{
		if(g2d_rotate_need || g2d_format_need)
			UseVSyncInterrupt = 0;
		else
			UseVSyncInterrupt = 1;
	}
	else
	{
		if(g2d_rotate_need || g2d_format_need || scaler_need )
		{
			#if defined(CONFIG_MACH_TCC8930ST)
				UseVSyncInterrupt = 1;
			#else
				UseVSyncInterrupt = 0;
			#endif
		}
		else
		{
			UseVSyncInterrupt = 1;
		}
	}

	#if defined(CONFIG_TCC_COMPOSITE_SIGNAL_QUALITY)
		if(type == TCC_OUTPUT_COMPOSITE)
			UseVSyncInterrupt = 1;
	#endif
			
	img_width = width;
	img_height = height;
	FBimg_buf_addr = addr;

	dprintk(" %s width=%d, height=%d, bpp=%d, lcd_width=%d, lcd_height=%d, rotate=%d, format=%d, scale=%d, type=%d   vioc_scaler_plug_in :%d \n", 
			__func__, width, height, bits_per_pixel, lcd_width, lcd_height, g2d_rotate_need, g2d_format_need, scaler_need, type,  vioc_scaler_plug_in  );
	
	if(g2d_rotate_need || g2d_format_need)
	{
		unsigned int rotate, taddr; 
		
		if(g2d_rotate_need)		{
			#if	defined(CONFIG_HDMI_FB_ROTATE_180)
				img_width = width;
				img_height = height;
				rotate = ROTATE_180;
			#elif defined(CONFIG_HDMI_FB_ROTATE_270)
				img_width = height;
				img_height= width;
				rotate = ROTATE_270;
			#elif defined(CONFIG_HDMI_FB_ROTATE_90)
				img_width = height;
				img_height= width;
				rotate = ROTATE_90;
			#endif//
		}
		else		{
			img_width = width;
			img_height = height;
			rotate = NOOP;
		}
		if(buf_index)
			taddr = fb_g2d_pbuf0;	// destination image address
		else
			taddr = fb_g2d_pbuf1;	// destination image address

		#if defined(CONFIG_TCC_COMPOSITE_SIGNAL_QUALITY)
			if(type == TCC_OUTPUT_COMPOSITE)
			{
				TCC_FB_G2D_FmtConvert(width, height, rotate, ifmt, TCC_LCDC_IMG_FMT_YUYV, addr, taddr );
				ifmt = TCC_LCDC_IMG_FMT_YUYV;
			}
			else
			{
				TCC_FB_G2D_FmtConvert(width, height, rotate, ifmt, TCC_LCDC_IMG_FMT_RGB888, addr, taddr );
				ifmt = TCC_LCDC_IMG_FMT_RGB888;
			}
		#else
			TCC_FB_G2D_FmtConvert(width, height, rotate, ifmt, TCC_LCDC_IMG_FMT_RGB888, addr, taddr );
			ifmt = TCC_LCDC_IMG_FMT_RGB888;
		#endif
	
		FBimg_buf_addr = taddr;
	}

	if( vioc_scaler_plug_in == 0 )
	{
		if(scaler_need)
		{
			int x_offset, y_offset;
			output_wmixer_info wmixer_info;
				
			//need to add scaler component
			#if defined(CONFIG_TCC_OUTPUT_3D_UI)
				fbscaler.responsetype = SCALER_POLLING;
			#else
				fbscaler.responsetype = SCALER_NOWAIT;
			#endif
			
			fbscaler.src_Yaddr = (char *)FBimg_buf_addr;

			if(ifmt == TCC_LCDC_IMG_FMT_YUYV)
				fbscaler.src_fmt = SCALER_YUV422_sq0;
			else if(ifmt == TCC_LCDC_IMG_FMT_RGB888)
				fbscaler.src_fmt = SCALER_ARGB8888;		
			else
				fbscaler.src_fmt = SCALER_RGB565;

			fbscaler.src_ImgWidth = img_width;
			fbscaler.src_ImgHeight = img_height;

			fbscaler.src_winLeft = 0;
			fbscaler.src_winTop = 0;
			fbscaler.src_winRight = img_width;
			fbscaler.src_winBottom = img_height;

			if(buf_index)
				fbscaler.dest_Yaddr = fb_scaler_pbuf0;	// destination image address
			else
				fbscaler.dest_Yaddr = fb_scaler_pbuf1;	// destination image address

			buf_index = !buf_index;

			if((lcd_width > FB_SCALE_MAX_WIDTH) && (!m2m_onthefly_use))
			{
				m2m_onthefly_use = 0;
				img_width = lcd_width / 2;
			}
			else	{
				img_width = lcd_width;
			}

			if((lcd_height > FB_SCALE_MAX_HEIGHT) && (!m2m_onthefly_use))	{
				m2m_onthefly_use = 0;
				img_height = lcd_height/2;
			}
			else	{
				img_height = lcd_height;
			}

		#if defined(CONFIG_TCC_OUTPUT_3D_UI)
			if(TCC_OUTPUT_FB_Get3DMode(&mode_3d))
			{
				//int x_offset, y_offset, img_wd, img_ht;

				img_width -= uiOutputResizeMode.resize_x << 4;
				img_height -= uiOutputResizeMode.resize_y << 3;
				lcd_w_pos = uiOutputResizeMode.resize_x << 3;
				lcd_h_pos = uiOutputResizeMode.resize_y << 2;
				
				if(mode_3d == 0)	/* 3D MKV : SBS(SideBySide) Mode */
				{
					img_width = img_width>>1;
					VIOC_WMIX_SetPosition(pDISP_OUTPUT[type].pVIOC_WMIXBase, 0, (lcd_w_pos + img_width)>>1, lcd_h_pos); 
				}
				else				/* 3D MKV : TNB(Top&Bottom) Mode */
				{
					img_height = img_height>>1;
					VIOC_WMIX_SetPosition(pDISP_OUTPUT[type].pVIOC_WMIXBase, 0, lcd_w_pos, (lcd_h_pos + img_height)>>1);
				}
		
				VIOC_WMIX_SetPosition(pDISP_OUTPUT[type].pVIOC_WMIXBase, 0, 0, 0);
						
				ifmt = TCC_LCDC_IMG_FMT_RGB888;
				fbscaler.dest_fmt = SCALER_ARGB8888;	// destination image format
				fbscaler.dest_ImgWidth = img_width;		// destination image width
				fbscaler.dest_ImgHeight = img_height; 	// destination image height
				fbscaler.dest_winLeft = 0;
				fbscaler.dest_winTop = 0;
				fbscaler.dest_winRight = img_width;
				fbscaler.dest_winBottom = img_height;
				fbscaler.divide_path = 0x00;

				#if defined(CONFIG_TCC_COMPOSITE_SIGNAL_QUALITY)
				if(type == TCC_OUTPUT_COMPOSITE)
					fbscaler.wdma_r2y = 1;
				#endif

				scaler_ioctl((struct file *)&scaler_filp, TCC_SCALER_IOCTRL_KERENL, &fbscaler);

				dprintk("src_fmt=%d, src_ImgWidth=%d, src_ImgHeight=%d, dest_ImgWidth=%d, dest_ImgHeight=%d\n",
						fbscaler.src_fmt, fbscaler.src_ImgWidth, fbscaler.src_ImgHeight, fbscaler.dest_ImgWidth, fbscaler.dest_ImgHeight);

				FBimg_buf_addr = fbscaler.dest_Yaddr;

				TCC_OUTPUT_FB_Update3D(img_width, img_height, pDISP_OUTPUT[type].LCDC_N, 0);
			}
			else
		#endif
			{
				img_width -= uiOutputResizeMode.resize_x << 4;
				img_height -= uiOutputResizeMode.resize_y << 3;

				lcd_w_pos = uiOutputResizeMode.resize_x << 3;
				if( interlace_output )
					lcd_h_pos = uiOutputResizeMode.resize_y << 1;
				else
					lcd_h_pos = uiOutputResizeMode.resize_y << 2;

				VIOC_WMIX_SetPosition(pDISP_OUTPUT[type].pVIOC_WMIXBase, 0, lcd_w_pos, lcd_h_pos);

			#if 0//defined(CONFIG_TCC_COMPOSITE_SIGNAL_QUALITY)
				if(type == TCC_OUTPUT_COMPOSITE)
				{
					ifmt = TCC_LCDC_IMG_FMT_YUYV;
					fbscaler.dest_fmt = SCALER_YUV422_sq0;	// destination image format
				}
				else
				{
					if(bits_per_pixel == 32)
					{
						ifmt = TCC_LCDC_IMG_FMT_RGB888;
						fbscaler.dest_fmt = SCALER_ARGB8888;	// destination image format
					}
					else
					{
						ifmt = TCC_LCDC_IMG_FMT_RGB565;
						fbscaler.dest_fmt = SCALER_RGB565;		// destination image format
					}
				}
			#else
				if(bits_per_pixel == 32)
				{
					ifmt = TCC_LCDC_IMG_FMT_RGB888;
					fbscaler.dest_fmt = SCALER_ARGB8888;	// destination image format
				}
				else
				{
					ifmt = TCC_LCDC_IMG_FMT_RGB565;
					fbscaler.dest_fmt = SCALER_RGB565;		// destination image format
				}
			#endif
				fbscaler.dest_ImgWidth = img_width;		// destination image width
				fbscaler.dest_ImgHeight = img_height; 	// destination image height
				fbscaler.dest_winLeft = 0;
				fbscaler.dest_winTop = 0;
				fbscaler.dest_winRight = img_width;
				fbscaler.dest_winBottom = img_height;
				fbscaler.divide_path = 0x00;

				#if defined(CONFIG_TCC_COMPOSITE_SIGNAL_QUALITY)
				if(type == TCC_OUTPUT_COMPOSITE)
					fbscaler.wdma_r2y = 1;
				#endif

				scaler_ioctl((struct file *)&scaler_filp, TCC_SCALER_IOCTRL_KERENL, &fbscaler);

				dprintk("src_fmt=%d, src_ImgWidth=%d, src_ImgHeight=%d, dest_ImgWidth=%d, dest_ImgHeight=%d\n",
						fbscaler.src_fmt, fbscaler.src_ImgWidth, fbscaler.src_ImgHeight, fbscaler.dest_ImgWidth, fbscaler.dest_ImgHeight);

				FBimg_buf_addr = fbscaler.dest_Yaddr;
			}
		}
	}
	else
	{
		lcd_width -= uiOutputResizeMode.resize_x << 4;
		lcd_height -= uiOutputResizeMode.resize_y << 3;

	        lcd_w_pos = uiOutputResizeMode.resize_x << 3;
		if( interlace_output )
			lcd_h_pos = uiOutputResizeMode.resize_x << 1;
		else
			lcd_h_pos = uiOutputResizeMode.resize_y << 2;

		VIOC_WMIX_SetPosition(pDISP_OUTPUT[type].pVIOC_WMIXBase, 0, lcd_w_pos, lcd_h_pos);

	#if defined(CONFIG_TCC_OUTPUT_3D_UI)
		if(TCC_OUTPUT_FB_Get3DMode(&mode_3d))
		{
			VIOC_SC_SetBypass (pSC, OFF);
			VIOC_SC_SetSrcSize(pSC, width, height);

			/* 3D MKV : SBS(SideBySide) Mode */
			if(mode_3d == 0)	
			{
				VIOC_SC_SetDstSize (pSC, lcd_width>>1, lcd_height);			// set destination size in scaler
				VIOC_SC_SetOutSize (pSC, lcd_width>>1, lcd_height);			// set output size in scaer
				VIOC_WMIX_SetPosition(pDISP_OUTPUT[type].pVIOC_WMIXBase, 0, lcd_w_pos>>1, lcd_h_pos);
			}
			else
			/* 3D MKV : TNB(Top&Bottom) Mode */
			{
				VIOC_SC_SetDstSize (pSC, lcd_width, lcd_height>>1);			// set destination size in scaler
				VIOC_SC_SetOutSize (pSC, lcd_width, lcd_height>>1);			// set output size in scaer
				VIOC_WMIX_SetPosition(pDISP_OUTPUT[type].pVIOC_WMIXBase, 0, lcd_w_pos, lcd_h_pos>>1);
			}

			VIOC_SC_SetUpdate (pSC);				// Scaler update
			VIOC_WMIX_SetUpdate (pDISP_OUTPUT[type].pVIOC_WMIXBase);			// WMIX update

			TCC_OUTPUT_FB_Update3D(width, height, pDISP_OUTPUT[type].LCDC_N, 1);
		}
		else
	#endif
		{
			#ifdef MVC_PROCESS
			if(TCC_OUTPUT_FB_GetMVCStatus()==0)
			{
				VIOC_SC_SetBypass (pSC, OFF);
				VIOC_SC_SetSrcSize(pSC, width, height);
				VIOC_SC_SetDstSize (pSC, lcd_width, lcd_height);			// set destination size in scaler
				VIOC_SC_SetOutSize (pSC, lcd_width, lcd_height);			// set output size in scaer
				VIOC_SC_SetUpdate (pSC);				// Scaler update
				VIOC_WMIX_SetUpdate (pDISP_OUTPUT[type].pVIOC_WMIXBase);			// WMIX update
			}
			#else
			VIOC_SC_SetBypass (pSC, OFF);
			VIOC_SC_SetSrcSize(pSC, width, height);
			VIOC_SC_SetDstSize (pSC, lcd_width, lcd_height);			// set destination size in scaler
			VIOC_SC_SetOutSize (pSC, lcd_width, lcd_height);			// set output size in scaer
			VIOC_SC_SetUpdate (pSC);				// Scaler update
			VIOC_WMIX_SetUpdate (pDISP_OUTPUT[type].pVIOC_WMIXBase);			// WMIX update
			#endif /* 0 */
		}
	}
			
	#if (defined(CONFIG_MACH_TCC8920ST) || defined(CONFIG_MACH_TCC8930ST)) && defined(CONFIG_TCC_OUTPUT_ATTACH)
		/* get and check the image format */
		iPrevfmt = (pDISP_OUTPUT[type].pVIOC_RDMA_FB->uCTRL.nREG & 0x1F);
		if(ifmt != iPrevfmt)
		{
			if(pDISP_OUTPUT[type].pVIOC_RDMA_FB->uCTRL.nREG & Hw28)
			{
				dprintk("%s, image format is changed, prev_fmt=%d, curr_fmt=%d\n", __func__, iPrevfmt, ifmt);
				VIOC_RDMA_SetImageFormat(pDISP_OUTPUT[type].pVIOC_RDMA_FB, ifmt);
				return 0;
			}
		}
	#endif

	// size
	VIOC_RDMA_SetImageSize(pDISP_OUTPUT[type].pVIOC_RDMA_FB, img_width, img_height);

	// format
	VIOC_RDMA_SetImageFormat(pDISP_OUTPUT[type].pVIOC_RDMA_FB, ifmt);
	if ( ifmt>= TCC_LCDC_IMG_FMT_YUV420SP && ifmt <= TCC_LCDC_IMG_FMT_YUV422ITL1)	{
		#if defined(CONFIG_TCC_COMPOSITE_SIGNAL_QUALITY)
			if(type != TCC_OUTPUT_COMPOSITE)
		#endif
			{
				VIOC_RDMA_SetImageY2REnable(pDISP_OUTPUT[type].pVIOC_RDMA_FB, TRUE);
				VIOC_RDMA_SetImageY2RMode(pDISP_OUTPUT[type].pVIOC_RDMA_FB, 0); /* Y2RMode Default 0 (Studio Color) */

				if( img_width <= RDMA_UVI_MAX_WIDTH )
					VIOC_RDMA_SetImageUVIEnable(pDISP_OUTPUT[type].pVIOC_RDMA_FB, TRUE);
				else
					VIOC_RDMA_SetImageUVIEnable(pDISP_OUTPUT[type].pVIOC_RDMA_FB, FALSE);
			}
	}
	
	VIOC_RDMA_SetImageIntl(pDISP_OUTPUT[type].pVIOC_RDMA_FB, interlace_output);

	//offset
	VIOC_RDMA_SetImageOffset(pDISP_OUTPUT[type].pVIOC_RDMA_FB, ifmt, img_width);

	// alpha & chroma key color setting
	#if 0//defined(CONFIG_TCC_COMPOSITE_SIGNAL_QUALITY)
		if(type == TCC_OUTPUT_COMPOSITE)
		{
			VIOC_RDMA_SetImageAlphaSelect(pDISP_OUTPUT[type].pVIOC_RDMA_FB, FALSE);
			VIOC_RDMA_SetImageAlphaEnable(pDISP_OUTPUT[type].pVIOC_RDMA_FB, FALSE);
		}
		else
		{
			VIOC_RDMA_SetImageAlphaSelect(pDISP_OUTPUT[type].pVIOC_RDMA_FB, TRUE);
			VIOC_RDMA_SetImageAlphaEnable(pDISP_OUTPUT[type].pVIOC_RDMA_FB, TRUE);
		}

		#if defined(CONFIG_ARCH_TCC893X)
			VIOC_WMIX_SetChromaKey(pDISP_OUTPUT[type].pVIOC_WMIXBase, 1, 1, 0x00, 0x80, 0x80, 0xFF, 0xFF, 0xFF);			
		#else
			VIOC_WMIX_SetChromaKey(pDISP_OUTPUT[type].pVIOC_WMIXBase, 1, 1, 0x00, 0x80, 0x80, 0xFF, 0xFF, 0xFF);			
		#endif
	#else
		VIOC_RDMA_SetImageAlphaSelect(pDISP_OUTPUT[type].pVIOC_RDMA_FB, TRUE);
		VIOC_RDMA_SetImageAlphaEnable(pDISP_OUTPUT[type].pVIOC_RDMA_FB, TRUE);

		VIOC_WMIX_SetChromaKey(pDISP_OUTPUT[type].pVIOC_WMIXBase, 0, chroma_en, chromaR, chromaG, chromaB, 0xF8, 0xFC, 0xF8);			
	#endif
	
	VIOC_RDMA_SetImageBase(pDISP_OUTPUT[type].pVIOC_RDMA_FB, FBimg_buf_addr, 0, 0);

	//VIOC_RDMA_SetImageEnable(pDISP_OUTPUT[type].pVIOC_RDMA_FB);

	VIOC_WMIX_SetUpdate(pDISP_OUTPUT[type].pVIOC_WMIXBase);
	
	dprintk(" end g2d_rotate_need=%d g2d_format_need=%d, scaler_need=%d end\n", g2d_rotate_need, g2d_format_need, scaler_need);

	return 1;
}

char TCC_OUTPUT_FB_Update(unsigned int width, unsigned int height, unsigned int bits_per_pixel, unsigned int addr, unsigned int type)
{
#if defined(CONFIG_HWCOMPOSER_OVER_1_1_FOR_MID)
#if defined(CONFIG_DRAM_16BIT_USED)
	if( width*height <= PRESENTATION_LIMIT_RESOLUTION)
		return 0;
#else
	return 0;
#endif
#endif

	return TCC_OUTPUT_FB_Process(width, height, bits_per_pixel, addr, type);
}

char TCC_OUTPUT_FB_Update_External(unsigned int width, unsigned int height, unsigned int bits_per_pixel, unsigned int addr, unsigned int type)
{
	dprintk(" TCC_OUTPUT_FB_Update_External :: %d x %d, %d bpp, 0x%x, type %d \n", width, height, bits_per_pixel, addr, type);
	return TCC_OUTPUT_FB_Process(width, height, bits_per_pixel, addr, type);
}


#ifdef MVC_PROCESS
static int tcc_output_mvc_status = 0;
static int tcc_wmix_overlay_priority = 0;

void TCC_OUTPUT_FB_SetMVCStatus(int status)
{
	unsigned int output_lcdc_num;
	VIOC_WMIX* pWIXBase;

	output_lcdc_num = tca_get_output_lcdc_num();
	if(output_lcdc_num)
		pWIXBase = (VIOC_WMIX*)tcc_p2v(HwVIOC_WMIX1);
	else
		pWIXBase = (VIOC_WMIX*)tcc_p2v(HwVIOC_WMIX0);

	printk("%s, mvs_state=%d\n", __func__, status);

	if(status)
	{
		tcc_output_mvc_status = 1;
		VIOC_WMIX_GetOverlayPriority(pWIXBase, &tcc_wmix_overlay_priority);
		VIOC_WMIX_SetOverlayPriority(pWIXBase, 16);		//Image1 - Image0 - Image2 - Image3
	}
	else
	{
		tcc_output_mvc_status = 0;
		VIOC_WMIX_SetOverlayPriority(pWIXBase, tcc_wmix_overlay_priority);		//restore
	}
}

char TCC_OUTPUT_FB_GetMVCStatus(void)
{
	return tcc_output_mvc_status;
}

char TCC_OUTPUT_FB_Update_3D(unsigned int width, unsigned int height, unsigned int bits_per_pixel, unsigned int addr, unsigned int type)
{
	unsigned int regl = 0, interlace_output = 0;
	unsigned int lcd_width = 0, lcd_height = 0, lcd_h_pos = 0, lcd_w_pos = 0;
	unsigned int img_width = 0, img_height = 0, ifmt = 0;
	unsigned int  chromaR = 0, chromaG = 0, chromaB = 0;
	unsigned int  chroma_en = 0, alpha_blending_en = 0, alpha_type = 0;
	unsigned int vioc_scaler_plug_in = 0;
	unsigned int iVBlank;
	char mode_3d = 0;

	SCALER_TYPE fbscaler;
	VIOC_SC *pSC0, *pSC1;
	pSC0 = (VIOC_SC *)tcc_p2v(HwVIOC_SC0);
	pSC1 = (VIOC_SC *)tcc_p2v(HwVIOC_SC2);

	memset(&fbscaler, 0x00, sizeof(SCALER_TYPE));

	if(pDISP_OUTPUT[type].pVIOC_DispBase == NULL)
	{
		dprintk("%s - Err: Output LCDC is not valid, type=%d\n", __func__, type);
		return 0;
	}
	
	dprintk(" %s width=%d, height=%d, bpp=%d, type=%d\n", __func__, width, height, bits_per_pixel, type);

	regl = pDISP_OUTPUT[type].pVIOC_DispBase->uLSIZE.nREG; // get LCD size 
	lcd_width = (regl & 0xFFFF);
	lcd_height = ((regl>>16) & 0xFFFF);

	if(((!lcd_width) || (!lcd_height)) || ((lcd_width > TCC_FB_OUT_MAX_WIDTH) || (lcd_height > TCC_FB_OUT_MAX_HEIGHT)))
	{
		dprintk(" %s ERROR width=%d, height=%d, bpp=%d, type=%d LCD W:%d H:%d \n", __func__, width, height, bits_per_pixel, type, lcd_width, lcd_height);
		return 0;
	}

	if(bits_per_pixel == 32 )	{
		chroma_en = 0;
		alpha_type = 1;
		alpha_blending_en = 1;
		ifmt = TCC_LCDC_IMG_FMT_RGB888;		
	}
	else  {
		chroma_en = 1;
		alpha_type = 0;
		alpha_blending_en = 0;
		ifmt = TCC_LCDC_IMG_FMT_RGB565;
	}

	img_width = width;
	img_height = height;

	iVBlank = hdmi_get_VBlank();
	lcd_height = (lcd_height - iVBlank)/2;
	FBimg_buf_addr = addr;
	
	if( lcd_width == 1280 && lcd_height == 720) {
		VIOC_SC_SetBypass (pSC0, ON);
		VIOC_SC_SetBypass (pSC1, ON);
	} else {
		VIOC_SC_SetBypass (pSC0, OFF);
		VIOC_SC_SetBypass (pSC1, OFF);
		}

	VIOC_SC_SetSrcSize(pSC0, width, height);
	VIOC_SC_SetDstSize (pSC0, lcd_width, lcd_height);			// set destination size in scaler
	VIOC_SC_SetOutSize (pSC0, lcd_width, lcd_height);			// set output size in scaer
	
	VIOC_SC_SetSrcSize(pSC1, width, height);
	VIOC_SC_SetDstSize (pSC1, lcd_width, lcd_height);			// set destination size in scaler
	VIOC_SC_SetOutSize (pSC1, lcd_width, lcd_height);			// set output size in scaer
	
	VIOC_SC_SetUpdate (pSC0);				// Scaler update
	VIOC_SC_SetUpdate (pSC1);				// Scaler update

	if(machine_is_tcc8920st() || machine_is_tcc8930st()) {
		VIOC_RDMA_SetImageUVIEnable(pDISP_OUTPUT[type].pVIOC_RDMA_FB, TRUE);
			}

	// size
	VIOC_RDMA_SetImageSize(pDISP_OUTPUT[type].pVIOC_RDMA_FB, img_width, img_height);

	// format
	VIOC_RDMA_SetImageFormat(pDISP_OUTPUT[type].pVIOC_RDMA_FB, ifmt);
	if ( ifmt>= TCC_LCDC_IMG_FMT_YUV420SP && ifmt <= TCC_LCDC_IMG_FMT_YUV422ITL1)	{
		VIOC_RDMA_SetImageY2REnable(pDISP_OUTPUT[type].pVIOC_RDMA_FB, TRUE);
		VIOC_RDMA_SetImageY2RMode(pDISP_OUTPUT[type].pVIOC_RDMA_FB, 0); /* Y2RMode Default 0 (Studio Color) */
	}
	
	VIOC_RDMA_SetImageIntl(pDISP_OUTPUT[type].pVIOC_RDMA_FB, interlace_output);

	//offset
	VIOC_RDMA_SetImageOffset(pDISP_OUTPUT[type].pVIOC_RDMA_FB, ifmt, img_width);

	// alpha & chroma key color setting
	VIOC_RDMA_SetImageAlphaSelect(pDISP_OUTPUT[type].pVIOC_RDMA_FB, TRUE);
	VIOC_RDMA_SetImageAlphaEnable(pDISP_OUTPUT[type].pVIOC_RDMA_FB, TRUE);

	if(machine_is_tcc8920st() || machine_is_tcc8930st()) {
		VIOC_RDMA_SetImageUVIEnable(pDISP_OUTPUT[type].pVIOC_RDMA_FB_3D, TRUE);
	}
			
	// size
	VIOC_RDMA_SetImageSize(pDISP_OUTPUT[type].pVIOC_RDMA_FB_3D, img_width, img_height);

	// format
	VIOC_RDMA_SetImageFormat(pDISP_OUTPUT[type].pVIOC_RDMA_FB_3D, ifmt);
	if ( ifmt>= TCC_LCDC_IMG_FMT_YUV420SP && ifmt <= TCC_LCDC_IMG_FMT_YUV422ITL1)	{
		VIOC_RDMA_SetImageY2REnable(pDISP_OUTPUT[type].pVIOC_RDMA_FB_3D, TRUE);
		VIOC_RDMA_SetImageY2RMode(pDISP_OUTPUT[type].pVIOC_RDMA_FB_3D, 0); /* Y2RMode Default 0 (Studio Color) */
	}
	
	VIOC_RDMA_SetImageIntl(pDISP_OUTPUT[type].pVIOC_RDMA_FB_3D, interlace_output);

	//offset
	VIOC_RDMA_SetImageOffset(pDISP_OUTPUT[type].pVIOC_RDMA_FB_3D, ifmt, img_width);

	// alpha & chroma key color setting
	VIOC_RDMA_SetImageAlphaSelect(pDISP_OUTPUT[type].pVIOC_RDMA_FB_3D, TRUE);
	VIOC_RDMA_SetImageAlphaEnable(pDISP_OUTPUT[type].pVIOC_RDMA_FB_3D, TRUE);

	VIOC_WMIX_SetPosition(pDISP_OUTPUT[type].pVIOC_WMIXBase, 0, 0, 0);
	VIOC_WMIX_SetPosition(pDISP_OUTPUT[type].pVIOC_WMIXBase, 1, 0, lcd_height+iVBlank);

	VIOC_WMIX_SetBGColor(pDISP_OUTPUT[type].pVIOC_WMIXBase, 0xff, 0xff, 0xff, 0xff);
	VIOC_WMIX_SetChromaKey(pDISP_OUTPUT[type].pVIOC_WMIXBase, 0, chroma_en, chromaR, chromaG, chromaB, 0xF8, 0xFC, 0xF8);	
	VIOC_WMIX_SetChromaKey(pDISP_OUTPUT[type].pVIOC_WMIXBase, 1, chroma_en, chromaR, chromaG, chromaB, 0xF8, 0xFC, 0xF8);			
	VIOC_WMIX_SetUpdate(pDISP_OUTPUT[type].pVIOC_WMIXBase);
	
	return 1;
}
#else
char TCC_OUTPUT_FB_Update_3D(unsigned int width, unsigned int height, unsigned int bits_per_pixel, unsigned int addr, unsigned int type)
{
	return 1;
}
void TCC_OUTPUT_FB_SetMVCStatus(int status)
{
	return;
}

char TCC_OUTPUT_FB_GetMVCStatus(void)
{
	return 0;
}
#endif

void TCC_OUTPUT_FB_UpdateSync(unsigned int type)
{
	VIOC_RDMA * pRDMA_OUTPUT;

	if(pDISP_OUTPUT[type].pVIOC_DispBase == NULL)
	{
		dprintk("%s - Err: Output DISP is not valid, type=%d\n", __func__, type);
		return;
	}

	#if defined(CONFIG_TCC_EXCLUSIVE_UI_LAYER)
		if(exclusive_ui_clear_force == 0)
		{
			dprintk("%s, force exclusive_ui not clear!!\n", __func__); 
			return;
		}
	#endif

	dprintk("%s type=%d UseVSyncInterrupt=%d\n", __func__, type, UseVSyncInterrupt);

	//pLCDC_OUTPUT_FB_CH = (volatile PLCDC_CHANNEL)&pLCDC_OUTPUT[type]->LI2C;

	VIOC_RDMA_SetImageBase(pDISP_OUTPUT[type].pVIOC_RDMA_FB, FBimg_buf_addr, 0, 0);
#ifdef MVC_PROCESS
	if(TCC_OUTPUT_FB_GetMVCStatus() && pDISP_OUTPUT[type].pVIOC_RDMA_FB_3D)
	VIOC_RDMA_SetImageBase(pDISP_OUTPUT[type].pVIOC_RDMA_FB_3D, FBimg_buf_addr, 0, 0);
#endif

	#if defined(CONFIG_TCC_OUTPUT_ATTACH_HDMI_CVBS) || defined(CONFIG_TCC_OUTPUT_ATTACH_DUAL_AUTO) || defined(TCC_OUTPUT_STARTER_ATTACH_HDMI_CVBS) || defined(TCC_OUTPUT_STARTER_ATTACH_DUAL_AUTO)
		TCC_OUTPUT_FB_AttachUpdateFlag(pDISP_OUTPUT[type].LCDC_N);
	#endif

	FBimg_buf_addr = 0;

	VIOC_RDMA_SetImageEnable(pDISP_OUTPUT[type].pVIOC_RDMA_FB);
#ifdef MVC_PROCESS
	if(TCC_OUTPUT_FB_GetMVCStatus() && pDISP_OUTPUT[type].pVIOC_RDMA_FB_3D)
	VIOC_RDMA_SetImageEnable(pDISP_OUTPUT[type].pVIOC_RDMA_FB_3D);
#endif

	if(UseVSyncInterrupt)	{
		TCC_OUTPUT_FB_WaitVsyncInterrupt(type);
	}
}

void TCC_OUTPUT_FB_WaitVsyncInterrupt(unsigned int type)
{
	int ret;
	
	if(type != OutputType)
		type = OutputType;

	if(pDISP_OUTPUT[type].pVIOC_DispBase == NULL)
	{
		dprintk("%s - Err: Output LCDC is not valid, OutputType=%d\n", __func__, type);
		return;
	}

	//pLCDC_OUTPUT[type]->LSTATUS = 0xFFFFFFFF; 
	BITCSET(pDISP_OUTPUT[type].pVIOC_DispBase->uLSTATUS.nREG, 0xFFFFFFFF, 0xFFFFFFFF);
	
	tca_lcdc_interrupt_onoff(TRUE, pDISP_OUTPUT[type].LCDC_N);

	#if defined(CONFIG_TCC_OUTPUT_DUAL_UI)
		if(pDISP_OUTPUT[type].LCDC_N)
		{
			Output_lcdc1_struct.state = 0;
			ret = wait_event_interruptible_timeout(Output_lcdc1_struct.waitq, Output_lcdc1_struct.state == 1, msecs_to_jiffies(50));
		}
		else
		{
			Output_lcdc0_struct.state = 0;
			ret = wait_event_interruptible_timeout(Output_lcdc0_struct.waitq, Output_lcdc0_struct.state == 1, msecs_to_jiffies(50));
		}
	#else
		Output_struct.state = 0;
		ret = wait_event_interruptible_timeout(Output_struct.waitq, Output_struct.state == 1, msecs_to_jiffies(50));
	#endif
#ifndef TCC_VIDEO_DISPLAY_BY_VSYNC_INT
	//tca_lcdc_interrupt_onoff(FALSE, pDISP_OUTPUT[type].LCDC_N);
#endif
	if(ret <= 0)	{
	 	printk("  [%s : %d]: tcc_setup_interrupt timed_out!! \n",__func__, ret);
	}
}


void TCC_OUTPUT_RDMA_Update(unsigned int type)
{
	VIOC_RDMA * pRDMA_OUTPUT;

	if(pDISP_OUTPUT[type].pVIOC_DispBase == NULL)
	{
		printk("%s - Err: Output DISP is not valid, type=%d\n", __func__, type);
		return;
	}

	VIOC_RDMA_SetImageUpdate(pDISP_OUTPUT[type].pVIOC_RDMA_FB);
#ifdef MVC_PROCESS
	if(TCC_OUTPUT_FB_GetMVCStatus() && pDISP_OUTPUT[type].pVIOC_RDMA_FB_3D)
	VIOC_RDMA_SetImageUpdate(pDISP_OUTPUT[type].pVIOC_RDMA_FB_3D);
#endif//
	VIOC_RDMA_SetImageUpdate(pDISP_OUTPUT[type].pVIOC_RDMA_Video);

	if(pDISP_OUTPUT[type].pVIOC_RDMA_Mouse != NULL)
	VIOC_RDMA_SetImageUpdate(pDISP_OUTPUT[type].pVIOC_RDMA_Mouse);
}


int TCC_OUTPUT_FB_MouseShow(unsigned int enable, unsigned int type)
{
	VIOC_RDMA * pRDMABase;
	
	dprintk("%s : enable = %d\n", __func__, enable);

	if(pDISP_OUTPUT[type].pVIOC_RDMA_Mouse == NULL)
	{
		dprintk("%s - Err: Output DISP is not valid, type=%d\n", __func__, type);
		return;
	}

	pRDMABase = pDISP_OUTPUT[type].pVIOC_RDMA_Mouse;

	if(enable)
	{

	}
	else
	{
		VIOC_RDMA_SetImageDisable( pRDMABase);
	}

	return 1;
}


int TCC_OUTPUT_FB_MouseMove(unsigned int width, unsigned int height, tcc_mouse *mouse, unsigned int type)
{
	VIOC_DISP * pDISPBase;
	VIOC_WMIX * pWMIXBase;
	VIOC_RDMA * pRDMABase;
	
	unsigned int lcd_width, lcd_height, lcd_w_pos,lcd_h_pos, mouse_x, mouse_y;
	unsigned int interlace_output, display_width, display_height;
	unsigned int wmix_channel = 0;

	if( (pDISP_OUTPUT[type].pVIOC_DispBase == NULL) ||
		(pDISP_OUTPUT[type].pVIOC_WMIXBase == NULL) ||
		(pDISP_OUTPUT[type].pVIOC_RDMA_Mouse == NULL) )
	{
		dprintk("%s - Err: Output LCDC is not valid, type=%d\n", __func__, type);
		return 0;
	}

	pDISPBase = pDISP_OUTPUT[type].pVIOC_DispBase;
	pWMIXBase = pDISP_OUTPUT[type].pVIOC_WMIXBase;
	pRDMABase = pDISP_OUTPUT[type].pVIOC_RDMA_Mouse;

#if defined(CONFIG_ARCH_TCC893X)
#if defined(CONFIG_CHIP_TCC8935S) && defined(CONFIG_STB_BOARD_DONGLE_TCC893X)
	interlace_output = 0;
	wmix_channel = 1;
#else
	interlace_output = 0;
	wmix_channel = 2;
#endif
#elif defined(CONFIG_SOC_TCC8925S)
#if defined(CONFIG_STB_BOARD_DONGLE)
	interlace_output = 0;
	wmix_channel = 1;
#endif
#else
	if( pDISPBase == (VIOC_DISP*)tcc_p2v(HwVIOC_DISP0))
	{
		if( !(pDISPBase->uCTRL.nREG & HwDISP_NI ))//interlace mode
			interlace_output = 1;
		else
			interlace_output = 0;
	}
	else
	{
		interlace_output = 0;
	}
	wmix_channel = 3;
#endif /* CONFIG_ARCH_TCC893X */

	dprintk("%s pRDMA:0x%08x, pWMIX:0x%08x, pDISP:0x%08x\n", __func__, pRDMABase, pWMIXBase, pDISPBase);

	VIOC_DISP_GetSize(pDISPBase, &lcd_width, &lcd_height);
	
	if((!lcd_width) || (!lcd_height))
		return;

	lcd_width -= uiOutputResizeMode.resize_x << 4;
	lcd_height -= uiOutputResizeMode.resize_y << 3;

	mouse_x = (unsigned int)(lcd_width * mouse->x / width);
	mouse_y = (unsigned int)(lcd_height *mouse->y / height);

	if( mouse_x > lcd_width - mouse_cursor_width )
		display_width = lcd_width - mouse_x;
	else
		display_width = mouse_cursor_width;

	if( mouse_y > lcd_height - mouse_cursor_height )
		display_height = lcd_height - mouse_y;
	else
		display_height = mouse_cursor_height;

	VIOC_RDMA_SetImageOffset(pRDMABase, TCC_LCDC_IMG_FMT_RGB888, mouse_cursor_width);
	VIOC_RDMA_SetImageFormat(pRDMABase, TCC_LCDC_IMG_FMT_RGB888);

	mouse_x += uiOutputResizeMode.resize_x << 3;
	if( interlace_output )
		mouse_y += uiOutputResizeMode.resize_y << 1;
	else
		mouse_y += uiOutputResizeMode.resize_y << 2;

	lcd_w_pos = mouse_x;
	lcd_h_pos = mouse_y;

	dprintk("%s lcd_width:%d, lcd_height:%d, lcd_w_pos:%d, lcd_h_pos:%d\n\n", __func__, lcd_width, lcd_height, lcd_w_pos, lcd_h_pos);
	
	// position
	VIOC_WMIX_SetPosition(pWMIXBase, wmix_channel, lcd_w_pos, lcd_h_pos);

	// scale
	//VIOC_RDMA_SetImageScale(pRDMABase, 0, 0);
	
	VIOC_RDMA_SetImageSize(pRDMABase, display_width, display_height);
		
	// position
	//if(ISZERO(pLCDC->LCTRL, HwLCTRL_NI)) //--
	if(interlace_output)
	{
		VIOC_RDMA_SetImageIntl(pRDMABase, 1);
		VIOC_WMIX_SetPosition(pWMIXBase, wmix_channel,  lcd_w_pos, (lcd_h_pos/2) );
	}
	else
	{
		VIOC_RDMA_SetImageIntl(pRDMABase, 0);
		VIOC_WMIX_SetPosition(pWMIXBase, wmix_channel, lcd_w_pos, lcd_h_pos);
	}

	// alpha & chroma key color setting
	VIOC_RDMA_SetImageAlphaSelect(pRDMABase, 1);
	VIOC_RDMA_SetImageAlphaEnable(pRDMABase, 1);
	
	// image address
	if(MouseBufIdx)
		VIOC_RDMA_SetImageBase(pRDMABase, pMouseBuffer1, NULL, NULL);
	else
		VIOC_RDMA_SetImageBase(pRDMABase, pMouseBuffer2, NULL, NULL);

	VIOC_WMIX_SetUpdate(pWMIXBase);
	VIOC_RDMA_SetImageEnable(pRDMABase);

	#if defined(CONFIG_TCC_OUTPUT_ATTACH)
		TCC_OUTPUT_FB_AttachUpdateFlag(pDISP_OUTPUT[type].LCDC_N);
	#endif

	return 1;
}

int TCC_OUTPUT_FB_MouseSetIcon(tcc_mouse_icon *mouse_icon)
{
    char *dst;
    char *src = mouse_icon->buf;
    int i, j , k;

    if( !MouseBufIdx )
        dst = (char *)Gmap_cpu;
    else
        dst = (char *)(Gmap_cpu + MOUSE_CURSOR_BUFF_SIZE/2);

    MouseBufIdx = !MouseBufIdx;

    mouse_cursor_width = ((mouse_icon->width + 3) >> 2) << 2;
    mouse_cursor_height = mouse_icon->height;

    for(i=0; i<mouse_icon->height; i++)
    {
        for(j=0; j<mouse_icon->width; j++)
        {
            *(dst+3) = *(src+3);
            *(dst+2) = *(src+0);
            *(dst+1) = *(src+1);
            *(dst+0) = *(src+2);
            dst+=4;
            src+=4;
        }
        for(k=0; k<(mouse_cursor_width-mouse_icon->width); k++)
        {
            *(dst+3) = 0;
            *(dst+2) = 0;
            *(dst+1) = 0;
            *(dst+0) = 0;
            dst+=4;
        }
    }
    return 1;
}

void TCC_OUTPUT_FB_Enable3D(char lcdc_num)
{
	if(output_3d_ui_enable)
	{
		dprintk("%s, 3D UI was already enabled!!\n", __func__);
		return;
	}

	#if defined(CONFIG_TCC_OUTPUT_ATTACH) && defined(CONFIG_ARCH_TCC892X)
		if(tcc_output_attach_flag)
		{
			TCC_OUTPUT_FB_DetachOutput(0);
			tcc_output_attach_force_disabled = 1;
		}
	#endif
	
	output_3d_ui_enable = 1;

	#if defined(CONFIG_TCC_OUTPUT_ATTACH) || defined(CONFIG_HDMI_DISPLAY_LASTFRAME)
		/* open M-to-M scaler driver */
		#if defined(CONFIG_ARCH_TCC893X)
			tccxxx_wmixer1_open((struct inode *)&wmixer_3d_inode, (struct file *)&wmixer_3d_filp);
		#else
			tccxxx_scaler2_open((struct inode *)&scaler_3d_inode, (struct file *)&scaler_3d_filp);
		#endif
		printk("%s, M-to-M scaler path is used for 3D UI\n", __func__);
	#else
		/* plug-in the scaler */		
		if(output_3d_ui_plugin == 0)
		{
		#if defined(CONFIG_ARCH_TCC893X)
			if(lcdc_num)
				VIOC_CONFIG_PlugIn(VIOC_SC0, VIOC_SC_RDMA_05);
			else
				VIOC_CONFIG_PlugIn(VIOC_SC0, VIOC_SC_RDMA_01);
		#else
			if(lcdc_num)
				VIOC_CONFIG_PlugIn(VIOC_SC0, VIOC_SC_RDMA_07);
			else
				VIOC_CONFIG_PlugIn(VIOC_SC0, VIOC_SC_RDMA_03);
		#endif

			output_3d_ui_plugin = 1;
		
			printk("%s, scaler is plugged-in for 3D UI\n", __func__);
		}
	#endif
}

void TCC_OUTPUT_FB_Disable3D(char lcdc_num)
{
	PVIOC_RDMA	pRDMA;

	if(output_3d_ui_enable == 0)
	{
		dprintk("%s, 3D UI was already disabled!!\n", __func__);
		return;
	}

	output_3d_ui_enable = 0;

	/* get the source resolution */
	#if defined(CONFIG_ARCH_TCC893X)
		if(lcdc_num)
			pRDMA = (VIOC_RDMA *)tcc_p2v(HwVIOC_RDMA05);
		else
			pRDMA = (VIOC_RDMA *)tcc_p2v(HwVIOC_RDMA01);
	#else
		if(lcdc_num)
			pRDMA = (VIOC_RDMA *)tcc_p2v(HwVIOC_RDMA07);
		else
			pRDMA = (VIOC_RDMA *)tcc_p2v(HwVIOC_RDMA03);
	#endif

	/* disable RDMA */
	VIOC_RDMA_SetImageDisable(pRDMA);
	
	#if defined(CONFIG_TCC_OUTPUT_ATTACH) || defined(CONFIG_HDMI_DISPLAY_LASTFRAME)
		/* release M-to-M scaler driver */
		#if defined(CONFIG_ARCH_TCC893X)
			tccxxx_wmixer1_release((struct inode *)&wmixer_3d_inode, (struct file *)&wmixer_3d_filp);
		#else
			tccxxx_scaler2_release((struct inode *)&scaler_3d_inode, (struct file *)&scaler_3d_filp);
		#endif
	#else
		/* plug-out the scaler */		
		if(output_3d_ui_plugin)
		{
			//VIOC_RDMA_SetImageDisable(pRDMA);
		
			if(lcdc_num)
				VIOC_CONFIG_PlugOut(VIOC_SC0);
			else
				VIOC_CONFIG_PlugOut(VIOC_SC0);

			output_3d_ui_plugin = 0;
		}
	#endif

	#if defined(CONFIG_TCC_OUTPUT_ATTACH)
		if(tcc_output_attach_force_disabled)
		{
			tcc_output_attach_force_disabled = 0;
			TCC_OUTPUT_FB_AttachOutput(pDISP_OUTPUT[OutputType].LCDC_N, tcc_output_attach_output, tcc_output_attach_resolution, 0);
		}
	#endif
}

void TCC_OUTPUT_FB_Update3D(unsigned int src_wd, unsigned int src_ht, char lcdc_num, char scaler_use)
{
	PVIOC_DISP	pDISP;
	PVIOC_WMIX	pWMIX;
	PVIOC_RDMA	pRDMA;
	PVIOC_SC 	pSC;
	int fmt, dst_wd, dst_ht, dst_sx, dst_sy, lcd_width, lcd_height;

	if(output_3d_ui_enable == 0)
	{
		dprintk("%s, 3D UI should be enabled in advance!!\n", __func__);
		return;
	}
	
	pSC = (VIOC_SC *)tcc_p2v(HwVIOC_SC0);

	/* get the source resolution */
	if(lcdc_num)
	{
		pDISP = (VIOC_DISP *)tcc_p2v(HwVIOC_DISP1);
		pWMIX = (VIOC_WMIX *)tcc_p2v(HwVIOC_WMIX1);
		#if defined(CONFIG_ARCH_TCC893X)
		pRDMA = (VIOC_RDMA *)tcc_p2v(HwVIOC_RDMA05);
		#else
		pRDMA = (VIOC_RDMA *)tcc_p2v(HwVIOC_RDMA07);
		#endif
	}
	else
	{
		pDISP = (VIOC_DISP *)tcc_p2v(HwVIOC_DISP0);
		pWMIX = (VIOC_WMIX *)tcc_p2v(HwVIOC_WMIX0);
		#if defined(CONFIG_ARCH_TCC893X)
		pRDMA = (VIOC_RDMA *)tcc_p2v(HwVIOC_RDMA01);
		#else
		pRDMA = (VIOC_RDMA *)tcc_p2v(HwVIOC_RDMA03);
		#endif
	}

	dst_wd = lcd_width = (pDISP->uLSIZE.nREG & 0xFFFF);
	dst_ht = lcd_height = ((pDISP->uLSIZE.nREG>>16) & 0xFFFF);

	fmt = TCC_LCDC_IMG_FMT_RGB888;
	
	dst_wd -= uiOutputResizeMode.resize_x << 4;
	dst_ht -= uiOutputResizeMode.resize_y << 3;

	dst_sx = uiOutputResizeMode.resize_x << 3;
	dst_sy = uiOutputResizeMode.resize_y << 2;

	#if defined(CONFIG_ARCH_TCC893X)
		if(output_3d_ui_mode == 0)	/* 3D MKV : SBS(SideBySide) Mode */
			VIOC_WMIX_SetPosition(pWMIX, 1, (dst_sx + lcd_width)>>1, dst_sy);
		else						/* 3D MKV : TNB(Top&Bottom) Mode */
			VIOC_WMIX_SetPosition(pWMIX, 1, dst_sx, (dst_sy + lcd_height)>>1);
	#else
		if(output_3d_ui_mode == 0)	/* 3D MKV : SBS(SideBySide) Mode */
			VIOC_WMIX_SetPosition(pWMIX, 3, (dst_sx + lcd_width)>>1, dst_sy);
		else						/* 3D MKV : TNB(Top&Bottom) Mode */
			VIOC_WMIX_SetPosition(pWMIX, 3, dst_sx, (dst_sy + lcd_height)>>1);
	#endif

	if(scaler_use)
	{
		if(output_3d_ui_plugin)
		{
			VIOC_SC_SetBypass(pSC, OFF);
			VIOC_SC_SetSrcSize(pSC, src_wd, src_ht);

			/* 3D MKV : SBS(SideBySide) Mode */
			if(output_3d_ui_mode == 0)
			{
				VIOC_SC_SetDstSize(pSC, dst_wd>>1, dst_ht);
				VIOC_SC_SetOutSize(pSC, dst_wd>>1, dst_ht);
			}
			else
			/* 3D MKV : TNB(Top&Bottom) Mode */
			{
				VIOC_SC_SetDstSize(pSC, dst_wd, dst_ht>>1);
				VIOC_SC_SetOutSize(pSC, dst_wd, dst_ht>>1);
			}

			/* set RDMA */
			VIOC_RDMA_SetImageAlphaSelect(pRDMA, TRUE);
			VIOC_RDMA_SetImageAlphaEnable(pRDMA, TRUE);
			VIOC_RDMA_SetImageFormat(pRDMA, fmt);
			VIOC_RDMA_SetImageSize(pRDMA, src_wd, src_ht);
			VIOC_RDMA_SetImageOffset(pRDMA, fmt, src_wd);
			VIOC_RDMA_SetImageBase(pRDMA, FBimg_buf_addr, 0, 0);
			VIOC_RDMA_SetImageEnable(pRDMA);

			VIOC_SC_SetUpdate(pSC);
		}
		else
		{
		#if defined(CONFIG_ARCH_TCC893X)
			WMIXER_ALPHASCALERING_INFO_TYPE fbmixer;

			memset(&fbmixer, 0x00, sizeof(fbmixer));
		       
			fbmixer.rsp_type = SCALER_POLLING;
			//fbmixer.rsp_type = SCALER_NOWAIT;
			
			fbmixer.src_y_addr = FBimg_buf_addr;
			fbmixer.src_fmt = fmt;		
			fbmixer.src_img_width = src_wd;
			fbmixer.src_img_height = src_ht;
			fbmixer.src_win_left = 0;
			fbmixer.src_win_top = 0;
			fbmixer.src_win_right = src_wd;
			fbmixer.src_win_bottom = src_ht;

			if(output_3d_ui_index)
				fbmixer.dst_y_addr = fb_scaler_pbuf0;
			else
				fbmixer.dst_y_addr = fb_scaler_pbuf1;

			output_3d_ui_index = !output_3d_ui_index;
				
			fmt = TCC_LCDC_IMG_FMT_RGB888;
			
			fbmixer.dst_fmt = fmt;

			/* 3D MKV : SBS(SideBySide) Mode */
			if(output_3d_ui_mode == 0)
			{
				fbmixer.dst_img_width = dst_wd>>1;
				fbmixer.dst_img_height = dst_ht;
				fbmixer.dst_win_left = 0;
				fbmixer.dst_win_top = 0;
				fbmixer.dst_win_right = dst_wd>>1;
				fbmixer.dst_win_bottom = dst_ht;
			}
			else
			/* 3D MKV : TNB(Top&Bottom) Mode */
			{
				fbmixer.dst_img_width = dst_wd;
				fbmixer.dst_img_height = dst_ht>>1;
				fbmixer.dst_win_left = 0;
				fbmixer.dst_win_top = 0;
				fbmixer.dst_win_right = dst_wd;
				fbmixer.dst_win_bottom = dst_ht>>1;
			}

			//tccxxx_wmixer1_open((struct inode *)&wmixer_3d_inode, (struct file *)&wmixer_3d_filp);
			tccxxx_wmixer1_ioctl((struct file *)&wmixer_3d_filp, TCC_WMIXER_ALPHA_SCALING_KERNEL, &fbmixer);
			//tccxxx_wmixer1_release((struct inode *)&wmixer_3d_inode, (struct file *)&wmixer_3d_filp);

			dst_wd = fbmixer.dst_img_width;
			dst_ht = fbmixer.dst_img_height;

			/* set RDMA */
			VIOC_RDMA_SetImageAlphaSelect(pRDMA, TRUE);
			VIOC_RDMA_SetImageAlphaEnable(pRDMA, TRUE);
			VIOC_RDMA_SetImageFormat(pRDMA, fmt);
			VIOC_RDMA_SetImageSize(pRDMA, dst_wd, dst_ht);
			VIOC_RDMA_SetImageOffset(pRDMA, fmt, dst_wd);
			VIOC_RDMA_SetImageBase(pRDMA, fbmixer.dst_y_addr, 0, 0);
			VIOC_RDMA_SetImageEnable(pRDMA);
		#else
			SCALER_TYPE fbscaler;
		
			memset(&fbscaler, 0x00, sizeof(fbscaler));
		       
			fbscaler.responsetype = SCALER_POLLING;
			//fbscaler.responsetype = SCALER_NOWAIT;
			
			fbscaler.src_Yaddr = FBimg_buf_addr;
			fbscaler.src_fmt = fmt;		
			fbscaler.src_ImgWidth = src_wd;
			fbscaler.src_ImgHeight = src_ht;
			fbscaler.src_winLeft = 0;
			fbscaler.src_winTop = 0;
			fbscaler.src_winRight = src_wd;
			fbscaler.src_winBottom = src_ht;

			if(output_3d_ui_index)
				fbscaler.dest_Yaddr = fb_scaler_pbuf0;
			else
				fbscaler.dest_Yaddr = fb_scaler_pbuf1;

			output_3d_ui_index = !output_3d_ui_index;
				
			fmt = TCC_LCDC_IMG_FMT_RGB888;
			
			fbscaler.dest_fmt = fmt;
			fbscaler.divide_path = 0x00;

			/* 3D MKV : SBS(SideBySide) Mode */
			if(output_3d_ui_mode == 0)
			{
				fbscaler.dest_ImgWidth = dst_wd>>1;
				fbscaler.dest_ImgHeight = dst_ht;
				fbscaler.dest_winLeft = 0;
				fbscaler.dest_winTop = 0;
				fbscaler.dest_winRight = dst_wd>>1;
				fbscaler.dest_winBottom = dst_ht;
			}
			else
			/* 3D MKV : TNB(Top&Bottom) Mode */
			{
				fbscaler.dest_ImgWidth = dst_wd;
				fbscaler.dest_ImgHeight = dst_ht>>1;
				fbscaler.dest_winLeft = 0;
				fbscaler.dest_winTop = 0;
				fbscaler.dest_winRight = dst_wd;
				fbscaler.dest_winBottom = dst_ht>>1;
			}

			tccxxx_scaler2_ioctl((struct file *)&scaler_3d_filp, TCC_SCALER_IOCTRL_KERENL, &fbscaler);

			dst_wd = fbscaler.dest_ImgWidth;
			dst_ht = fbscaler.dest_ImgHeight;

			/* set RDMA */
			VIOC_RDMA_SetImageAlphaSelect(pRDMA, TRUE);
			VIOC_RDMA_SetImageAlphaEnable(pRDMA, TRUE);
			VIOC_RDMA_SetImageFormat(pRDMA, fmt);
			VIOC_RDMA_SetImageSize(pRDMA, dst_wd, dst_ht);
			VIOC_RDMA_SetImageOffset(pRDMA, fmt, dst_wd);
			VIOC_RDMA_SetImageBase(pRDMA, fbscaler.dest_Yaddr, 0, 0);
			VIOC_RDMA_SetImageEnable(pRDMA);
		#endif
		}
	}
	else
	{
		if(output_3d_ui_plugin)
		{
			VIOC_SC_SetBypass(pSC, ON);
			VIOC_SC_SetUpdate(pSC);
		}
		
		if(output_3d_ui_mode == 0)	/* 3D MKV : SBS(SideBySide) Mode */
			dst_wd = dst_wd>>1;
		else						/* 3D MKV : TNB(Top&Bottom) Mode */
			dst_ht = dst_ht>>1;
		
		/* set RDMA */
		VIOC_RDMA_SetImageAlphaSelect(pRDMA, TRUE);
		VIOC_RDMA_SetImageAlphaEnable(pRDMA, TRUE);
		VIOC_RDMA_SetImageFormat(pRDMA, fmt);
		VIOC_RDMA_SetImageSize(pRDMA, dst_wd, dst_ht);
		VIOC_RDMA_SetImageOffset(pRDMA, fmt, dst_wd);
		VIOC_RDMA_SetImageBase(pRDMA, FBimg_buf_addr, 0, 0);
		VIOC_RDMA_SetImageEnable(pRDMA);
	}

	VIOC_WMIX_SetUpdate(pWMIX);
}

int TCC_OUTPUT_FB_Set3DMode(char enable, char mode)
{
	if(OutputType != TCC_OUTPUT_HDMI)
	{
		printk("%s, 3D UI is only supported on HDMI output!!\n", __func__);
		return 0;
	}

	printk("%s, enable=%d, mode=%d\n", __func__, enable, mode);

	if(output_3d_ui_flag)
	{
		if(enable == 0)
			output_3d_ui_flag = 0;
	}
	else
	{
		if(enable == 1)
			output_3d_ui_flag = 1;
	}
	
	output_3d_ui_mode = mode;
	output_ui_resize_count = 4;

	if(pDISP_OUTPUT[OutputType].pVIOC_DispBase)
	{
		#if defined(CONFIG_ARCH_TCC892X)
		if((pDISP_OUTPUT[OutputType].pVIOC_DispBase != (VIOC_DISP*)tcc_p2v(HwVIOC_DISP0)) || (pDISP_OUTPUT[OutputType].pVIOC_DispBase->uCTRL.nREG & HwDISP_NI)) 
		#endif
		{
			if(enable)
				TCC_OUTPUT_FB_Enable3D(pDISP_OUTPUT[OutputType].LCDC_N);	
			else
				TCC_OUTPUT_FB_Disable3D(pDISP_OUTPUT[OutputType].LCDC_N);	
		}
	}

	return 0;
}

int TCC_OUTPUT_FB_Get3DMode(char *mode)
{
	if(OutputType != TCC_OUTPUT_HDMI)
	{
		dprintk("%s, 3D UI is only supported on HDMI output!!\n", __func__);
		return 0;
	}
	
	dprintk("%s, flag=%d, mode=%d\n", __func__, output_3d_ui_flag, output_3d_ui_mode);

	*mode = output_3d_ui_mode;

	return output_3d_ui_flag;
}

void TCC_OUTPUT_FB_AttachWork(struct work_struct *work)
{
	TCC_OUTPUT_FB_AttachUpdateScreen(pDISP_OUTPUT[OutputType].LCDC_N);
}

void TCC_OUTPUT_FB_AttachOutput(char src_lcdc_num, char dst_output, char resolution, char starter_flag)
{
	PVIOC_DISP	pDISP;
	PVIOC_WMIX	pWMIX;
	PVIOC_RDMA	pRDMA, pRDMA1, pRDMA2, pRDMA3;
	PVIOC_WDMA	pWDMA;
	PVIOC_SC 	pSC;
	int dst_lcdc_num, fmt, src_wd, src_ht, dst_wd, dst_ht, count, scaler_num;
	unsigned char *pDstAddr, *pBaseAddr;

	/* check the flag */
	if(tcc_output_attach_flag)
	{
		dprintk("%s, output(%d) was already attached!!\n", __func__, tcc_output_attach_output);
		return;
	}

	/* check the destination output */
	if((dst_output != TCC_OUTPUT_COMPOSITE) && (dst_output != TCC_OUTPUT_COMPONENT))
	{
		dprintk("%s, output type(%d) is not valid!!\n", __func__, dst_output);
		return;
	}

	/* check the disabled flag */
	if(tcc_output_attach_force_disabled)
	{
		dprintk("%s, the attached output is disabled by force!!\n", __func__);
		return;
	}
		
	/* set the scaler number */		
	#if defined(CONFIG_ARCH_TCC893X)
		scaler_num = VIOC_SC3;
		pSC = (VIOC_SC *)tcc_p2v(HwVIOC_SC3);
	#else
		scaler_num = VIOC_SC0;
		pSC = (VIOC_SC *)tcc_p2v(HwVIOC_SC0);
	#endif

	/* set the flag */
	//tcc_output_attach_state = 1;
	
	/* get the source resolution */
	if(src_lcdc_num)
	{
		pDISP = (VIOC_DISP *)tcc_p2v(HwVIOC_DISP1);
		pWDMA = (VIOC_WDMA *)tcc_p2v(HwVIOC_WDMA01);
	}
	else
	{
		pDISP = (VIOC_DISP *)tcc_p2v(HwVIOC_DISP0);
		pWDMA = (VIOC_WDMA *)tcc_p2v(HwVIOC_WDMA00);
	}

	src_wd = (pDISP->uLSIZE.nREG & 0xFFFF);
	src_ht = ((pDISP->uLSIZE.nREG>>16) & 0xFFFF);
	fmt = TCC_LCDC_IMG_FMT_RGB888;

	if(src_lcdc_num)
		dst_lcdc_num = 0;
	else
		dst_lcdc_num = 1;

	dprintk("%s, attach_lcdc_num=%d, attach_output=%d\n", __func__, dst_lcdc_num, dst_output);

	/* set the register */
	if(dst_lcdc_num)
	{
		pDISP = (VIOC_DISP *)tcc_p2v(HwVIOC_DISP1);
		pWMIX = (VIOC_WMIX *)tcc_p2v(HwVIOC_WMIX1);	
		pRDMA = (VIOC_RDMA *)tcc_p2v(HwVIOC_RDMA04);
		pRDMA1 = (VIOC_RDMA *)tcc_p2v(HwVIOC_RDMA05);
		pRDMA2 = (VIOC_RDMA *)tcc_p2v(HwVIOC_RDMA06);
		pRDMA3 = (VIOC_RDMA *)tcc_p2v(HwVIOC_RDMA07);
	}
	else
	{
		pDISP = (VIOC_DISP *)tcc_p2v(HwVIOC_DISP0);
		pWMIX = (VIOC_WMIX *)tcc_p2v(HwVIOC_WMIX0);	
		pRDMA = (VIOC_RDMA *)tcc_p2v(HwVIOC_RDMA00);
		pRDMA1 = (VIOC_RDMA *)tcc_p2v(HwVIOC_RDMA01);
		pRDMA2 = (VIOC_RDMA *)tcc_p2v(HwVIOC_RDMA02);
		pRDMA3 = (VIOC_RDMA *)tcc_p2v(HwVIOC_RDMA03);
	}

	#if 1//defined(CONFIG_ARCH_TCC892X)
		/* set the default clock for attached lcdc */
		if(dst_lcdc_num)
			tca_ckc_setperi(PERI_LCD1, ENABLE, 1000000);
		else
			tca_ckc_setperi(PERI_LCD0, ENABLE, 1000000);
	#endif
	
	VIOC_DISP_TurnOff(pDISP);

	VIOC_RDMA_SetImageDisable(pRDMA);
	VIOC_RDMA_SetImageDisable(pRDMA1);
	VIOC_RDMA_SetImageDisable(pRDMA2);
	VIOC_RDMA_SetImageDisable(pRDMA3);

	#if 0
	count = 0;
	while(count < 0xF0000)
	{
		volatile unsigned int status;

		status = pDISP->uLSTATUS.nREG;
		if(status & HwLSTATUS_DD)
		{
			dprintk("%s, display device disabled!!, count=%d\n", __func__, count);
			break;
		}

		count++;
	}
	#endif

	if(dst_output == TCC_OUTPUT_COMPOSITE)
	{
		#if defined(CONFIG_FB_TCC_COMPOSITE)
		tcc_composite_attach(dst_lcdc_num, resolution, 1);
		#endif
	}
	else
	{
		#if defined(CONFIG_FB_TCC_COMPONENT)
		tcc_component_attach(dst_lcdc_num, resolution, 1);
		#endif
	}
	
	/* get the destination resolution */
	dst_wd = (pDISP->uLSIZE.nREG & 0xFFFF);
	dst_ht = ((pDISP->uLSIZE.nREG>>16) & 0xFFFF);
		
	pDstAddr = (char *)output_attach_pbuf0;

	if(pDstAddr)
	{
		pBaseAddr = (unsigned)ioremap_nocache(pDstAddr, src_wd*src_ht*4);
		if(pBaseAddr)
		{
			memset(pBaseAddr, 0x00, src_wd*src_ht*4);
 			iounmap(pBaseAddr);
		}
		else
			printk("%s logical memory can't be allocated!!\n", __func__);

		dprintk("%s fb_paddr=0x%08x fb_laddr=0x%08x size=%d\n", __func__, pDstAddr, pBaseAddr, src_wd*src_ht*4);
	}
	
	#if defined(CONFIG_HDMI_DISPLAY_LASTFRAME) && defined(CONFIG_ARCH_TCC892X)
		/* initialize work-queue */
	    INIT_WORK(&tcc_output_attach_work_q, TCC_OUTPUT_FB_AttachWork);

		/* open M-to-M scaler driver */
	
		tccxxx_wmixer1_open((struct inode *)&wmixer_attach_inode, (struct file *)&wmixer_attach_filp);
	

		/* set WDMA */
		VIOC_WDMA_SetImageFormat(pWDMA, fmt);
		VIOC_WDMA_SetImageSize(pWDMA, src_wd, src_ht);
		VIOC_WDMA_SetImageOffset(pWDMA, fmt, src_wd);
		VIOC_WDMA_SetImageBase(pWDMA, pDstAddr, NULL, NULL);
	
		printk("%s, attach_lcdc_num=%d, attach_output=%d, M-to-M scaler used!!\n", __func__, dst_lcdc_num, dst_output);
	#else
		/* set SCALER before plugging-in */
		VIOC_SC_SetBypass(pSC, OFF);
		VIOC_SC_SetSrcSize(pSC, src_wd, src_ht);
		VIOC_SC_SetDstSize(pSC, dst_wd, dst_ht);
		VIOC_SC_SetOutSize(pSC, dst_wd, dst_ht);
		VIOC_SC_SetUpdate(pSC);

		/* set the scaler */		
		if(tcc_output_attach_plugin == 0)
		{
			if(src_lcdc_num)
				VIOC_CONFIG_PlugIn(scaler_num, VIOC_SC_WDMA_01);
			else
				VIOC_CONFIG_PlugIn(scaler_num, VIOC_SC_WDMA_00);

			tcc_output_attach_plugin = 1;
		}

		printk("%s, attach_lcdc_num=%d, attach_output=%d, scaler plugged-in!!\n", __func__, dst_lcdc_num, dst_output);
	#endif

	/* set WMIX path to Bypass */
	if(dst_lcdc_num)
		VIOC_CONFIG_WMIXPath(WMIX10, 0);
	else
		VIOC_CONFIG_WMIXPath(WMIX00, 0);

	/* set RDMA */
	VIOC_RDMA_SetImageAlphaSelect(pRDMA, 0);
	VIOC_RDMA_SetImageAlphaEnable(pRDMA, 0);
	VIOC_RDMA_SetImageFormat(pRDMA, fmt);
	VIOC_RDMA_SetImageSize(pRDMA, dst_wd, dst_ht);
	VIOC_RDMA_SetImageIntl(pRDMA, 0);
	VIOC_RDMA_SetImageOffset(pRDMA, fmt, dst_wd);
	VIOC_RDMA_SetImageBase(pRDMA, pDstAddr, NULL, NULL);
	VIOC_RDMA_SetImageEnable(pRDMA);

	VIOC_DISP_TurnOn(pDISP);

	/* set the flag */
	tcc_output_attach_flag = 1;
	/* set the number of lcdc */
	tcc_output_attach_lcdc = dst_lcdc_num;
	/* set the output type */
	tcc_output_attach_output = dst_output;
	/* set the resolution */
	tcc_output_attach_resolution = resolution;
	/* set the starter flag */
	tcc_output_attach_starter = starter_flag;
	/* init index number */
	tcc_output_attach_index = 0;
	/* init index number for memory */
	tcc_output_attach_index_memory = 0;
	/* init index number for displaying */
	tcc_output_attach_index_display = 0;
	/* init count number for updating */
	tcc_output_attach_update_count = 0;
}

void TCC_OUTPUT_FB_DetachOutput(char disable_all)
{
	int i = 0, scaler_num;
	PVIOC_DISP	pDISP;
	PVIOC_RDMA	pRDMA;
	PVIOC_WDMA	pWDMA;

	PVIOC_DISP	pDISP_1st;
	PVIOC_RDMA	pRDMA_1st;
	PVIOC_DISP	pDISP_2nd;
	PVIOC_RDMA	pRDMA_2nd;

	/* check the flag */
	if(tcc_output_attach_flag == 0)
	{
		dprintk("%s, attach_lcdc_num=%d, attach_output(%d) was already detached!!\n", __func__, tcc_output_attach_lcdc, tcc_output_attach_output);
		return;
	}

	dprintk("%s, attach_lcdc_num=%d, attach_output=%d, disable_all=%d\n", __func__, tcc_output_attach_lcdc, tcc_output_attach_output, disable_all);

	/* clear the flag */
	tcc_output_attach_flag = 0;

	if(tcc_output_attach_output == TCC_OUTPUT_COMPOSITE)
	{
		#if defined(CONFIG_FB_TCC_COMPOSITE)
		if(OutputType != TCC_OUTPUT_COMPOSITE)	
			tcc_composite_attach(tcc_output_attach_lcdc, 0, 0);
		#endif
	}
	else if(tcc_output_attach_output == TCC_OUTPUT_COMPONENT)
	{
		#if defined(CONFIG_FB_TCC_COMPONENT)
		if(OutputType != TCC_OUTPUT_COMPONENT)	
			tcc_component_attach(tcc_output_attach_lcdc, 0, 0);
		#endif
	}
	
	/* set the register */
	if(tcc_output_attach_lcdc)
	{
		pWDMA = (VIOC_WDMA *)tcc_p2v(HwVIOC_WDMA00);
		pDISP_1st = (VIOC_DISP *)tcc_p2v(HwVIOC_DISP1);
		pRDMA_1st = (VIOC_RDMA *)tcc_p2v(HwVIOC_RDMA04);
		pDISP_2nd = (VIOC_DISP *)tcc_p2v(HwVIOC_DISP0);
		pRDMA_2nd = (VIOC_RDMA *)tcc_p2v(HwVIOC_RDMA00);
	}
	else
	{
		pWDMA = (VIOC_WDMA *)tcc_p2v(HwVIOC_WDMA01);
		pDISP_1st = (VIOC_DISP *)tcc_p2v(HwVIOC_DISP0);
		pRDMA_1st = (VIOC_RDMA *)tcc_p2v(HwVIOC_RDMA00);
		pDISP_2nd = (VIOC_DISP *)tcc_p2v(HwVIOC_DISP1);
		pRDMA_2nd = (VIOC_RDMA *)tcc_p2v(HwVIOC_RDMA04);
	}

	/* disable the attached output */
	BITCSET(pDISP_1st->uCTRL.nREG, 0xFFFFFFFF, 0);

	VIOC_DISP_TurnOff(pDISP_1st);
	VIOC_RDMA_SetImageDisable(pRDMA_1st);

	VIOC_DISP_Wait_DisplayDone(pDISP_1st);

	#if defined(CONFIG_HDMI_DISPLAY_LASTFRAME) && defined(CONFIG_ARCH_TCC892X)
		/* release M-to-M scaler driver */
		#if defined(CONFIG_ARCH_TCC892X)
		tccxxx_scaler2_release((struct inode *)&scaler_attach_inode, (struct file *)&scaler_attach_filp);
		#else
		tccxxx_wmixer1_release((struct inode *)&wmixer_attach_inode, (struct file *)&wmixer_attach_filp);
		#endif
	#else
		/* set the scaler number */		
		#if defined(CONFIG_ARCH_TCC893X)
			scaler_num = VIOC_SC3;
		#else
			scaler_num = VIOC_SC0;
		#endif

		/* plug-out scaler */
		if(tcc_output_attach_plugin)
		{
			VIOC_CONFIG_PlugOut(scaler_num);
			tcc_output_attach_plugin = 0;
		}
	#endif
		
	#if 0
	/* set WMIX path to Mixing */
	if(tcc_output_attach_lcdc)
		VIOC_CONFIG_WMIXPath(WMIX10, 1);
	else
		VIOC_CONFIG_WMIXPath(WMIX00, 1);
	#endif

	/* disable another output */
	if(disable_all)
	{
		BITCSET(pDISP_2nd->uCTRL.nREG, 0xFFFFFFFF, 0);
		
		VIOC_DISP_TurnOff(pDISP_2nd);
		VIOC_RDMA_SetImageDisable(pRDMA_2nd);


		VIOC_DISP_Wait_DisplayDone(pDISP_2nd);

		/* plug-out scaler */
		if(output_lcdc_onoff)
			TCC_OUTPUT_UPDATE_OnOff(0, OutputType);
	}

	/* disable WDMA */
	VIOC_WDMA_SetImageDisable(pWDMA);
	
}

void TCC_OUTPUT_FB_AttachUpdateFlag(char src_lcdc_num)
{
	int count = 0;
	PVIOC_WDMA pWDMA;
	char *pWDMA_Addr;
			
	if(tcc_output_attach_flag)
	{
		#if defined(CONFIG_HDMI_DISPLAY_LASTFRAME) && defined(CONFIG_ARCH_TCC892X)
			if(src_lcdc_num)
				pWDMA = (VIOC_WDMA *)tcc_p2v(HwVIOC_WDMA01);
			else
				pWDMA = (VIOC_WDMA *)tcc_p2v(HwVIOC_WDMA00);

			if(tcc_output_attach_index_memory)
				pWDMA_Addr = (char *)output_attach_pbuf0;
			else
				pWDMA_Addr = (char *)output_attach_pbuf1;
		
			//tcc_output_attach_index_memory = !tcc_output_attach_index_memory;

			VIOC_WDMA_SetImageBase(pWDMA, pWDMA_Addr, NULL, NULL);
			VIOC_WDMA_SetImageEnable(pWDMA, 0 /* OFF */);

			if(tcc_output_attach_starter)
			{
				/* Wait EOF status */
				BITSET(pWDMA->uIRQSTS.nREG, Hw5);
				for(count=0; count<0x50000; count++)
				{
					__asm("NOP");

					if(pWDMA->uIRQSTS.nREG & Hw5)
					{
						BITSET(pWDMA->uIRQSTS.nREG, Hw5);
						break;
					}
				}
			}
		#endif

		if(tcc_output_attach_starter)
		{
			TCC_OUTPUT_FB_AttachUpdateScreen(src_lcdc_num);
			tcc_output_attach_starter = 0;
		}
		else
			tcc_output_attach_update = 1;
	}
}

void  TCC_OUTPUT_FB_AttachUpdateScreen(char src_lcdc_num)
{
	PVIOC_DISP	pDISP;
	PVIOC_WMIX	pWMIX;
	PVIOC_WDMA	pWDMA;
	PVIOC_RDMA	pRDMA;
	PVIOC_SC 	pSC;
	char *pWDMA_Addr;
	int dst_lcdc_num, fmt, src_wd, src_ht, dst_wd, dst_ht, dst_w_pos, dst_h_pos;

	struct inode sc_inode;
	struct file sc_filp;
	SCALER_TYPE fbscaler;
	
	/* check the flag */
	if(tcc_output_attach_flag == 0)
	{
		dprintk("%s, No output was attached!!\n", __func__);
		return;
	}

	dprintk("%s, index:%d\n", __func__, tcc_output_attach_index);

	/* set the scaler number */		
	#if defined(CONFIG_ARCH_TCC893X)
		pSC = (VIOC_SC *)tcc_p2v(HwVIOC_SC3);
	#else
		pSC = (VIOC_SC *)tcc_p2v(HwVIOC_SC0);
	#endif

	pSC = (VIOC_SC *)tcc_p2v(HwVIOC_SC0);

	/* get the source resolution */
	if(src_lcdc_num)
	{
		dst_lcdc_num = 0;
		pDISP = (VIOC_DISP *)tcc_p2v(HwVIOC_DISP1);
		pWDMA = (VIOC_WDMA *)tcc_p2v(HwVIOC_WDMA01);
	}
	else
	{
		dst_lcdc_num = 1;
		pDISP = (VIOC_DISP *)tcc_p2v(HwVIOC_DISP0);
		pWDMA = (VIOC_WDMA *)tcc_p2v(HwVIOC_WDMA00);
	}

	//dprintk("%s, attach_lcdc_num=%d\n", __func__, dst_lcdc_num);

	src_wd = (pDISP->uLSIZE.nREG & 0xFFFF);
	src_ht = ((pDISP->uLSIZE.nREG>>16) & 0xFFFF);
	fmt = TCC_LCDC_IMG_FMT_RGB888;
	
	/* get the destination resolution */
	if(dst_lcdc_num)
	{
		pDISP = (VIOC_DISP *)tcc_p2v(HwVIOC_DISP1);
		pWMIX = (VIOC_WMIX *)tcc_p2v(HwVIOC_WMIX1);
		pRDMA = (VIOC_RDMA *)tcc_p2v(HwVIOC_RDMA04);
	}
	else
	{
		pDISP = (VIOC_DISP *)tcc_p2v(HwVIOC_DISP0);
		pWMIX = (VIOC_WMIX *)tcc_p2v(HwVIOC_WMIX0);
		pRDMA = (VIOC_RDMA *)tcc_p2v(HwVIOC_RDMA00);
	}

	dst_wd = (pDISP->uLSIZE.nREG & 0xFFFF);
	dst_ht = ((pDISP->uLSIZE.nREG>>16) & 0xFFFF);

#if defined(CONFIG_HDMI_DISPLAY_LASTFRAME) && defined(CONFIG_ARCH_TCC892X)
	if(tcc_output_attach_index)
		pWDMA_Addr = (char *)output_attach_pbuf0;
	else
		pWDMA_Addr = (char *)output_attach_pbuf1;
	
	#if 0
	dst_wd -= uiOutputResizeMode.resize_x << 3;
	dst_ht -= uiOutputResizeMode.resize_y << 2;
    dst_w_pos = uiOutputResizeMode.resize_x << 2;
	dst_h_pos = uiOutputResizeMode.resize_y << 1;
	VIOC_WMIX_SetPosition(pWMIX, 0, dst_w_pos, dst_h_pos);
	#endif

	#if defined(CONFIG_ARCH_TCC892X)
	memset(&fbscaler, 0x00, sizeof(fbscaler));
       
	fbscaler.responsetype = SCALER_POLLING;
	//fbscaler.responsetype = SCALER_NOWAIT;
	
	fbscaler.src_Yaddr = pWDMA_Addr;
	fbscaler.src_fmt = fmt;		
	fbscaler.src_ImgWidth = src_wd;
	fbscaler.src_ImgHeight = src_ht;
	fbscaler.src_winLeft = 0;
	fbscaler.src_winTop = 0;
	fbscaler.src_winRight = src_wd;
	fbscaler.src_winBottom = src_ht;

	if(tcc_output_attach_index)
		fbscaler.dest_Yaddr = output_attach_pbuf2;
	else
		fbscaler.dest_Yaddr = output_attach_pbuf3;

	//tcc_output_attach_index = !tcc_output_attach_index;
		
	fmt = TCC_LCDC_IMG_FMT_RGB888;
	
	fbscaler.dest_fmt = fmt;					// destination image format
	fbscaler.dest_ImgWidth = dst_wd;			// destination image width
	fbscaler.dest_ImgHeight = dst_ht; 			// destination image height
	fbscaler.dest_winLeft = 0;
	fbscaler.dest_winTop = 0;
	fbscaler.dest_winRight = dst_wd;
	fbscaler.dest_winBottom = dst_ht;
	fbscaler.divide_path = 0x00;

	//scaler_open((struct inode *)&scaler_attach_inode, (struct file *)&scaler_attach_filp);
	tccxxx_scaler2_ioctl((struct file *)&scaler_attach_filp, TCC_SCALER_IOCTRL_KERENL, &fbscaler);
	//scaler_release((struct inode *)&scaler_attach_inode, (struct file *)&scaler_attach_filp);

	pWDMA_Addr = fbscaler.dest_Yaddr;
	#else
	WMIXER_ALPHASCALERING_INFO_TYPE fbmixer;

	memset(&fbmixer, 0x00, sizeof(fbmixer));
	   
	fbmixer.rsp_type = WMIXER_POLLING;

	fbmixer.src_y_addr = pWDMA_Addr;
	fbmixer.src_fmt = fmt;		
	fbmixer.src_img_width = src_wd;
	fbmixer.src_img_height = src_ht;
	fbmixer.src_win_left = 0;
	fbmixer.src_win_top = 0;
	fbmixer.src_win_right = src_wd;
	fbmixer.src_win_bottom = src_ht;

	if(tcc_output_attach_index)
		fbmixer.dst_y_addr = output_attach_pbuf2;
	else
		fbmixer.dst_y_addr = output_attach_pbuf3;

	//output_3d_ui_index = !output_3d_ui_index;
		
	fmt = TCC_LCDC_IMG_FMT_RGB888;

	fbmixer.dst_fmt = fmt;

	fbmixer.dst_img_width = dst_wd;
	fbmixer.dst_img_height = dst_ht;
	fbmixer.dst_win_left = 0;
	fbmixer.dst_win_top = 0;
	fbmixer.dst_win_right = dst_wd;
	fbmixer.dst_win_bottom = dst_ht;

	//tccxxx_wmixer1_open((struct inode *)&wmixer_attach_inode, (struct file *)&wmixer_attach_filp);
	tccxxx_wmixer1_ioctl((struct file *)&wmixer_attach_filp, TCC_WMIXER_ALPHA_SCALING_KERNEL, &fbmixer);
	//tccxxx_wmixer1_release((struct inode *)&wmixer_attach_inode, (struct file *)&wmixer_attach_filp);
	
	pWDMA_Addr = fbmixer.dst_y_addr;
	#endif
	
	/* set RDMA */
	VIOC_RDMA_SetImageFormat(pRDMA, fmt);
	VIOC_RDMA_SetImageSize(pRDMA, dst_wd, dst_ht);
	VIOC_RDMA_SetImageOffset(pRDMA, fmt, dst_wd);
	VIOC_RDMA_SetImageBase(pRDMA, pWDMA_Addr, 0, 0);
	VIOC_RDMA_SetImageEnable(pRDMA);

	VIOC_WMIX_SetUpdate(pWMIX);
#else
	pWDMA_Addr = (char *)output_attach_pbuf0;
		
	VIOC_WDMA_SetImageFormat(pWDMA, fmt);
	VIOC_WDMA_SetImageSize(pWDMA, dst_wd, dst_ht);
	VIOC_WDMA_SetImageOffset(pWDMA, fmt, dst_wd);
	VIOC_WDMA_SetImageBase(pWDMA, pWDMA_Addr, NULL, NULL);
	VIOC_WDMA_SetImageEnable(pWDMA, 0 /* OFF */);
	
	#if 0
	dst_wd -= uiOutputResizeMode.resize_x << 3;
	dst_ht -= uiOutputResizeMode.resize_y << 2;
    dst_w_pos = uiOutputResizeMode.resize_x << 2;
	dst_h_pos = uiOutputResizeMode.resize_y << 1;
	VIOC_WMIX_SetPosition(pWMIX, 0, dst_w_pos, dst_h_pos);
	#endif

	VIOC_SC_SetBypass(pSC, OFF);
	VIOC_SC_SetSrcSize(pSC, src_wd, src_ht);
	VIOC_SC_SetDstSize(pSC, dst_wd, dst_ht);
	VIOC_SC_SetOutSize(pSC, dst_wd, dst_ht);

	/* set RDMA */
	VIOC_RDMA_SetImageSize(pRDMA, dst_wd, dst_ht);
	VIOC_RDMA_SetImageOffset(pRDMA, fmt, dst_wd);
	VIOC_RDMA_SetImageBase(pRDMA, pWDMA_Addr, 0, 0);
	VIOC_RDMA_SetImageEnable(pRDMA);

	if(tcc_output_attach_plugin)
		VIOC_SC_SetUpdate(pSC);

	VIOC_WMIX_SetUpdate(pWMIX);
#endif
}

void TCC_OUTPUT_FB_AttachSetSate(char attach_state)
{
	printk("%s, attach_state=%d\n", __func__, attach_state);

	if(attach_state)
	{
		/* attach new output */
		/* set the flag */
		tcc_output_attach_state = 1;
	}
	else
	{
		/* detach the attached output */
		TCC_OUTPUT_FB_DetachOutput(0);
		/* clear the flag */
		tcc_output_attach_state = 0;
	}
}

char TCC_OUTPUT_FB_AttachGetSate(void)
{
	return tcc_output_attach_state;
}

void TCC_OUTPUT_FB_setFBInfo(struct tccfb_info* info)
{
	printk(" %s info 0x%08x\n",__func__,info);
	fbInfo = info;
}

