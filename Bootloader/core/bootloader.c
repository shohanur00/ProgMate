#include "bootloader.h"

#include <stdint.h>
#include <stdbool.h>

#include "cdefs/cdefs.h"
#include "../config/bl_config.h"
#include "../config/bl_flash_config.h"
#include "../transport/bl_transport.h"
#include "../flash/bl_flash.h"
#include "../metadata/bl_metadata.h"

#define BL_MAX_RETRY_COUNT    (10U)

/* ==========================================================================
 * Private types and context
 * ========================================================================== */

typedef struct
{
    BL_Protocol_Packet_t last_packet;
    BL_State_t           last_state;
    bool                 last_packet_valid;
} BL_RetryContext_t;

typedef struct
{
    BL_Protocol_Parser_t parser;
    BL_State_t           state;
    BL_FirmwareInfo_t    firmware_info;
    BL_RetryContext_t    retry;
    uint8_t              retry_count;
    BL_FirmwareMetadata_t metadata;
} BL_Context_t;

static BL_Context_t bl_context =
{
    .parser = {0},
    .state = BL_STATE_WAIT_SYNC,
    .firmware_info = {0},
    .retry =
    {
        .last_packet = {0},
        .last_state = BL_STATE_WAIT_SYNC,
        .last_packet_valid = false
    },
    .retry_count = 0U,
    .metadata = {0}
};

/* ==========================================================================
 * Response helpers
 * ========================================================================== */

static void Bootloader_SendPacket(const BL_Protocol_Packet_t *packet)
{
    uint8_t buffer[BL_PROTOCOL_MAX_FRAME_SIZE];
    uint16_t length;

    if ((packet != NULL) &&
        BL_Protocol_PacketToBytes(packet, buffer, sizeof(buffer), &length))
    {
        BL_Transport_Write(buffer, length);
    }
}

static void Bootloader_SendACK(void)
{
    BL_Protocol_Packet_t packet;

    if (BL_Protocol_CreateCommandPacket(&packet, BL_CMD_ACK))
    {
        Bootloader_SendPacket(&packet);
    }

    bl_context.retry_count = 0U;
}

static void Bootloader_SendNACK(BL_ErrorCode_t error)
{
    BL_Protocol_Packet_t packet;
    uint8_t error_data = (uint8_t)error;

    if (BL_Protocol_CreateCommandDataPacket(
            &packet, BL_CMD_NACK, &error_data, sizeof(error_data)))
    {
        Bootloader_SendPacket(&packet);
    }
}

static void Bootloader_SendRETX(void)
{
    BL_Protocol_Packet_t packet;

    if (BL_Protocol_CreateCommandPacket(&packet, BL_CMD_RETX))
    {
        Bootloader_SendPacket(&packet);
    }
}

static void Bootloader_SendCommandPacket(BL_Command_t command)
{
    BL_Protocol_Packet_t packet;

    if (!BL_Protocol_CreateCommandPacket(&packet, command))
    {
        Bootloader_SendNACK(BL_ERROR_PROTOCOL);
        return;
    }

    Bootloader_SendPacket(&packet);
}

static void Bootloader_SendDeviceID(void)
{
    BL_Protocol_Packet_t packet;
    const volatile uint8_t *uid =
        (const volatile uint8_t *)BL_DEVICE_UID_BASE;

    if (!BL_Protocol_CreateCommandDataPacket(
            &packet,
            BL_CMD_DEVICE_ID_RES,
            (const uint8_t *)uid,
            BL_DEVICE_ID_SIZE))
    {
        Bootloader_SendNACK(BL_ERROR_PROTOCOL);
        return;
    }

    Bootloader_SendPacket(&packet);
}

/* ==========================================================================
 * Retry helpers
 * ========================================================================== */

static void Bootloader_SaveRetryContext(
    const BL_Protocol_Packet_t *packet,
    BL_State_t state_before)
{
    if (packet == NULL)
    {
        return;
    }

    bl_context.retry.last_packet = *packet;
    bl_context.retry.last_state = state_before;
    bl_context.retry.last_packet_valid = true;
}

static bool Bootloader_SendNACKAndCheckRetry(BL_ErrorCode_t error)
{
    Bootloader_SendNACK(error);
    bl_context.retry_count++;

    if (bl_context.retry_count >= BL_MAX_RETRY_COUNT)
    {
        bl_context.retry_count = 0U;
        bl_context.retry.last_packet_valid = false;
        bl_context.state = BL_STATE_WAIT_SYNC;
        return false;
    }

    return true;
}

/* ==========================================================================
 * Data / flash helpers
 * ========================================================================== */

static bool Bootloader_ExtractU32(
    const BL_Protocol_Packet_t *packet,
    uint32_t *value)
{
    uint8_t data[4];
    uint8_t length;

    if ((packet == NULL) || (value == NULL) ||
        !BL_Protocol_ExtractData(packet, data, &length) ||
        (length != sizeof(data)))
    {
        return false;
    }

    *value = ((uint32_t)data[0]) |
             ((uint32_t)data[1] << 8U) |
             ((uint32_t)data[2] << 16U) |
             ((uint32_t)data[3] << 24U);

    return true;
}

static bool Bootloader_WriteFirmwareData(
    uint32_t address,
    const uint8_t *data,
    uint8_t length)
{
    uint8_t buffer[BL_PROTOCOL_MAX_DATA_SIZE];

    if ((data == NULL) ||
        (length == 0U) ||
        (length > BL_PROTOCOL_MAX_DATA_SIZE))
    {
        return false;
    }

    for (uint8_t i = 0U; i < length; i++)
    {
        buffer[i] = data[i];
    }

    for (uint8_t i = length; i < BL_PROTOCOL_MAX_DATA_SIZE; i++)
    {
        buffer[i] = 0xFFU;
    }

    return BL_Flash_Write(address, buffer, sizeof(buffer)) &&
           BL_Flash_Verify(address, buffer, sizeof(buffer));
}

/* ==========================================================================
 * Command handlers: connection
 * ========================================================================== */

static void Bootloader_HandleSync(const BL_Protocol_Packet_t *packet)
{
    if (bl_context.state != BL_STATE_WAIT_SYNC)
    {
        Bootloader_SendNACK(BL_ERROR_STATE);
        bl_context.state = BL_STATE_WAIT_SYNC;
        return;
    }

    Bootloader_SaveRetryContext(packet, bl_context.state);
    bl_context.state = BL_STATE_WAIT_DEVICE_ID_REQ;
    Bootloader_SendACK();
}

static void Bootloader_HandleDeviceIDRequest(const BL_Protocol_Packet_t *packet)
{
    if (bl_context.state != BL_STATE_WAIT_DEVICE_ID_REQ)
    {
        Bootloader_SendNACK(BL_ERROR_STATE);
        return;
    }

    Bootloader_SaveRetryContext(packet, bl_context.state);
    Bootloader_SendDeviceID();
    bl_context.state = BL_STATE_WAIT_DEVICE_ID_CONFIRM;
}

static void Bootloader_HandleDeviceIDConfirm(void)
{
    if (bl_context.state != BL_STATE_WAIT_DEVICE_ID_CONFIRM)
    {
        Bootloader_SendNACK(BL_ERROR_STATE);
        return;
    }

    Bootloader_SendACK();
    bl_context.state = BL_STATE_CONNECTED;
}

/* ==========================================================================
 * Command handlers: firmware setup and erase
 * ========================================================================== */

static void Bootloader_HandleFirmwareSize(
    const BL_Protocol_Packet_t *packet)
{
    uint32_t firmware_size;

    if (bl_context.state != BL_STATE_CONNECTED)
    {
        Bootloader_SendNACK(BL_ERROR_STATE);
        bl_context.state = BL_STATE_WAIT_SYNC;
        return;
    }

    if (!Bootloader_ExtractU32(packet, &firmware_size))
    {
        Bootloader_SendNACKAndCheckRetry(BL_ERROR_LENGTH);
        return;
    }

    if ((firmware_size == 0U) || (firmware_size > BL_APP_SIZE))
    {
        Bootloader_SendNACK(BL_ERROR_SIZE);
        bl_context.state = BL_STATE_WAIT_SYNC;
        return;
    }

    bl_context.firmware_info.size = firmware_size;
    bl_context.firmware_info.received_size = 0U;

    Bootloader_SaveRetryContext(packet, bl_context.state);
    Bootloader_SendACK();
    bl_context.state = BL_STATE_WAIT_FW_START_ADDRESS;
}

static bool Bootloader_EraseApplication(void)
{
    if (!BL_Flash_Erase_MetaData(BL_METADATA_START_ADDRESS, BL_METADATA_SIZE) ||
        !BL_Flash_IsErased_MetaData(BL_METADATA_START_ADDRESS, BL_METADATA_SIZE))
    {
        return false;
    }

    if (!BL_Flash_Erase(BL_APP_START_ADDRESS, BL_APP_SIZE) ||
        !BL_Flash_IsErased(BL_APP_START_ADDRESS, BL_APP_SIZE))
    {
        return false;
    }

    return true;
}

static void Bootloader_HandleSetApplicationAddress(
    const BL_Protocol_Packet_t *packet)
{
    uint32_t start_address;

    if (bl_context.state != BL_STATE_WAIT_FW_START_ADDRESS)
    {
        Bootloader_SendNACK(BL_ERROR_STATE);
        bl_context.state = BL_STATE_WAIT_SYNC;
        return;
    }

    if (!Bootloader_ExtractU32(packet, &start_address))
    {
        Bootloader_SendNACKAndCheckRetry(BL_ERROR_LENGTH);
        return;
    }

    if ((start_address < BL_APP_START_ADDRESS) ||
        (start_address >= BL_APP_END_ADDRESS))
    {
        Bootloader_SendNACK(BL_ERROR_ADDRESS);
        bl_context.state = BL_STATE_WAIT_SYNC;
        return;
    }

    if (bl_context.firmware_info.size >
        (BL_APP_END_ADDRESS - start_address))
    {
        Bootloader_SendNACK(BL_ERROR_SIZE);
        bl_context.state = BL_STATE_WAIT_SYNC;
        return;
    }

    bl_context.firmware_info.start_address = start_address;
    bl_context.firmware_info.offset = start_address - BL_APP_START_ADDRESS;
    bl_context.firmware_info.write_address = start_address;

    if (!Bootloader_EraseApplication())
    {
        Bootloader_SendNACKAndCheckRetry(BL_ERROR_FLASH_ERASE);
        return;
    }

    Bootloader_SaveRetryContext(packet, bl_context.state);
    Bootloader_SendACK();
    bl_context.state = BL_STATE_PROGRAMMING;
}

static void Bootloader_HandleEraseFirmware(void)
{
    if (bl_context.state != BL_STATE_CONNECTED)
    {
        Bootloader_SendNACK(BL_ERROR_STATE);
        return;
    }

    if (!Bootloader_EraseApplication())
    {
        Bootloader_SendNACKAndCheckRetry(BL_ERROR_FLASH_ERASE);
        return;
    }

    Bootloader_SendACK();
    bl_context.state = BL_STATE_CONNECTED;
}

/* ==========================================================================
 * Command handlers: firmware data and validation
 * ========================================================================== */

static void Bootloader_HandleFirmwareData(
    const BL_Protocol_Packet_t *packet)
{
    uint8_t data[BL_PROTOCOL_MAX_DATA_SIZE];
    uint8_t data_length = 0U;
    uint32_t remaining;

    if (bl_context.state != BL_STATE_PROGRAMMING)
    {
        Bootloader_SendNACK(BL_ERROR_STATE);
        bl_context.state = BL_STATE_WAIT_SYNC;
        return;
    }

    if (!BL_Protocol_ExtractData(packet, data, &data_length))
    {
        Bootloader_SendNACKAndCheckRetry(BL_ERROR_PROTOCOL);
        return;
    }

    if ((data_length == 0U) ||
        (bl_context.firmware_info.received_size >
         bl_context.firmware_info.size))
    {
        Bootloader_SendNACKAndCheckRetry(BL_ERROR_DATA);
        return;
    }

    remaining = bl_context.firmware_info.size -
                bl_context.firmware_info.received_size;

    if ((uint32_t)data_length > remaining)
    {
        Bootloader_SendNACKAndCheckRetry(BL_ERROR_DATA);
        return;
    }

    if (!Bootloader_WriteFirmwareData(
            bl_context.firmware_info.write_address,
            data,
            data_length))
    {
        Bootloader_SendNACKAndCheckRetry(BL_ERROR_DATA);
        return;
    }

    bl_context.firmware_info.received_size += data_length;
    bl_context.firmware_info.write_address += BL_PROTOCOL_MAX_DATA_SIZE;

    if (bl_context.firmware_info.received_size ==
        bl_context.firmware_info.size)
    {
        bl_context.state = BL_STATE_COMPLETE;
        Bootloader_SendCommandPacket(BL_CMD_UPDATE_SUCCESSFUL);
    }
    else
    {
        Bootloader_SendACK();
    }
}

static void Bootloader_HandleCRCCheck(
    const BL_Protocol_Packet_t *packet)
{
    uint8_t data[BL_PROTOCOL_MAX_DATA_SIZE];
    uint8_t data_length = 0U;
    uint16_t expected_crc;
    uint16_t calculated_crc;

    if (bl_context.state != BL_STATE_COMPLETE)
    {
        Bootloader_SendNACK(BL_ERROR_STATE);
        bl_context.state = BL_STATE_WAIT_SYNC;
        return;
    }

    if (!BL_Protocol_ExtractData(packet, data, &data_length))
    {
        Bootloader_SendNACKAndCheckRetry(BL_ERROR_PROTOCOL);
        return;
    }

    if (data_length != 2U)
    {
        Bootloader_SendNACKAndCheckRetry(BL_ERROR_DATA);
        return;
    }

    expected_crc = ((uint16_t)data[0] << 8U) | (uint16_t)data[1];

    calculated_crc = BL_Flash_CalculateCRC(
        bl_context.firmware_info.start_address,
        bl_context.firmware_info.size);

    if (calculated_crc != expected_crc)
    {
        Bootloader_SendNACKAndCheckRetry(BL_ERROR_CRC);
        return;
    }

    bl_context.firmware_info.crc = calculated_crc;

    bl_context.metadata.magic = BL_FIRMWARE_METADATA_MAGIC;
    bl_context.metadata.start_address = bl_context.firmware_info.start_address;
    bl_context.metadata.size = bl_context.firmware_info.size;
    bl_context.metadata.crc = calculated_crc;
    bl_context.metadata.update_status = BL_UPDATE_STATUS_VALID;

    if (!BL_Metadata_Save(&bl_context.metadata))
    {
        Bootloader_SendNACKAndCheckRetry(BL_ERROR_DATA);
        return;
    }

    Bootloader_SendACK();
    bl_context.state = BL_STATE_VALID;
}

/* ==========================================================================
 * Command dispatcher
 * ========================================================================== */

static void Bootloader_ProcessPacket(const BL_Protocol_Packet_t *packet)
{
    BL_Command_t command;

    if ((packet == NULL) ||
        !BL_Protocol_ExtractCommand(packet, &command))
    {
        Bootloader_SendNACKAndCheckRetry(BL_ERROR_COMMAND);
        return;
    }

    switch (command)
    {
        case BL_CMD_SYNC_OBSERVED:
            Bootloader_HandleSync(packet);
            break;

        case BL_CMD_DEVICE_ID_REQ:
            Bootloader_HandleDeviceIDRequest(packet);
            break;

        case BL_CMD_DEVICE_ID_CONFIRM:
            Bootloader_HandleDeviceIDConfirm();
            break;

        case BL_CMD_FW_SIZE:
            Bootloader_HandleFirmwareSize(packet);
            break;

        case BL_CMD_SET_APP_START_ADDRESS:
            Bootloader_HandleSetApplicationAddress(packet);
            break;

        case BL_CMD_FW_DATA:
            Bootloader_HandleFirmwareData(packet);
            break;

        case BL_CMD_CRC_CHECK:
            Bootloader_HandleCRCCheck(packet);
            break;

        case BL_CMD_ERASE_FIRMWARE:
            Bootloader_HandleEraseFirmware();
            break;

        case BL_CMD_RETX:
        {
            BL_Protocol_Packet_t retry_packet;

            if (!bl_context.retry.last_packet_valid)
            {
                Bootloader_SendNACK(BL_ERROR_NO_RETRY_PACKET);
                bl_context.state = BL_STATE_WAIT_SYNC;
                break;
            }

            bl_context.state = bl_context.retry.last_state;
            retry_packet = bl_context.retry.last_packet;
            Bootloader_ProcessPacket(&retry_packet);
            break;
        }

        default:
            Bootloader_SendNACKAndCheckRetry(BL_ERROR_COMMAND);
            break;
    }
}

/* ==========================================================================
 * Public API
 * ========================================================================== */

void Bootloader_Init(void)
{
    BL_Protocol_Parser_Init(&bl_context.parser);
    BL_Flash_Init();
    BL_Transport_Init();
}

BL_State_t Bootloader_GetState(void)
{
    return bl_context.state;
}

void Bootloader_Process(void)
{
    uint8_t byte;

    while (BL_Transport_DataAvailable() > 0U)
    {
        if (!BL_Transport_ReadByte(&byte))
        {
            continue;
        }

        switch (BL_Protocol_Parser_PushByte(&bl_context.parser, byte))
        {
            case BL_PROTOCOL_PARSE_PACKET_READY:
                Bootloader_ProcessPacket(&bl_context.parser.packet);
                break;

            case BL_PROTOCOL_PARSE_CRC_ERROR:
                Bootloader_SendNACKAndCheckRetry(BL_ERROR_CRC);
                break;

            case BL_PROTOCOL_PARSE_ERROR:
                Bootloader_SendNACKAndCheckRetry(BL_ERROR_PROTOCOL);
                break;

            case BL_PROTOCOL_PARSE_IN_PROGRESS:
            default:
                break;
        }
    }
}

bool Bootloader_ValidateApplication(void)
{
    uint16_t calculated_crc;

    if (!BL_Metadata_Read(&bl_context.metadata) ||
        !BL_Metadata_IsValid(&bl_context.metadata) ||
        (bl_context.metadata.update_status != BL_UPDATE_STATUS_VALID))
    {
        return false;
    }

    calculated_crc = BL_Flash_CalculateCRC(
        bl_context.metadata.start_address,
        bl_context.metadata.size);

    return (calculated_crc == bl_context.metadata.crc);
}

void Bootloader_GetBackTo_Idle(void)
{
    bl_context.state = BL_STATE_WAIT_SYNC;
    bl_context.retry_count = 0U;
    bl_context.retry.last_packet_valid = false;

    bl_context.firmware_info.size = 0U;
    bl_context.firmware_info.received_size = 0U;
    bl_context.firmware_info.start_address = 0U;
    bl_context.firmware_info.write_address = 0U;
    bl_context.firmware_info.offset = 0U;
    bl_context.firmware_info.crc = 0U;
}
