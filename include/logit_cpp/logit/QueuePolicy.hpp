#pragma once
#ifndef LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_QUEUEPOLICY_HPP_INCLUDED
#define LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_QUEUEPOLICY_HPP_INCLUDED

#include "detail/QueuePolicy.hpp"

namespace logit {

    /// \brief Public queue overflow handling policy.
    using QueuePolicy = detail::QueuePolicy;

} // namespace logit

#endif // LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_QUEUEPOLICY_HPP_INCLUDED
