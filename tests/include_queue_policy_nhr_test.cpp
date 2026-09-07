#include <logit/QueuePolicy.hpp>

int main() {
    logit::QueuePolicy policy = logit::QueuePolicy::Block;
    return policy == logit::QueuePolicy::Block ? 0 : 1;
}
