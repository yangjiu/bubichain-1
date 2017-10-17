﻿/*
Copyright Bubi Technologies Co., Ltd. 2017 All Rights Reserved.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "common.h"
#include <openssl/ec.h>
#include <openssl/bn.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/ecdsa.h>
#include <openssl/ecdh.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/obj_mac.h>
#include <openssl/x509.h>
#include "ecc_sm2.h"
#include "sm3.h"
#include "strings.h"
namespace utils {

#define free_bn(x) do{ 	\
	if (x != NULL)		\
		BN_free (x);	\
	}while (0) 

#define free_ec_point(x) do{	\
	if (x != NULL)				\
		EC_POINT_free (x);		\
}while (0)	


	//#define ABORT do { \
	//	fflush(stdout); \
	//	fdebug_prt(stderr, "%s:%d: ABORT\n", __FILE__, __LINE__); \
	//	exit (1);	\
	//} while (0)


#define  handle_err {printf("error\n"); assert (false);}

	EC_GROUP* EccSm2::cfca_group_ = NULL;
	EccSm2::EccSm2(EC_GROUP* curv) :group_(curv) {
		valid_ = false;
		dA_ = BN_new();
		pkey_ = EC_POINT_new(curv);
	}

	EccSm2::~EccSm2() {
		free_bn(dA_);
		free_ec_point(pkey_);
	}

	EC_GROUP* EccSm2::GetCFCAGroup() {
		if (cfca_group_ != NULL) {
			return cfca_group_;
		}
		BN_CTX* ctx = BN_CTX_new();
		BN_CTX_start(ctx);
		EC_POINT* G = NULL;
		BIGNUM* p = BN_CTX_get(ctx);
		BIGNUM* a = BN_CTX_get(ctx);
		BIGNUM* b = BN_CTX_get(ctx);
		BIGNUM* xG = BN_CTX_get(ctx);
		BIGNUM* yG = BN_CTX_get(ctx);
		BIGNUM* n = BN_CTX_get(ctx);
		do {
			BN_hex2bn(&p, "FFFFFFFEFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF00000000FFFFFFFFFFFFFFFF");
			BN_hex2bn(&a, "FFFFFFFEFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF00000000FFFFFFFFFFFFFFFC");
			BN_hex2bn(&b, "28E9FA9E9D9F5E344D5A9E4BCF6509A7F39789F515AB8F92DDBCBD414D940E93");
			BN_hex2bn(&xG, "32C4AE2C1F1981195F9904466A39C9948FE30BBFF2660BE1715A4589334C74C7");
			BN_hex2bn(&yG, "BC3736A2F4F6779C59BDCEE36B692153D0A9877CC62A474002DF32E52139F0A0");
			BN_hex2bn(&n, "FFFFFFFEFFFFFFFFFFFFFFFFFFFFFFFF7203DF6B21C6052B53BBF40939D54123");
			cfca_group_ = EC_GROUP_new(EC_GFp_mont_method());
			if (!EC_GROUP_set_curve_GFp(cfca_group_, p, a, b, ctx)) {
				break;
			}
			G = EC_POINT_new(cfca_group_);
			EC_POINT_set_affine_coordinates_GFp(cfca_group_, G, xG, yG, ctx);
			if (!EC_GROUP_set_generator(cfca_group_, G, n, BN_value_one())) {
				break;
			}
		} while (false);
		free_ec_point(G);
		return cfca_group_;
	}

	EC_GROUP* EccSm2::NewGroup(GROUP_TYPE type, std::string phex, std::string ahex, std::string bhex, std::string xGhex, std::string yGhex, std::string nhex) {
		EC_POINT *G = NULL;
		EC_POINT *R = NULL;
		EC_GROUP* group = NULL;
		BN_CTX *ctx = BN_CTX_new();
		BN_CTX_start(ctx);
		BIGNUM* p = BN_CTX_get(ctx);
		BIGNUM* a = BN_CTX_get(ctx);
		BIGNUM* b = BN_CTX_get(ctx);
		BIGNUM* xG = BN_CTX_get(ctx);
		BIGNUM* yG = BN_CTX_get(ctx);
		BIGNUM* n = BN_CTX_get(ctx);
		BIGNUM* bn_4a3 = BN_CTX_get(ctx);
		BIGNUM* bn_27b2 = BN_CTX_get(ctx);
		BIGNUM* bn_4a3_add_27b2 = BN_CTX_get(ctx);
		BIGNUM* bn_191 = BN_CTX_get(ctx);

		BN_hex2bn(&p, phex.c_str());
		BN_hex2bn(&a, ahex.c_str());
		BN_hex2bn(&b, bhex.c_str());
		BN_hex2bn(&xG, xGhex.c_str());
		BN_hex2bn(&yG, yGhex.c_str());
		BN_hex2bn(&n, nhex.c_str());
		std::string err_desc;
		BN_hex2bn(&bn_191, "400000000000000000000000000000000000000000000000");
		bool ret = false;
		do {
			if (type == GFP) {
				//n>2^191
				if (BN_cmp(n, bn_191) <= 0) {
					err_desc = "n is smaller than 2^191";
					break;
				}
				if (!BN_is_prime_ex(p, BN_prime_checks, NULL, NULL)) {
					err_desc = "p is not a prime number";
					break;
				}
				if (!BN_is_odd(p)) {
					err_desc = "p is not a odd number";
					break;
				}
				group = EC_GROUP_new(EC_GFp_mont_method());
				if (group == NULL) {
					err_desc = "internel error";
					break;
				}
				if (!EC_GROUP_set_curve_GFp(group, p, a, b, ctx)) {
					err_desc = "internel error";
					break;
				}
				G = EC_POINT_new(group);
				EC_POINT_set_affine_coordinates_GFp(group, G, xG, yG, ctx);

				if (!EC_GROUP_set_generator(group, G, n, BN_value_one())) {
					err_desc = "internel error";
					break;
				}
				//bn_4a3=4*a^3
				BN_sqr(bn_4a3, a, ctx);
				BN_mul(bn_4a3, bn_4a3, a, ctx);
				BN_mul_word(bn_4a3, 4);
				//bn_27b2=27*b^2
				BN_mul(bn_27b2, b, b, ctx);
				BN_mul_word(bn_27b2, 27);
				//bn_4a3_add_27b2=(4*a^3 + 27*b^)2 mod p
				BN_mod_add(bn_4a3_add_27b2, bn_4a3, bn_27b2, p, ctx);
				if (BN_is_zero(bn_4a3_add_27b2)) {
					err_desc = "(4*a^3 + 27*b^2) mod p = 0";
					break;
				}
				BIGNUM* y2modp = BN_CTX_get(ctx);
				BN_mod_mul(y2modp, yG, yG, p, ctx);
				BIGNUM* tmp = BN_CTX_get(ctx);
				BN_mul(tmp, xG, xG, ctx);
				BN_mul(tmp, tmp, xG, ctx);
				BIGNUM* tmp2 = BN_CTX_get(ctx);
				BN_mul(tmp2, a, xG, ctx);
				BN_add(tmp2, tmp2, b);
				BIGNUM* x3axb = BN_CTX_get(ctx);
				BN_mod_add(x3axb, tmp, tmp2, p, ctx);
				if (BN_cmp(y2modp, x3axb) != 0) {
					err_desc = "y2!=x3+ax+b mod p";
					break;
				}
				//n 是素数
				if (!BN_is_prime_ex(n, BN_prime_checks, NULL, NULL)) {
					err_desc = "n is not a prime number";
					break;
				}
				R = EC_POINT_new(group);
				EC_POINT_mul(group, R, n, NULL, NULL, ctx);
				if (EC_POINT_is_at_infinity(group, R) != 1) {
					err_desc = "nG != O";
					break;
				}
				ret = true;
			}
			else {
				group = EC_GROUP_new(EC_GF2m_simple_method());
				if (group == NULL) {
					err_desc = "internel error";
					break;
				}
				if (!EC_GROUP_set_curve_GF2m(group, p, a, b, ctx)) {
					err_desc = "internel error";
					break;
				}
				G = EC_POINT_new(group);
				EC_POINT_set_affine_coordinates_GF2m(group, G, xG, yG, ctx);

				if (!EC_GROUP_set_generator(group, G, n, BN_value_one())) {
					err_desc = "internel error";
					break;
				}
			}
			
			if (!EC_GROUP_check(group, ctx)){
				EC_GROUP_free(group);
				return NULL;
				break;
			}
			
		} while (false);
		BN_CTX_end(ctx);
		BN_CTX_free(ctx);
		free_ec_point(R);
		free_ec_point(G);
		return group;
	}

	std::string EccSm2::getZA(EC_GROUP* group, std::string id, const EC_POINT* pkey) {
		BN_CTX *ctx = BN_CTX_new();
		BN_CTX_start(ctx);

		BIGNUM *xA = BN_CTX_get(ctx);
		BIGNUM* yA = BN_CTX_get(ctx);
		unsigned char bin[MAX_BITS];
		int len = 0;
		if (EC_METHOD_get_field_type(EC_GROUP_method_of(group)) == NID_X9_62_prime_field) {
			EC_POINT_get_affine_coordinates_GFp(group, pkey, xA, yA, NULL);
		}
		else {
			EC_POINT_get_affine_coordinates_GF2m(group, pkey, xA, yA, NULL);
		}

		const EC_POINT* G = EC_GROUP_get0_generator(group);
		BIGNUM* xG = BN_CTX_get(ctx);
		BIGNUM* yG = BN_CTX_get(ctx);
		if (EC_METHOD_get_field_type(EC_GROUP_method_of(group)) == NID_X9_62_prime_field)
			EC_POINT_get_affine_coordinates_GFp(group, G, xG, yG, ctx);
		else
			EC_POINT_get_affine_coordinates_GF2m(group, G, xG, yG, ctx);

		BIGNUM* a = BN_CTX_get(ctx);
		BIGNUM* b = BN_CTX_get(ctx);
		EC_GROUP_get_curve_GFp(group, NULL, a, b, ctx);
		///国标不写英文注释
		//////////////////////////////////////////////////////////////////////////
		uint32_t entla = id.length() * 8;
		std::string za = "";
		//拼接ENTLA
		unsigned char c1 = entla >> 8;
		unsigned char c2 = entla & 0xFF;
		za.push_back(c1);
		za.push_back(c2);
		//拼接用户ID
		za += id;
		//拼接a
		len = BN_bn2bin(a, bin);
		za.append((char*)bin, len);
		//拼接b
		len = BN_bn2bin(b, bin);
		za.append((char*)bin, len);
		//拼接xG
		len = BN_bn2bin(xG, bin);
		za.append((char*)bin, len);
		//拼接yG
		len = BN_bn2bin(yG, bin);
		za.append((char*)bin, len);
		len = BN_bn2bin(xA, bin);
		za.append((const char*)bin, len); //拼接xA
		len = BN_bn2bin(yA, bin);
		za.append((const char*)bin, len);//拼接yA
		std::string ZA = utils::Sm3::Crypto(za);
		BN_CTX_end(ctx);
		BN_CTX_free(ctx);
		return ZA;
	}

	bool  EccSm2::From(std::string skey_bin) {
		valid_ = false;
		skey_bin_ = skey_bin;
		BN_CTX* ctx = BN_CTX_new();
		BN_CTX_start(ctx);
		BIGNUM* x = BN_CTX_get(ctx);
		BIGNUM* y = BN_CTX_get(ctx);
		BIGNUM* order = BN_CTX_get(ctx);
		EC_GROUP_get_order(group_, order, ctx);
		do {
			BN_bin2bn((const unsigned char*)skey_bin_.c_str(), skey_bin_.length(), dA_);
			if (BN_cmp(dA_, order) == 0) {
				error_ = "dA must be less than order i.e. n ";
				break;;
			}
			if (!EC_POINT_mul(group_, pkey_, dA_, NULL, NULL, NULL)) {
				error_ = "unknown error";
				break;
			}

			if (EC_METHOD_get_field_type(EC_GROUP_method_of(group_)) == NID_X9_62_prime_field) {
				if (!EC_POINT_get_affine_coordinates_GFp(group_, pkey_, x, y, ctx)) {
					error_ = "unknown error";
					break;
				}
			}
			else {
				if (!EC_POINT_get_affine_coordinates_GF2m(group_, pkey_, x, y, ctx)) {
					error_ = "unknown error";
					break;
				}
			}
			//国标第一版要求，公钥的x，y坐标第一字节不能为0
			//if (BN_num_bytes(order) > BN_num_bytes(x) || BN_num_bytes(order) > BN_num_bytes(y)) {
			//	error_ = "SM2 rule: the first byte of publickey can not be zero"; 
			//	break;
			//}
			valid_ = true;
		} while (false);
		BN_CTX_end(ctx);
		BN_CTX_free(ctx);

		return valid_;
	}

	bool  EccSm2::NewRandom() {
		BN_CTX* ctx = BN_CTX_new();
		BN_CTX_start(ctx);
		BIGNUM* x = BN_CTX_get(ctx);
		BIGNUM* y = BN_CTX_get(ctx);
		BIGNUM* order = BN_CTX_get(ctx);
		EC_GROUP_get_order(group_, order, ctx);

		do {
			if (!BN_rand_range(dA_, order)) {
				continue;
			}
			if (BN_cmp(dA_, order) == 0) {
				continue;
			}
			if (!EC_POINT_mul(group_, pkey_, dA_, NULL, NULL, NULL))
				continue;

			if (EC_METHOD_get_field_type(EC_GROUP_method_of(group_)) == NID_X9_62_prime_field) {
				if (!EC_POINT_get_affine_coordinates_GFp(group_, pkey_, x, y, ctx)) {
					continue;
				}
			}
			else {
				if (!EC_POINT_get_affine_coordinates_GF2m(group_, pkey_, x, y, ctx)) {
					continue;
				}
			}
			
			//国标第一版要求，公钥的x，y坐标第一字节不能为0
			//if (BN_num_bytes(order) > BN_num_bytes(x) || BN_num_bytes(order) > BN_num_bytes(y)) {
			//	continue;
			//}
			break;
		} while (true);
		BN_CTX_end(ctx);
		BN_CTX_free(ctx);
		valid_ = true;
		return valid_;
	}

	std::string EccSm2::getSkeyHex() {
		char* buff = BN_bn2hex(dA_);
		std::string str = "";
		str.append(buff);
		OPENSSL_free(buff);
		return str;
	}

	std::string EccSm2::getSkeyBin() {
		unsigned char buff[32] = { 0 };
		BN_bn2bin(dA_, (unsigned char*)&buff);
		std::string str = "";
		str.append((char*)buff, 32);
		return str;
	}

	std::string EccSm2::Sign(const std::string& id, const std::string& msg) {
		std::string sigr;
		std::string sigs;

		if (!valid_) {
			return "";
		}
		bool ok = false;

		BN_CTX *ctx = BN_CTX_new();
		BN_CTX_start(ctx);
		EC_POINT* pt1 = EC_POINT_new(group_);
		std::string M, stre, ZA;
		int dgstlen;
		unsigned char dgst[32];
		BIGNUM* r = BN_CTX_get(ctx);
		BIGNUM* s = BN_CTX_get(ctx);
		BIGNUM* e = BN_CTX_get(ctx);
		BIGNUM* bn = BN_CTX_get(ctx);
		BIGNUM* k = BN_CTX_get(ctx);
		BIGNUM* x1 = BN_CTX_get(ctx);
		BIGNUM* order = BN_CTX_get(ctx);
		BIGNUM* p = BN_CTX_get(ctx);

		if (!group_ || !dA_) {
			goto end;
		}

		if (!r || !s || !ctx || !order || !e || !bn) {
			goto end;
		}
		EC_GROUP_get_order(group_, order, ctx);
		EC_GROUP_get_curve_GFp(group_, p, NULL, NULL, ctx);

		//第一步  M^ = ZA||M
		ZA = getZA(group_, id, pkey_);
		M = ZA + msg;

		//第二步 e=Hv(M^)
		stre = utils::Sm3::Crypto(M);

		dgstlen = sizeof(dgst) / sizeof(unsigned char);
		memcpy(dgst, stre.c_str(), dgstlen);
		if (!BN_bin2bn(dgst, dgstlen, e)) {
			goto end;
		}

		do {
			//第三步 产生随机数k [1,n-1]
			do {
				do {
					if (!BN_rand_range(k, order)) {
						goto end;
					}
				} while (BN_is_zero(k) || (BN_ucmp(k, order) == 0));

				//第四步  计算pt1(x1,y1) = [K]G这个点
				if (!EC_POINT_mul(group_, pt1, k, NULL, NULL, ctx)) {
					goto end;
				}

				//获取pt1的坐标
				if (EC_METHOD_get_field_type(EC_GROUP_method_of(group_)) == NID_X9_62_prime_field) {
					if (!EC_POINT_get_affine_coordinates_GFp(group_, pt1, x1, NULL, ctx)) {
						goto end;
					}
				}
				else /* NID_X9_62_characteristic_two_field */ {
					if (!EC_POINT_get_affine_coordinates_GF2m(group_, pt1, x1, NULL, ctx)) {
						goto end;
					}
				}

				if (!BN_nnmod(x1, x1, order, ctx)) {
					goto end;
				}

			} while (BN_is_zero(x1));

			//第五步 计算 r = (e + x1) mod n
			BN_copy(r, x1);
			if (!BN_mod_add(r, r, e, order, ctx)) {
				goto end;
			}

			if (!BN_mod_add(bn, r, k, order, ctx)) {
				goto end;
			}

			//确保r!=0 且 r+k!=n 也就是 (r+k) != 0 mod n 
			if (BN_is_zero(r) || BN_is_zero(bn)) {
				continue;
			}

			//第六步 计算 s = ((1 + d)^-1 * (k - rd)) mod n 
			if (!BN_one(bn)) {
				goto end;
			}

			if (!BN_mod_add(s, dA_, bn, order, ctx)) {
				goto end;
			}
			if (!BN_mod_inverse(s, s, order, ctx)) {
				goto end;
			}

			if (!BN_mod_mul(bn, r, dA_, order, ctx)) {
				goto end;
			}
			if (!BN_mod_sub(bn, k, bn, order, ctx)) {
				goto end;
			}
			if (!BN_mod_mul(s, s, bn, order, ctx)) {
				goto end;
			}

			//确保s != 0 
			if (!BN_is_zero(s)) {
				break;
			}
			//第七步 输出r和s
		} while (1);

		ok = true;
	end:

		int olen = BN_num_bytes(p);

		sigr.resize(0);
		unsigned char rr[MAX_BITS];
		int rn = BN_bn2bin(r, rr);
		sigr.append(olen - rn, 0);
		sigr.append((char*)rr, rn);

		sigs.resize(0);
		unsigned char ss[MAX_BITS];
		int sn = BN_bn2bin(s, ss);
		sigs.append(olen - sn, 0);
		sigs.append((char*)ss, sn);

		free_ec_point(pt1);
		BN_CTX_free(ctx);
		return sigr + sigs;
	}


	int EccSm2::verify(EC_GROUP* group, const std::string& pkey, 
		const std::string& id, const std::string& msg, const std::string& strsig) {
		std::string px = "";
		std::string py = "";
		int len = (pkey.size() - 1) / 2;
		px = pkey.substr(1, len);
		py = pkey.substr(1 + len, len);

		std::string sigr = strsig.substr(0, strsig.size() / 2);
		std::string sigs = strsig.substr(strsig.size() / 2, strsig.size() / 2);

		int ret = -1;
		EC_POINT *pub_key = NULL;
		EC_POINT *point = NULL;
		BN_CTX *ctx = NULL;

		ECDSA_SIG* sig = NULL;

		std::string M, ZA, stre;

		pub_key = EC_POINT_new(group);
		point = EC_POINT_new(group);
		unsigned char dgst[32];
		int dgstlen;

		ctx = BN_CTX_new();
		BN_CTX_start(ctx);

		BIGNUM* xp = BN_CTX_get(ctx);
		BIGNUM* yp = BN_CTX_get(ctx);
		BIGNUM*x1 = BN_CTX_get(ctx);
		BIGNUM*R = BN_CTX_get(ctx);
		BIGNUM *order = BN_CTX_get(ctx);
		BIGNUM *e = BN_CTX_get(ctx);
		BIGNUM *t = BN_CTX_get(ctx);

		EC_GROUP_get_order(group, order, ctx);
		BN_bin2bn((const unsigned char*)px.c_str(), px.size(), xp);
		BN_bin2bn((const unsigned char*)py.c_str(), py.size(), yp);
		if (EC_METHOD_get_field_type(EC_GROUP_method_of(group)) == NID_X9_62_prime_field) {
			EC_POINT_set_affine_coordinates_GFp(group, pub_key, xp, yp, NULL);
		}
		else {
			EC_POINT_set_affine_coordinates_GF2m(group, pub_key, xp, yp, NULL);
		}

		sig = ECDSA_SIG_new();
		BN_bin2bn((const unsigned char*)sigr.c_str(), sigr.size(), sig->r);
		BN_bin2bn((const unsigned char*)sigs.c_str(), sigs.size(), sig->s);

		e = BN_CTX_get(ctx);
		t = BN_CTX_get(ctx);
		if (!ctx || !order || !e || !t) {
			goto end;
		}

		// 第1,2步: r,s 在 [1, n-1]范围  且 r + s != 0 (mod n) 
		if (BN_is_zero(sig->r) ||
			BN_is_negative(sig->r) ||
			BN_ucmp(sig->r, order) >= 0 ||
			BN_is_zero(sig->s) ||
			BN_is_negative(sig->s) ||
			BN_ucmp(sig->s, order) >= 0) {
			ret = 0;
			goto end;
		}

		//第5步 (r' + s') != 0 mod n
		if (!BN_mod_add(t, sig->r, sig->s, order, ctx)) {
			goto end;
		}
		if (BN_is_zero(t)) {
			ret = 0;
			goto end;
		}

		//第3步 计算 _M = ZA||M'
		ZA = getZA(group, id, pub_key);
		M = ZA + msg;

		//第4步  计算e' = Hv(_M)
		stre = utils::Sm3::Crypto(M);
		memcpy(dgst, stre.c_str(), stre.length());
		dgstlen = stre.length();

		if (!BN_bin2bn(dgst, dgstlen, e)) {
			goto end;
		}

		//第6步计算点 (x',y')=sG + tP  P是公钥点

		if (!EC_POINT_mul(group, point, sig->s, pub_key, t, ctx)) {
			goto end;
		}
		if (EC_METHOD_get_field_type(EC_GROUP_method_of(group)) == NID_X9_62_prime_field) {
			if (!EC_POINT_get_affine_coordinates_GFp(group, point, x1, NULL, ctx)) {
				goto end;
			}
		}
		else /* NID_X9_62_characteristic_two_field */ {
			if (!EC_POINT_get_affine_coordinates_GF2m(group, point, x1, NULL, ctx)) {
				goto end;
			}
		}
		if (!BN_nnmod(x1, x1, order, ctx)) {
			goto end;
		}

		//第7步 R=(e+x') mod n

		if (!BN_mod_add(R, x1, e, order, ctx)) {
			goto end;
		}

		BN_nnmod(R, R, order, ctx);

		if (BN_ucmp(R, sig->r) == 0) {
			ret = 1;
		}
		else {
			ret = 0;
		}

	end:
		free_ec_point(point);
		free_ec_point(pub_key);

		BN_CTX_end(ctx);
		BN_CTX_free(ctx);
		ECDSA_SIG_free(sig);
		if (ret != 1) {
			int x = 2;
		}

		return ret;
	}


	std::string EccSm2::GetPublicKey() {
		std::string xPA;
		std::string yPA;
		if (!valid_) {
			return "";
		}
		BN_CTX *ctx = BN_CTX_new();
		BN_CTX_start(ctx);
		BIGNUM *bn_x = BN_CTX_get(ctx);
		BIGNUM *bn_y = BN_CTX_get(ctx);

		if (EC_METHOD_get_field_type(EC_GROUP_method_of(group_)) == NID_X9_62_prime_field)
			EC_POINT_get_affine_coordinates_GFp(group_, pkey_, bn_x, bn_y, NULL);
		else
			EC_POINT_get_affine_coordinates_GF2m(group_, pkey_, bn_x, bn_y, NULL);

		xPA.resize(0);
		unsigned char xx[MAX_BITS];
		BIGNUM* order = BN_CTX_get(ctx);
		EC_GROUP_get_order(group_, order, ctx);

		BIGNUM* p = BN_CTX_get(ctx);
		EC_GROUP_get_curve_GFp(group_, p, NULL, NULL, ctx);
		int olen = BN_num_bytes(p);

		int xlen = BN_bn2bin(bn_x, xx);
		xPA.append(olen - xlen, 0);
		xPA.append((char*)xx, xlen);
		yPA.resize(0);
		unsigned char yy[MAX_BITS];
		int ylen = BN_bn2bin(bn_y, yy);
		yPA.append(olen - ylen, 0);
		yPA.append((char*)yy, ylen);
		BN_CTX_end(ctx);
		BN_CTX_free(ctx);
		std::string out;
		out.push_back(4);
		out += xPA;
		out += yPA;
		return out;
	}

}
