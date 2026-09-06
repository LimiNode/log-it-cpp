#include <logit/detail/MpscRingAny.hpp>

#include <cassert>

namespace {

struct MoveOnly {
    static int alive;

    explicit MoveOnly(int value) : value(value) { ++alive; }
    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;

    MoveOnly(MoveOnly&& other) noexcept : value(other.value) {
        ++alive;
        other.value = -1;
    }

    MoveOnly& operator=(MoveOnly&& other) noexcept {
        value = other.value;
        other.value = -1;
        return *this;
    }

    ~MoveOnly() { --alive; }

    int value;
};

int MoveOnly::alive = 0;

} // namespace

int main() {
    {
        logit::detail::MpscRingAny<MoveOnly> queue(2);
        assert(queue.try_push(MoveOnly(42)));
        assert(MoveOnly::alive == 1);

        MoveOnly output(0);
        assert(queue.try_pop(output));
        assert(output.value == 42);
    }

    assert(MoveOnly::alive == 0);

    {
        logit::detail::MpscRingAny<MoveOnly> queue(2);
        assert(queue.try_push(MoveOnly(7)));
        assert(MoveOnly::alive == 1);
    }

    assert(MoveOnly::alive == 0);
    return 0;
}
