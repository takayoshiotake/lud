//
//  lud.cpp
//  lud
//
//  Created by OTAKE Takayoshi on 2017/02/28.
//  Copyright © 2017 OTAKE Takayoshi. All rights reserved.
//

#include <memory>
#include <vector>

#include <stdio.h>
#include "bbusb.hpp"

//#define TEST_MODE

int main(int argc, const char * argv[]) {
    auto um = std::make_shared<bbusb::usb_manager>();
    auto devices = um->list_devices();
    for (auto device : devices) {
        device->print();
    }
    
#ifdef TEST_MODE
    printf("\n");
    printf("TEST:\n");
    printf("- vid: %04x\n", 0x04c5);
    printf("- pid: %04x\n", 0x11a6);
    // Fujitsu, F-01A
    auto ids = um->find_devices(0x04c5, 0x11a6);
    if (ids.size() > 0) {
        auto device_id = ids.front();
        
        auto device_handle = um->open<bbusb::usb_device_handle>(device_id);
        if (device_handle->is_kernel_driver_active()) {
            device_handle->detach_kernel_driver();
        }
        
        int configuration = device_handle->get_configuration();
        printf("configuration = %d\n", configuration);
        
        // Reset the device forcefully.
        device_handle->set_configuration(0);
        device_handle->set_configuration(1);
        
        device_handle->claim_interface();
        
        // ...
        
        device_handle->release_interface();
    }
#endif
    
    return 0;
}
