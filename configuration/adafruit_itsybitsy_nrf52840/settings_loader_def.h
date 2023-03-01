#ifndef DIY_GNSS_V2_SETTINGS_LOADER_DEF_H
#define DIY_GNSS_V2_SETTINGS_LOADER_DEF_H
#include <caf/events/module_state_event.h>

static inline void get_req_modules(struct module_flags *mf) {
    module_flags_set_bit(mf, MODULE_IDX(main));
//#if CONFIG_CAF_BLE_ADV
//    module_flags_set_bit(mf, MODULE_IDX(ble_adv));
//#endif
};

#endif //DIY_GNSS_V2_SETTINGS_LOADER_DEF_H
