#pragma once

// Inline implementations for ch_stream member functions
// This file must be included AFTER both:
//   - bundle/stream_bundle.h
//   - chlib/stream_pipeline.h

namespace ch {

template <typename T>
auto ch_stream<T>::m2sPipe() {
    return chlib::stream_m2s_pipe(*this);
}

template <typename T>
auto ch_stream<T>::stage() {
    return chlib::stream_m2s_pipe(*this);
}

template <typename T>
auto ch_stream<T>::s2mPipe() {
    return chlib::stream_s2m_pipe(*this);
}

template <typename T>
auto ch_stream<T>::halfPipe() {
    return chlib::stream_half_pipe(*this);
}

} // namespace ch
