#pragma once
#ifndef LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_DETAIL_QUEUEPOLICY_HPP_INCLUDED
#define LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_DETAIL_QUEUEPOLICY_HPP_INCLUDED

namespace logit { namespace detail {

/// \brief Queue overflow handling policy used by TaskExecutor and SingleThreadExecutor.
enum class QueuePolicy {
    DropNewest, ///< Reject the incoming task when the queue is full.
    DropOldest, ///< Drop the oldest queued task.
    Block       ///< Producers wait until capacity is available.
};

}} // namespace logit::detail

#endif // LOGIT_CPP_HEADER_LOGIT_CPP_LOGIT_DETAIL_QUEUEPOLICY_HPP_INCLUDED
