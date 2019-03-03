/* linux/drivers/misc/gnss/gnss_main.c
 *
 * Copyright (C) 2010 Google, Inc.
 * Copyright (C) 2010 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/if_arp.h>

#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/mfd/syscon.h>
#include <linux/clk-private.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_platform.h>
#endif

#include "gnss_prj.h"

extern struct shmem_conf shmem_conf;

static struct gnss_ctl *create_gnssctl_device(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct gnss_data *pdata = pdev->dev.platform_data;
	struct gnss_ctl *gnssctl;
	struct clk *qch_clk;
	int ret;

	/* create GNSS control device */
	gnssctl = devm_kzalloc(dev, sizeof(struct gnss_ctl), GFP_KERNEL);
	if (!gnssctl) {
		gif_err("%s: gnssctl devm_kzalloc fail\n", pdata->name);
		return NULL;
	}

	gnssctl->dev = dev;
	gnssctl->gnss_state = STATE_OFFLINE;

	gnssctl->gnss_data = pdata;
	gnssctl->name = pdata->name;

	qch_clk = devm_clk_get(dev, "ccore_qch_lh_gnss");
	if (!IS_ERR(qch_clk)) {
		gif_err("Found Qch clk!\n");
		gnssctl->ccore_qch_lh_gnss = qch_clk;
	}
	else {
		gnssctl->ccore_qch_lh_gnss = NULL;
	}

#ifdef USE_IOREMAP_NOPMU
	gnssctl->pmu_reg = devm_ioremap(dev, PMU_ADDR, PMU_SIZE);
	if (gnssctl->pmu_reg == NULL) {
		gif_err("%s: pmu ioremap failed.\n", pdata->name);
		return NULL;
	} else
		gif_err("pmu_reg : 0x%p\n", gnssctl->pmu_reg);
#endif

	/* init gnssctl device for getting gnssctl operations */
	ret = init_gnssctl_device(gnssctl, pdata);
	if (ret) {
		gif_err("%s: init_gnssctl_device fail (err %d)\n",
			pdata->name, ret);
		devm_kfree(dev, gnssctl);
		return NULL;
	}

	gif_info("%s is created!!!\n", pdata->name);

	return gnssctl;
}

static struct io_device *create_io_device(struct platform_device *pdev,
		struct gnss_io_t *io_t, struct link_device *ld,
		struct gnss_ctl *gnssctl, struct gnss_data *pdata)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct io_device *iod;

	iod = devm_kzalloc(dev, sizeof(struct io_device), GFP_KERNEL);
	if (!iod) {
		gif_err("iod is NULL\n");
		return NULL;
	}

	iod->name = io_t->name;
	iod->app = io_t->app;
	atomic_set(&iod->opened, 0);

	/* link between io device and gnss control */
	iod->gc = gnssctl;
	gnssctl->iod = iod;

	/* link between io device and link device */
	iod->ld = ld;
	ld->iod = iod;

	/* register misc device */
	ret = exynos_init_gnss_io_device(iod);
	if (ret) {
		devm_kfree(dev, iod);
		gif_err("exynos_init_gnss_io_device fail (%d)\n", ret);
		return NULL;
	}

	gif_info("%s created\n", iod->name);
	return iod;
}

#ifdef CONFIG_OF
static int parse_dt_common_pdata(struct device_node *np,
					struct gnss_data *pdata)
{
	gif_dt_read_string(np, "shmem,name", pdata->name);
	gif_dt_read_string(np, "shmem,device_node_name", pdata->device_node_name);
	gif_dt_read_u32(np, "shmem,ipc_offset", pdata->ipcmem_offset);
	gif_dt_read_u32(np, "shmem,ipc_size", pdata->ipc_size);
	gif_dt_read_u32(np, "shmem,ipc_reg_cnt", pdata->ipc_reg_cnt);

	/* Shared Memory Configuration from reserved_mem */
	pdata->shmem_base = shmem_conf.shmem_base;
	pdata->shmem_size = shmem_conf.shmem_size;

	return 0;
}

static int parse_dt_mbox_pdata(struct device *dev, struct device_node *np,
					struct gnss_data *pdata)
{
	struct gnss_mbox *mbox = pdata->mbx;

	mbox = devm_kzalloc(dev, sizeof(struct gnss_mbox), GFP_KERNEL);
	if (!mbox) {
		gif_err("mbox: failed to alloc memory\n");
		return -ENOMEM;
	}
	pdata->mbx = mbox;

	gif_dt_read_u32(np, "mbx,int_ap2gnss_bcmd", mbox->int_ap2gnss_bcmd);
	gif_dt_read_u32(np, "mbx,int_ap2gnss_req_fault_info",
			mbox->int_ap2gnss_req_fault_info);
	gif_dt_read_u32(np, "mbx,int_ap2gnss_ipc_msg", mbox->int_ap2gnss_ipc_msg);
	gif_dt_read_u32(np, "mbx,int_ap2gnss_ack_wake_set",
			mbox->int_ap2gnss_ack_wake_set);
	gif_dt_read_u32(np, "mbx,int_ap2gnss_ack_wake_clr",
			mbox->int_ap2gnss_ack_wake_clr);

	gif_dt_read_u32(np, "mbx,irq_gnss2ap_bcmd", mbox->irq_gnss2ap_bcmd);
	gif_dt_read_u32(np, "mbx,irq_gnss2ap_rsp_fault_info",
			mbox->irq_gnss2ap_rsp_fault_info);
	gif_dt_read_u32(np, "mbx,irq_gnss2ap_ipc_msg", mbox->irq_gnss2ap_ipc_msg);
	gif_dt_read_u32(np, "mbx,irq_gnss2ap_req_wake_clr",
			mbox->irq_gnss2ap_req_wake_clr);

	gif_dt_read_u32_array(np, "mbx,reg_bcmd_ctrl", mbox->reg_bcmd_ctrl,
			      BCMD_CTRL_COUNT);

	return 0;
}

static int alloc_gnss_reg(struct device *dev, struct gnss_shared_reg **areg,
		const char *reg_name, u32 reg_device, u32 reg_value)
{
	struct gnss_shared_reg *ret = NULL;
	if (!(*areg)) {
		ret = devm_kzalloc(dev, sizeof(struct gnss_shared_reg), GFP_KERNEL);
		if (ret) {
			ret->name = reg_name;
			ret->device = reg_device;
			ret->value.index = reg_value;
			*areg = ret;
		}
	}
	else {
		gif_err("Register %s is already allocated!\n", reg_name);
	}
	return (*areg != NULL);
}


static int parse_single_dt_reg(struct device *dev, const char *propname,
		struct gnss_shared_reg **reg)
{
	struct device_node *np = dev->of_node;
	u32 val[2];
	if (!of_property_read_u32_array(np, propname, val, 2)) {
		if (!alloc_gnss_reg(dev, reg, propname, val[0], val[1]))
			return -EINVAL;
	}
	return 0;
}

static int parse_dt_reg_mbox_pdata(struct device *dev, struct gnss_data *pdata)
{
	int i;

	if (parse_single_dt_reg(dev, "reg_rx_ipc_msg",
			&pdata->reg[GNSS_REG_RX_IPC_MSG]) != 0) {
		goto parse_dt_reg_nomem;
	}

	if (parse_single_dt_reg(dev, "reg_tx_ipc_msg",
			&pdata->reg[GNSS_REG_TX_IPC_MSG]) != 0) {
		goto parse_dt_reg_nomem;
	}

	if (parse_single_dt_reg(dev, "reg_wake_lock",
			&pdata->reg[GNSS_REG_WAKE_LOCK]) != 0) {
		goto parse_dt_reg_nomem;
	}

	if (parse_single_dt_reg(dev, "reg_rx_head",
			&pdata->reg[GNSS_REG_RX_HEAD]) != 0) {
		goto parse_dt_reg_nomem;
	}

	if (parse_single_dt_reg(dev, "reg_rx_tail",
			&pdata->reg[GNSS_REG_RX_TAIL]) != 0) {
		goto parse_dt_reg_nomem;
	}

	if (parse_single_dt_reg(dev, "reg_tx_head",
			&pdata->reg[GNSS_REG_TX_HEAD]) != 0) {
		goto parse_dt_reg_nomem;
	}

	if (parse_single_dt_reg(dev, "reg_tx_tail",
			&pdata->reg[GNSS_REG_TX_TAIL]) != 0) {
		goto parse_dt_reg_nomem;
	}

	return 0;

parse_dt_reg_nomem:
	for (i = 0; i < GNSS_REG_COUNT; i++)
		if (pdata->reg[i])
			devm_kfree(dev, pdata->reg[i]);

	gif_err("reg: could not allocate register memory\n");
	return -ENOMEM;
}

static int parse_dt_fault_pdata(struct device *dev, struct gnss_data *pdata)
{
	struct device_node *np = dev->of_node;
	u32 tmp[3];

	if (!of_property_read_u32_array(np, "fault_info", tmp, 3)) {
		(pdata)->fault_info.name = "gnss_fault_info";
		(pdata)->fault_info.device = tmp[0];
		(pdata)->fault_info.value.index = tmp[1];
		(pdata)->fault_info.size = tmp[2];
	}
	else {
		return -EINVAL;
	}
	return 0;
}

static struct gnss_data *gnss_if_parse_dt_pdata(struct device *dev)
{
	struct gnss_data *pdata;
	int i;
	u32 ret;

	pdata = devm_kzalloc(dev, sizeof(struct gnss_data), GFP_KERNEL);
	if (!pdata) {
		gif_err("gnss_data: alloc fail\n");
		return ERR_PTR(-ENOMEM);
	}

	ret = parse_dt_common_pdata(dev->of_node, pdata);
	if (ret != 0) {
		gif_err("Failed to parse common pdata.\n");
		goto parse_dt_pdata_err;
	}

	ret = parse_dt_mbox_pdata(dev, dev->of_node, pdata);
	if (ret != 0) {
		gif_err("Failed to parse mailbox pdata.\n");
		goto parse_dt_pdata_err;
	}

	ret = parse_dt_reg_mbox_pdata(dev, pdata);
	if (ret != 0) {
		gif_err("Failed to parse mbox register pdata.\n");
		goto parse_dt_pdata_err;
	}

	ret = parse_dt_fault_pdata(dev, pdata);
	if (ret != 0) {
		gif_err("Failed to parse fault info pdata.\n");
		goto parse_dt_pdata_err;
	}

	for (i = 0; i < GNSS_REG_COUNT; i++) {
		if (pdata->reg[i])
			gif_err("Found reg: [%d:%d] %s\n",
					pdata->reg[i]->device,
					pdata->reg[i]->value.index,
					pdata->reg[i]->name);
	}

	gif_err("Fault info: %s [%d:%d:%d]\n",
			pdata->fault_info.name,
			pdata->fault_info.device,
			pdata->fault_info.value.index,
			pdata->fault_info.size);

	dev->platform_data = pdata;
	gif_info("DT parse complete!\n");
	return pdata;

parse_dt_pdata_err:
	if (pdata)
		devm_kfree(dev, pdata);

	return ERR_PTR(-EINVAL);
}

static const struct of_device_id sec_gnss_match[] = {
	{ .compatible = "samsung,gnss_shdmem_if", },
	{},
};
MODULE_DEVICE_TABLE(of, sec_gnss_match);
#else /* !CONFIG_OF */
static struct gnss_data *gnss_if_parse_dt_pdata(struct device *dev)
{
	return ERR_PTR(-ENODEV);
}
#endif /* CONFIG_OF */

static int gnss_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct gnss_data *pdata = dev->platform_data;
	struct gnss_ctl *gnssctl;
	struct io_device *iod;
	struct link_device *ld;
	unsigned size;

	gif_err("%s: +++\n", pdev->name);

	if (dev->of_node) {
		pdata = gnss_if_parse_dt_pdata(dev);
		if (IS_ERR(pdata)) {
			gif_err("GIF DT parse error!\n");
			return PTR_ERR(pdata);
		}
	}

	/* allocate iodev */
	size = sizeof(struct gnss_io_t);
	pdata->iodev = devm_kzalloc(dev, size, GFP_KERNEL);
	if (!pdata->iodev) {
		if (pdata->iodev)
			devm_kfree(dev, pdata->iodev);
		gif_err("iodev: failed to alloc memory\n");
		return PTR_ERR(pdata);
	}

	gnssctl = create_gnssctl_device(pdev);
	if (!gnssctl) {
		gif_err("%s: gnssctl == NULL\n", pdata->name);
		return -ENOMEM;
	}

	/* GNSS uses one IO device and does not need to be parsed from DT. */
	pdata->iodev->name = pdata->device_node_name;
	pdata->iodev->id = 0; /* Fixed channel 0. */
	pdata->iodev->app = "SLL";

	/* create link device */
	ld = gnss_shmem_create_link_device(pdev);
	if (!ld)
		goto free_gc;

	ld->gc = gnssctl;

	gif_err("%s: %s link created\n", pdata->name, ld->name);

	/* create io device and connect to gnssctl device */
	size = sizeof(struct io_device *);
	iod = (struct io_device *)devm_kzalloc(dev, size, GFP_KERNEL);

	iod = create_io_device(pdev, pdata->iodev, ld, gnssctl, pdata);
	if (!iod) {
		gif_err("%s: iod == NULL\n", pdata->name);
		goto free_iod;
	}

	/* attach device */
	gif_debug("set %s->%s\n", iod->name, ld->name);
	set_current_link(iod, iod->ld);

	platform_set_drvdata(pdev, gnssctl);

	gif_err("%s: ---\n", pdata->name);

	return 0;

free_iod:
	devm_kfree(dev, iod);

free_gc:
	devm_kfree(dev, gnssctl);

	gif_err("%s: xxx\n", pdata->name);

	return -ENOMEM;
}

static void gnss_shutdown(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct gnss_ctl *gc = dev_get_drvdata(dev);

	/* Matt - Implement Shutdown */
	gc->gnss_state = STATE_OFFLINE;
}

#ifdef CONFIG_PM
static int gnss_suspend(struct device *pdev)
{
	struct gnss_ctl *gc = dev_get_drvdata(pdev);

	/* Matt - Implement Suspend */
	if (gc->ops.suspend_gnss_ctrl != NULL) {
		gif_err("%s: pd_active:0\n", gc->name);
		gc->ops.suspend_gnss_ctrl(gc);
	}

	return 0;
}

static int gnss_resume(struct device *pdev)
{
	struct gnss_ctl *gc = dev_get_drvdata(pdev);

	/* Matt - Implement Resume */
	if (gc->ops.resume_gnss_ctrl != NULL) {
		gif_err("%s: pd_active:1\n", gc->name);
		gc->ops.resume_gnss_ctrl(gc);
	}

	return 0;
}
#else
#define gnss_suspend	NULL
#define gnss_resume	NULL
#endif

static const struct dev_pm_ops gnss_pm_ops = {
	.suspend = gnss_suspend,
	.resume = gnss_resume,
};

static struct platform_driver gnss_driver = {
	.probe = gnss_probe,
	.shutdown = gnss_shutdown,
	.driver = {
		.name = "gif_exynos",
		.owner = THIS_MODULE,
		.pm = &gnss_pm_ops,

#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(sec_gnss_match),
#endif
	},
};

module_platform_driver(gnss_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Samsung GNSS Interface Driver");
