#include "board.h"
#include "LoRaMac.h"
#include "Comissioning.h"

#define APP_DEFAULT_DATARATE                DR_5    // SF7 - BW125
#define APP_ADR_ON                          1       // Whether we use Adaptive Data Rate or not
#define APP_CONFIRMED_MSG_ON                0       // Whether this example will transmit confirmed or unconfirmed packets

#define HDLC_FLAG           0x7E
#define HDLC_ESCAPE         0x7D
#define HDLC_ESCAPE_MASK    0x20
#define CRC_INIT            0xffff

#if (OVER_THE_AIR_ACTIVATION == 0)
    static uint8_t  NwkSKey[] = LORAWAN_NWKSKEY;
    static uint8_t  AppSKey[] = LORAWAN_APPSKEY;
    static uint32_t DevAddr   = LORAWAN_DEVICE_ADDRESS;
#endif

static uint8_t DevEui[] = LORAWAN_DEVICE_EUI;
static uint8_t AppEui[] = LORAWAN_APPLICATION_EUI;
static uint8_t AppKey[] = LORAWAN_APPLICATION_KEY;

static bool joined = false;	// We have to join the network of the gateway before we
static bool busy = false; 	// We ignore UART messages while we are still handling the last one

static uint16_t uartBufferIndex = 0;
static uint8_t uartBuffer[1+(2*(1+1+255+2))+1]; // start + [type + data length + data + CRC](encoded) + end

static uint16_t uartMessageLength;
static uint8_t uartMessage[1+1+255]; // type + data length + data

static const uint16_t crc16_table[256] = {
    0x0000, 0x1189, 0x2312, 0x329B, 0x4624, 0x57AD, 0x6536, 0x74BF,
    0x8C48, 0x9DC1, 0xAF5A, 0xBED3, 0xCA6C, 0xDBE5, 0xE97E, 0xF8F7,
    0x1081, 0x0108, 0x3393, 0x221A, 0x56A5, 0x472C, 0x75B7, 0x643E,
    0x9CC9, 0x8D40, 0xBFDB, 0xAE52, 0xDAED, 0xCB64, 0xF9FF, 0xE876,
    0x2102, 0x308B, 0x0210, 0x1399, 0x6726, 0x76AF, 0x4434, 0x55BD,
    0xAD4A, 0xBCC3, 0x8E58, 0x9FD1, 0xEB6E, 0xFAE7, 0xC87C, 0xD9F5,
    0x3183, 0x200A, 0x1291, 0x0318, 0x77A7, 0x662E, 0x54B5, 0x453C,
    0xBDCB, 0xAC42, 0x9ED9, 0x8F50, 0xFBEF, 0xEA66, 0xD8FD, 0xC974,
    0x4204, 0x538D, 0x6116, 0x709F, 0x0420, 0x15A9, 0x2732, 0x36BB,
    0xCE4C, 0xDFC5, 0xED5E, 0xFCD7, 0x8868, 0x99E1, 0xAB7A, 0xBAF3,
    0x5285, 0x430C, 0x7197, 0x601E, 0x14A1, 0x0528, 0x37B3, 0x263A,
    0xDECD, 0xCF44, 0xFDDF, 0xEC56, 0x98E9, 0x8960, 0xBBFB, 0xAA72,
    0x6306, 0x728F, 0x4014, 0x519D, 0x2522, 0x34AB, 0x0630, 0x17B9,
    0xEF4E, 0xFEC7, 0xCC5C, 0xDDD5, 0xA96A, 0xB8E3, 0x8A78, 0x9BF1,
    0x7387, 0x620E, 0x5095, 0x411C, 0x35A3, 0x242A, 0x16B1, 0x0738,
    0xFFCF, 0xEE46, 0xDCDD, 0xCD54, 0xB9EB, 0xA862, 0x9AF9, 0x8B70,
    0x8408, 0x9581, 0xA71A, 0xB693, 0xC22C, 0xD3A5, 0xE13E, 0xF0B7,
    0x0840, 0x19C9, 0x2B52, 0x3ADB, 0x4E64, 0x5FED, 0x6D76, 0x7CFF,
    0x9489, 0x8500, 0xB79B, 0xA612, 0xD2AD, 0xC324, 0xF1BF, 0xE036,
    0x18C1, 0x0948, 0x3BD3, 0x2A5A, 0x5EE5, 0x4F6C, 0x7DF7, 0x6C7E,
    0xA50A, 0xB483, 0x8618, 0x9791, 0xE32E, 0xF2A7, 0xC03C, 0xD1B5,
    0x2942, 0x38CB, 0x0A50, 0x1BD9, 0x6F66, 0x7EEF, 0x4C74, 0x5DFD,
    0xB58B, 0xA402, 0x9699, 0x8710, 0xF3AF, 0xE226, 0xD0BD, 0xC134,
    0x39C3, 0x284A, 0x1AD1, 0x0B58, 0x7FE7, 0x6E6E, 0x5CF5, 0x4D7C,
    0xC60C, 0xD785, 0xE51E, 0xF497, 0x8028, 0x91A1, 0xA33A, 0xB2B3,
    0x4A44, 0x5BCD, 0x6956, 0x78DF, 0x0C60, 0x1DE9, 0x2F72, 0x3EFB,
    0xD68D, 0xC704, 0xF59F, 0xE416, 0x90A9, 0x8120, 0xB3BB, 0xA232,
    0x5AC5, 0x4B4C, 0x79D7, 0x685E, 0x1CE1, 0x0D68, 0x3FF3, 0x2E7A,
    0xE70E, 0xF687, 0xC41C, 0xD595, 0xA12A, 0xB0A3, 0x8238, 0x93B1,
    0x6B46, 0x7ACF, 0x4854, 0x59DD, 0x2D62, 0x3CEB, 0x0E70, 0x1FF9,
    0xF78F, 0xE606, 0xD49D, 0xC514, 0xB1AB, 0xA022, 0x92B9, 0x8330,
    0x7BC7, 0x6A4E, 0x58D5, 0x495C, 0x3DE3, 0x2C6A, 0x1EF1, 0x0F78
};

enum DataType
{
	PacketForGateway = 1,
	PacketForEndDevice = 2
};


uint16_t CrcCalculationStep(uint8_t byte, uint16_t crc)
{
	return crc16_table[byte ^ (uint8_t)(crc >> 8)] ^ (crc << 8);
}

void HdlcEncodeByte(uint8_t byte)
{
	if (byte == HDLC_FLAG || byte == HDLC_ESCAPE)
	{
		uartBuffer[uartBufferIndex++] = HDLC_ESCAPE;
		uartBuffer[uartBufferIndex++] = byte ^ HDLC_ESCAPE_MASK;
	}
	else
		uartBuffer[uartBufferIndex++] = byte;
}

void HdlcEncode(void)
{
	uartBuffer[0] = HDLC_FLAG;
	uartBufferIndex = 1;

	uint16_t crc = CRC_INIT;
	for (uint8_t i = 0; i < uartMessageLength; ++i)
	{
		uint8_t byte = uartMessage[i];

		HdlcEncodeByte(byte);
		crc = CrcCalculationStep(byte, crc);
	}

	// Escape the CRC bytes
	HdlcEncodeByte((crc >> 8) & 0xFF);
	HdlcEncodeByte(crc & 0xFF);

	// Add the ending HDLC_FLAG byte
	uartBuffer[uartBufferIndex++] = HDLC_FLAG;
}

bool HdlcDecode(void)
{
	uint16_t crc = CRC_INIT;
	uartMessageLength = 0;
	bool escaping = false;
	for (uint16_t i = 1; i < uartBufferIndex-2; ++i)
	{
		uint8_t byte = uartBuffer[i];

		if (escaping)
		{
			byte = byte ^ HDLC_ESCAPE_MASK;
			escaping = false;
		}
		else if (byte == HDLC_ESCAPE)
		{
			escaping = true;
			continue;
		}

		uartMessage[uartMessageLength++] = byte;
		crc = CrcCalculationStep(byte, crc);
	}

	// Add the crc bytes to the crc calculation so that the result becomes 0 when the crc was correct
	crc = CrcCalculationStep(uartBuffer[uartBufferIndex-2], crc);
	crc = CrcCalculationStep(uartBuffer[uartBufferIndex-1], crc);

	// Check if the packet contents is valid
	if (!escaping && (uartMessage[1]+2 == uartMessageLength) && (crc == 0))
	{
		if (uartMessage[0] == PacketForGateway)
			return true;
	}

	return false;
}

static bool SendFrame(void)
{
    McpsReq_t mcpsReq;
    LoRaMacTxInfo_t txInfo;

    if (LoRaMacQueryTxPossible(uartMessageLength-2, &txInfo) != LORAMAC_STATUS_OK)
    {
        // Send empty frame in order to flush MAC commands
        mcpsReq.Type = MCPS_UNCONFIRMED;
        mcpsReq.Req.Unconfirmed.fBuffer = NULL;
        mcpsReq.Req.Unconfirmed.fBufferSize = 0;
        mcpsReq.Req.Unconfirmed.Datarate = APP_DEFAULT_DATARATE;
    }
    else // Send the data
    {
	LoRaMacSetSrvAckRequested(uartMessage[4]);

        if (uartMessage[3])
        {
            // Send a confirmed packet
            mcpsReq.Type = MCPS_CONFIRMED;
            mcpsReq.Req.Confirmed.fPort = uartMessage[2];
            mcpsReq.Req.Confirmed.fBuffer = &uartMessage[5];
            mcpsReq.Req.Confirmed.fBufferSize = uartMessageLength-5;
            mcpsReq.Req.Confirmed.NbTrials = 1; // No retransmissions
            mcpsReq.Req.Confirmed.Datarate = APP_DEFAULT_DATARATE;
        }
        else
        {
            // Send an unconfirmed packet
            mcpsReq.Type = MCPS_UNCONFIRMED;
            mcpsReq.Req.Unconfirmed.fPort = uartMessage[2];
            mcpsReq.Req.Unconfirmed.fBuffer = &uartMessage[5];
            mcpsReq.Req.Unconfirmed.fBufferSize = uartMessageLength-5;
            mcpsReq.Req.Unconfirmed.Datarate = APP_DEFAULT_DATARATE;
        }
    }

    if (LoRaMacMcpsRequest(&mcpsReq) == LORAMAC_STATUS_OK)
        return false;

    return true;
}

static void McpsConfirm(McpsConfirm_t *mcpsConfirm)
{
}

static void McpsIndication(McpsIndication_t *mcpsIndication)
{
    if (mcpsIndication->Status == LORAMAC_EVENT_INFO_STATUS_OK)
    {
    	// Packet received, send the data over the UART
    	if (mcpsIndication->RxData || mcpsIndication->AckReceived)
    	{
    		// uartMessage contents:
			// 0: DataType (PacketForEndDevice)
			// 1: Length
			// 2: FPort
			// 3: Reply with confirmed packet?
			// 4: Received ACK from gateway?
    		// 5: FramePending
			// 6+: Data for end-device

    		uartMessageLength = mcpsIndication->BufferSize + 6;
    		uartMessage[0] = PacketForEndDevice;
    		uartMessage[1] = mcpsIndication->BufferSize + 4;
    		uartMessage[2] = mcpsIndication->Port;
    		uartMessage[3] = (mcpsIndication->McpsIndication == MCPS_CONFIRMED);
    		uartMessage[4] = mcpsIndication->AckReceived;
    		uartMessage[5] = mcpsIndication->FramePending;
    		for (uint8_t i = 0; i < mcpsIndication->BufferSize; ++i)
    			uartMessage[i+6] = mcpsIndication->Buffer[i];

			HdlcEncode();
			UartPutBuffer(&Uart1, uartBuffer, uartBufferIndex);
    	}
    }
}

static void MlmeConfirm(MlmeConfirm_t *mlmeConfirm)
{
    if (mlmeConfirm->Status == LORAMAC_EVENT_INFO_STATUS_OK)
    {
        if (mlmeConfirm->MlmeRequest == MLME_JOIN)
            joined = true;
    }
}

//static void MlmeIndication(MlmeIndication_t *mlmeIndication)
//{
//}

void JoinNetwork(void)
{
#if (OVER_THE_AIR_ACTIVATION != 0)
	MlmeReq_t mlmeReq;

	// Initialize LoRaMac device unique ID
	BoardGetUniqueId(DevEui);

	mlmeReq.Type = MLME_JOIN;
	mlmeReq.Req.Join.DevEui = DevEui;
	mlmeReq.Req.Join.AppEui = AppEui;
	mlmeReq.Req.Join.AppKey = AppKey;
	LoRaMacMlmeRequest(&mlmeReq);
#else
	// Choose a random device address if not already defined in Comissioning.h
	if (DevAddr == 0)
	{
		srand1(BoardGetRandomSeed());
		DevAddr = randr(0, 0x01FFFFFF);
	}

	mibReq.Type = MIB_NET_ID;
	mibReq.Param.NetID = LORAWAN_NETWORK_ID;
	LoRaMacMibSetRequestConfirm(&mibReq);

	mibReq.Type = MIB_DEV_ADDR;
	mibReq.Param.DevAddr = DevAddr;
	LoRaMacMibSetRequestConfirm(&mibReq);

	mibReq.Type = MIB_NWK_SKEY;
	mibReq.Param.NwkSKey = NwkSKey;
	LoRaMacMibSetRequestConfirm(&mibReq);

	mibReq.Type = MIB_APP_SKEY;
	mibReq.Param.AppSKey = AppSKey;
	LoRaMacMibSetRequestConfirm(&mibReq);

	mibReq.Type = MIB_NETWORK_JOINED;
	mibReq.Param.IsNetworkJoined = true;
	LoRaMacMibSetRequestConfirm(&mibReq);

	joined = true;
#endif
}

void UartIrqNotify(UartNotifyId_t id)
{
	uint8_t byte;
	if ((id == UART_NOTIFY_RX) && (UartGetChar(&Uart1, &byte) == 0) && !busy)
	{
		uartBuffer[uartBufferIndex] = byte;

		// When at the beginning of the buffer, reject every byte except the HDLC_FLAG which indicates the start of the packet
		if (uartBufferIndex == 0)
		{
			if (byte != HDLC_FLAG)
				return;
		}
		else // This is not the first byte
		{
			// Check if we received the end of the packet byte
			if (byte == HDLC_FLAG)
			{
				// If the packet is empty then assume we are out of sync and this was the opening byte
				if (uartBufferIndex == 1)
					return;

				// The packet is at least 6 bytes long (start + type + length + CRC + end)
				if (uartBufferIndex < 5)
				{
					// Discard the frame if it is too small
					uartBufferIndex = 0;
					return;
				}

				// Decode the packet
				if (HdlcDecode())
				{
					// uartMessage contents:
					// 0: DataType (PacketForGateway)
					// 1: Length
					// 2: FPort
					// 3: Send a confirmed packet?
					// 4: Send an uplink ACK?
					// 5+: Data for gateway

					// Transmit the packet to the gateway
					SendFrame();
				}

				uartBufferIndex = 0;
			}
		}

		uartBufferIndex++;
		if (uartBufferIndex == sizeof(uartBuffer))
		{
			// Buffer is full, ignore bytes until we are back like we expect it
			uartBufferIndex = 0;
		}
	}
}

int main(void)
{
    LoRaMacPrimitives_t LoRaMacPrimitives;
    LoRaMacCallback_t LoRaMacCallbacks;
    MibRequestConfirm_t mibReq;

    BoardInitMcu();
    BoardInitPeriph();

    LoRaMacPrimitives.MacMcpsConfirm = McpsConfirm;
	LoRaMacPrimitives.MacMcpsIndication = McpsIndication;
	LoRaMacPrimitives.MacMlmeConfirm = MlmeConfirm;
	//LoRaMacPrimitives.MacMlmeIndication = MlmeIndication;
	//LoRaMacCallbacks.GetBatteryLevel = BoardGetBatteryLevel;
	//LoRaMacCallbacks.GetTemperatureLevel = NULL;
	LoRaMacInitialization(&LoRaMacPrimitives, &LoRaMacCallbacks);

	// Enable or disable Adaptive Data Rate
	mibReq.Type = MIB_ADR;
	mibReq.Param.AdrEnable = APP_ADR_ON;
	LoRaMacMibSetRequestConfirm(&mibReq);

	// Set whether this is a public or private network
	mibReq.Type = MIB_PUBLIC_NETWORK;
	mibReq.Param.EnablePublicNetwork = LORAWAN_PUBLIC_NETWORK;
	LoRaMacMibSetRequestConfirm( &mibReq );

	// Add some channels other than the default 3 channels
	// Channel = { Frequency [Hz], { ( ( DrMax << 4 ) | DrMin ) }, Band }
	LoRaMacChannelAdd(3, (ChannelParams_t){867100000, {((DR_5 << 4) | DR_0)}, 0});
	LoRaMacChannelAdd(4, (ChannelParams_t){867300000, {((DR_5 << 4) | DR_0)}, 0});
	LoRaMacChannelAdd(5, (ChannelParams_t){867500000, {((DR_5 << 4) | DR_0)}, 0});

	// Set which channel to use for RX2
	mibReq.Type = MIB_RX2_CHANNEL;
	mibReq.Param.Rx2Channel = (Rx2ChannelParams_t){869525000, DR_3};
	LoRaMacMibSetRequestConfirm(&mibReq);

	// Join the network of the gateway
	JoinNetwork();
	while (!joined)
		RtcEnterLowPowerStopMode();

	// Start receiving data through the UART
	Uart1.IrqNotify = UartIrqNotify;
	while (true)
		RtcEnterLowPowerStopMode();
}
