
        void as_slave() override {
            // 实现slave方向
            this->make_input(placeholder);
        }
    };
};

// 便捷的类型别名
template<typename BundleT>
using bundle_to_pod_t = typename bundle_to_pod<BundleT>::type;

template<typename PodT>
using pod_to_bundle_t = typename pod_to_bundle<PodT>::type;

} // namespace ch::core

#endif // CH_CORE_BUNDLE_POD_TRAITS_H