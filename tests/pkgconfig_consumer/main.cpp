#include <logit.hpp>

int main() {
    LOGIT_ADD_CONSOLE_DEFAULT();
    LOGIT_INFO("pkg-config consumer works");
    LOGIT_WAIT();
    return 0;
}
