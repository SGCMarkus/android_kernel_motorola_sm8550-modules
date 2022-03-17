/*
 * Copyright © 2020, ConvenientPower
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __CPS_WLS_CHARGER_H__
#define __CPS_WLS_CHARGER_H__


#define CPS_WLS_FAIL    -1
#define CPS_WLS_SUCCESS 0

/*interupt define*/
#define INT_TX_DATA_RECEIVED            (0x01 << 0)
#define INT_UV                          (0x01 << 1)
#define INT_OT                          (0x01 << 2)
#define INT_OC                          (0x01 << 3)
#define INT_OV                          (0x01 << 4)
#define INT_OPERATION_MODE              (0x01 << 5)
#define INT_STAT_VRECT                  (0x01 << 6)
#define INT_STAT_VOUT                   (0x01 << 7)
#define INT_RSV_BIT_8                   (0x01 << 8)
#define INT_RSV_BIT_9                   (0x01 << 9)
#define INT_RSV_BIT_A                   (0x01 << 10)
#define INT_RSV_BIT_B                   (0x01 << 11)
#define INT_RSV_BIT_C                   (0x01 << 12)
#define INT_RSV_BIT_D                   (0x01 << 13)
#define INT_DATA_STORE                  (0x01 << 14)
#define INT_AC_MIS_DET                  (0x01 << 15)

/*command define*/
#define CMD_SEND_RX_DATA                (0x01 << 0)
#define CMD_LDO_TOGGLE                  (0x01 << 1)
#define CMD_MCU_RESET                   (0x01 << 2)
#define CMD_SEND_EPT_PKT                (0x01 << 3)
#define CMD_SEND_CS_PKT                 (0x01 << 4)
#define CMD_CLEAR_INT                   (0x01 << 5)
#define CMD_WATCHDOG_RESET              (0x01 << 6)
#define CMD_WATCHDOG_EN                 (0x01 << 7)

/*firmware download define*/
#define ADDR_BUFFER0        0x20000600
#define ADDR_BUFFER1        0x20000700

#define ADDR_CMD            0x200005FC
#define ADDR_FLAG           0x200005F8
#define ADDR_BUF_SIZE       0x200005F4

#define PGM_BUFFER0         0x10
#define PGM_BUFFER1         0x20
#define PGM_BUFFER2         0x30
#define PGM_BUFFER3         0x40
#define PGM_BUFFER0_1       0x50
#define PGM_ERASER_0        0x60
#define PGM_ERASER_1        0x70
#define PGM_WR_FLAG         0x80

#define CACL_CRC_APP        0x90
#define CACL_CRC_TEST       0xB0

#define PGM_ADDR_SET        0xC0

#define RUNNING             0x66
#define PASS                0x55
#define FAIL                0xAA
#define ILLEGAL             0x40
#define CPS_PROGRAM_BUFFER_SIZE 64


/*****************************************************************************
 *  Log
 ****************************************************************************/
#define CPS_LOG_NONE    0
#define CPS_LOG_ERR     1
#define CPS_LOG_DEBG    2
#define CPS_LOG_FULL    3
    
#define ENABLE_CPS_LOG CPS_LOG_FULL
    
#define cps_wls_log(num, fmt, args...) \
    do { \
            if (ENABLE_CPS_LOG >= (int)num) \
                pr_err(fmt, ##args); \
    } while (0)
    
/*-------------------------------------------------------------------*/
struct cps_wls_chrg_chip {
    struct i2c_client *client;
    struct device *dev;
    struct regmap *regmap;
    char *name;
    struct power_supply *wl_psy;
    struct power_supply *batt_psy;
    struct power_supply *usb_psy;
    struct power_supply *dc_psy;
    struct power_supply_desc wl_psd;
    struct power_supply_config wl_psc; 
    struct power_supply_desc batt_psd;    
    struct power_supply_desc usb_psd;
    struct power_supply_desc dc_psd;
    struct pinctrl *cps_pinctrl;
    struct pinctrl_state *cps_gpio_active;
    struct pinctrl_state *cps_gpio_suspend;

    struct wake_lock cps_wls_wake_lock;
    struct mutex   irq_lock;
    struct mutex   i2c_lock;
    int state;
    int wls_charge_int;
    int cps_wls_irq;
    int reg_addr;
    int reg_data;
    int rx_ovp;
    int rx_ocp;
    int rx_opp;
    int rx_ht_thl;
    int rx_vout_target;
    int rx_ept_rsn;
    int rx_iout;
    int rx_vrect;
    int rx_vout;
    int rx_die_temp;
    int rx_ntc;
    int rx_neg_power;
    int rx_neg_protocol;
    int command_flag;
};

#endif
