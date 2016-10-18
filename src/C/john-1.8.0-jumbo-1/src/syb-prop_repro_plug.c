#include "syb-prop_repro.h"
#include "memdbg.h"

static unsigned char joke[] =
    "Q:Whydidtheflydanceonthejar?A:Becausethelidsaidtwisttoopen.HaHa!";

static void rnd_srand(unsigned int seed, int *g_seed)
{
	*g_seed = seed;
}

static int rnd_rand(int *g_seed)
{
	*g_seed = (*g_seed) * 0x343FD + 0x269EC3;
	return ((*g_seed) >> 0x10) & 0x7FFF;
}

/* static void print_block(unsigned char *bytes, int endpos, const char *szoveg)
{
	int i;
	printf("\n%s:\n", szoveg);
	for (i = 0; i < endpos; i++) {
		if (i % 8 == 0)
			printf("\n");
		printf("%.2x", bytes[i]);
	}
	printf("\n");
} */

static void feal_keysch_repro(unsigned char *key, struct JtR_FEAL8_CTX *ctx)
{
	feal_SetKey((ByteType *) key, ctx);
}

static void feal_encrypt_repro(unsigned char *plaintext, unsigned char *ciphertext, struct JtR_FEAL8_CTX *ctx)
{
	feal_Encrypt(plaintext, ciphertext, ctx);
}

static void ccat_pad_repro(unsigned char *password, unsigned char *expanded_password)
{
	int i, pwdlen;
	pwdlen = strlen((const char *) password);
	for (i = 0; i < pwdlen; i++) {
		expanded_password[i] = password[i];
	}
	for (; i < EXPANDED_PWDLEN; i++)
		expanded_password[i] = 0x1D;
}

static void syb_XOR(unsigned char *enc_result, unsigned char *password_block,
    unsigned char *result)
{
	int i;
	for (i = 0; i < 8; i++)
		result[i] = enc_result[i] ^ password_block[i];
}

static void syb_apply_salt_byte(unsigned char salt, unsigned char *password,
    unsigned char *result)
{
	result[0] = salt ^ password[0];
	result[1] = result[0] ^ password[1];
	result[2] = result[1] ^ password[2];
	result[3] = result[1] ^ password[3];
	result[4] = result[1] ^ password[4];
	result[5] = result[1] ^ password[5];
	result[6] = result[1] ^ password[6];
	result[7] = result[1] ^ password[7];
}

static unsigned char salt_prob(int *g_seed)
{
	unsigned int random = rnd_rand(g_seed);

	random = random >> 8;
	random = random % 0xFF;

	return (unsigned char) random;
}

static void meta_keysch_repro(unsigned int seed, unsigned char *password,
    unsigned char *result, int *g_seed, struct JtR_FEAL8_CTX *ctx)
{
	unsigned char expanded_password[EXPANDED_PWDLEN];
	unsigned char salt_byte;
	unsigned char act_FEAL_key[8];
	unsigned char act_XOR_result[8];
	unsigned int start_pos, block_counter;

	ccat_pad_repro(password, expanded_password);

	salt_byte = salt_prob(g_seed);

	syb_apply_salt_byte(salt_byte, expanded_password, act_FEAL_key);
	feal_keysch_repro(act_FEAL_key, ctx);
	start_pos = seed % 0x30;
	feal_encrypt_repro((joke + start_pos), result, ctx);

	syb_XOR(result, (expanded_password + 1), act_XOR_result);

	salt_byte = salt_prob(g_seed);

	syb_apply_salt_byte(salt_byte, act_XOR_result, act_FEAL_key);
	feal_keysch_repro(act_FEAL_key, ctx);
	start_pos = seed % 0x30;
	feal_encrypt_repro((joke + start_pos + 1), (result + 8), ctx);

	for (block_counter = 2; block_counter < 8; block_counter++) {
		syb_XOR(result + 8 * (block_counter - 1),
		    (expanded_password + (block_counter * 8) - 7),
		    act_XOR_result);
		salt_byte = salt_prob(g_seed);
		syb_apply_salt_byte(salt_byte, act_XOR_result,
		    result + block_counter * 8);
	}
}

static void meta_encrypt_repro(unsigned char *meta_keysched, unsigned char *result, struct JtR_FEAL8_CTX *ctx)
{
	unsigned char act_XOR_copy_result[8];
	unsigned int i;

	feal_keysch_repro(meta_keysched, ctx);
	feal_encrypt_repro(joke, result, ctx);
	for (i = 1; i < 8; i++) {
		feal_keysch_repro(meta_keysched + 8 * i, ctx);
		syb_XOR(joke + 8 * i, result + 8 * (i - 1),
		    act_XOR_copy_result);
		feal_encrypt_repro(act_XOR_copy_result, (result + 8 * i), ctx);
	}
}

// static int state;

/* static int myrand(void) {
	int const a = 1103515245;
	int const c = 12345;
	state = a * state + c;

	return (state >> 16) & 0x7FFF;
}
static int myrand(void)

{
	int const a = 69069;
	int const c = 1;
	state = a * state + c;
	return (state >> 16) & 0x7FFF;
} */

void generate_hash(unsigned char *password, unsigned char seed,
    unsigned char *result_hash, int *g_seed, struct JtR_FEAL8_CTX *ctx)
{
	unsigned char meta_keysch_result[META_KEYSCH_LEN];
	unsigned char meta_encrypt_result[META_KEYSCH_LEN];

	// srand(seed);
	rnd_srand(seed, g_seed);
	meta_keysch_repro(seed, password, meta_keysch_result, g_seed, ctx);
	meta_encrypt_repro(meta_keysch_result, meta_encrypt_result, ctx);

	memcpy(result_hash, meta_encrypt_result + META_KEYSCH_LEN - HASH_LEN,
	    HASH_LEN);
}
