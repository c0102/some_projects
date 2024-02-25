#include "main.h"

#ifdef ATECC608A_CRYPTO_ENABLED
//#if 1 //#ifdef ATECC608A_CRYPTO

static int atecc608_test_assert_config_is_locked(void);
static int atecc608_test_assert_data_is_locked(void);

static const char *TAG = "ATECC608A";
ATCAIfaceCfg cfg = {
		.iface_type             = ATCA_I2C_IFACE,
		.devtype                = ATECC608A,
		.atcai2c.slave_address  = 0XC0,
		.atcai2c.bus            = 0,
		.atcai2c.baud           = 400000,
		.wake_delay             = 1500,
		.rx_retries             = 20
};

/** \brief Print each hex character in the binary buffer
 *  \param[in] binary input buffer to print
 *  \param[in] bin_len length of buffer to print
 *  \param[in] add_space indicates whether spaces and returns should be added for pretty printing
 * \return ATCA_SUCCESS on success, otherwise an error code.
 */
ATCA_STATUS atcab_printbin(uint8_t* binary, size_t bin_len, bool add_space)
{
    size_t i = 0;
    size_t line_len = 16;

    // Verify the inputs
    if (binary == NULL)
    {
        return ATCA_BAD_PARAM;
    }

    // Set the line length
    line_len = add_space ? 16 : 32;

    // Print the bytes
    for (i = 0; i < bin_len; i++)
    {
        // Print the byte
        if (add_space)
        {
            printf("%02X ", binary[i]);
        }
        else
        {
            printf("%02X", binary[i]);
        }

        // Break at the line_len
        if ((i + 1) % line_len == 0)
        {
            printf("\r\n");
        }
    }
    // Print the last carriage return
    //printf("\r\n");

    return ATCA_SUCCESS;
}

int atecc608a_sleep()
{
  ATCA_STATUS status;

  status = atcab_sleep();
  if (status != ATCA_SUCCESS)
    return -1;
  else
    return 0;
}

void atecc608_convert_signature_to_hex_format(uint8_t *signature, char *signature_hex)
{
	char stmp[10];
	uint8_t add_first_line_zero_byte = 0;
	uint8_t add_second_line_zero_byte = 0;
	int i;

	// Verify the inputs
	if (signature == NULL || signature_hex == NULL)
	{
		MY_LOGE(TAG, "%s BAD INPUT(s)",__FUNCTION__);
		return;
	}

	if(signature[0] & 0x80)
		add_first_line_zero_byte = 1;

	if(signature[32] & 0x80)
		add_second_line_zero_byte = 1;

	sprintf(signature_hex, "30%2d", 44 + add_first_line_zero_byte + add_second_line_zero_byte); //add 00 if line starts with negative number (highest bit set)
	if(add_first_line_zero_byte)
		strcat(signature_hex, "022100");
	else
		strcat(signature_hex, "0220");

	for(i = 0; i < ATCA_SIG_SIZE/2; i++)
	{
		sprintf(stmp, "%02X", signature[i]);
		strcat(signature_hex, stmp);
	}

	if(add_second_line_zero_byte)
		strcat(signature_hex, "022100");
	else
		strcat(signature_hex, "0220");

	for(i = ATCA_SIG_SIZE/2; i < ATCA_SIG_SIZE; i++)
	{
		sprintf(stmp, "%02X", signature[i]);
		strcat(signature_hex, stmp);
	}
}

ATCA_STATUS atecc608a_get_public_key(int private_key_id, uint8_t *pubkey)
{
	ATCA_STATUS status;

	// Verify the inputs
	if (pubkey == NULL || private_key_id < 0 || private_key_id > 7)
	{
		MY_LOGE(TAG, "%s BAD INPUT(s)",__FUNCTION__);
		return ATCA_BAD_PARAM;
	}

	status = atcab_init(&cfg);
	if(status == ATCA_SUCCESS)
		MY_LOGI(TAG, "ATECC608A Init OK");
	else
	{
		MY_LOGE(TAG, "ATEC INIT FAILED: %d", status);
		atcab_release();
		return status;
	}

	status = atcab_get_pubkey(private_key_id, pubkey);
	if (status != ATCA_SUCCESS)
		MY_LOGE(TAG, "atcab_get_pubkey() failed with ret=0x%08X", status);

	atcab_release();

	return status;
}

ATCA_STATUS atecc608a_sign(const uint8_t *message, int message_len, uint8_t *signature)
{
	ATCA_STATUS status;
	uint16_t private_key_id = 0;
	uint8_t digest[ATCA_SHA_DIGEST_SIZE];

	// Verify the inputs
	if (message == NULL || signature == NULL || message_len > 1024)
	{
		MY_LOGE(TAG, "%s BAD INPUT(s)",__FUNCTION__);
		return ATCA_BAD_PARAM;
	}

	status = atcab_init(&cfg);
	if(status == ATCA_SUCCESS)
		MY_LOGI(TAG, "ATECC608A Init OK");
	else
	{
		MY_LOGE(TAG, "ATEC INIT FAILED: %d", status);
		atcab_release();
		return status;
	}

	if(atecc608_test_assert_data_is_locked() == 0)
	{
		atcab_release();
		return ATCA_DATA_ZONE_LOCKED;
	}

	//printf("SIGN MESSAGE\n\r%s\n\r", message);

	//generate sha
	status = atcab_sha(message_len, message, digest);
	if (status != ATCA_SUCCESS)
	{
		MY_LOGE(TAG, "atcab_sha() failed with ret=0x%08X", status);
		atcab_release();
		return status;
	}

	//sign sha
	status = atcab_sign(private_key_id, digest, signature);
	if (status == ATCA_SUCCESS)
	{
		//MY_LOGI(TAG, "atcab_sign OK. Signature:");
		//MY_LOG_BUFFER_HEX(TAG, signature, ATCA_SIG_SIZE);

		//convert signature to hex format
		//atecc608_convert_signature_to_hex_format(signature, signature_hex);
	}
	else
		MY_LOGE(TAG, "atcab_sign() failed with ret=0x%08X", status);

	atcab_release();

	return status;
}

ATCA_STATUS atecc608a_verify(uint8_t *message, int message_len, uint8_t *signature, bool *is_verified)
{
	uint8_t pubkey[ATCA_PUB_KEY_SIZE];
	uint8_t digest[ATCA_SHA_DIGEST_SIZE];
	uint8_t private_key_id = 0;
	ATCA_STATUS status;

	// Verify the inputs
	if (message == NULL || signature == NULL || message_len > 1024)
	{
		MY_LOGE(TAG, "%s BAD INPUT(s)",__FUNCTION__);
		return ATCA_BAD_PARAM;
	}

	status = atcab_init(&cfg);
	if(status == ATCA_SUCCESS)
		MY_LOGI(TAG, "ATECC608A Init OK");
	else
	{
		MY_LOGE(TAG, "ATEC INIT FAILED: %d", status);
		atcab_release();
		return status;
	}

	//generate sha
	status = atcab_sha(message_len, message, digest);
	if (status != ATCA_SUCCESS)
	{
		MY_LOGE(TAG, "atcab_sha() failed with ret=0x%08X", status);
		atcab_release();
		return status;
	}

	status = atcab_get_pubkey(private_key_id, pubkey);
	if (status != ATCA_SUCCESS)
	{
		MY_LOGE(TAG, "atcab_get_pubkey() failed with ret=0x%08X", status);
		atcab_release();
		return status;
	}

	status = atcab_verify_extern(digest, signature, pubkey, is_verified);
	if (status != ATCA_SUCCESS)
		MY_LOGE(TAG, "atcab_verify_extern() failed with ret=0x%08X", status);

	atcab_release();
	return status;
}

void atecc608a_sign_verify_test()
{
	uint8_t pubkey[ATCA_PUB_KEY_SIZE];
	uint8_t signature[ATCA_SIG_SIZE];
	char signature_hex[150];
	uint8_t message[] = "{\"FV\":\"1.1\",\"VI\":\"SEAL AG\",\"VV\":\"1.p12\",\"PG\":\"T8568\",\"MV\":\"Iskra\",\"MM\":\"EM340-DIN.AV2.3.X.S1.PF\",\"MS\":\"******240084S\",\"IS\":\"HEARSAY\",\"IF\":[],\"IT\":\"UNDEFINED\",\"ID\":\"nnn\",\"RD\":[{\"TM\":\"2019-01-30T13:41:58,922+0000 U\",\"TX\":\"C\",\"RV\":268.978,\"RI\":\"1-b:1.8.e\",\"RU\":\"kWh\",\"EI\":62,\"ST\":\"G\"}]}";
	uint8_t private_key_id = 0;
	bool is_verified = false;
	ATCA_STATUS status;

	status = atecc608a_get_public_key(private_key_id, pubkey);
	if (status == ATCA_SUCCESS)
	{
		MY_LOGI(TAG, "PUBLIC KEY FROM PRIVATE KEY AT SLOT %d:", private_key_id);
		MY_LOG_BUFFER_HEX(TAG, pubkey, ATCA_PUB_KEY_SIZE);

		//print public key in HEX format
		printf("PUBLIC KEY HEX:\n\r04"); //Public key in HEX must start with 04
		status = atcab_printbin(pubkey, ATCA_PUB_KEY_SIZE, false);
		if (status != ATCA_SUCCESS)
			MY_LOGE(TAG, "atcab_printbin() failed with ret=0x%08X", status);
	}
	else
	{
		MY_LOGE(TAG, "atecc608a_get_public_key Failed:%d", status);
		return;
	}

	status = atecc608a_sign(message, sizeof(message) - 1, signature);
	if(status == ATCA_SUCCESS)
	{
		printf("SIGNATURE BIN:\n\r");
		atcab_printbin(signature, ATCA_SIG_SIZE, false);

		atecc608_convert_signature_to_hex_format(signature, signature_hex);
		printf("SIGNATURE HEX:\n\r%s\n\r", signature_hex);
	}
	else
	{
		MY_LOGE(TAG, "atecc608a_sign Failed:%d", status);
		return;
	}

	status = atecc608a_verify(message, sizeof(message) - 1, signature, &is_verified);
	if (status == ATCA_SUCCESS)
	{
		if(is_verified == 0)
			MY_LOGE(TAG, "VERIFY FAILED");
		else
			MY_LOGI(TAG, "VERIFY OK!!!!!!!!!!!!!!!!!!!!!!!");
	}
	else
		MY_LOGE(TAG, "atecc608a_verify() failed with ret=0x%08X", status);
}

void atecc608a_sign_verify_test_full()
{
	ATCA_STATUS status;
	bool is_verified = false;
	//uint8_t message[ATCA_KEY_SIZE];
	uint8_t message[] = "{\"FV\":\"1.1\",\"VI\":\"SEAL AG\",\"VV\":\"1.p12\",\"PG\":\"T8568\",\"MV\":\"Iskra\",\"MM\":\"EM340-DIN.AV2.3.X.S1.PF\",\"MS\":\"******240084S\",\"IS\":\"HEARSAY\",\"IF\":[],\"IT\":\"UNDEFINED\",\"ID\":\"nnn\",\"RD\":[{\"TM\":\"2019-01-30T13:41:58,922+0000 U\",\"TX\":\"C\",\"RV\":268.978,\"RI\":\"1-b:1.8.e\",\"RU\":\"kWh\",\"EI\":62,\"ST\":\"G\"}]}";
	uint8_t public_key_curve_info[] = {0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07, 0x03, 0x42, 0x00, 0x04};
	uint8_t digest[ATCA_SHA_DIGEST_SIZE];
	uint8_t signature[ATCA_SIG_SIZE];
	uint8_t pubkey[ATCA_PUB_KEY_SIZE];
	uint8_t sernum[10];
	const uint16_t private_key_id = 0;
	uint8_t rightAnswer[] = { 0x2e, 0x8f, 0xcc, 0x42, 0x30, 0x99, 0x76, 0x3d,
				0x46, 0x5c, 0x6f, 0x05, 0xb3, 0xc2, 0x2e, 0x67,
				0x76, 0x64, 0x7a, 0x19, 0x43, 0x8d, 0x2d, 0x48,
				0x91, 0x3b, 0x17, 0xf4, 0x96, 0xb7, 0x4b, 0xf6 };

	status = atcab_init(&cfg);
	if(status == ATCA_SUCCESS)
		MY_LOGI(TAG, "ATECC608A Init OK");
	else
	{
		MY_LOGE(TAG, "ATEC INIT FAILED: %d", status);
		atcab_release();
		return;
	}

	uint8_t revision[4];
	status = atcab_info(revision);
	if (status != ATCA_SUCCESS)
		MY_LOGE(TAG, "atcab_info() failed with ret=0x%08X", status);
	else
	{
		MY_LOGI(TAG, "REVISION:");
		MY_LOG_BUFFER_HEX(TAG, revision, 4);
	}

	char version[10];
	status = atcab_version(version);
	if (status == ATCA_SUCCESS)
	{
		MY_LOGI(TAG, "VERSION: %s", version);
	}
	else
		MY_LOGE(TAG, "atcab_version() failed with ret=0x%08X", status);

	status = atcab_read_serial_number(sernum);
	if (status == ATCA_SUCCESS)
	{
		MY_LOGI(TAG, "SERIAL NUMBER:");
		MY_LOG_BUFFER_HEX(TAG, sernum, 9);
	}
	else
		MY_LOGE(TAG, "atcab_read_serial_number() failed with ret=0x%08X", status);

	if(atecc608_test_assert_data_is_locked() == 0 || atecc608_test_assert_config_is_locked() == 0)
	{
		atcab_release();
		return;
	}

#if 0//dont generate keypair
	status = atcab_genkey(private_key_id, pubkey);
	if (status == ATCA_SUCCESS)
	{
		MY_LOGI(TAG, "PUBLIC KEY:");
		MY_LOG_BUFFER_HEX(TAG, pubkey, ATCA_PUB_KEY_SIZE);
	}
	else
		MY_LOGE(TAG, "atcab_genkey() failed with ret=0x%08X", status);
#endif

	status = atcab_get_pubkey(private_key_id, pubkey);
	//status = atcab_get_pubkey(private_key_id, public_key_from_slot);
	if (status == ATCA_SUCCESS)
	{
		MY_LOGI(TAG, "PUBLIC KEY FROM PRIVATE KEY AT SLOT %d:", private_key_id);
		MY_LOG_BUFFER_HEX(TAG, pubkey, ATCA_PUB_KEY_SIZE);

		//print public key in HEX format
		printf("PUBLIC KEY HEX:\n\r04"); //Public key in HEX must start with 04
		status = atcab_printbin(pubkey, ATCA_PUB_KEY_SIZE, false);
		if (status != ATCA_SUCCESS)
			MY_LOGE(TAG, "atcab_printbin() failed with ret=0x%08X", status);

		//encode public key in BASE64 format
		uint8_t public_key_with_curve_info[ATCA_PUB_KEY_SIZE + 27];
		char encoded[150];
		memset(encoded, 0x00, 150);
		memcpy(public_key_with_curve_info, public_key_curve_info, sizeof(public_key_curve_info));
		memcpy(public_key_with_curve_info + sizeof(public_key_curve_info), pubkey, ATCA_PUB_KEY_SIZE);
		size_t encodedLen = sizeof(encoded);
		status = atcab_base64encode(public_key_with_curve_info, ATCA_PUB_KEY_SIZE + 27, encoded, &encodedLen);
		if (status == ATCA_SUCCESS)
		{
			printf("PUBLIC KEY BASE64:\n\r-----BEGIN PUBLIC KEY-----\n\r%s\n\r-----END PUBLIC KEY-----\n\r", encoded);
		}
		else
			MY_LOGE(TAG, "atcab_base64encode() failed with ret=0x%08X", status);
	}
	else
		MY_LOGE(TAG, "atcab_get_pubkey() failed with ret=0x%08X", status);

#if 0
	status = atcab_random(message);
	if (status == ATCA_SUCCESS)
		MY_LOGI(TAG, "atcab_random OK");
	else
		MY_LOGE(TAG, "atcab_random() failed with ret=0x%08X", status);
#endif

	printf("%s\n\r", message);
	status = atcab_sha(sizeof(message) - 1, message, digest);
	if (status == ATCA_SUCCESS)
	{
		int i;
		for(i = 0; i < ATCA_SHA_DIGEST_SIZE; i++)
		{
			if(rightAnswer[i] != digest[i])
				MY_LOGE(TAG, "SHA FAILED i:%d digest:%x rightAnswer:%x", i, digest[i], rightAnswer[i]);
		}
		MY_LOGI(TAG, "SHA TEST FINISHED size:%d SHA:", sizeof(message) - 1);
		MY_LOG_BUFFER_HEX(TAG, digest, ATCA_SHA_DIGEST_SIZE);
	}

	status = atcab_sign(private_key_id, digest, signature);
	if (status == ATCA_SUCCESS)
	{
		uint8_t add_first_line_zero_byte = 0;
		uint8_t add_second_line_zero_byte = 0;

		MY_LOGI(TAG, "atcab_sign OK. Signature:");
		MY_LOG_BUFFER_HEX(TAG, signature, ATCA_SIG_SIZE);

		if(signature[0] & 0x80)
			add_first_line_zero_byte = 1;

		if(signature[32] & 0x80)
			add_second_line_zero_byte = 1;

		printf("SIGNATURE HEX:\n\r");
		printf("30%2d", 44 + add_first_line_zero_byte + add_second_line_zero_byte); //add 00 if line starts with negative number (highest bit set)
		if(add_first_line_zero_byte)
			printf("022100");
		else
			printf("0220");
		atcab_printbin(signature, ATCA_SIG_SIZE/2, false);

		if(add_second_line_zero_byte)
			printf("022100");
		else
			printf("0220");
		atcab_printbin(signature + ATCA_SIG_SIZE/2, ATCA_SIG_SIZE/2, false);
	}
	else
		MY_LOGE(TAG, "atcab_sign() failed with ret=0x%08X", status);

	status = atcab_verify_extern(digest, signature, pubkey, &is_verified);
	if (status == ATCA_SUCCESS)
	{
		MY_LOGI(TAG, "atcab_verify_extern OK");
		if(is_verified == 0)
			MY_LOGE(TAG, "VERIFY FAILED");
		else
			MY_LOGI(TAG, "VERIFY OK!!!!!!!!!!!!!!!!!!!!!!!");
	}
	else
		MY_LOGE(TAG, "atcab_verify_extern() failed with ret=0x%08X", status);

	// Verify with bad message, should fail
	MY_LOGI(TAG, "VERIFY MUST FAIL");
	digest[0]++;
	status = atcab_verify_extern(digest, signature, pubkey, &is_verified);
	if (status == ATCA_SUCCESS)
	{
		MY_LOGI(TAG, "atcab_verify_extern OK");
		if(is_verified == 0)
			MY_LOGI(TAG, "VERIFY FAILED");
		else
			MY_LOGE(TAG, "VERIFY OK, BUT IT SHOULDN'T BE!!!!!!!!!!!!!!!");
	}
	else
		MY_LOGE(TAG, "atcab_verify_extern() failed with ret=0x%08X", status);

	atcab_release();
}

static int atecc608_test_assert_data_is_locked(void)
{
	bool is_locked = false;
	ATCA_STATUS status = atcab_is_locked(LOCK_ZONE_DATA, &is_locked);

	if (status == ATCA_SUCCESS)
	{
		if (is_locked)
			return 1;
		else
		{
			MY_LOGE(TAG, "Data zone must be locked for this test.");
			atcab_lock_data_zone();
		}
	}
	else
		MY_LOGE(TAG, "atcab_is_locked(LOCK_ZONE_DATA) failed with ret=0x%08X", status);

	return 0;
}

static int atecc608_test_assert_config_is_locked(void)
{
	bool is_locked = false;
	ATCA_STATUS status = atcab_is_locked(LOCK_ZONE_CONFIG, &is_locked);

	if (status == ATCA_SUCCESS)
	{
		if (is_locked)
			return 1;
		else
			MY_LOGE(TAG, "Config zone must be locked for this test.");
	}
	else
		MY_LOGE(TAG, "atcab_is_locked(LOCK_ZONE_CONFIG) failed with ret=0x%08X", status);

	return 0;
}

#if 0
#define I2C_MASTER_NUM I2C_NUM_1
#define I2C_MASTER_SDA_IO 16
#define I2C_MASTER_SCL_IO 17
#define I2C_MASTER_FREQ_HZ 100000
#define I2C_MASTER_TX_BUF_DISABLE 0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0                           /*!< I2C master doesn't need buffer */

int atecc608a_init()
{
	ATCA_STATUS status;
	ATCAIfaceCfg cfg = {
			.iface_type             = ATCA_I2C_IFACE,
			.devtype                = ATECC608A,
			.atcai2c.slave_address  = 0XC0,
			.atcai2c.bus            = 0,
			.atcai2c.baud           = 400000,
			.wake_delay             = 1500,
			.rx_retries             = 20
	};

	status = atcab_init(&cfg);
	return status;
}

/**
 * @brief i2c master initialization
 */
static esp_err_t i2c_master_init()
{
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_param_config(i2c_master_port, &conf);
    return i2c_driver_install(i2c_master_port, conf.mode,
                              I2C_MASTER_RX_BUF_DISABLE,
                              I2C_MASTER_TX_BUF_DISABLE, 0);
}

void atecc608a_test()
{
	uint8_t rand_out[RANDOM_RSP_SIZE];
	uint8_t public_key[ATCA_PUB_KEY_SIZE];
	uint8_t public_key_from_slot[ATCA_PUB_KEY_SIZE];
	uint8_t signature[ATCA_SIG_SIZE];
	const uint16_t private_key_id = 0;
	const uint16_t public_key_id = 9;
	uint8_t sernum[10];
	char version[10];
	bool config_zone_locked;
	bool is_verified;
	ATCA_STATUS status;
	int i;

	status = atcab_init(&cfg);
	if(status == ATCA_SUCCESS)
		MY_LOGI(TAG, "ATECC608A Init OK");
	else
	{
		MY_LOGE(TAG, "ATEC INIT FAILED: %d", status);
		return;
	}

	uint8_t revision[4];
	status = atcab_info(revision);
	if (status != ATCA_SUCCESS)
		MY_LOGE(TAG, "atcab_info() failed with ret=0x%08X", status);
	else
	{
		MY_LOGI(TAG, "REVISION:");
		MY_LOG_BUFFER_HEX(TAG, revision, 4);
	}

	status = atcab_version(version);
	if (status == ATCA_SUCCESS)
	{
		MY_LOGI(TAG, "VERSION: %s", version);
	}
	else
		MY_LOGE(TAG, "atcab_version() failed with ret=0x%08X", status);

	status = atcab_read_serial_number(sernum);
	if (status == ATCA_SUCCESS)
	{
		MY_LOGI(TAG, "SERIAL NUMBER:");
		ESP_LOG_BUFFER_HEX(TAG, sernum, 9);
	}
	else
		MY_LOGE(TAG, "atcab_read_serial_number() failed with ret=0x%08X", status);

	status = atcab_is_locked(LOCK_ZONE_CONFIG, &config_zone_locked);
	if (status == ATCA_SUCCESS)
	{
		MY_LOGI(TAG, "CONFIG ZONE LOCKED: %d", config_zone_locked);
	}
	else
		MY_LOGE(TAG, "atcab_is_locked() failed with ret=0x%08X", status);

#if 0
	if(config_zone_locked == 0) //lock if unlocked
	{
		status = atcab_lock_config_zone();
		if (status == ATCA_SUCCESS)
		{
			MY_LOGI(TAG, "CONFIG ZONE LOCKED!!!!!!!!!!!!!!!!!");
		}
		else
			MY_LOGE(TAG, "atcab_lock_config_zone() failed with ret=0x%08X", status);
	}
#endif

	for (i = 0; i < 5; i++)
	{
		status = atcab_random(rand_out);
		if (status == ATCA_SUCCESS)
		{
			MY_LOGI(TAG, "RANDOM NUMBER:");
			MY_LOG_BUFFER_HEX(TAG, rand_out, 32);
		}
		else
			MY_LOGE(TAG, "ATEC RANDOM NUMBER FAILED: %d", status);
	}

	// Read the config zone
	uint8_t valid_buf[4];
	//status = atcab_read_config_zone(config);
	status = atcab_read_zone(ATCA_ZONE_DATA, public_key_id, 0, 0, valid_buf, 4);
	if (status == ATCA_SUCCESS)
	{
		MY_LOGI(TAG, "CONFIG:");
		ESP_LOG_BUFFER_HEX(TAG, valid_buf, 4);
	}
	else
		MY_LOGE(TAG, "atcab_read_config_zone() failed with ret=0x%08X", status);

	//#define GENERATE_KEYPAIR
#ifdef GENERATE_KEYPAIR
	//status = atcab_genkey(ATCA_TEMPKEY_KEYID, public_key);
	status = atcab_genkey(private_key_id, public_key);
	if (status == ATCA_SUCCESS)
	{
		MY_LOGI(TAG, "PUBLIC KEY:");
		ESP_LOG_BUFFER_HEX(TAG, public_key, ATCA_PUB_KEY_SIZE);
	}
	else
		MY_LOGE(TAG, "atcab_genkey() failed with ret=0x%08X", status);

	// Write public key to slot
	status = atcab_write_pubkey(public_key_id, public_key);
	if (status == ATCA_SUCCESS)
	{
		MY_LOGI(TAG, "PUBLIC KEY WRITTEN TO SLOT:%d", public_key_id);
	}
	else
		MY_LOGE(TAG, "atcab_write_pubkey() failed with ret=0x%08X", status);

	// Read the config zone
	//status = atcab_read_config_zone(config);
	status = atcab_read_zone(ATCA_ZONE_DATA, public_key_id, 0, 0, valid_buf, 4);
	if (status == ATCA_SUCCESS)
	{
		MY_LOGI(TAG, "CONFIG:");
		ESP_LOG_BUFFER_HEX(TAG, valid_buf, 4);
	}
	else
		MY_LOGE(TAG, "atcab_read_config_zone() failed with ret=0x%08X", status);
#endif

	if(test_assert_config_is_locked() == 0)
	{
		return;
	}

	if(test_assert_data_is_locked() == 0)
	{
		return;
	}

	//sha test
	//ATCA_STATUS status = ATCA_GEN_FAIL;
	//uint8_t message[ATCA_SHA256_BLOCK_SIZE + 63];  // just short of two blocks
	uint8_t message[] = "{\"FV\":\"1.1\",\"VI\":\"SEAL AG\",\"VV\":\"1.p12\",\"PG\":\"T8568\",\"MV\":\"Iskra\",\"MM\":\"EM340-DIN.AV2.3.X.S1.PF\",\"MS\":\"******240084S\",\"IS\":\"HEARSAY\",\"IF\":[],\"IT\":\"UNDEFINED\",\"ID\":\"nnn\",\"RD\":[{\"TM\":\"2019-01-30T13:41:58,922+0000 U\",\"TX\":\"C\",\"RV\":268.978,\"RI\":\"1-b:1.8.e\",\"RU\":\"kWh\",\"EI\":62,\"ST\":\"G\"}]}";
	uint8_t digest[ATCA_SHA_DIGEST_SIZE];
	uint8_t rightAnswer[] = { 0x2e, 0x8f, 0xcc, 0x42, 0x30, 0x99, 0x76, 0x3d,
			0x46, 0x5c, 0x6f, 0x05, 0xb3, 0xc2, 0x2e, 0x67,
			0x76, 0x64, 0x7a, 0x19, 0x43, 0x8d, 0x2d, 0x48,
			0x91, 0x3b, 0x17, 0xf4, 0x96, 0xb7, 0x4b, 0xf6 };

	memset(digest, 0x00, ATCA_SHA_DIGEST_SIZE);

	status = atcab_sha(sizeof(message) - 1, message, digest);
	if (status == ATCA_SUCCESS)
	{
		//TEST_ASSERT_EQUAL_MEMORY(rightAnswer, digest, ATCA_SHA_DIGEST_SIZE);
		for(i = 0; i < ATCA_SHA_DIGEST_SIZE; i++)
		{
			if(rightAnswer[i] != digest[i])
				MY_LOGE(TAG, "SHA FAILED i:%d digest:%x rightAnswer:%x", i, digest[i], rightAnswer[i]);
		}
		MY_LOGI(TAG, "SHA TEST FINISHED size:%d SHA:", sizeof(message));
		ESP_LOG_BUFFER_HEX(TAG, digest, ATCA_SHA_DIGEST_SIZE);

		// Sign message
		status = atcab_sign(private_key_id, digest, signature);
		if (status == ATCA_SUCCESS)
		{
			MY_LOGI(TAG, "SIGN OK");
			ESP_LOG_BUFFER_HEX(TAG, signature, ATCA_SIG_SIZE);

			status = atcab_read_pubkey(public_key_id, public_key_from_slot);
			if (status == ATCA_SUCCESS)
			{
				MY_LOGI(TAG, "PUBLIC KEY FROM SLOT %d :", public_key_id);
				ESP_LOG_BUFFER_HEX(TAG, public_key_from_slot, ATCA_PUB_KEY_SIZE);
			}
			else
				MY_LOGE(TAG, "atcab_read_pubkey() failed with ret=0x%08X", status);

			status = atcab_get_pubkey(private_key_id, public_key_from_slot);
			if (status == ATCA_SUCCESS)
			{
				MY_LOGI(TAG, "PUBLIC KEY FROM PRIVATE KEY %d:", private_key_id);
				ESP_LOG_BUFFER_HEX(TAG, public_key_from_slot, ATCA_PUB_KEY_SIZE);
			}
			else
				MY_LOGE(TAG, "atcab_get_pubkey() failed with ret=0x%08X", status);

			status = atcab_verify_extern(digest, signature, public_key, &is_verified);
			//status = atcab_verify_stored(digest, signature, public_key_id, &is_verified);
			if (status == ATCA_SUCCESS)
			{
				if(is_verified == 0)
					MY_LOGE(TAG, "VERIFY FAILED");
				else
					MY_LOGI(TAG, "VERIFY OK!!!!!!!!!!!!!!!!!!!!!!!");
			}
			else
				MY_LOGE(TAG, "atcab_verify_extern() failed with ret=0x%08X", status);

			//verify again with wrong data
			MY_LOGI(TAG, "VERIFY MUST FAIL");
			signature[0]++;//verify must fail!
			status = atcab_verify_extern(digest, signature, public_key, &is_verified);
			if (status == ATCA_SUCCESS)
			{
				if(is_verified == 0)
					MY_LOGE(TAG, "VERIFY FAILED");
				else
					MY_LOGI(TAG, "VERIFY OK!!!!!!!!!!!!!!!!!!!!!!!");
			}
			else
				MY_LOGE(TAG, "atcab_verify_extern() failed with ret=0x%08X", status);
		}
		else
			MY_LOGE(TAG, "atcab_sign() failed with ret=0x%08X", status);


	}
	else
		MY_LOGE(TAG, "atcab_sha() failed with ret=0x%08X", status);

}
#endif

#endif //ATECC608A_CRYPTO

