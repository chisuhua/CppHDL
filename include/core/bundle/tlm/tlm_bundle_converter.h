// include/tlm/tlm_bundle_converter.h
#ifndef CH_TLM_BUNDLE_CONVERTER_H
#define CH_TLM_BUNDLE_CONVERTER_H

// 注意：这个文件需要SystemC和TLM头文件
// 在实际项目中应该有条件编译

#ifdef USE_SYSTEMC_TLM

#include "core/bundle_serialization.h"
#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

namespace ch::tlm_integration {

// Bundle到TLM转换器
template<typename BundleT>
class bundle_tlm_converter {
public:
    // Bundle → TLM Generic Payload
    static tlm::tlm_generic_payload* bundle_to_tlm(const BundleT& bundle) {
        auto* payload = new tlm::tlm_generic_payload();
        
        // 序列化Bundle
        constexpr unsigned bundle_width = ch::core::bundle_width_v<BundleT>;
        constexpr size_t byte_count = (bundle_width + 7) / 8;
        
        // 设置数据
        auto* data_ptr = new unsigned char[byte_count];
        auto bits = ch::core::serialize(bundle);
        ch::core::bits_to_bytes(bits, data_ptr, byte_count);
        
        payload->set_data_ptr(data_ptr);
        payload->set_data_length(byte_count);
        payload->set_command(tlm::TLM_IGNORE_COMMAND);
        payload->set_response_status(tlm::TLM_OK_RESPONSE);
        
        return payload;
    }
    
    // TLM Generic Payload → Bundle
    static BundleT tlm_to_bundle(const tlm::tlm_generic_payload* payload) {
        constexpr unsigned bundle_width = ch::core::bundle_width_v<BundleT>;
        constexpr size_t byte_count = (bundle_width + 7) / 8;
        
        if (payload->get_data_length() < byte_count) {
            throw std::runtime_error("Insufficient data in TLM payload");
        }
        
        auto bits = ch::core::bytes_to_bits<bundle_width>(
            payload->get_data_ptr(), payload->get_data_length());
        
        return ch::core::deserialize<BundleT>(bits);
    }
    
    // 清理TLM payload
    static void cleanup_tlm_payload(tlm::tlm_generic_payload* payload) {
        if (payload) {
            if (payload->get_data_ptr()) {
                delete[] payload->get_data_ptr();
            }
            delete payload;
        }
    }
};

// Bundle TLM扩展
template<typename BundleT>
class bundle_tlm_extension : public tlm::tlm_extension<bundle_tlm_extension<BundleT>> {
public:
    BundleT bundle_data;
    
    bundle_tlm_extension() = default;
    bundle_tlm_extension(const BundleT& bundle) : bundle_data(bundle) {}
    
    virtual tlm_extension_base* clone() const override {
        return new bundle_tlm_extension(*this);
    }
    
    virtual void copy_from(tlm_extension_base& ext) override {
        bundle_data = static_cast<bundle_tlm_extension&>(ext).bundle_data;
    }
};

// Bundle TLM桥接器
template<typename BundleT>
class bundle_tlm_bridge {
public:
    explicit bundle_tlm_bridge(const std::string& name = "bundle_tlm_bridge")
        : socket((name + "_socket").c_str()) {}
    
    // 发送Bundle事务
    void send_bundle(const BundleT& bundle, sc_core::sc_time& delay) {
        auto* payload = bundle_tlm_converter<BundleT>::bundle_to_tlm(bundle);
        
        // 添加Bundle扩展
        auto* ext = new bundle_tlm_extension<BundleT>(bundle);
        payload->set_extension(ext);
        
        tlm::tlm_phase phase = tlm::BEGIN_REQ;
        socket->b_transport(*payload, delay);
        
        // 清理
        bundle_tlm_converter<BundleT>::cleanup_tlm_payload(payload);
    }
    
    // 接收Bundle事务
    BundleT receive_bundle(const tlm::tlm_generic_payload& payload) {
        // 首先尝试从扩展获取
        bundle_tlm_extension<BundleT>* ext = nullptr;
        if (payload.get_extension(ext)) {
            return ext->bundle_data;
        }
        
        // 否则从数据指针反序列化
        return bundle_tlm_converter<BundleT>::tlm_to_bundle(&payload);
    }
    
    tlm_utils::simple_initiator_socket<bundle_tlm_bridge> socket;
};

} // namespace ch::tlm_integration

#endif // USE_SYSTEMC_TLM

#endif // CH_TLM_BUNDLE_CONVERTER_H
