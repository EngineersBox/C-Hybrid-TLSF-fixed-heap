#include "controller.h"
#include "../error/allocator_errno.h"

int controller_new(Controller* controller) {
    if (controller == NULL) {
        set_alloc_errno(NULL_CONTROLLER_INSTANCE);
        return -1;
    }
    controller->block_null.next_free = &controller->block_null;
    controller->block_null.prev_free = &controller->block_null;
    controller->fl_bitmap = 0;
    for (int i = 0; i < FL_INDEX_COUNT; i++) {
        controller->sl_bitmap[i] = 0;
        for (int j = 0; j < SL_INDEX_COUNT; j++) {
            controller->blocks[i][j] = &controller->block_null;
        }
    }
    return 0;
}
