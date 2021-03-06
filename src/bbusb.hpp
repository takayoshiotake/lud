//
//  bbusb.hpp
//  lud
//
//  Created by OTAKE Takayoshi on 2017/03/01.
//  Copyright © 2017 OTAKE Takayoshi. All rights reserved.
//

#ifndef bbusb_h
#define bbusb_h

#include <string>
#include <locale>
#include <codecvt>

#include <functional>
#include <exception>
#include <memory>
#include <vector>
#include <unordered_map>

#include <stdio.h>
#include <libusb.h>

namespace bbusb {
    
    struct scope_exit {
        explicit scope_exit(std::function<void()>&& defer) {
            defer_ = std::move(defer);
        }
        
        ~scope_exit() {
            defer_();
        }
    private:
        std::function<void()> defer_;
        void operator =(const scope_exit&);
        scope_exit(const scope_exit&);
    };
    
    
    enum class usb_log_level {
        none,
        error,
        warning,
        info,
        debug,
    };
    
    struct usb_device {
        libusb_device_descriptor device_descriptor;
        libusb_config_descriptor* config_descriptor;
        
        explicit usb_device(libusb_device* device) {
            device_ = device;
            libusb_ref_device(device_);
            
            int rc;
            rc = libusb_get_device_descriptor(device_, &device_descriptor);
            if (rc != LIBUSB_SUCCESS) {
                throw rc;
            }
            rc = libusb_get_config_descriptor(device_, 0, &config_descriptor);
            if (rc != LIBUSB_SUCCESS) {
                throw rc;
            }
        }
        
        ~usb_device() {
            libusb_free_config_descriptor(config_descriptor);
            libusb_unref_device(device_);
        }
        
        int id() {
            return libusb_get_bus_number(device_) << 8 | libusb_get_device_address(device_);
        }
        
        void print() {
            libusb_device_handle* handle = nullptr;
            int rc;
            
            rc = libusb_open(device_, &handle);
            if (rc != LIBUSB_SUCCESS) {
            }
            scope_exit se1([handle](){
                if (handle != nullptr) {
                    libusb_close(handle);
                }
            });
            
            printf("device:\n");
            printf("  id: %d\n", id());
            
            printf("  device_descriptor:\n");
            printf("    bcdUSB: 0x%04x\n", device_descriptor.bcdUSB);
            printf("    bDeviceClass: %d\n", device_descriptor.bDeviceClass);
            printf("    bDeviceSubClass: %d\n", device_descriptor.bDeviceSubClass);
            printf("    bDeviceProtocol: %d\n", device_descriptor.bDeviceProtocol);
            printf("    bMaxPacketSize0: %d\n", device_descriptor.bMaxPacketSize0);
            printf("    idVendor: 0x%04x\n", device_descriptor.idVendor);
            printf("    idProduct: 0x%04x\n", device_descriptor.idProduct);
            printf("    bcdDevice: 0x%04x\n", device_descriptor.bcdDevice);
            if (device_descriptor.iManufacturer != 0) {
                std::string string = string_descriptor(handle, device_descriptor.iManufacturer);
                printf("    iManufacture: %s\n", string.c_str());
            }
            if (device_descriptor.iProduct != 0) {
                std::string string = string_descriptor(handle, device_descriptor.iProduct);
                printf("    iProduct: %s\n", string.c_str());
            }
            if (device_descriptor.iSerialNumber != 0) {
                std::string string = string_descriptor(handle, device_descriptor.iSerialNumber);
                printf("    iSerialNumber: %s\n", string.c_str());
            }
            printf("    bNumConfigurations: %d\n", device_descriptor.bNumConfigurations);
            printf("  config_descriptor:\n");
            printf("    bNumInterfaces: %d\n", config_descriptor->bNumInterfaces);
            printf("    bConfigurationValue: %d\n", config_descriptor->bConfigurationValue);
            if (config_descriptor->iConfiguration != 0) {
                std::string string = string_descriptor(handle, config_descriptor->iConfiguration);
                printf("    iConfiguration: %s\n", string.c_str());
            }
            printf("    bmAttributes: 0x%02x\n", config_descriptor->bmAttributes);
            printf("    bMaxPower: %dmA\n", config_descriptor->MaxPower * 2);
        }
    private:
        libusb_device* device_;
        
        std::string string_descriptor(libusb_device_handle* handle, int index) {
            unsigned char temp[2];
            int rc;
            
            rc = libusb_get_string_descriptor(handle, index, 0, temp, 2);
            if (rc == 2 && temp[0] > 0) {
                unsigned char string[temp[0]];
                if (libusb_get_string_descriptor(handle, index, 0, string, temp[0]) == temp[0]) {
                    
                    // Convert UTF16LE to UTF8 (std::wstring to std::string)
                    char* begin = (char*)&string[2];
                    char* end = (char*)&string[string[0]];
                    if ((end - begin) % 2 == 0) {
                        wchar_t utf16[(end - begin) / 2 + 1];
                        for (int i = 0; i < sizeof(utf16)/sizeof(utf16[0]); ++i) {
                            utf16[i] = begin[i*2+0] | (begin[i*2+1] << 8);
                        }
                        utf16[sizeof(utf16)/sizeof(utf16[0]) - 1] = 0;
                        
                        std::wstring ws(utf16);
                        std::wstring_convert<std::codecvt_utf8<wchar_t>,wchar_t> cv;
                        return cv.to_bytes(ws.c_str());
                    }
                }
            }
            
            return "";
        }
    };
    
    struct usb_manager {
        libusb_context* ctx_ = nullptr;
        
        explicit usb_manager() {
            auto rc = libusb_init(&ctx_);
            if (rc < 0) {
                throw rc;
            }
        }
        
        ~usb_manager() {
            libusb_exit(ctx_);
        }
        
        /*
         * - parameter level: Default value is usb_log_level::none.
         */
        void set_log_level(usb_log_level level) {
            static const std::unordered_map<usb_log_level, int> map = {
                { usb_log_level::none, LIBUSB_LOG_LEVEL_NONE },
                { usb_log_level::error, LIBUSB_LOG_LEVEL_ERROR },
                { usb_log_level::warning, LIBUSB_LOG_LEVEL_WARNING },
                { usb_log_level::info, LIBUSB_LOG_LEVEL_INFO },
                { usb_log_level::debug, LIBUSB_LOG_LEVEL_DEBUG },
            };
            libusb_set_debug(ctx_, map.at(level));
        }
        
        [[deprecated("Use list_devices() and usb_device::print().")]]
        void print_devices() {
            libusb_device** devs;
            auto rc = libusb_get_device_list(ctx_, &devs);
            if (rc < 0) {
                throw rc;
            }
            scope_exit se1([devs](){
                libusb_free_device_list(devs, 1);
            });
            
            auto** dev_ptr = devs;
            while (auto* dev = *dev_ptr++) {
                struct libusb_device_descriptor desc;
                auto rc = libusb_get_device_descriptor(dev, &desc);
                if (rc < 0) {
                    throw rc;
                }
                
                printf("id=%d, vid=%04x, pid=%04x", libusb_get_bus_number(dev) << 8 | libusb_get_device_address(dev), desc.idVendor, desc.idProduct);
                putchar('\n');
            }
        }
        
        std::vector<std::shared_ptr<usb_device>> list_devices() {
            std::vector<std::shared_ptr<usb_device>> devices;
            
            libusb_device** devs;
            auto rc = libusb_get_device_list(ctx_, &devs);
            if (rc < 0) {
                throw rc;
            }
            scope_exit se1([devs](){
                libusb_free_device_list(devs, 1);
            });
            
            auto** dev_ptr = devs;
            while (auto* dev = *dev_ptr++) {
                struct libusb_device_descriptor desc;
                auto rc = libusb_get_device_descriptor(dev, &desc);
                if (rc < 0) {
                    throw rc;
                }
                
                devices.push_back(std::make_shared<usb_device>(dev));
            }
            
            return devices;
        }
        
        /*
         * - parameter vid: Vendor-ID
         * - parameter pid: Product-ID
         *
         * - returns: a list of found device IDs.
         */
        std::vector<int> find_devices(int vid, int pid) {
            libusb_device** devs;
            auto rc = libusb_get_device_list(ctx_, &devs);
            if (rc < 0) {
                throw rc;
            }
            scope_exit se1([devs](){
                libusb_free_device_list(devs, 1);
            });
            
            std::vector<int> ids;
            auto** dev_ptr = devs;
            while (auto* dev = *dev_ptr++) {
                struct libusb_device_descriptor desc;
                auto rc = libusb_get_device_descriptor(dev, &desc);
                if (rc < 0) {
                    throw rc;
                }
                
                if (vid > 0 && desc.idVendor != vid) {
                    continue;
                }
                if (pid > 0 && desc.idProduct != pid) {
                    continue;
                }
                
                ids.push_back(libusb_get_bus_number(dev) << 8 | libusb_get_device_address(dev));
            }
            return ids;
        }
        
        /*
         * - parameter _T_usb_device_handle:
         * - parameter device_id:
         *
         * - returns: device; null when failed to open device.
         *
         * - see: `class usb_device_handle`
         */
        template <class _T_usb_device_handle>
        std::shared_ptr<_T_usb_device_handle> open(int device_id) {
            std::shared_ptr<_T_usb_device_handle> usb_device_handle = nullptr;
            
            {
                libusb_device** devs;
                auto rc = libusb_get_device_list(ctx_, &devs);
                if (rc < 0) {
                    throw rc;
                }
                scope_exit se1([devs](){
                    libusb_free_device_list(devs, 1);
                });
                
                std::vector<int> ids;
                auto** dev_ptr = devs;
                while (auto* dev = *dev_ptr++) {
                    int id = libusb_get_bus_number(dev) << 8 | libusb_get_device_address(dev);
                    if (device_id == id) {
                        libusb_device_handle* handle;
                        auto rc = libusb_open(dev, &handle);
                        if (rc < 0) {
                            throw rc;
                        }
                        
                        try {
                            usb_device_handle = std::make_shared<_T_usb_device_handle>(this, dev, handle);
                        }
                        catch (...) {
                            libusb_close(handle);
                            std::rethrow_exception(std::current_exception());
                        }
                        break;
                    }
                }
            }
            
            return usb_device_handle;
        }
        
        void print_endpoints(libusb_device* device) {
            libusb_config_descriptor* config_desc;
            {
                auto rc = libusb_get_config_descriptor(device, 0, &config_desc);
                if (rc < 0) {
                    throw rc;
                }
            }
            
            if (config_desc->bNumInterfaces == 1) {
                const libusb_interface& interface = config_desc->interface[0];
                if (interface.num_altsetting == 1) {
                    const libusb_interface_descriptor& interface_desc = interface.altsetting[0];
                    for (int ei = 0; ei < interface_desc.bNumEndpoints; ++ei) {
                        const libusb_endpoint_descriptor& endpoint_desc = interface_desc.endpoint[ei];
                        
                        printf("Endpoint:\n");
                        printf("- bLength: %d\n", endpoint_desc.bLength);
                        printf("- bDescriptorType: %d\n", endpoint_desc.bDescriptorType);
                        printf("- bEndpointAddress: %02x\n", endpoint_desc.bEndpointAddress);
                        printf("- bmAttributes: %02x\n", endpoint_desc.bmAttributes);
                        printf("- wMaxPacketSize: %d\n", endpoint_desc.wMaxPacketSize);
                        printf("- bInterval: %d\n", endpoint_desc.bInterval);
                    }
                }
            }
            libusb_free_config_descriptor(config_desc);
        }
        
    private:
        void operator =(const usb_manager&);
        usb_manager(const usb_manager&);
    };
    
    class usb_device_handle {
    protected:
        libusb_device_handle* handle_;
    public:
        explicit usb_device_handle(usb_manager* um, libusb_device* device, libusb_device_handle* handle) noexcept
        : handle_(handle) {
            um->print_endpoints(device);
        }
        
        virtual ~usb_device_handle() {
            if (is_claimed_) {
                try {
                    release_interface();
                }
                catch (...) {
                }
            }
            libusb_close(handle_);
        }
        
        int get_configuration() {
            int configuration;
            auto rc = libusb_get_configuration(handle_, &configuration);
            if (rc < 0) {
                throw rc;
            }
            return configuration;
        }
        
        void set_configuration(int configuration) {
            auto rc = libusb_set_configuration(handle_, configuration);
            if (rc < 0) {
                throw rc;
            }
        }
        
        bool is_kernel_driver_active(int interface_number = 0) {
            auto rc = libusb_kernel_driver_active(handle_, interface_number);
            switch (rc) {
                case 0:
                    return false;
                case 1:
                    return true;
                default:
                    throw rc;
            }
        }
        
        void detach_kernel_driver(int interface_number = 0) {
            auto rc = libusb_detach_kernel_driver(handle_, interface_number);
            if (rc < 0) {
                throw rc;
            }
        }
        
        void claim_interface(int interface_number = 0) {
            auto rc = libusb_claim_interface(handle_, interface_number);
            if (rc < 0) {
                throw rc;
            }
            is_claimed_ = true;
        }
        
        void release_interface(int interface_number = 0) {
            auto rc = libusb_release_interface(handle_, interface_number);
            if (rc < 0) {
                throw rc;
            }
            is_claimed_ = false;
        }
    private:
        bool is_claimed_ = false;
    };
    
}

#endif /* bbusb_h */
