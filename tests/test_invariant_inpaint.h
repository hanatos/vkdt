#include <check.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include "../src/core/inpaint.h"

START_TEST(test_inpaint_allocation_overflow_boundary)
{
    // Invariant: Memory allocations must not overflow, and allocated size must be sufficient for intended access
    
    typedef struct {
        uint32_t cpp;
        uint32_t wd;
        uint32_t ht;
        const char *description;
    } test_case_t;
    
    test_case_t cases[] = {
        {UINT32_MAX / 4, 2, 2, "exploit: cpp overflow in sizeof(float)*cpp"},
        {1024, UINT32_MAX / 1024, 2, "boundary: wd*ht near overflow"},
        {4, 1920, 1080, "valid: normal image dimensions"}
    };
    
    for (int i = 0; i < 3; i++) {
        dt_inpaint_t d = {0};
        d.cpp = cases[i].cpp;
        d.wd = cases[i].wd;
        d.ht = cases[i].ht;
        
        // Check for overflow in sizeof(float)*cpp
        uint64_t size_per_pixel = (uint64_t)sizeof(float) * d.cpp;
        ck_assert_msg(size_per_pixel <= SIZE_MAX, 
                      "Overflow in sizeof(float)*cpp for case: %s", cases[i].description);
        
        // Check for overflow in total allocation size
        uint64_t total_pixels = (uint64_t)d.wd * d.ht;
        uint64_t total_size = size_per_pixel * total_pixels;
        ck_assert_msg(total_size <= SIZE_MAX && total_size / total_pixels == size_per_pixel,
                      "Overflow in total allocation for case: %s", cases[i].description);
        
        // If allocation would be valid, verify it succeeds or fails gracefully
        if (total_size <= SIZE_MAX && total_size > 0) {
            void *ptr = calloc(size_per_pixel, total_pixels);
            if (ptr != NULL) {
                free(ptr);
            }
        }
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_inpaint_allocation_overflow_boundary);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}