#include <stdio.h>

#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>

#include "test_uri.h"
#include "test_encode.h"
#include "test_options.h"
#include "test_pdu.h"
#include "test_error_response.h"
#include "test_session.h"
#include "test_sendqueue.h"
#include "test_wellknown.h"
#include "test_tls.h"
#include "coap2/libcoap.h"

int
main(int argc COAP_UNUSED, char **argv COAP_UNUSED) {
  CU_ErrorCode result;
  CU_BasicRunMode run_mode = CU_BRM_VERBOSE;

  if (CU_initialize_registry() != CUE_SUCCESS) {
    fprintf(stderr, "E: test framework initialization failed\n");
    return -2;
  }

  coap_startup();
  t_init_uri_tests();
  t_init_encode_tests();
  t_init_option_tests();
  t_init_pdu_tests();
  t_init_error_response_tests();
  t_init_session_tests();
  t_init_sendqueue_tests();
  t_init_wellknown_tests();
  t_init_tls_tests();

  CU_basic_set_mode(run_mode);
  result = CU_basic_run_tests();

  CU_cleanup_registry();
  coap_cleanup();

  return result;
}
