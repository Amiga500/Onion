/**
 * Unit Tests for RetroArch Command Utilities
 * 
 * Tests emulation I/O operations for correctness
 * Uses mocks for network operations
 */

#include "../unity/unity.h"
#include <string.h>
#include <stdio.h>

/* Mock UDP functions for testing without actual network */
static char mock_response[1024];
static int mock_send_result = 0;
static int mock_receive_result = 0;

/* Mock implementation of udp_send */
int udp_send(const char *ipAddress, int port, const char *message)
{
    (void)ipAddress;
    (void)port;
    (void)message;
    return mock_send_result;
}

/* Mock implementation of udp_send_receive */
int udp_send_receive(const char *ipAddress, int port, const char *message, 
                     char *response, size_t response_size)
{
    (void)ipAddress;
    (void)port;
    (void)message;
    
    if (mock_receive_result == 0 && response != NULL) {
        strncpy(response, mock_response, response_size - 1);
        response[response_size - 1] = '\0';
    }
    
    return mock_receive_result;
}

/* Include the retroarch_cmd implementation */
#include "../../src/common/utils/retroarch_cmd.c"

/* Test: retroarch_quit sends correct command */
void test_retroarch_quit(void)
{
    mock_send_result = 0;
    int result = retroarch_quit();
    TEST_ASSERT_EQUAL_INT(0, result);
}

/* Test: retroarch_pause sends correct command */
void test_retroarch_pause(void)
{
    mock_send_result = 0;
    int result = retroarch_pause();
    TEST_ASSERT_EQUAL_INT(0, result);
}

/* Test: retroarch_unpause sends correct command */
void test_retroarch_unpause(void)
{
    mock_send_result = 0;
    int result = retroarch_unpause();
    TEST_ASSERT_EQUAL_INT(0, result);
}

/* Test: retroarch_getStateSlot parses response correctly */
void test_retroarch_getStateSlot_success(void)
{
    strcpy(mock_response, "GET_STATE_SLOT 5");
    mock_receive_result = 0;
    
    int slot = -1;
    int result = retroarch_getStateSlot(&slot);
    
    TEST_ASSERT_EQUAL_INT(1, result);  /* sscanf returns 1 on success */
    TEST_ASSERT_EQUAL_INT(5, slot);
}

/* Test: retroarch_getStateSlot handles network failure */
void test_retroarch_getStateSlot_network_failure(void)
{
    mock_receive_result = -1;
    
    int slot = -1;
    int result = retroarch_getStateSlot(&slot);
    
    TEST_ASSERT_EQUAL_INT(-1, result);
}

/* Test: retroarch_setStateSlot formats command correctly */
void test_retroarch_setStateSlot(void)
{
    mock_send_result = 0;
    
    int result = retroarch_setStateSlot(3);
    TEST_ASSERT_EQUAL_INT(0, result);
}

/* Test: retroarch_save formats command with slot */
void test_retroarch_save(void)
{
    mock_send_result = 0;
    
    int result = retroarch_save(7);
    TEST_ASSERT_EQUAL_INT(0, result);
}

/* Test: retroarch_load formats command with slot */
void test_retroarch_load(void)
{
    mock_send_result = 0;
    
    int result = retroarch_load(2);
    TEST_ASSERT_EQUAL_INT(0, result);
}

/* Test: retroarch_getStatus parses PLAYING state */
void test_retroarch_getStatus_playing(void)
{
    strcpy(mock_response, "GET_STATUS PLAYING game_boy_advance,Pokemon,crc32=12345678");
    mock_receive_result = 0;
    
    RetroArchStatus_s status;
    int result = retroarch_getStatus(&status);
    
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_INT(RETROARCH_STATE_PLAYING, status.state);
    TEST_ASSERT_EQUAL_STRING("game_boy_advance,Pokemon,crc32=12345678", status.content_info);
}

/* Test: retroarch_getStatus parses PAUSED state */
void test_retroarch_getStatus_paused(void)
{
    strcpy(mock_response, "GET_STATUS PAUSED snes,Super Mario World,crc32=abcdef12");
    mock_receive_result = 0;
    
    RetroArchStatus_s status;
    int result = retroarch_getStatus(&status);
    
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_INT(RETROARCH_STATE_PAUSED, status.state);
}

/* Test: retroarch_getStatus handles CONTENTLESS state */
void test_retroarch_getStatus_contentless(void)
{
    strcpy(mock_response, "GET_STATUS CONTENTLESS");
    mock_receive_result = 0;
    
    RetroArchStatus_s status;
    int result = retroarch_getStatus(&status);
    
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_INT(RETROARCH_STATE_CONTENTLESS, status.state);
}

/* Test: retroarch_getInfo parses response correctly */
void test_retroarch_getInfo(void)
{
    strcpy(mock_response, "GET_INFO 4 2 5");
    mock_receive_result = 0;
    
    RetroArchInfo_s info;
    int result = retroarch_getInfo(&info);
    
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_UINT(4, info.max_disk_slots);
    TEST_ASSERT_EQUAL_UINT(2, info.disk_slot);
    TEST_ASSERT_EQUAL_INT(5, info.state_slot);
    TEST_ASSERT_TRUE(info.has_state_slot);
}

/* Main test runner */
int main(void)
{
    UNITY_BEGIN();
    
    /* Basic command tests */
    RUN_TEST(test_retroarch_quit);
    RUN_TEST(test_retroarch_pause);
    RUN_TEST(test_retroarch_unpause);
    
    /* State slot tests */
    RUN_TEST(test_retroarch_getStateSlot_success);
    RUN_TEST(test_retroarch_getStateSlot_network_failure);
    RUN_TEST(test_retroarch_setStateSlot);
    RUN_TEST(test_retroarch_save);
    RUN_TEST(test_retroarch_load);
    
    /* Status and info tests */
    RUN_TEST(test_retroarch_getStatus_playing);
    RUN_TEST(test_retroarch_getStatus_paused);
    RUN_TEST(test_retroarch_getStatus_contentless);
    RUN_TEST(test_retroarch_getInfo);
    
    return UNITY_END();
}
